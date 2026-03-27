/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreLdapClient.h

    Qore Programming Language

    Copyright 2012 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_QORELDAPCLIENT_H

#define _QORE_QORELDAPCLIENT_H

#include <ldap.h>

#include <errno.h>
#include <string.h>

#include <memory>
#include <set>
#include <vector>

// default ldap operation timeout in milliseconds
#define QORE_LDAP_DEFAULT_TIMEOUT_MS 60000

// default ldap protocol version
#define QORE_LDAP_DEFAULT_PROTOCOL 3

template<typename T>
DLLLOCAL const T* check_hash_key(ExceptionSink *xsink, const QoreHashNode& h, const char* key, const char* err, const char* hash_name = 0) {
    QoreValue p = h.getKeyValue(key);
    if (p.isNullOrNothing()) {
        if (hash_name)
            xsink->raiseException(err, "no value for '%s' key present in %s", key, hash_name);
        return nullptr;
    }

    if (p.getType() != T::getStaticTypeCode()) {
        xsink->raiseException(err, "'%s' key is not type '%s' but is type '%s'", key, T::getStaticTypeName(), p.getTypeName());
        return nullptr;
    }
    return p.get<const T>();
}

template <typename T>
class LdapListHelper {
protected:
   T* l;
   size_t len;

   DLLLOCAL virtual int addElement(const ConstListIterator& li, ExceptionSink* xsink) = 0;

   DLLLOCAL LdapListHelper() : l(0), len(0) {
   }

   DLLLOCAL int init(const QoreListNode* ql, ExceptionSink* xsink) {
      // convert list to attribute list
      if (!ql || ql->empty())
         return 0;

      len = ql->size();

      l = new T[ql->size() + 1];
      ConstListIterator li(ql);
      while (li.next()) {
         if (addElement(li, xsink)) {
            len = li.index();
            return -1;
         }
      }
      // terminate list with a 0
      l[li.max()] = 0;
      return 0;
   }

public:
   DLLLOCAL virtual ~LdapListHelper() {
   }

   DLLLOCAL T* operator*() const {
      return l;
   }

   DLLLOCAL size_t size() const {
      return len;
   }
};

class QoreBerval : public berval {
public:
   DLLLOCAL QoreBerval(const QoreString& str) {
      bv_val = new char[str.size() + 1];
      strcpy(bv_val, str.getBuffer());
      bv_len = str.size();
   }

   DLLLOCAL ~QoreBerval() {
      delete [] bv_val;
   }
};

class BervalListHelper : public LdapListHelper<QoreBerval*> {
protected:
   DLLLOCAL virtual int addElement(const ConstListIterator& li, ExceptionSink* xsink) {
      QoreStringValueHelper str(li.getValue(), QCS_UTF8, xsink);
      if (*xsink)
         return -1;
      QoreBerval*& e = l[li.index()];
      e = new QoreBerval(**str);
      return 0;
   }

public:
   DLLLOCAL BervalListHelper(const QoreListNode* strl, ExceptionSink* xsink) : LdapListHelper<QoreBerval*>() {
      init(strl, xsink);
   }

   DLLLOCAL virtual ~BervalListHelper() {
      for (unsigned i = 0; i < len; ++i)
         delete l[i];
      delete [] l;
   }
};

class AttrListHelper : public LdapListHelper<char*> {
protected:
   DLLLOCAL virtual int addElement(const ConstListIterator& li, ExceptionSink* xsink) {
      QoreStringValueHelper str(li.getValue(), QCS_UTF8, xsink);
      if (*xsink)
         return -1;
      char*& e = l[li.index()];
      e = new char[str->size() + 1];
      strcpy(e, str->getBuffer());
      return 0;
   }

public:
   DLLLOCAL AttrListHelper(const QoreListNode* attrl, ExceptionSink* xsink) : LdapListHelper<char*>() {
      init(attrl, xsink);
   }

   DLLLOCAL virtual ~AttrListHelper() {
      for (unsigned i = 0; i < len; ++i)
         delete [] l[i];
      delete [] l;
   }
};

class QoreLDAPMod : public LDAPMod {
protected:
    DLLLOCAL int assignString(qore_size_t i, QoreValue p, const char* err, ExceptionSink* xsink) {
        QoreStringValueHelper str(p, QCS_UTF8, xsink);
        if (*xsink)
            return -1;

        if (mod_op != LDAP_MOD_DELETE && str->empty())
            return missingValueError(err, xsink);

        mod_values[i] = new char[str->size() + 1];
        strcpy(mod_values[i], str->getBuffer());

        //printd(5, "QoreLDAPMod::assignString() this: %p value[%zd]: '%s'\n", this, i, mod_values[i] ? mod_values[i] : "n/a");
        return 0;
    }

    DLLLOCAL int missingValueError(const char* err, ExceptionSink* xsink) const {
        xsink->raiseException("LDAP-MODIFY-ERROR", "missing value for '%s' operation for attribute '%s'", mod_op == LDAP_MOD_ADD ? "add" : "replace", mod_type);
        return -1;
    }

public:
    DLLLOCAL QoreLDAPMod(int n_mod_op, const char* attr, QoreValue p, const char* err, ExceptionSink* xsink) {
        mod_op = n_mod_op;
        mod_type = (char*)attr;
        mod_values = 0;

        qore_type_t t = p.getType();
        if (t == NT_NOTHING) {
            if (mod_op != LDAP_MOD_DELETE)
                missingValueError(err, xsink);
            return;
        }

        if (t == NT_LIST) {
            const QoreListNode* l = p.get<const QoreListNode>();
            if (l->empty())
                return;

            mod_values = new char*[l->size() + 1];

            ConstListIterator li(l);
            while (li.next()) {
                if (assignString(li.index(), li.getValue(), err, xsink)) {
                    mod_values[li.index() + 1] = 0;
                    return;
                }
            }
            // null-terminate the array
            mod_values[l->size()] = 0;
        }
        else {
            mod_values = new char*[2];
            assignString(0, p, err, xsink);
            mod_values[1] = 0;
        }
    }

    DLLLOCAL ~QoreLDAPMod() {
        if (mod_op & LDAP_MOD_BVALUES) {
            if (mod_bvalues) {
                for (berval** p = mod_bvalues; *p; ++p)
                    delete [] *p;
                delete [] mod_bvalues;
            }
        }
        else {
            if (mod_values) {
                for (char** p = mod_values; *p; ++p)
                    delete [] *p;
                delete [] mod_values;
            }
        }
    }
};

class ModListHelper : public LdapListHelper<QoreLDAPMod*> {
protected:
    bool op_add;

    DLLLOCAL virtual int addElement(const ConstListIterator& li, ExceptionSink* xsink) {
        QoreValue p = li.getValue();
        if (p.getType() != NT_HASH) {
            xsink->raiseException("LDAP-MODIFY-ERROR", "element %d/%d (starting from 0) is type '%s'; expecting 'hash'", li.index(), li.max(), p.getTypeName());
            return -1;
        }
        const QoreHashNode* h = p.get<const QoreHashNode>();

        const QoreStringNode* mod = check_hash_key<QoreStringNode>(xsink, *h, "mod", "LDAP-MODIFY-ERROR", "ldap modification hash");
        if (!mod)
            return -1;

        int mod_op = modmap.get(mod->c_str());
        if (mod_op == -1) {
            xsink->raiseException("LDAP-MODIFY-ERROR", "element %d/%d (starting with 0) don't know how to process modification action '%s' (expecting one of 'add', 'delete', 'replace')", li.index(), li.max(), mod->c_str());
            return -1;
        }

        const QoreStringNode* attr = check_hash_key<QoreStringNode>(xsink, *h, "attr", "LDAP-MODIFY-ERROR", "ldap modification hash");
        if (!attr)
            return -1;

        p = h->getKeyValue("value");

        std::unique_ptr<QoreLDAPMod> modp(new QoreLDAPMod(mod_op, attr->c_str(), p, "LDAP-MODIFY-ERROR", xsink));
        if (*xsink)
            return -1;

        l[li.index()] = modp.release();
        return 0;
    }

