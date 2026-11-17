/* indent-tabs-mode: nil -*- */
/*
  openldap Qore module

  Copyright (C) 2012 - 2026 David Nichols, all rights reserved

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "openldap-module.h"

#include "QoreLdapClient.h"

static void openldap_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void openldap_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void openldap_module_delete();

// the module's namespace; purged when the module is deleted (see QoreModuleInfo::ns)
static QoreNamespace OLNS("Qore::OpenLdap");

extern "C" DLLEXPORT void openldap_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "openldap";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "openldap module";
    mod_info.author = "David Nichols";
    mod_info.url = "http://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = openldap_module_init;
    mod_info.ns_init = openldap_module_ns_init;
    mod_info.del = openldap_module_delete;
#ifdef QORE_HAS_MODULE_NS_PURGE
    // the namespace is purged when the module is deleted, before the types are freed (see QoreModuleInfo::ns)
    mod_info.ns = &OLNS;
#endif
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

DLLLOCAL QoreClass* initLdapClientClass(QoreNamespace& ns);

// modify action map
ModMap modmap;

// default binary attributes (lowercase for case-insensitive matching)
const QoreLdapClient::strset_t QoreLdapClient::default_binary_attrs = {
    "jpegphoto", "usercertificate", "cacertificate", "objectguid",
    "objectsid", "usersmimecertificate", "userpkcs12", "audio", "photo",
    "personalsignature", "x500uniqueidentifier", "supportedalgorithms",
    "crosscertificatepair", "userpassword", "msexchmailboxguid",
    "msexchmailboxsecuritydescriptor", "thumbnailphoto",
    "logonhours", "replicauptodate", "securityidentifier",
};

#ifdef QORE_OPENLDAP_ISOLATE
#include <dlfcn.h>

QoreOpenLdapApi qore_openldap_api;

// the handle of the library, opened in isolation
static void* ldap_handle = nullptr;

static void close_failed_load();

QoreStringNode* qore_openldap_load() {
   if (ldap_handle) {
      return nullptr;
   }
   // RTLD_DEEPBIND: the references of libldap (to liblber and to itself) resolve in its own scope before the global
   // scope, where another module's library (Oracle's libclntsh) can export symbols of the same names (#1043);
   // RTLD_LOCAL: the library's symbols are not added to the global scope
   const int flags = RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND;
   std::string err;
   for (const char* name : {QORE_OPENLDAP_DLOPEN_NAMES}) {
      ldap_handle = dlopen(name, flags);
      if (ldap_handle) {
         break;
      }
      const char* e = dlerror();
      if (!err.empty()) {
         err += "; ";
      }
      err += e ? e : name;
   }
   if (!ldap_handle) {
      QoreStringNode* str = new QoreStringNode;
      str->sprintf("cannot load the openldap library: %s", err.c_str());
      return str;
   }

   const char* missing = nullptr;
#define QORE_OPENLDAP_RESOLVE(n) \
   if (!missing) { \
      void* sym = dlsym(ldap_handle, #n); \
      if (sym) { \
         qore_openldap_api.p_##n = reinterpret_cast<decltype(qore_openldap_api.p_##n)>(sym); \
      } else { \
         missing = #n; \
      } \
   }
   QORE_OPENLDAP_FUNCS(QORE_OPENLDAP_RESOLVE)
#undef QORE_OPENLDAP_RESOLVE
   if (!missing) {
      qore_openldap_api.ber_pvt_opt_on_addr = reinterpret_cast<char*>(dlsym(ldap_handle, "ber_pvt_opt_on"));
      if (!qore_openldap_api.ber_pvt_opt_on_addr) {
         missing = "ber_pvt_opt_on";
      }
   }
   if (missing) {
      QoreStringNode* str = new QoreStringNode;
      str->sprintf("the openldap library does not provide '%s'", missing);
      close_failed_load();
      return str;
   }
   return nullptr;
}

// closes the library of a load that failed
static void close_failed_load() {
   if (ldap_handle) {
      dlclose(ldap_handle);
      ldap_handle = nullptr;
   }
   qore_openldap_api = QoreOpenLdapApi();
}

void qore_openldap_unload() {
   // the library stays mapped for the life of the process, as it did when it was linked: it keeps global state (its
   // options, the TLS and SASL contexts) that its dependencies and other threads may still use at exit, and unmapping
   // it hides that memory from leak checkers
}
#endif

static void openldap_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
#ifdef QORE_OPENLDAP_ISOLATE
   if (QoreStringNode* load_err = qore_openldap_load()) {
      xsink.raiseException("MODULE-INIT-ERROR", load_err);
      return;
   }
#endif

   // this also serves to initialize the library in a single-threaded way
   QoreStringNode* err = QoreLdapClient::checkLibrary();
   if (err) {
      xsink.raiseException("MODULE-INIT-ERROR", err);
      return;
   }

   // disable openssl cleanup when using the openldap module, since it will do that itself, and
   // calling openssl cleanup routines twice can cause segmentation faults
   qore_set_library_cleanup_options(QLO_DISABLE_OPENSSL_CLEANUP);

   OLNS.addSystemClass(initLdapClientClass(OLNS));
}

static void openldap_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
   qns->addNamespace(OLNS.copy());
}

static void openldap_module_delete() {
#ifdef QORE_OPENLDAP_ISOLATE
   qore_openldap_unload();
#endif
}
