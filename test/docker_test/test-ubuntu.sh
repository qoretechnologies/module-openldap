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

# Configure slapd - use the default debconf-installed config and just add our test data
echo "Configuring OpenLDAP..."

# Reconfigure slapd non-interactively with our test domain
service slapd stop || true
echo "slapd slapd/internal/adminpw password admin" | debconf-set-selections
echo "slapd slapd/internal/generated_adminpw password admin" | debconf-set-selections
echo "slapd slapd/password1 password admin" | debconf-set-selections
echo "slapd slapd/password2 password admin" | debconf-set-selections
echo "slapd slapd/domain string example.com" | debconf-set-selections
echo "slapd shared/organization string Example" | debconf-set-selections
echo "slapd slapd/purge_database boolean true" | debconf-set-selections
echo "slapd slapd/move_old_database boolean true" | debconf-set-selections
echo "slapd slapd/no_configuration boolean false" | debconf-set-selections
rm -rf /var/lib/ldap/* /etc/ldap/slapd.d/*
dpkg-reconfigure -f noninteractive slapd

# Start slapd
service slapd start
sleep 2

# Add test OUs and entries (base dc=example,dc=com is created by dpkg-reconfigure)
ldapadd -x -H ldap://localhost -D "cn=admin,dc=example,dc=com" -w admin << 'BASE_LDIF'
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
QORE_BIN=$(which qore)
su -c "${QORE_BIN} openldap.qtest -v" qore

echo && echo "-- tests completed successfully --"
