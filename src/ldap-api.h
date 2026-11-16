/* indent-tabs-mode: nil -*- */
/*
  openldap Qore module

  Copyright (C) 2026 David Nichols, all rights reserved

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

#ifndef _QORE_OPENLDAP_LDAP_API_H
#define _QORE_OPENLDAP_LDAP_API_H

/** The module's own binding of libldap and liblber (issue #1043).

    Qore loads binary modules with RTLD_GLOBAL, so the symbols of every library that a module links are visible to the
    modules loaded after it.  Oracle's libclntsh exports unversioned ldap_* and ber_* symbols that win over libldap's in
    that global scope, for the references of this module and for libldap's own calls (it is not linked with
    -Bsymbolic): with the oracle module loaded first, ldap_get_option() was the Oracle client's and failed with
    LDAP_PARAM_ERROR.

    With QORE_OPENLDAP_ISOLATE (glibc: RTLD_DEEPBIND) the module is not linked with libldap; it opens libldap itself
    with RTLD_LOCAL | RTLD_DEEPBIND, so that the library's own references resolve to itself and its dependencies
    first, and calls it through the pointers that qore_openldap_load() resolves from that handle.  The names of the
    functions below are macros for those pointers in the module's code.  Without it (macOS, whose two-level
    namespaces do not have the problem; musl, which has no RTLD_DEEPBIND), the module is linked with libldap as
    before and calls it directly.
*/

#ifdef QORE_OPENLDAP_ISOLATE

#include <qore/Qore.h>
#include <ldap.h>

//! the functions of libldap and liblber that the module calls
#define QORE_OPENLDAP_FUNCS(X) \
    X(ber_bvecfree) X(ber_bvfree) X(ber_free) X(ber_memfree) \
    X(ldap_add_ext) X(ldap_compare_ext) X(ldap_control_create) X(ldap_control_free) X(ldap_controls_free) \
    X(ldap_create_page_control) X(ldap_delete_ext) X(ldap_err2string) X(ldap_extended_operation) \
    X(ldap_first_attribute) X(ldap_first_entry) X(ldap_get_dn) X(ldap_get_option) X(ldap_get_values_len) \
    X(ldap_initialize) X(ldap_memfree) X(ldap_memvfree) X(ldap_modify_ext) X(ldap_msgfree) \
    X(ldap_next_attribute) X(ldap_next_entry) X(ldap_parse_extended_result) X(ldap_parse_pageresponse_control) \
    X(ldap_parse_result) X(ldap_passwd) X(ldap_rename) X(ldap_result) X(ldap_sasl_bind) \
    X(ldap_sasl_interactive_bind_s) X(ldap_search_ext) X(ldap_set_option) X(ldap_start_tls_s) \
    X(ldap_tls_inplace) X(ldap_txn_end_s) X(ldap_txn_start_s) X(ldap_unbind_ext_s)

//! the resolved functions and variables of the library
struct QoreOpenLdapApi {
#define QORE_OPENLDAP_DECL(n) decltype(&::n) p_##n;
    QORE_OPENLDAP_FUNCS(QORE_OPENLDAP_DECL)
#undef QORE_OPENLDAP_DECL
    //! the address of the variable liblber's ber_pvt_opt_on (the value of LBER_OPT_ON)
    char* ber_pvt_opt_on_addr;
};

DLLLOCAL extern QoreOpenLdapApi qore_openldap_api;

//! opens libldap in isolation and resolves its functions; returns an error string or nullptr
DLLLOCAL QoreStringNode* qore_openldap_load();

//! closes the library
DLLLOCAL void qore_openldap_unload();

// the module's code calls the library's functions through the table
#define ber_bvecfree (*qore_openldap_api.p_ber_bvecfree)
#define ber_bvfree (*qore_openldap_api.p_ber_bvfree)
#define ber_free (*qore_openldap_api.p_ber_free)
#define ber_memfree (*qore_openldap_api.p_ber_memfree)
#define ber_pvt_opt_on (*qore_openldap_api.ber_pvt_opt_on_addr)
#define ldap_add_ext (*qore_openldap_api.p_ldap_add_ext)
#define ldap_compare_ext (*qore_openldap_api.p_ldap_compare_ext)
#define ldap_control_create (*qore_openldap_api.p_ldap_control_create)
#define ldap_control_free (*qore_openldap_api.p_ldap_control_free)
#define ldap_controls_free (*qore_openldap_api.p_ldap_controls_free)
#define ldap_create_page_control (*qore_openldap_api.p_ldap_create_page_control)
#define ldap_delete_ext (*qore_openldap_api.p_ldap_delete_ext)
#define ldap_err2string (*qore_openldap_api.p_ldap_err2string)
#define ldap_extended_operation (*qore_openldap_api.p_ldap_extended_operation)
#define ldap_first_attribute (*qore_openldap_api.p_ldap_first_attribute)
#define ldap_first_entry (*qore_openldap_api.p_ldap_first_entry)
#define ldap_get_dn (*qore_openldap_api.p_ldap_get_dn)
#define ldap_get_option (*qore_openldap_api.p_ldap_get_option)
#define ldap_get_values_len (*qore_openldap_api.p_ldap_get_values_len)
#define ldap_initialize (*qore_openldap_api.p_ldap_initialize)
#define ldap_memfree (*qore_openldap_api.p_ldap_memfree)
#define ldap_memvfree (*qore_openldap_api.p_ldap_memvfree)
#define ldap_modify_ext (*qore_openldap_api.p_ldap_modify_ext)
#define ldap_msgfree (*qore_openldap_api.p_ldap_msgfree)
#define ldap_next_attribute (*qore_openldap_api.p_ldap_next_attribute)
#define ldap_next_entry (*qore_openldap_api.p_ldap_next_entry)
#define ldap_parse_extended_result (*qore_openldap_api.p_ldap_parse_extended_result)
#define ldap_parse_pageresponse_control (*qore_openldap_api.p_ldap_parse_pageresponse_control)
#define ldap_parse_result (*qore_openldap_api.p_ldap_parse_result)
#define ldap_passwd (*qore_openldap_api.p_ldap_passwd)
#define ldap_rename (*qore_openldap_api.p_ldap_rename)
#define ldap_result (*qore_openldap_api.p_ldap_result)
#define ldap_sasl_bind (*qore_openldap_api.p_ldap_sasl_bind)
#define ldap_sasl_interactive_bind_s (*qore_openldap_api.p_ldap_sasl_interactive_bind_s)
#define ldap_search_ext (*qore_openldap_api.p_ldap_search_ext)
#define ldap_set_option (*qore_openldap_api.p_ldap_set_option)
#define ldap_start_tls_s (*qore_openldap_api.p_ldap_start_tls_s)
#define ldap_tls_inplace (*qore_openldap_api.p_ldap_tls_inplace)
#define ldap_txn_end_s (*qore_openldap_api.p_ldap_txn_end_s)
#define ldap_txn_start_s (*qore_openldap_api.p_ldap_txn_start_s)
#define ldap_unbind_ext_s (*qore_openldap_api.p_ldap_unbind_ext_s)

#endif // QORE_OPENLDAP_ISOLATE

#endif
