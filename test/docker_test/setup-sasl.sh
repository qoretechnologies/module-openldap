#!/bin/bash
# Setup SASL authentication for testing
# Run this inside the OpenLDAP container after slapd starts

set -e

# Install saslpasswd2 if not present
if ! command -v saslpasswd2 &>/dev/null; then
    apt-get update -qq && apt-get install -y -qq sasl2-bin >/dev/null 2>&1
fi

# Create SASL user in sasldb
echo "testpassword" | saslpasswd2 -c -p -u example.com testuser

# Set permissions so slapd can read sasldb
chown openldap:openldap /etc/sasldb2 2>/dev/null || true
chmod 640 /etc/sasldb2

# Configure SASL-to-LDAP DN mapping in slapd
# Maps SASL identity "uid=testuser,cn=example.com,cn=DIGEST-MD5,cn=auth"
# to LDAP DN "uid=testuser1,ou=people,dc=example,dc=com"
ldapmodify -Y EXTERNAL -H ldapi:/// <<'EOF'
dn: cn=config
changetype: modify
add: olcAuthzRegexp
olcAuthzRegexp: uid=([^,]*),cn=example.com,cn=.*,cn=auth uid=$1,ou=people,dc=example,dc=com
EOF

echo "SASL setup complete"
sasldblistusers2