    DLLLOCAL virtual int addElement(size_t index, const ConstHashIterator& hi, ExceptionSink* xsink) {
        std::unique_ptr<QoreLDAPMod> mod(new QoreLDAPMod(LDAP_MOD_ADD, hi.getKey(), hi.get(), "LDAP-ADD-ERROR", xsink));
        if (*xsink)
            return -1;

        l[index] = mod.release();
        return 0;
    }

public:
    DLLLOCAL ModListHelper(ExceptionSink* xsink, const QoreListNode* ql) : LdapListHelper<QoreLDAPMod*>(), op_add(false) {
        init(ql, xsink);

#if 0
        printd(0, "ModListHelper::ModListHelper() this: %p len: %d\n", this, len);
        for (unsigned i = 0; i < len; ++i) {
            printd(0, "ModListHelper::ModListHelper() this: %p i: %d mod: %d attr: '%s'\n", this, i, l[i]->mod_op, l[i]->mod_type);
            if (l[i]->mod_values) {
                for (char** p = l[i]->mod_values; *p; ++p)
                printd(0, "  + val: '%s'\n", *p);
            }
        }
#endif
    }

    DLLLOCAL ModListHelper(ExceptionSink* xsink, const QoreHashNode* attr) : LdapListHelper<QoreLDAPMod*>(), op_add(true) {
        // convert list to attribute list
        if (!attr || attr->empty())
            return;

        len = attr->size();

        l = new QoreLDAPMod*[attr->size() + 1];
        ConstHashIterator hi(attr);
        size_t index = 0;
        while (hi.next()) {
            if (addElement(index, hi, xsink)) {
                len = index;
                return;
            }
            ++index;
        }
        // terminate list with a 0
        l[index] = 0;
        return;
    }

    DLLLOCAL virtual ~ModListHelper() {
        if (!l)
            return;

        for (unsigned i = 0; i < len; ++i)
            delete l[i];
        delete [] l;
    }
};

class QoreLDAPAPIInfoHelper {
private:
    DLLLOCAL QoreLDAPAPIInfoHelper(const QoreLDAPAPIInfoHelper &);
    DLLLOCAL QoreLDAPAPIInfoHelper &operator=(const QoreLDAPAPIInfoHelper &);
protected:
    bool initialized;
    LDAPAPIInfo apiInfo;
public:

    DLLLOCAL QoreLDAPAPIInfoHelper() : initialized(false) {
#if LDAP_API_INFO_VERSION == 1
        apiInfo.ldapai_info_version = LDAP_API_INFO_VERSION;
#else
#error "unsupported LDAP_API_INFO_VERSION"
#endif
    }

    DLLLOCAL ~QoreLDAPAPIInfoHelper() {
        if (initialized) {
            /* from man page for ldap_get_info:
             *  version passed in does not match
             *  the current library version, the expected version number will be
             *  stored in the struct and the call  will  fail.   The  caller  is
             *  responsible  for  freeing  the elements of the ldapai_extensions
             *  array and the array itself using  ldap_memfree(3).   The  caller
             *  must  also  free  the  ldapi_vendor_name.
             */
            ldap_memfree(apiInfo.ldapai_vendor_name);
            apiInfo.ldapai_vendor_name = 0;
            if (apiInfo.ldapai_extensions) {
                for (size_t i = 0; apiInfo.ldapai_extensions[i]; ++i) {
                    ldap_memfree(apiInfo.ldapai_extensions[i]);
                }
                ldap_memfree(apiInfo.ldapai_extensions);
                apiInfo.ldapai_extensions = 0;
            }
        }
    }

    DLLLOCAL int init() {
        assert(!initialized);
        int ec = ldap_get_option(0, LDAP_OPT_API_INFO, &apiInfo);
        if (!ec) {
            initialized = true;
        }
        // error code is returned and handled by caller (checkLibrary)
        return ec;
    }

    DLLLOCAL QoreStringNode* checkVersion() {
        if (apiInfo.ldapai_info_version != LDAP_API_INFO_VERSION)
            return new QoreStringNodeMaker("cannot load the openldap module due to a library info version mismatch; module was compiled with API info version %d but the library provides API info version %d", LDAP_API_INFO_VERSION, apiInfo.ldapai_info_version);

        if (apiInfo.ldapai_api_version != LDAP_API_VERSION)
            return new QoreStringNodeMaker("cannot load the openldap module due to a library version mismatch; module was compiled with API version %d but the library provides API version %d", LDAP_API_VERSION, apiInfo.ldapai_api_version);

        if (strcmp(apiInfo.ldapai_vendor_name, LDAP_VENDOR_NAME))
            return new QoreStringNodeMaker("cannot load the openldap module due to a library vendor name mismatch; module was compiled with a library from '%s' but the library is now running with a library from '%s'", LDAP_VENDOR_NAME, apiInfo.ldapai_vendor_name);

        if (apiInfo.ldapai_vendor_version != LDAP_VENDOR_VERSION)
            return new QoreStringNodeMaker("cannot load the openldap module due to a library vendor version mismatch; module was compiled with API vendor version %d but the library provides API vendor version %d", LDAP_VENDOR_VERSION, apiInfo.ldapai_vendor_version);

        return 0;
    }

    DLLLOCAL void fillInfoHash(QoreHashNode* h) {
        assert(h);
        h->setKeyValue("ApiVersion", apiInfo.ldapai_api_version, nullptr);
        h->setKeyValue("ProtocolVersion", apiInfo.ldapai_protocol_version, nullptr);
        h->setKeyValue("VendorName", new QoreStringNode(apiInfo.ldapai_vendor_name), nullptr);
        h->setKeyValue("VendorVersion", apiInfo.ldapai_vendor_version, nullptr);

        QoreListNode* el = new QoreListNode(autoTypeInfo);
        for (unsigned i = 0; apiInfo.ldapai_extensions[i]; ++i)
            el->push(new QoreStringNode(apiInfo.ldapai_extensions[i]), nullptr);

        h->setKeyValue("Extensions", el, nullptr);
    }
};

struct TimeoutHelper : public timeval {
    DLLLOCAL TimeoutHelper(int ms) {
        assign(ms);
    }

    DLLLOCAL TimeoutHelper& operator=(int ms) {
        assign(ms);
        return *this;
    }

    DLLLOCAL void assign(int ms) {
        if (ms < 0)
            ms = 0;
        tv_sec = ms / 1000;
        tv_usec = (ms - (tv_sec * 1000)) * 1000;
    }
};

// RAII helper to convert a Qore list of LdapControlInfo hashes to an LDAPControl** array
class LdapControlListHelper {
protected:
    std::vector<LDAPControl*> ctrls;

public:
    DLLLOCAL LdapControlListHelper() {
    }

    DLLLOCAL LdapControlListHelper(const QoreListNode* l, ExceptionSink* xsink) {
        if (!l || l->empty()) {
            return;
        }

        ConstListIterator li(l);
        while (li.next()) {
            QoreValue v = li.getValue();
            if (v.getType() != NT_HASH) {
                xsink->raiseException("LDAP-CONTROL-ERROR",
                    "control list element %zd/%zd is type '%s'; expecting 'hash'",
                    li.index(), li.max(), v.getTypeName());
                return;
            }
            const QoreHashNode* ch = v.get<const QoreHashNode>();

            // get OID (required)
            const QoreStringNode* oid = check_hash_key<QoreStringNode>(xsink, *ch, "oid",
                "LDAP-CONTROL-ERROR", "LDAP control hash");
            if (!oid) {
                return;
            }

            // get criticality (optional, default false)
            bool critical = ch->getKeyValue("critical").getAsBool();

            // get value (optional binary data)
            QoreValue val_node = ch->getKeyValue("value");
            struct berval bv = {0, 0};
            struct berval* bvp = nullptr;
            if (!val_node.isNullOrNothing()) {
                if (val_node.getType() == NT_BINARY) {
                    const BinaryNode* bn = val_node.get<const BinaryNode>();
                    bv.bv_val = (char*)bn->getPtr();
                    bv.bv_len = bn->size();
                    bvp = &bv;
                } else if (val_node.getType() == NT_STRING) {
                    const QoreStringNode* sn = val_node.get<const QoreStringNode>();
                    bv.bv_val = (char*)sn->c_str();
                    bv.bv_len = sn->size();
                    bvp = &bv;
                } else {
                    xsink->raiseException("LDAP-CONTROL-ERROR",
                        "control 'value' key is type '%s'; expecting 'binary' or 'string'",
                        val_node.getTypeName());
                    return;
                }
            }

            LDAPControl* ctrl = nullptr;
            int rc = ldap_control_create(oid->c_str(), critical ? 1 : 0, bvp, 0, &ctrl);
            if (rc != LDAP_SUCCESS || !ctrl) {
                xsink->raiseException("LDAP-CONTROL-ERROR",
                    "failed to create LDAP control with OID '%s': %s",
                    oid->c_str(), ldap_err2string(rc));
                return;
            }
            ctrls.push_back(ctrl);
        }
    }

