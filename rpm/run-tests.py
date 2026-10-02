#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Qualify installed LDAP helpers with a private SASL and StartTLS directory."""
import argparse
from contextlib import contextmanager
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import re
import select
import shutil
import signal
import socket
import subprocess
import tempfile
import time

MODULES = ('LdapHelper', 'LdapConnectionPool', 'LdapAsync', 'LdifHelper', 'LdapActiveDirectory', 'LdapCli')


def await_startup(stream, timeout=20):
    deadline = time.monotonic() + timeout
    output = bytearray()
    while not re.search(rb'\bslapd starting\r?\n', output):
        ready, _, _ = select.select([stream], [], [], max(0, deadline - time.monotonic()))
        if not ready:
            raise RuntimeError('LDAP startup deadline exceeded: ' + output.decode(errors='replace'))
        data = os.read(stream.fileno(), 4096)
        if not data:
            raise RuntimeError('LDAP server exited before startup: ' + output.decode(errors='replace'))
        output.extend(data)
    return output.decode(errors='replace')


@contextmanager
def running_server(command, env):
    server = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              start_new_session=True)
    # Drain the pipe throughout the suites, so negative authentication tests
    # cannot block slapd by filling its logging pipe.
    with ThreadPoolExecutor(max_workers=1) as reader:
        output = None
        try:
            print(await_startup(server.stdout), end='', flush=True)
            output = reader.submit(server.stdout.read)
            yield server
        finally:
            try:
                os.killpg(server.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                server.wait(timeout=20)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(server.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                server.wait()
                raise
            finally:
                try:
                    text = output.result() if output else server.stdout.read()
                finally:
                    server.stdout.close()
                print(text.decode(errors='replace'), end='', flush=True)


def module_paths(build, env):
    paths = subprocess.check_output(['/usr/bin/qore', '--module-path'], env=env, text=True).strip().split(':')
    native_dirs = [build.resolve()] if build else [Path(path) for path in paths]
    natives = [file for directory in native_dirs for file in directory.glob('openldap-api-*.qmod')]
    if len(natives) != 1:
        raise RuntimeError('Expected one native OpenLDAP module: ' + repr(natives))
    aot_dir = build.resolve() / 'qlib-qmod' if build else natives[0].parent
    modules = []
    for name in MODULES:
        candidates = [path for path in (aot_dir / (name + '.qmod'), aot_dir / name / (name + '.qmod'))
                      if path.is_file()]
        if len(candidates) != 1:
            raise RuntimeError('Expected one compiled artifact for ' + name)
        modules.append(candidates[0])
    env.update(QORE_MODULE_DIR=':'.join(dict.fromkeys([str(natives[0].parent), str(aot_dir), *paths])),
               QORE_MODULE_DIR_ONLY='1')
    return [*natives, *modules]


def server_config(root):
    schema = Path('/etc/openldap/schema')
    schemas = [schema / (name + '.schema') for name in ('core', 'cosine', 'inetorgperson', 'nis')]
    if not all(path.is_file() for path in schemas):
        raise RuntimeError('OpenLDAP server schemas are required')
    backends = subprocess.run(['/usr/sbin/slapd', '-VVV'], text=True, check=True,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout
    module = ''
    if not re.search(r'^\s*mdb\s*$', backends, re.M):
        candidates = [path for directory in ('/usr/lib64/openldap', '/usr/lib/openldap')
                      for path in Path(directory).glob('back_mdb.so')]
        if len(candidates) != 1:
            raise RuntimeError('Expected the MDB backend built in or as one loadable module')
        module = f'moduleload {candidates[0]}\n'
    return '\n'.join(f'include {path}' for path in schemas) + '\n' + module + f'''
pidfile {root}/slapd.pid
argsfile {root}/slapd.args
sasl-host localhost
sasl-realm example.com
sasl-secprops none
TLSCertificateFile {root}/tls.crt
TLSCertificateKeyFile {root}/tls.key
authz-regexp "uid=([^,]+),cn=example.com,cn=[^,]+,cn=auth" "uid=$1,ou=people,dc=example,dc=com"
authz-regexp "uid=([^,]+),cn=[^,]+,cn=auth" "uid=$1,ou=people,dc=example,dc=com"
database mdb
maxsize 1073741824
suffix "dc=example,dc=com"
rootdn "cn=admin,dc=example,dc=com"
rootpw admin
directory {root}/db
access to attrs=userPassword by self write by anonymous auth by * none
access to * by users write by * read
'''


def check_cli(qore, directory, env, root):
    for name in ('qldapsearch', 'qldapadd', 'qldapmodify', 'qldapdelete', 'qldapcompare',
                 'qldappasswd', 'qldapwhoami'):
        subprocess.run([*qore, str(directory / name), '--help'], env=env, cwd=root,
                       check=True, timeout=30, stdout=subprocess.DEVNULL)
    auth = ['-C', '-H', env['LDAP_URI'], '-D', env['LDAP_BINDDN'], '-w', env['LDAP_PASSWORD']]
    query = subprocess.check_output([*qore, str(directory / 'qldapsearch'), *auth, '-v',
            '-b', env['LDAP_BASEDN'], '-s', 'base', '(objectClass=*)', 'dc'],
            env=env, cwd=root, text=True, timeout=30)
    if '<redacted>' not in query or 'example' not in query or re.search(r'password[^,}]*admin', query):
        raise RuntimeError('Authenticated CLI search or password redaction failed')
    identity = subprocess.check_output([*qore, str(directory / 'qldapwhoami'), *auth, '-Z'],
                                      env=env, cwd=root, text=True, timeout=30)
    if env['LDAP_BINDDN'].lower() not in identity.lower():
        raise RuntimeError('Verified StartTLS CLI identity failed')
    print('All seven CLI help commands, search, password redaction and verified StartTLS passed.', flush=True)


def run(build=None, compiler=False):
    if os.getuid() == 0:
        raise RuntimeError('LDAP package tests must run unprivileged')
    source = Path(__file__).resolve().parents[1]
    env = os.environ.copy()
    for key in list(env):
        if key.startswith(('LDAP', 'SASL_')) or key in (
                'QORE_MODULE_DIR', 'QORE_MODULE_DIR_ONLY', 'QORE_INCLUDE_DIR', 'LD_LIBRARY_PATH', 'LD_PRELOAD'):
            env.pop(key)
    env.update(LC_ALL='C.UTF-8', TZ='UTC')
    modules = module_paths(build, env)
    qore = ['/usr/bin/qore', '-b', '--enable-debug']
    for module in modules:
        qore += ['-l', str(module)]
    with tempfile.TemporaryDirectory(prefix='qore-ldap-rpm-') as directory:
        root = Path(directory)
        (root / 'db').mkdir(mode=0o700)
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                        '-subj', '/CN=localhost', '-addext', 'subjectAltName=IP:127.0.0.1,DNS:localhost',
                        '-keyout', str(root / 'tls.key'), '-out', str(root / 'tls.crt')],
                       check=True, timeout=60, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (root / 'slapd.conf').write_text(server_config(root))
        (root / 'ldap.conf').write_text(f'TLS_CACERT {root}/tls.crt\nTLS_REQCERT demand\nSASL_NOCANON on\n')
        env.update(HOME=str(root), LDAPCONF=str(root / 'ldap.conf'), LDAPRC=str(root / 'ldap.conf'),
                   LDAPTLS_CACERT=str(root / 'tls.crt'), LDAPTLS_REQCERT='demand')
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            port = listener.getsockname()[1]
        env.update(LDAP_URI=f'ldap://127.0.0.1:{port}', LDAP_BINDDN='cn=admin,dc=example,dc=com',
                   LDAP_PASSWORD='admin', LDAP_BASEDN='dc=example,dc=com',
                   LDAP_PEOPLEDN='ou=people,dc=example,dc=com', LDAP_GROUPSDN='ou=groups,dc=example,dc=com',
                   LDAP_SASL_URI=f'ldap://127.0.0.1:{port}', LDAP_SASL_USER='testuser1', LDAP_SASL_PASSWORD='testpass1')
        command = ['/usr/sbin/slapd', '-f', str(root / 'slapd.conf'), '-h', env['LDAP_URI'], '-d', '256']
        with running_server(command, env):
            auth = ['-x', '-H', env['LDAP_URI'], '-D', env['LDAP_BINDDN'], '-w', env['LDAP_PASSWORD']]
            subprocess.run(['ldapadd', *auth], input='dn: dc=example,dc=com\nobjectClass: top\n'
                    'objectClass: dcObject\nobjectClass: organization\no: Qore package tests\ndc: example\n',
                    env=env, text=True, check=True, timeout=30)
            subprocess.run(['ldapadd', *auth, '-f', str(source / 'test/fixtures/test-data.ldif')],
                           env=env, check=True, timeout=30)
            for mechanism in ('DIGEST-MD5', 'CRAM-MD5'):
                subprocess.run(['ldapwhoami', '-H', env['LDAP_URI'], '-Y', mechanism,
                                '-U', env['LDAP_SASL_USER'], '-w', env['LDAP_SASL_PASSWORD']],
                               env=env, check=True, timeout=30)
            suite = root / 'openldap.qtest'
            suite.write_text(re.sub(r'^%prepend-module-path .*\n', '',
                                   (source / 'test/openldap.qtest').read_text(), flags=re.M))
            subprocess.run([*qore, str(suite), '-v'], env=env, cwd=root, check=True, timeout=600)
            check_cli(qore, source / 'bin' if build else Path('/usr/bin'), env, root)
            if compiler:
                text = (source / 'debian/tests/compiler').read_text().split("<<'EOF'\n", 1)[1].split('\nEOF', 1)[0]
                (root / 'ldap-smoke.q').write_text(text + '\n')
                subprocess.run(['/usr/bin/qcc', '-o', str(root / 'ldap-smoke'), str(root / 'ldap-smoke.q')],
                               env=env, cwd=root, check=True, timeout=120)
                subprocess.run([str(root / 'ldap-smoke')], env=env, cwd=root, check=True, timeout=60)
        print('OpenLDAP suites and CLI checks passed against the private directory.', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--build-dir', type=Path)
    mode.add_argument('--installed', action='store_true')
    parser.add_argument('--compiler', action='store_true')
    arguments = parser.parse_args()
    run(arguments.build_dir, arguments.compiler)

