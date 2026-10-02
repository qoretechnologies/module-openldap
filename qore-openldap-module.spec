# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
# Use the pinned source epoch for RPM headers and installed file timestamps.
%global source_date_epoch_from_changelog 1
%global use_source_date_epoch_as_buildtime 1
%if v"%{rpmversion}" >= v"4.20"
%global build_mtime_policy clamp_to_source_date_epoch
%else
%global clamp_mtime_to_source_date_epoch 1
%endif
%bcond_without tests
%bcond_without docs
Name: qore-openldap-module
Version: 2.0
Release: 2%{?dist}
Summary: LDAP, SASL and directory tools for Qore
License: MIT
URL: https://github.com/qoretechnologies/module-openldap
Source0: %{name}-%{version}.tar.xz
%global _find_debuginfo_dwz_opts %{nil}
BuildRequires: cmake >= 3.5
BuildRequires: make
BuildRequires: gcc-c++
BuildRequires: pkgconfig(ldap) >= 2.6
BuildRequires: pkgconfig(libsasl2)
BuildRequires: qore-devel >= 3.0.0~
BuildRequires: qore-rpm-macros >= 3.0.0~
BuildRequires: python3
%if %{with tests}
BuildRequires: openssl
%if 0%{?suse_version}
BuildRequires: openldap2
BuildRequires: openldap2-client
BuildRequires: cyrus-sasl-digestmd5(openssl3-private-provider) = 1
BuildRequires: cyrus-sasl-crammd5
%else
BuildRequires: openldap-servers
BuildRequires: openldap-clients
BuildRequires: cyrus-sasl-md5
%endif
BuildRequires: cyrus-sasl-plain
%endif
%if %{with docs}
BuildRequires: doxygen
%if 0%{?suse_version}
BuildRequires: util-linux
%else
BuildRequires: util-linux-core
%endif
%endif
%{?qore_enable_aot_post}

%description
Native OpenLDAP bindings with six compiled/source helper modules, compiler
metadata and seven qldap command-line tools. Supports directory operations,
SASL, transactions, connection pools, asynchronous operations, LDIF and Active
Directory helpers. Uses system OpenLDAP and Cyrus SASL libraries.

%if %{with docs}
%package doc
Summary: LDAP, SASL and directory tools reference documentation
BuildArch: noarch
%description doc
Seven native and user-module API references for OpenLDAP and helper modules.
%endif

%prep
%autosetup
%build
%{?set_build_flags}
. %{_rpmconfigdir}/qore/module-env.sh
qore_set_source_prefix_maps "%{qore_debug_source_dir}"
cmake -S . -B build -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE=-DNDEBUG \
  -DCMAKE_INSTALL_PREFIX=%{_prefix} \
  -DCMAKE_SKIP_RPATH=ON -DCMAKE_IGNORE_PREFIX_PATH=/usr/local \
  -DQore_DIR=%{_libdir}/cmake/Qore -DQORE_EXECUTABLE=/usr/bin/qore \
  -DQORE_QPP_EXECUTABLE=/usr/bin/qpp -DQORE_QCC_EXECUTABLE=/usr/bin/qcc \
  -DQORE_BUILD_AOT_MODULES=ON -DQORE_AOT_LINK_SOURCE_MODULES=OFF \
  -DQORE_GENERATE_JAVA_BINDINGS=OFF -DQORE_OPENLDAP_STRICT_DOCS=ON \
  -DQORE_QM_METADATA_ENV:STRING="QORE_MODULE_DIR=$QORE_MODULE_DIR:$PWD/qlib;QORE_MODULE_DIR_ONLY=1;QORE_INCLUDE_DIR=;LD_LIBRARY_PATH=" \
  -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=%{!?with_docs:ON}%{?with_docs:OFF}
cmake --build build -- %{?_smp_mflags}
%if %{with docs}
cmake --build build --target docs -- %{?_smp_mflags}
%endif
%install
DESTDIR=%{buildroot} cmake --install build
%qore_install_aot_sources qlib
install -d %{buildroot}%{_mandir}/man1
install -m 644 debian/man/*.1 %{buildroot}%{_mandir}/man1/
sed -i '1s|^#!/usr/bin/env qore$|#!/usr/bin/qore|' %{buildroot}%{_bindir}/qldap*
find %{buildroot}%{_libdir}/qore-modules -type f -name '*.qmod' -exec chmod 755 {} +
%if %{with docs}
install -d %{buildroot}%{_docdir}/%{name}-doc
cp -a build/docs %{buildroot}%{_docdir}/%{name}-doc/
hardlink -t -O %{buildroot}%{_docdir}/%{name}-doc
%endif
%check
%if %{with tests}
. %{_rpmconfigdir}/qore/module-env.sh
python3 -B -W error debian/tests/test_aot_metadata.py
python3 -B -W error rpm/test_fixture.py -v
python3 -B -W error rpm/run-tests.py --build-dir "$PWD/build"
%endif
%files
%license COPYING.MIT COPYING.LGPL
%doc README RELEASE-NOTES
%{_libdir}/qore-modules/*
%{_datadir}/qore-modules/*
%dir %{_datadir}/qore/metadata/openldap
%{_datadir}/qore/metadata/openldap/*.meta.json
%{_bindir}/qldap*
%{_mandir}/man1/qldap*.1*
%if %{with docs}
%files doc
%license COPYING.MIT COPYING.LGPL
%doc %{_docdir}/%{name}-doc/
%endif
%changelog
* Fri Oct 02 2026 David Nichols <david@qore.org> - 2.0-2
- Package six compiled/source helpers, compiler metadata and all seven CLI tools.
- Test private LDAP, SASL, transactions, verified StartTLS and command-line tools.