    DLLLOCAL ~LdapControlListHelper() {
        for (auto* ctrl : ctrls) {
            ldap_control_free(ctrl);
        }
    }

    // Returns NULL-terminated LDAPControl** array, or nullptr if empty
    DLLLOCAL LDAPControl** getControls() {
        if (ctrls.empty()) {
            return nullptr;
        }
        // ensure NULL terminator
        if (ctrls.back() != nullptr) {
            ctrls.push_back(nullptr);
        }
        return ctrls.data();
    }

    DLLLOCAL operator LDAPControl**() {
        return getControls();
    }

    DLLLOCAL bool empty() const {
        return ctrls.empty();
    }
};

// Helper to extract response controls from an LDAP result message and convert to Qore data
class LdapResponseControlHelper {
public:
    DLLLOCAL static QoreListNode* getResponseControls(LDAP* ldp, LDAPMessage* res, ExceptionSink* xsink) {
        LDAPControl** resp_ctrls = nullptr;
        int rc = ldap_parse_result(ldp, res, nullptr, nullptr, nullptr, nullptr, &resp_ctrls, 0);
        if (rc != LDAP_SUCCESS || !resp_ctrls) {
            return nullptr;
        }

        ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);

        for (int i = 0; resp_ctrls[i]; ++i) {
            LDAPControl* ctrl = resp_ctrls[i];
            ReferenceHolder<QoreHashNode> ch(new QoreHashNode(autoTypeInfo), xsink);
            ch->setKeyValue("oid", new QoreStringNode(ctrl->ldctl_oid), xsink);
            ch->setKeyValue("critical", ctrl->ldctl_iscritical ? true : false, xsink);
            if (ctrl->ldctl_value.bv_len > 0) {
                BinaryNode* bn = new BinaryNode;
                bn->append(ctrl->ldctl_value.bv_val, ctrl->ldctl_value.bv_len);
                ch->setKeyValue("value", bn, xsink);
            }
            result->push(ch.release(), xsink);
        }

        ldap_controls_free(resp_ctrls);
        return result->empty() ? nullptr : result.release();
    }
};

// Data structure for SASL interactive bind callback
struct QoreSaslInteractData {
    const char* authcid;   // authentication identity
    const char* authzid;   // authorization identity
    const char* realm;     // SASL realm
    const char* password;  // password/credentials
};

// SASL interaction callback for ldap_sasl_interactive_bind_s()
static int qore_sasl_interact_cb(LDAP* ld, unsigned flags, void* defaults, void* interact) {
    sasl_interact_t* in = (sasl_interact_t*)interact;
    QoreSaslInteractData* data = (QoreSaslInteractData*)defaults;
    for (; in->id != SASL_CB_LIST_END; ++in) {
        in->result = nullptr;
        in->len = 0;
        switch (in->id) {
            case SASL_CB_AUTHNAME:
                if (data->authcid) {
                    in->result = data->authcid;
                    in->len = strlen(data->authcid);
                }
                break;
            case SASL_CB_USER:
                if (data->authzid) {
                    in->result = data->authzid;
                    in->len = strlen(data->authzid);
                }
                break;
            case SASL_CB_PASS:
                if (data->password) {
                    in->result = data->password;
                    in->len = strlen(data->password);
                }
                break;
            case SASL_CB_GETREALM:
                if (data->realm) {
                    in->result = data->realm;
                    in->len = strlen(data->realm);
                }
                break;
            default:
                // use default if available
                if (in->defresult) {
                    in->result = in->defresult;
                    in->len = strlen(in->defresult);
                }
                break;
        }
    }
    return LDAP_SUCCESS;
}

class QoreStringBervalHelper : public berval, public QoreStringValueHelper {
public:
    DLLLOCAL QoreStringBervalHelper(const AbstractQoreNode* n, ExceptionSink* xsink) : QoreStringValueHelper(n, QCS_UTF8, xsink) {
        if (*xsink)
            return;

        if (!**this) {
            bv_val = 0;
            bv_len = 0;
        }
        else {
            bv_val = (char*)(*this)->getBuffer();
            bv_len = (*this)->size();
        }
    }
};

class QoreLdapClient;

class QoreLdapParseResultHelper {
protected:
    const char* meth;
    const char* f;
    QoreLdapClient* l;
    ExceptionSink* xsink;
    int err;
    char* matched;
    char* text;
    char** refs;

public:
    DLLLOCAL QoreLdapParseResultHelper(const char *n_meth, const char* n_f, QoreLdapClient* n_l, LDAPMessage* msg, ExceptionSink* xs);

    DLLLOCAL ~QoreLdapParseResultHelper() {
        if (matched)
            ldap_memfree(matched);
        if (text)
            ldap_memfree(text);
        if (refs)
            ldap_memvfree((void**)refs);
    }

    DLLLOCAL int getError() const {
        return err;
    }

    DLLLOCAL int check() const;
};

// the c++ object
class QoreLdapClient : public AbstractPrivateData {
    friend class QoreLdapParseResultHelper;

protected:
    // ldap context
    LDAP* ldp;
    // mutual-exclusion lock
    mutable QoreThreadLock m;
    // saved URI
    QoreStringNode* uri;
    // saved bind parameters
    QoreHashNode* bh;
    // ldap protocol version
    int prot;
    // ldap default timeout in ms
    int timeout_ms;
    // boolean flags
    bool tls : 1,        // issue a STARTTLS command if the session is not already secure
        no_referrals : 1, // do not follow referrals
        all_binary : 1;  // return all attribute values as binary

public:
    // set of attribute names that should be returned as binary
    typedef std::set<std::string> strset_t;

protected:
    strset_t binary_attrs;

    // built-in list of known binary attributes
    static const strset_t default_binary_attrs;

    QoreStringNode* getErrorText(const char* meth, const char* f, int ec) const {
        QoreStringNode* desc = new QoreStringNode("ldap server ");
        if (uri)
            desc->sprintf("'%s' ", uri->getBuffer());
        desc->sprintf("returned error code %d", ec);
        desc->sprintf(" when calling %s() in LdapClient::%s(): %s", f, meth, ldap_err2string(ec));
        return desc;
    }

    void doLdapError(const char* meth, const char* f, int ec, ExceptionSink* xsink) const {
        xsink->raiseException("LDAP-ERROR", getErrorText(meth, f, ec));
    }

    // Check if an attribute should be returned as binary
    DLLLOCAL bool isBinaryAttribute(const char* attr, const strset_t* search_binary = nullptr) const {
        if (all_binary) {
            return true;
        }
        std::string attr_lc(attr);
        // case-insensitive comparison: convert to lowercase
        for (auto& c : attr_lc) {
            c = tolower(c);
        }
        if (search_binary && search_binary->count(attr_lc)) {
            return true;
        }
        if (binary_attrs.count(attr_lc)) {
            return true;
        }
        return default_binary_attrs.count(attr_lc) > 0;
    }

