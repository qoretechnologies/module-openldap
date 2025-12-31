#!/bin/bash
#
# Run OpenLDAP module tests
#
# Usage: ./run-tests.sh [--docker] [--no-cleanup]
#
# Options:
#   --docker     Start a Docker OpenLDAP server for testing
#   --no-cleanup Don't stop Docker container after tests
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
USE_DOCKER=0
CLEANUP=1

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --docker)
            USE_DOCKER=1
            shift
            ;;
        --no-cleanup)
            CLEANUP=0
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Start Docker container if requested
if [ $USE_DOCKER -eq 1 ]; then
    echo "Starting OpenLDAP Docker container..."
    cd "$SCRIPT_DIR"
    docker-compose up -d

    # Wait for OpenLDAP to be ready
    echo "Waiting for OpenLDAP to be ready..."
    for i in {1..30}; do
        if docker-compose exec -T openldap ldapsearch -x -H ldap://localhost -b "dc=example,dc=com" -D "cn=admin,dc=example,dc=com" -w admin > /dev/null 2>&1; then
            echo "OpenLDAP is ready!"
            break
        fi
        if [ $i -eq 30 ]; then
            echo "Timeout waiting for OpenLDAP"
            docker-compose logs
            exit 1
        fi
        sleep 1
    done

    # Load test data
    echo "Loading test data..."
    docker-compose exec -T openldap ldapadd -x -H ldap://localhost -D "cn=admin,dc=example,dc=com" -w admin -f /container/service/slapd/assets/config/bootstrap/ldif/custom/test-data.ldif || true
fi

# Set environment variables for tests
export LDAP_URI="${LDAP_URI:-ldap://localhost:389}"
export LDAP_BINDDN="${LDAP_BINDDN:-cn=admin,dc=example,dc=com}"
export LDAP_PASSWORD="${LDAP_PASSWORD:-admin}"
export LDAP_BASEDN="${LDAP_BASEDN:-dc=example,dc=com}"
export LDAP_PEOPLEDN="${LDAP_PEOPLEDN:-ou=people,dc=example,dc=com}"
export LDAP_GROUPSDN="${LDAP_GROUPSDN:-ou=groups,dc=example,dc=com}"

# Run the tests
echo "Running tests..."
cd "$SCRIPT_DIR"
qore openldap.qtest -v
TEST_RESULT=$?

# Cleanup Docker container if requested
if [ $USE_DOCKER -eq 1 ] && [ $CLEANUP -eq 1 ]; then
    echo "Stopping OpenLDAP Docker container..."
    docker-compose down -v
fi

exit $TEST_RESULT
