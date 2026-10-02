RPM packaging
=============

Copyright 2026 Qore Technologies, s.r.o.

The portable recipe targets Fedora, Enterprise Linux and openSUSE with the
Qore 3.0 SDK and matching qore-rpm-macros. It packages the native LDAP module,
six source/compiled helper modules, compiler metadata, seven qldap tools and
their manual pages. Seven HTML references form a separate documentation package.
System OpenLDAP and Cyrus SASL supply the protocol implementations.

From qore-packaging, prepare a clean, committed source bundle::

    python3 tools/packaging.py prepare --repo ../module-openldap --ref COMMIT \
      --name qore-openldap-module --version 2.0 \
      --spec qore-openldap-module.spec --exclude m4/acx_pthread.m4 \
      --output work/openldap-source
    python3 tools/build-local.py --source work/openldap-source \
      --image TARGET_SDK_IMAGE --output results/openldap-build --jobs 2

The unused legacy macro is excluded for the license reason documented in
Debian's copyright inventory. Binary packages use the project's MIT license
alternative; both upstream license texts and source notices are retained.

Default builds keep all tests and strict API documentation enabled. The test
fixture creates a private MDB directory and an ephemeral TLS identity, starts
unprivileged slapd on a loopback port, and waits for its startup log event.
Logs drain continuously and the process group is terminated and reaped on
success or failure. Existing services and LDAP configuration are not modified.

The suite covers SASL DIGEST-MD5 and CRAM-MD5 as well as normal LDAP operations,
negative authentication, transactions, helper modules and asynchronous calls.
CLI checks cover all seven help commands, authenticated search, password
redaction and StartTLS with mandatory certificate verification. SASL mechanism
plugins are test dependencies; deploy the mechanisms required by your server.
Leap 16 qualification uses qore-packaging's tested DIGEST-MD5 plugin backport:
the distribution's original plugin can crash after OpenSSL 3 cipher initialization
fails. When deploying DIGEST-MD5 on Leap, install the backport with the
``cyrus-sasl-digestmd5(openssl3-private-provider)`` capability. Other system
SASL libraries, plugins and crypto configuration remain unchanged.

Installed tests use the packaged native and compiled artifacts outside the
source tree. Run unprivileged in a disposable runtime image::

    python3 -B -W error rpm/test_fixture.py -v
    python3 -B -W error rpm/run-tests.py --installed

Add ``--compiler`` on an SDK image to compile and execute a named-argument
LDAP query. Test-only slapd and client dependencies are not runtime requirements
of the Qore module.