    int checkLdapError(const char* meth, const char* f, int ec, ExceptionSink* xsink) const {
        //printd(5, "QoreLdapClient::checkLdapError() %s() rc: %d\n", f, ec);
        if (ec == LDAP_SUCCESS)
            return 0;
        doLdapError(meth, f, ec, xsink);
        return -1;
    }

    DLLLOCAL int checkFreeResult(const char* meth, const char* f, LDAPMessage* res, ExceptionSink* xsink) {
        QoreLdapParseResultHelper prh(meth, f, this, res, xsink);
        if (*xsink)
            return -1;
        return prh.check();
    }

    int checkLdapResult(const char* meth, const char* f, int ec, ExceptionSink* xsink) const {
        //printd(5, "QoreLdapClient::checkLdapResult() rc: %d\n", ec);
        // timeout
        if (!ec) {
            doLdapError(meth, f, LDAP_TIMEOUT, xsink);
            return -1;
        }
        if (ec == -1) {
            doLdapError(meth, f, ec, xsink);
            return -1;
        }
        return 0;
    }

    DLLLOCAL int checkValidIntern(const char* m, ExceptionSink* xsink) const {
        if (!ldp) {
            xsink->raiseException("LDAP-NO-CONTEXT", "cannot execute LdapClient::%s(); the LdapClient object has been destroyed or the session context has been unbound", m);
            return -1;
        }

        return 0;
    }

    DLLLOCAL int unbindIntern(ExceptionSink* xsink, int my_timeout_ms = 0) {
        ldap_unbind_ext_s(ldp, 0, 0);
        ldp = 0;

        return initIntern(xsink, "bind", my_timeout_ms);
    }

    DLLLOCAL int initIntern(ExceptionSink* xsink, const char* m, const QoreStringNode& uristr) {
        assert(!ldp);
        assert(!uri);
        uri = uristr.stringRefSelf();
        return initIntern(xsink, m);
    }

    DLLLOCAL int initIntern(ExceptionSink* xsink, const char* m, int my_timeout_ms = 0) {
        if (checkLdapError(m, "ldap_initialize", ldap_initialize(&ldp, uri->getBuffer()), xsink))
            return -1;

        // set protocol version
        if (ldap_set_option(ldp, LDAP_OPT_PROTOCOL_VERSION, &prot)) {
            xsink->raiseException("LDAP-ERROR", "failed to set LDAP protocol v%d; ldap_set_option(LDAP_OPT_PROTOCOL_VERSION) failed", prot);
            return -1;
        }

        // set restart option
        if (ldap_set_option(ldp, LDAP_OPT_RESTART, LDAP_OPT_ON)) {
            xsink->raiseException("LDAP-ERROR", "failed to set LDAP restart option; ldap_set_option(LDAP_OPT_RESTART) failed");
            return -1;
        }

        // set timeout
        TimeoutHelper timeout(timeout_ms);

        if (ldap_set_option(ldp, LDAP_OPT_TIMEOUT, &timeout)) {
            xsink->raiseException("LDAP-ERROR", "failed to set default LDAP timeout to %d ms; ldap_set_option(LDAP_OPT_TIMEOUT) failed", timeout_ms);
            return -1;
        }

        // disable referrals if necessary
        if (no_referrals && ldap_set_option(ldp, LDAP_OPT_REFERRALS, LDAP_OPT_OFF)) {
            xsink->raiseException("LDAP-ERROR", "failed to disable LDAP referrals; ldap_set_option(LDAP_OPT_REFERRALS) failed");
            return -1;
        }

        if (my_timeout_ms && my_timeout_ms != timeout_ms)
            timeout = my_timeout_ms;

        // Check for interrupt before connection
        if (qore_check_cancel(xsink))
            return -1;

        // force a connection to the server with an empty search request and ignore the result
        int msgid;
        if (checkLdapError(m, "ldap_search_ext", ldap_search_ext(ldp, 0, LDAP_SCOPE_BASE, 0, 0, 0, 0, 0, 0, 0, &msgid), xsink))
            return -1;
        LDAPMessage* res = 0;
        if (checkLdapResult(m, "ldap_search_ext", ldap_result(ldp, msgid, LDAP_MSG_ALL, &timeout, &res), xsink)) {
            assert(!res);
            return -1;
        }
        ldap_msgfree(res);

        // issue a STARTTLS if necessary
        if (tls && !ldap_tls_inplace(ldp)) {
            if (checkLdapError("constructor", "ldap_start_tls_s", ldap_start_tls_s(ldp, 0, 0), xsink))
                return -1;
            //printd(0, "QoreLdapClient::initIntern() STARTTLS successful\n");
        }

        return 0;
    }

    DLLLOCAL int bindInitIntern(ExceptionSink* xsink, const char* m, const QoreHashNode& bindh, int my_timeout_ms = 0) {
        assert(ldp);

        const QoreStringNode* password = check_hash_key<QoreStringNode>(xsink, bindh, "password", "LDAP-BIND-ERROR");

        const QoreStringNode* binddn = check_hash_key<QoreStringNode>(xsink, bindh, "binddn", "LDAP-BIND-ERROR");
        if (!binddn) {
            if (password && !password->empty()) {
                xsink->raiseException("LDAP-BIND-ERROR", "password given but no bind DN given for bind");
            }
            return -1;
        }

        QoreStringValueHelper bstr(binddn, QCS_UTF8, xsink);
        if (*xsink) {
            return -1;
        }

        QoreStringBervalHelper passwd(password, xsink);
        if (*xsink) {
            return -1;
        }

        // Check for interrupt before bind
        if (qore_check_cancel(xsink)) {
            return -1;
        }

        int msgid;

        if (checkLdapError(m, "ldap_sasl_bind", ldap_sasl_bind(ldp, bstr->getBuffer(), LDAP_SASL_SIMPLE, &passwd, 0, 0, &msgid), xsink)) {
            return -1;
        }

        LDAPMessage* result = 0;
        TimeoutHelper timeout(my_timeout_ms);

        if (checkLdapResult(m, "ldap_sasl_bind", ldap_result(ldp, msgid, LDAP_MSG_ALL, my_timeout_ms ? &timeout : 0, &result), xsink)) {
            assert(!result);
            return -1;
        }

        int rc = checkFreeResult(m, "ldap_sasl_bind", result, xsink);
        if (!rc) {
            // save bind parameters for copy constructor
            if (bh) {
                bh->deref(xsink);
            }
            bh = bindh.hashRefSelf();
        }
        return rc;
    }

public:
    DLLLOCAL QoreLdapClient(const QoreStringNode* uristr, const QoreHashNode* opth, ExceptionSink* xsink) : ldp(0), uri(0), bh(0), prot(QORE_LDAP_DEFAULT_PROTOCOL), timeout_ms(QORE_LDAP_DEFAULT_TIMEOUT_MS), tls(false), no_referrals(false), all_binary(false) {
        if (opth) {
            QoreValue p = opth->getKeyValue("protocol");
            int i = p.getAsBigInt();
            if (i) {
                prot = i;
            }

            i = getMsZeroInt(opth->getKeyValue("timeout"));
            if (i) {
                timeout_ms = i;
            }

            p = opth->getKeyValue("no-referrals");
            bool refp = p.getAsBool();
            if (refp)
                no_referrals = true;

            p = opth->getKeyValue("starttls");
            tls = p.getAsBool();

            // binary attribute options
            p = opth->getKeyValue("all-binary");
            all_binary = p.getAsBool();

            QoreValue ba = opth->getKeyValue("binary-attributes");
            if (!ba.isNullOrNothing() && ba.getType() == NT_LIST) {
                const QoreListNode* bal = ba.get<const QoreListNode>();
                ConstListIterator li(bal);
                while (li.next()) {
                    QoreStringValueHelper str(li.getValue(), QCS_UTF8, xsink);
                    if (*xsink) {
                        return;
                    }
                    std::string s(str->c_str());
                    for (auto& c : s) {
                        c = tolower(c);
                    }
                    binary_attrs.insert(std::move(s));
                }
            }
        }

        if (initIntern(xsink, "constructor", *uristr))
            return;

        if (opth) {
            bindInitIntern(xsink, "constructor", *opth);
            if (*xsink)
                return;
        }
    }

