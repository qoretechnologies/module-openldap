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

echo "export QORE_UID=1000" >> ${ENV_FILE}
echo "export QORE_GID=1000" >> ${ENV_FILE}

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
if ! grep -q "^qore:x:${QORE_GID}" /etc/group; then
    addgroup -g ${QORE_GID} qore
fi
if ! grep -q "^qore:x:${QORE_UID}" /etc/passwd; then
    adduser -u ${QORE_UID} -D -G qore -h /home/qore -s /bin/bash qore
fi

# own everything by the qore user
chown -R qore:qore ${MODULE_SRC_DIR}

# Install OpenLDAP test server
echo && echo "-- installing OpenLDAP server for testing --"
apk add --no-cache openldap openldap-clients openldap-back-mdb

# Configure slapd
echo "Configuring OpenLDAP..."

# Create clean directories
rm -rf /var/lib/openldap/openldap-data/*
rm -rf /etc/openldap/slapd.d/*
mkdir -p /var/lib/openldap/run
mkdir -p /etc/openldap/slapd.d
mkdir -p /var/lib/openldap/openldap-data
chown -R ldap:ldap /var/lib/openldap /etc/openldap

# Start slapd directly with slapd.conf (Alpine doesn't require cn=config)
cat > /etc/openldap/slapd.conf << 'SLAPD_CONF'
include         /etc/openldap/schema/core.schema
include         /etc/openldap/schema/cosine.schema
include         /etc/openldap/schema/nis.schema
include         /etc/openldap/schema/inetorgperson.schema

pidfile         /var/lib/openldap/run/slapd.pid
argsfile        /var/lib/openldap/run/slapd.args

modulepath      /usr/lib/openldap
moduleload      back_mdb

database        mdb
maxsize         1073741824
suffix          "dc=example,dc=com"
rootdn          "cn=admin,dc=example,dc=com"
rootpw          admin
directory       /var/lib/openldap/openldap-data

index           objectClass eq
SLAPD_CONF

chown -R ldap:ldap /etc/openldap /var/lib/openldap

# Start slapd directly with slapd.conf
slapd -f /etc/openldap/slapd.conf -h "ldap://localhost:389" -u ldap -g ldap
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
