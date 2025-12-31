#!/bin/bash

set -e
set -x

ENV_FILE=/tmp/env.sh

. ${ENV_FILE}

# setup MODULE_SRC_DIR env var
cwd=`pwd`
if [ -z "${MODULE_SRC_DIR}" ]; then
    if [ -e "$cwd/src/openldap-module.cpp" ]; then
        MODULE_SRC_DIR=$cwd
    else
        MODULE_SRC_DIR=$WORKDIR/module-openldap
    fi
fi
echo "export MODULE_SRC_DIR=${MODULE_SRC_DIR}" >> ${ENV_FILE}

echo "export QORE_UID=999" >> ${ENV_FILE}
echo "export QORE_GID=999" >> ${ENV_FILE}

. ${ENV_FILE}

export MAKE_JOBS=4

# build module and install
echo && echo "-- building module --"
mkdir -p ${MODULE_SRC_DIR}/build
cd ${MODULE_SRC_DIR}/build
cmake .. -DCMAKE_BUILD_TYPE=debug -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}
make -j${MAKE_JOBS}
make install

# add Qore user and group
groupadd -o -g ${QORE_GID} qore || true
useradd -o -m -d /home/qore -u ${QORE_UID} -g ${QORE_GID} qore || true

# own everything by the qore user
chown -R qore:qore ${MODULE_SRC_DIR}

# Install OpenLDAP test server
echo && echo "-- installing OpenLDAP server for testing --"
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y slapd ldap-utils

# Configure slapd
echo "Configuring OpenLDAP..."
service slapd stop || true

# Create test directory structure
rm -rf /var/lib/ldap/*
rm -rf /etc/ldap/slapd.d/*

# Initialize with our test domain
cat > /tmp/slapd.conf << 'SLAPD_CONF'
include         /etc/ldap/schema/core.schema
include         /etc/ldap/schema/cosine.schema
include         /etc/ldap/schema/nis.schema
include         /etc/ldap/schema/inetorgperson.schema

pidfile         /var/run/slapd/slapd.pid
argsfile        /var/run/slapd/slapd.args

modulepath      /usr/lib/ldap
moduleload      back_mdb

database        mdb
maxsize         1073741824
suffix          "dc=example,dc=com"
rootdn          "cn=admin,dc=example,dc=com"
rootpw          admin
directory       /var/lib/ldap

index           objectClass eq
SLAPD_CONF

slaptest -f /tmp/slapd.conf -F /etc/ldap/slapd.d
chown -R openldap:openldap /etc/ldap/slapd.d /var/lib/ldap

# Start slapd
service slapd start
sleep 2

# Add base entries
ldapadd -x -H ldap://localhost -D "cn=admin,dc=example,dc=com" -w admin << 'BASE_LDIF'
dn: dc=example,dc=com
objectClass: top
objectClass: dcObject
objectClass: organization
o: Example Organization
dc: example

dn: ou=people,dc=example,dc=com
objectClass: organizationalUnit
ou: people

dn: ou=groups,dc=example,dc=com
objectClass: organizationalUnit
ou: groups

dn: uid=testuser1,ou=people,dc=example,dc=com
objectClass: inetOrgPerson
objectClass: posixAccount
objectClass: shadowAccount
uid: testuser1
cn: Test User 1
sn: User1
uidNumber: 10001
gidNumber: 10001
homeDirectory: /home/testuser1
loginShell: /bin/bash
userPassword: testpass1
BASE_LDIF

# run the tests
echo && echo "-- running tests --"
export QORE_MODULE_DIR=${MODULE_SRC_DIR}/qlib:${QORE_MODULE_DIR}
export LDAP_URI="ldap://localhost:389"
export LDAP_BINDDN="cn=admin,dc=example,dc=com"
export LDAP_PASSWORD="admin"
export LDAP_BASEDN="dc=example,dc=com"
export LDAP_PEOPLEDN="ou=people,dc=example,dc=com"
export LDAP_GROUPSDN="ou=groups,dc=example,dc=com"

cd ${MODULE_SRC_DIR}/test
su -c "qore openldap.qtest -v" qore

echo && echo "-- tests completed successfully --"