    DLLLOCAL QoreLdapClient(const QoreLdapClient& old, ExceptionSink* xsink) : ldp(0), uri(0), bh(0), prot(old.prot), timeout_ms(old.timeout_ms), tls(old.tls), no_referrals(old.no_referrals), all_binary(old.all_binary), binary_attrs(old.binary_attrs) {
        AutoLocker al(old.m);
        if (old.checkValidIntern("copy", xsink))
            return;

        if (initIntern(xsink, "copy", *old.uri))
            return;

        if (old.bh && bindInitIntern(xsink, "copy", *old.bh))
            return;
    }

    DLLLOCAL ~QoreLdapClient() {
        assert(!ldp);
        assert(!uri);
        assert(!bh);
    }

    DLLLOCAL int destructor(ExceptionSink* xsink) {
        AutoLocker al(m);
        if (ldp) {
            ldap_unbind_ext_s(ldp, 0, 0);
            ldp = 0;
        }

        if (uri) {
            uri->deref();
            uri = 0;
        }

        if (bh) {
            bh->deref(xsink);
            bh = 0;
        }

        return 0;
    }

    DLLLOCAL bool isSecure(ExceptionSink* xsink) {
        AutoLocker al(m);
        if (checkValidIntern("isSecure", xsink))
            return false;

        return ldap_tls_inplace(ldp);
    }

    DLLLOCAL int bind(ExceptionSink* xsink, const QoreHashNode& bindh, int my_timeout_ms = 0) {
        AutoLocker al(m);
        if (checkValidIntern("bind", xsink))
            return -1;

        if (unbindIntern(xsink, my_timeout_ms))
            return -1;

        return bindInitIntern(xsink, "bind", bindh, my_timeout_ms);
    }

    DLLLOCAL int saslBind(ExceptionSink* xsink, const QoreHashNode& opts, int my_timeout_ms = 0) {
        // extract mechanism (required)
        const QoreStringNode* mechanism = check_hash_key<QoreStringNode>(xsink, opts, "mechanism", "LDAP-SASL-BIND-ERROR", "SASL bind options");
        if (!mechanism) {
            return -1;
        }
        QoreStringValueHelper mechstr(mechanism, QCS_UTF8, xsink);
        if (*xsink) {
            return -1;
        }

        // extract optional parameters
        const QoreStringNode* authcid_node = check_hash_key<QoreStringNode>(xsink, opts, "authcid", "LDAP-SASL-BIND-ERROR");
        if (*xsink) {
            return -1;
        }
        const QoreStringNode* authzid_node = check_hash_key<QoreStringNode>(xsink, opts, "authzid", "LDAP-SASL-BIND-ERROR");
        if (*xsink) {
            return -1;
        }
        const QoreStringNode* realm_node = check_hash_key<QoreStringNode>(xsink, opts, "realm", "LDAP-SASL-BIND-ERROR");
        if (*xsink) {
            return -1;
        }
        const QoreStringNode* password_node = check_hash_key<QoreStringNode>(xsink, opts, "password", "LDAP-SASL-BIND-ERROR");
        if (*xsink) {
            return -1;
        }

        // convert optional strings to UTF-8 (stack-allocated)
        QoreString authcid_buf, authzid_buf, realm_buf, password_buf;
        if (authcid_node) {
            QoreStringValueHelper tmp(authcid_node, QCS_UTF8, xsink);
            if (*xsink) {
                return -1;
            }
            authcid_buf = **tmp;
        }
        if (authzid_node) {
            QoreStringValueHelper tmp(authzid_node, QCS_UTF8, xsink);
            if (*xsink) {
                return -1;
            }
            authzid_buf = **tmp;
        }
        if (realm_node) {
            QoreStringValueHelper tmp(realm_node, QCS_UTF8, xsink);
            if (*xsink) {
                return -1;
            }
            realm_buf = **tmp;
        }
        if (password_node) {
            QoreStringValueHelper tmp(password_node, QCS_UTF8, xsink);
            if (*xsink) {
                return -1;
            }
            password_buf = **tmp;
        }

        // set up interaction data
        QoreSaslInteractData interact_data;
        interact_data.authcid = authcid_node ? authcid_buf.c_str() : nullptr;
        interact_data.authzid = authzid_node ? authzid_buf.c_str() : nullptr;
        interact_data.realm = realm_node ? realm_buf.c_str() : nullptr;
        interact_data.password = password_node ? password_buf.c_str() : nullptr;

        AutoLocker al(m);
        if (checkValidIntern("saslBind", xsink)) {
            return -1;
        }

        // Check for interrupt before SASL bind
        if (qore_check_cancel(xsink)) {
            return -1;
        }

        // set timeout on connection before calling the synchronous SASL bind
        if (my_timeout_ms) {
            TimeoutHelper timeout(my_timeout_ms);
            ldap_set_option(ldp, LDAP_OPT_TIMEOUT, &timeout);
        }

        int rc = ldap_sasl_interactive_bind_s(ldp, nullptr, mechstr->c_str(),
            nullptr, nullptr, LDAP_SASL_QUIET, qore_sasl_interact_cb, &interact_data);

        // restore default timeout
        if (my_timeout_ms) {
            TimeoutHelper timeout(timeout_ms);
            ldap_set_option(ldp, LDAP_OPT_TIMEOUT, &timeout);
        }

        if (rc != LDAP_SUCCESS) {
            doLdapError("saslBind", "ldap_sasl_interactive_bind_s", rc, xsink);
            return -1;
        }

        return 0;
    }

    DLLLOCAL QoreHashNode* search(ExceptionSink* xsink, const QoreStringNode* base, int scope, const QoreStringNode* filter, const QoreListNode* attrl = 0, bool attrsonly = false, int my_timeout_ms = 0, const QoreListNode* server_controls = nullptr, const strset_t* search_binary = nullptr) {
        // convert strings to UTF-8 if necessary
        QoreStringValueHelper bstr(base, QCS_UTF8, xsink);
        if (*xsink)
            return 0;

        QoreStringValueHelper fstr(filter, QCS_UTF8, xsink);
        if (*xsink)
            return 0;

        // get attribute list
        AttrListHelper attrs(attrl, xsink);
        if (*xsink)
            return 0;

        // build server controls array
        LdapControlListHelper sctrls(server_controls, xsink);
        if (*xsink)
            return 0;

        AutoLocker al(m);
        if (checkValidIntern("search", xsink))
            return 0;

        // Check for interrupt before search
        if (qore_check_cancel(xsink))
            return 0;

        int msgid;
        if (checkLdapError("search", "ldap_search_ext", ldap_search_ext(ldp, bstr->empty() ? 0 : bstr->getBuffer(), scope, fstr->empty() ? 0 : fstr->getBuffer(), *attrs, (int)attrsonly, sctrls, 0, 0, 0, &msgid), xsink))
            return 0;

        LDAPMessage* res = 0;
        TimeoutHelper timeout(my_timeout_ms);

        if (checkLdapResult("search", "ldap_search_ext", ldap_result(ldp, msgid, LDAP_MSG_ALL, my_timeout_ms ? &timeout : 0, &res), xsink)) {
            assert(!res);
            return 0;
        }

        ON_BLOCK_EXIT(ldap_msgfree, res);

        ReferenceHolder<QoreHashNode> h(new QoreHashNode, xsink);

        //printd(5, "LdapClient::search() results: %d entries: %d\n", ldap_count_messages(ldp, res), ldap_count_entries(ldp, res));

        LDAPMessage* e = ldap_first_entry(ldp, res);
        for (int i = 0; e; ++i, e = ldap_next_entry(ldp, e)) {
            ReferenceHolder<QoreHashNode> he(new QoreHashNode, xsink);

            BerElement* ber;
            char* attr = ldap_first_attribute(ldp, e, &ber);
            for (; attr; attr = ldap_next_attribute(ldp, e, ber)) {
                struct berval** vals;

                ReferenceHolder<> aval(xsink);
                QoreListNode* al = 0;
                if ((vals = ldap_get_values_len(ldp, e, attr))) {
                    bool is_binary = isBinaryAttribute(attr, search_binary);
                    for (unsigned i = 0; vals[i]; ++i) {
                        AbstractQoreNode* val_node;
                        if (is_binary) {
                            BinaryNode* bn = new BinaryNode;
                            bn->append(vals[i]->bv_val, vals[i]->bv_len);
                            val_node = bn;
                        } else {
                            val_node = new QoreStringNode(vals[i]->bv_val, vals[i]->bv_len, QCS_UTF8);
                        }
                        if (!i) {
                            aval = val_node;
                        } else {
                            if (i == 1) {
                                al = new QoreListNode(autoTypeInfo);
                                al->push(aval.release(), xsink);
                                aval = al;
                            }
                            al->push(val_node, xsink);
                        }
                    }

                    ber_bvecfree(vals);
                }

                he->setKeyValue(attr, aval.release(), 0);
                ldap_memfree(attr);
            }
            if (ber) {
                ber_free(ber, 0);
            }

            char* p = ldap_get_dn(ldp, e);
            if (p) {
                h->setKeyValue(p, he.release(), 0);
                ldap_memfree(p);
            } else {
                // use a generated key if DN cannot be retrieved
                QoreStringMaker key("<entry-%d>", i);
                h->setKeyValue(key.c_str(), he.release(), 0);
            }
        }

        return h.release();
    }

    // Helper: extract entries from an LDAP result message and append to hash h
    DLLLOCAL void extractSearchEntries(QoreHashNode* h, LDAPMessage* res, int& entry_index, ExceptionSink* xsink, const strset_t* search_binary = nullptr) {
        LDAPMessage* e = ldap_first_entry(ldp, res);
        for (; e; e = ldap_next_entry(ldp, e), ++entry_index) {
            ReferenceHolder<QoreHashNode> he(new QoreHashNode, xsink);

            BerElement* ber;
            char* attr = ldap_first_attribute(ldp, e, &ber);
            for (; attr; attr = ldap_next_attribute(ldp, e, ber)) {
                struct berval** vals;
                ReferenceHolder<> aval(xsink);
                QoreListNode* al = 0;
                if ((vals = ldap_get_values_len(ldp, e, attr))) {
                    bool is_binary = isBinaryAttribute(attr, search_binary);
                    for (unsigned j = 0; vals[j]; ++j) {
                        AbstractQoreNode* val_node;
                        if (is_binary) {
                            BinaryNode* bn = new BinaryNode;
                            bn->append(vals[j]->bv_val, vals[j]->bv_len);
                            val_node = bn;
                        } else {
                            val_node = new QoreStringNode(vals[j]->bv_val, vals[j]->bv_len, QCS_UTF8);
                        }
                        if (!j) {
                            aval = val_node;
                        } else {
                            if (j == 1) {
                                al = new QoreListNode(autoTypeInfo);
                                al->push(aval.release(), xsink);
                                aval = al;
                            }
                            al->push(val_node, xsink);
                        }
                    }
                    ber_bvecfree(vals);
                }
                he->setKeyValue(attr, aval.release(), 0);
                ldap_memfree(attr);
            }
            if (ber) {
                ber_free(ber, 0);
            }

            char* p = ldap_get_dn(ldp, e);
            if (p) {
                h->setKeyValue(p, he.release(), 0);
                ldap_memfree(p);
            } else {
                QoreStringMaker key("<entry-%d>", entry_index);
                h->setKeyValue(key.c_str(), he.release(), 0);
            }
        }
    }

    DLLLOCAL QoreHashNode* searchPaged(ExceptionSink* xsink, const QoreStringNode* base, int scope, const QoreStringNode* filter, const QoreListNode* attrl = 0, bool attrsonly = false, int page_size = 500, int my_timeout_ms = 0, const QoreListNode* extra_controls = nullptr, const strset_t* search_binary = nullptr) {
        // convert strings to UTF-8 if necessary
        QoreStringValueHelper bstr(base, QCS_UTF8, xsink);
        if (*xsink) {
            return nullptr;
        }
        QoreStringValueHelper fstr(filter, QCS_UTF8, xsink);
        if (*xsink) {
            return nullptr;
        }
        AttrListHelper attrs(attrl, xsink);
        if (*xsink) {
            return nullptr;
        }
        // build extra server controls
        LdapControlListHelper extra_sctrls(extra_controls, xsink);
        if (*xsink) {
            return nullptr;
        }

        AutoLocker al(m);
        if (checkValidIntern("search", xsink)) {
            return nullptr;
        }
        if (qore_check_cancel(xsink)) {
            return nullptr;
        }

        ReferenceHolder<QoreHashNode> h(new QoreHashNode, xsink);
        int entry_index = 0;
        struct berval cookie = {0, nullptr};

        do {
            // check for cancellation between pages
            if (qore_check_cancel(xsink)) {
                if (cookie.bv_val) {
                    ber_memfree(cookie.bv_val);
                }
                return nullptr;
            }

            // create paged results control
            LDAPControl* page_ctrl = nullptr;
            int rc = ldap_create_page_control(ldp, page_size, &cookie, 0, &page_ctrl);
            if (rc != LDAP_SUCCESS) {
                if (cookie.bv_val) {
                    ber_memfree(cookie.bv_val);
                }
                doLdapError("search", "ldap_create_page_control", rc, xsink);
                return nullptr;
            }

            // build combined controls array: paged control + any extra controls
            std::vector<LDAPControl*> ctrl_array;
            ctrl_array.push_back(page_ctrl);
            LDAPControl** extra = extra_sctrls.getControls();
            if (extra) {
                for (int i = 0; extra[i]; ++i) {
                    ctrl_array.push_back(extra[i]);
                }
            }
            ctrl_array.push_back(nullptr);

            int msgid;
            rc = ldap_search_ext(ldp,
                bstr->empty() ? nullptr : bstr->getBuffer(),
                scope,
                fstr->empty() ? nullptr : fstr->getBuffer(),
                *attrs, (int)attrsonly,
                ctrl_array.data(), nullptr, nullptr, 0, &msgid);

            ldap_control_free(page_ctrl);

            if (rc != LDAP_SUCCESS) {
                if (cookie.bv_val) {
                    ber_memfree(cookie.bv_val);
                }
                doLdapError("search", "ldap_search_ext", rc, xsink);
                return nullptr;
            }

            LDAPMessage* res = nullptr;
            TimeoutHelper timeout(my_timeout_ms);
            rc = ldap_result(ldp, msgid, LDAP_MSG_ALL, my_timeout_ms ? &timeout : nullptr, &res);
            if (!rc || rc == -1) {
                if (cookie.bv_val) {
                    ber_memfree(cookie.bv_val);
                }
                if (res) {
                    ldap_msgfree(res);
                }
                doLdapError("search", "ldap_search_ext", rc == 0 ? LDAP_TIMEOUT : rc, xsink);
                return nullptr;
            }

            // extract entries from this page
            extractSearchEntries(*h, res, entry_index, xsink, search_binary);

            // free old cookie
            if (cookie.bv_val) {
                ber_memfree(cookie.bv_val);
                cookie.bv_val = nullptr;
                cookie.bv_len = 0;
            }

            // parse response controls to get new cookie
            LDAPControl** resp_ctrls = nullptr;
            ldap_parse_result(ldp, res, nullptr, nullptr, nullptr, nullptr, &resp_ctrls, 0);
            if (resp_ctrls) {
                for (int i = 0; resp_ctrls[i]; ++i) {
                    if (!strcmp(resp_ctrls[i]->ldctl_oid, LDAP_CONTROL_PAGEDRESULTS)) {
                        ber_int_t total_count = 0;
                        ldap_parse_pageresponse_control(ldp, resp_ctrls[i], &total_count, &cookie);
                        break;
                    }
                }
                ldap_controls_free(resp_ctrls);
            }

            ldap_msgfree(res);

        } while (cookie.bv_val && cookie.bv_len > 0);

        if (cookie.bv_val) {
            ber_memfree(cookie.bv_val);
        }

        return h.release();
    }

    DLLLOCAL int add(ExceptionSink* xsink, const QoreStringNode* dn, const QoreHashNode* attr, int my_timeout_ms = 0, const QoreListNode* server_controls = nullptr) {
        // convert strings to UTF-8 if necessary
        QoreStringValueHelper dnstr(dn, QCS_UTF8, xsink);
        if (*xsink)
            return -1;

        ModListHelper mods(xsink, attr);
        if (*xsink)
            return -1;

        // build server controls array
        LdapControlListHelper sctrls(server_controls, xsink);
        if (*xsink)
            return -1;

        AutoLocker al(m);
        if (checkValidIntern("add", xsink))
            return -1;

        // Check for interrupt before add
        if (qore_check_cancel(xsink))
            return -1;

        int msgid;
        if (checkLdapError("add", "ldap_add_ext", ldap_add_ext(ldp, dnstr->empty() ? 0 : dnstr->getBuffer(), (LDAPMod**)*mods, sctrls, 0, &msgid), xsink))
            return -1;

        LDAPMessage* res = 0;
        TimeoutHelper timeout(my_timeout_ms);

        if (checkLdapResult("add", "ldap_add_ext", ldap_result(ldp, msgid, LDAP_MSG_ALL, my_timeout_ms ? &timeout : 0, &res), xsink)) {
            assert(!res);
            return -1;
        }

        return checkFreeResult("add", "ldap_add_ext", res, xsink);
    }

    DLLLOCAL int modify(ExceptionSink* xsink, const QoreStringNode* dn, const QoreListNode* ml, int my_timeout_ms = 0, const QoreListNode* server_controls = nullptr) {
        // convert strings to UTF-8 if necessary
        QoreStringValueHelper dnstr(dn, QCS_UTF8, xsink);
        if (*xsink)
            return -1;

        ModListHelper mods(xsink, ml);
        if (*xsink)
            return -1;

        // build server controls array
        LdapControlListHelper sctrls(server_controls, xsink);
        if (*xsink)
            return -1;

        AutoLocker al(m);
        if (checkValidIntern("modify", xsink))
            return -1;

        // Check for interrupt before modify
        if (qore_check_cancel(xsink))
            return -1;

        int msgid;
        if (checkLdapError("modify", "ldap_modify_ext", ldap_modify_ext(ldp, dnstr->empty() ? 0 : dnstr->getBuffer(), (LDAPMod**)*mods, sctrls, 0, &msgid), xsink))
            return -1;

        LDAPMessage* res = 0;
        TimeoutHelper timeout(my_timeout_ms);

        if (checkLdapResult("modify", "ldap_modify_ext", ldap_result(ldp, msgid, LDAP_MSG_ALL, my_timeout_ms ? &timeout : 0, &res), xsink)) {
            assert(!res);
            return -1;
        }

        return checkFreeResult("modify", "ldap_modify_ext", res, xsink);
    }

    DLLLOCAL int del(ExceptionSink* xsink, const QoreStringNode* dn, int my_timeout_ms = 0, const QoreListNode* server_controls = nullptr) {
        // convert strings to UTF-8 if necessary
        QoreStringValueHelper dnstr(dn, QCS_UTF8, xsink);
        if (*xsink)
            return -1;

        // build server controls array
        LdapControlListHelper sctrls(server_controls, xsink);
        if (*xsink)
            return -1;

        AutoLocker al(m);
        if (checkValidIntern("del", xsink))
            return -1;

        // Check for interrupt before delete
        if (qore_check_cancel(xsink))
            return -1;

        int msgid;
        if (checkLdapError("del", "ldap_delete_ext", ldap_delete_ext(ldp, dnstr->empty() ? 0 : dnstr->getBuffer(), sctrls, 0, &msgid), xsink))
            return -1;

        LDAPMessage* res = 0;
        TimeoutHelper timeout(my_timeout_ms);

        if (checkLdapResult("del", "ldap_delete_ext", ldap_result(ldp, msgid, LDAP_MSG_ALL, my_timeout_ms ? &timeout : 0, &res), xsink)) {
            assert(!res);
            return -1;
        }

        return checkFreeResult("del", "ldap_delete_ext", res, xsink);
    }

    DLLLOCAL bool compare(ExceptionSink* xsink, const QoreStringNode* dn, const QoreStringNode* attr, const QoreListNode* vl, int my_timeout_ms = 0, const QoreListNode* server_controls = nullptr) {
        // convert strings to UTF-8 if necessary
        QoreStringValueHelper dnstr(dn, QCS_UTF8, xsink);
        if (*xsink)
            return false;

        QoreStringValueHelper attrstr(attr, QCS_UTF8, xsink);
        if (*xsink)
            return false;

        BervalListHelper bval(vl, xsink);
        if (*xsink)
            return false;

        // build server controls array
        LdapControlListHelper sctrls(server_controls, xsink);
        if (*xsink)
            return false;

        AutoLocker al(m);
        if (checkValidIntern("compare", xsink))
            return false;

        // Check for interrupt before compare
        if (qore_check_cancel(xsink))
            return false;

        int msgid;
        if (checkLdapError("compare", "ldap_compare_ext", ldap_compare_ext(ldp, dnstr->empty() ? 0 : dnstr->getBuffer(), attrstr->empty() ? 0 : attrstr->getBuffer(), **bval, sctrls, 0, &msgid), xsink))
            return false;

        LDAPMessage* res = 0;
        TimeoutHelper timeout(my_timeout_ms);

        if (checkLdapResult("compare", "ldap_compare_ext", ldap_result(ldp, msgid, LDAP_MSG_ALL, my_timeout_ms ? &timeout : 0, &res), xsink)) {
            assert(!res);
            return false;
        }

        QoreLdapParseResultHelper prh("compare", "ldap_compare_ext", this, res, xsink);
        if (*xsink)
            return false;

        int rc = prh.getError();
        if (rc == LDAP_COMPARE_TRUE)
            return true;
        if (rc == LDAP_COMPARE_FALSE)
            return false;

        prh.check();
        return false;
    }

    DLLLOCAL int rename(ExceptionSink* xsink, const QoreStringNode* dn, const QoreStringNode* newrdn, const QoreStringNode* newparent, bool deleteoldrdn = true, int my_timeout_ms = 0, const QoreListNode* server_controls = nullptr) {
        // convert strings to UTF-8 if necessary
        QoreStringValueHelper dnstr(dn, QCS_UTF8, xsink);
        if (*xsink)
            return -1;

        QoreStringValueHelper newrdnstr(newrdn, QCS_UTF8, xsink);
        if (*xsink)
            return -1;

        QoreStringValueHelper newparentstr(newparent, QCS_UTF8, xsink);
        if (*xsink)
            return -1;

        // build server controls array
        LdapControlListHelper sctrls(server_controls, xsink);
        if (*xsink)
            return -1;

        AutoLocker al(m);
        if (checkValidIntern("rename", xsink))
            return -1;

        // Check for interrupt before rename
        if (qore_check_cancel(xsink))
            return -1;

        int msgid;
        if (checkLdapError("rename", "ldap_rename", ldap_rename(ldp, dnstr->empty() ? 0 : dnstr->getBuffer(), newrdnstr->empty() ? 0 : newrdnstr->getBuffer(), newparentstr->empty() ? 0 : newparentstr->getBuffer(), (int)deleteoldrdn, sctrls, 0, &msgid), xsink))
            return -1;

        LDAPMessage* res = 0;
        TimeoutHelper timeout(my_timeout_ms);

        if (checkLdapResult("rename", "ldap_rename", ldap_result(ldp, msgid, LDAP_MSG_ALL, my_timeout_ms ? &timeout : 0, &res), xsink)) {
            assert(!res);
            return -1;
        }

        return checkFreeResult("rename", "ldap_rename", res, xsink);
    }

    DLLLOCAL int passwd(ExceptionSink* xsink, const QoreStringNode* dn, const QoreStringNode* op, const QoreStringNode* np, int my_timeout_ms = 0, const QoreListNode* server_controls = nullptr) {
        // convert strings to UTF-8 if necessary
        QoreStringBervalHelper dnstr(dn, xsink);
        if (*xsink)
            return -1;

        QoreStringBervalHelper opstr(op, xsink);
        if (*xsink)
            return -1;

        QoreStringBervalHelper npstr(np, xsink);
        if (*xsink)
            return -1;

        // build server controls array
        LdapControlListHelper sctrls(server_controls, xsink);
        if (*xsink)
            return -1;

        AutoLocker al(m);
        if (checkValidIntern("passwd", xsink))
            return -1;

        // Check for interrupt before passwd
        if (qore_check_cancel(xsink))
            return -1;

        int msgid;
        if (checkLdapError("passwd", "ldap_passwd", ldap_passwd(ldp, &dnstr, &opstr, &npstr, sctrls, 0, &msgid), xsink))
            return -1;

        LDAPMessage* res = 0;
        TimeoutHelper timeout(my_timeout_ms);

        if (checkLdapResult("passwd", "ldap_passwd", ldap_result(ldp, msgid, LDAP_MSG_ALL, my_timeout_ms ? &timeout : 0, &res), xsink)) {
            assert(!res);
            return -1;
        }

        return checkFreeResult("passwd", "ldap_passwd", res, xsink);
    }

    DLLLOCAL QoreHashNode* extendedOp(ExceptionSink* xsink, const QoreStringNode* oid, const BinaryNode* value = nullptr, int my_timeout_ms = 0) {
        // convert OID to UTF-8
        QoreStringValueHelper oidstr(oid, QCS_UTF8, xsink);
        if (*xsink) {
            return nullptr;
        }

        // prepare request data
        struct berval reqdata = {0, nullptr};
        if (value && value->size() > 0) {
            reqdata.bv_val = (char*)value->getPtr();
            reqdata.bv_len = value->size();
        }

        AutoLocker al(m);
        if (checkValidIntern("extendedOp", xsink)) {
            return nullptr;
        }

        // Check for interrupt
        if (qore_check_cancel(xsink)) {
            return nullptr;
        }

        int msgid;
        if (checkLdapError("extendedOp", "ldap_extended_operation",
                ldap_extended_operation(ldp, oidstr->c_str(),
                    value && value->size() > 0 ? &reqdata : nullptr,
                    nullptr, nullptr, &msgid), xsink)) {
            return nullptr;
        }

        LDAPMessage* res = nullptr;
        TimeoutHelper timeout(my_timeout_ms);

        if (checkLdapResult("extendedOp", "ldap_extended_operation",
                ldap_result(ldp, msgid, LDAP_MSG_ALL,
                    my_timeout_ms ? &timeout : nullptr, &res), xsink)) {
            assert(!res);
            return nullptr;
        }

        ON_BLOCK_EXIT(ldap_msgfree, res);

        // parse extended result
        char* retoid = nullptr;
        struct berval* retdata = nullptr;
        int rc = ldap_parse_extended_result(ldp, res, &retoid, &retdata, 0);
        if (rc != LDAP_SUCCESS) {
            if (retoid) {
                ldap_memfree(retoid);
            }
            if (retdata) {
                ber_bvfree(retdata);
            }
            doLdapError("extendedOp", "ldap_parse_extended_result", rc, xsink);
            return nullptr;
        }

        // check the result code from the response
        int err = 0;
        ldap_parse_result(ldp, res, &err, nullptr, nullptr, nullptr, nullptr, 0);
        if (err != LDAP_SUCCESS) {
            if (retoid) {
                ldap_memfree(retoid);
            }
            if (retdata) {
                ber_bvfree(retdata);
            }
            doLdapError("extendedOp", "ldap_extended_operation", err, xsink);
            return nullptr;
        }

        // build result hash
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
        if (retoid) {
            h->setKeyValue("oid", new QoreStringNode(retoid), xsink);
            ldap_memfree(retoid);
        }
        if (retdata && retdata->bv_len > 0) {
            BinaryNode* bn = new BinaryNode;
            bn->append(retdata->bv_val, retdata->bv_len);
            h->setKeyValue("value", bn, xsink);
        }
        if (retdata) {
            ber_bvfree(retdata);
        }

        return h.release();
    }

    DLLLOCAL BinaryNode* txnStart(ExceptionSink* xsink, int my_timeout_ms = 0) {
        AutoLocker al(m);
        if (checkValidIntern("txnStart", xsink)) {
            return nullptr;
        }
        if (qore_check_cancel(xsink)) {
            return nullptr;
        }

        // set timeout before synchronous call
        if (my_timeout_ms) {
            TimeoutHelper timeout(my_timeout_ms);
            ldap_set_option(ldp, LDAP_OPT_TIMEOUT, &timeout);
        }

        struct berval* txnid = nullptr;
        int rc = ldap_txn_start_s(ldp, nullptr, nullptr, &txnid);

        // restore default timeout
        if (my_timeout_ms) {
            TimeoutHelper timeout(timeout_ms);
            ldap_set_option(ldp, LDAP_OPT_TIMEOUT, &timeout);
        }

        if (rc != LDAP_SUCCESS) {
            doLdapError("txnStart", "ldap_txn_start_s", rc, xsink);
            return nullptr;
        }

        if (!txnid || !txnid->bv_val || txnid->bv_len == 0) {
            xsink->raiseException("LDAP-TXN-ERROR", "ldap_txn_start_s returned empty transaction ID");
            if (txnid) {
                ber_bvfree(txnid);
            }
            return nullptr;
        }

        BinaryNode* bn = new BinaryNode;
        bn->append(txnid->bv_val, txnid->bv_len);
        ber_bvfree(txnid);
        return bn;
    }

    DLLLOCAL int txnEnd(ExceptionSink* xsink, const BinaryNode* txnid, bool commit, int my_timeout_ms = 0) {
        if (!txnid || txnid->size() == 0) {
            xsink->raiseException("LDAP-TXN-ERROR", "invalid or empty transaction ID");
            return -1;
        }

        struct berval bv;
        bv.bv_val = (char*)txnid->getPtr();
        bv.bv_len = txnid->size();

        AutoLocker al(m);
        if (checkValidIntern("txnEnd", xsink)) {
            return -1;
        }
        if (qore_check_cancel(xsink)) {
            return -1;
        }

        // set timeout before synchronous call
        if (my_timeout_ms) {
            TimeoutHelper timeout(my_timeout_ms);
            ldap_set_option(ldp, LDAP_OPT_TIMEOUT, &timeout);
        }

        int retidp = 0;
        int rc = ldap_txn_end_s(ldp, commit ? 1 : 0, &bv, nullptr, nullptr, &retidp);

        // restore default timeout
        if (my_timeout_ms) {
            TimeoutHelper timeout(timeout_ms);
            ldap_set_option(ldp, LDAP_OPT_TIMEOUT, &timeout);
        }

        if (rc != LDAP_SUCCESS) {
            QoreStringNode* desc = getErrorText("txnEnd", "ldap_txn_end_s", rc);
            if (retidp) {
                desc->sprintf(" (failed operation message ID: %d)", retidp);
            }
            xsink->raiseException("LDAP-TXN-ERROR", desc);
            return -1;
        }

        return 0;
    }

    DLLLOCAL QoreStringNode* getUriStr() const {
        assert(uri);
        return uri->stringRefSelf();
    }

    DLLLOCAL static QoreStringNode* checkLibrary() {
        QoreLDAPAPIInfoHelper ai;
        int ec = ai.init();
        if (ec)
            return new QoreStringNodeMaker("the openldap library returned error code %d: %s to the ldap_get_option(LDAP_OPT_API_INFO) function", ec, ldap_err2string(ec));

        QoreStringNode *ret = ai.checkVersion();
        return ret;
    }

    DLLLOCAL static QoreHashNode* getInfo() {
        QoreHashNode* h = new QoreHashNode;

        QoreLDAPAPIInfoHelper ai;
        if (ai.init())
            return h;

        ai.fillInfoHash(h);
        return h;
    }
};

#endif

