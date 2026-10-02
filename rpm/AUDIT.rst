OpenLDAP RPM qualification review
=================================

Copyright 2026 Qore Technologies, s.r.o.

Scope: portable spec, rpm/run-tests.py, rpm/test_fixture.py and rpm/README.rst.
The native documentation change and SASL dependency each have separate audits.
Candidate 2 build and installed manifests are retained under qore-packaging/results.
An orchestration container-name collision interrupted SDK installation; the
corrected driver resumes that step and all three SDK tests pass. This was not
a package or test failure. Canonical and OBS results remain separate gates.

.. list-table:: Full audit-changes checklist
   :header-rows: 1
   :widths: 48 8 44

   * - Check
     - Status
     - Evidence

   * - Entry exists in doxygen/lang/120_modules.dox.tmpl (for modules in the Qore repo; N/A for external module repos)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Entry exists in doxygen/lang/900_release_notes.dox.tmpl (for modules in the Qore repo; external modules have release notes in their .qm)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - qore_user_module() or qore_external_user_module() call in CMakeLists.txt
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Module added to QMOD list in CMakeLists.txt
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - .qm file has @section <lowercasemodname>intro as first doc section — must be all lowercase (e.g., avrodataproviderintro, not AvroDataProviderintro)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - %modern in .qm file — no redundant %new-style, %require-types, %strict-args, %enable-all-warnings
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - No parse directives (%requires, %modern, %new-style) in separated .qc files (check OUTSIDE of @code blocks only)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - No %include usage (deprecated for modules)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Copyright 2026 on all new files
     - Pass
     - Recipe, fixture, regression tests, RPM README and audit carry 2026 copyright notices.

   * - Directory layout: .qm inside qlib/<ModuleName>/ directory (not at qlib/<ModuleName>.qm for multi-file modules)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - No second .qm for the same module at qlib/<ModuleName>.qm
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - ns=Qore::XX matches the QoreNamespace constructor path
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - %modern directive present
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Executable permission set (chmod +x)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Uses %prepend-module-path  before %requires for in-repo modules (Qore and Qore modules only; not Qorus)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - External module dependencies use %try-module — except modules delivered with the project itself (Qore ex: DataProvider, ConnectionProvider, QUnit, etc.) which use hard %requires
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - No filesystem operations (fopen, open, creat, unlink, remove, rename, mkdir, rmdir, stat, chmod) without sandbox checks
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - No network operations (connect, bind, socket, getaddrinfo, gethostbyname) without sandbox checks
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - If filesystem/network ops exist, verify QoreSandboxManagerHelper usage
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - No File::, Dir::, Socket::, HTTPClient:: usage without justification
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - All for/while loops that could iterate >100 times have qore_check_cancel() checks
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Uses qore_check_cancel() (NOT deprecated qore_check_io_interrupt())
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Check frequency: every 100 iterations for tight loops, every 10 for expensive iterations
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - No blocking operations without cancellation support
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Every action has display_name, short_desc (plain text, <80 chars), desc (markdown)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Every action has options populated via getActionOptionFromFields() — without this, the action shows an empty, unusable form
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Every action has output_type set to a typed data type constant (e.g., MyResponseDataType) — not omitted
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - DPAT_API actions: provider has "supports_request": True and implements doRequestImpl()
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - DPAT_FIND actions: every option exists in SearchOptions, getRecordTypeImpl() returns *hash<string, AbstractDataField>
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Scheme-based apps (with "scheme" in registerApp): actions use "path" and do NOT use "cls" — having both scheme and cls causes a runtime error
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Single-key hash slices use trailing comma: Fields{"key",} (without trailing comma, Fields{"key"} returns the value, not a hash)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Typed data type classes exist for request and response types — inherit HashDataType, have const Fields hash, call addQoreFields(Fields) in constructor, export public constant at bottom (e.g., public const MyDataType = new MyDataType();)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Request/input types use public Fields (enables ClassName::Fields in action registration)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Response/output types use private Fields
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Each field in data types has display_name, type, and desc (markdown-formatted)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Input fields have example_value where useful (string fields, endpoint URIs, SQL queries, etc.)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Fields with finite allowed values use allowed_values with AllowedValueInfo containing both value and display_name (Title Case, human-readable) — never bare values, never described only in text
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Password/secret fields have "sensitive": True
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - groups uses AppGroup enum values from qlib/DataProvider/AppGroup.qc
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - App logo stored as separate file, loaded at module level in Priv namespace
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - App desc uses markdown: bullet list of capabilities, links to project website, business-language explanation of value
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - display_name is user-friendly ("Apache Avro" not "avro")
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - short_desc is plain text, under 80 chars, single sentence — no markdown
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - desc uses markdown: backticks for code/field refs ( field_name ,  True ,  pdf ), \n\n for paragraphs, -  bullet lists for enumerations, bold for caveats
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Descriptions use plain business language relating to common challenges — not just technical "what" but "why" and "when to use"
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - No bare True/False/NOTHING — must be backtick-wrapped in desc
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - No bare field/option names in prose — must use backticks
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Long descriptions (>500 chars) use bold section headers and bullet lists
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Factory registration in Qore repo: every factory name registered in qlib/DataProvider/DataProvider.qc → FactoryMap (without this, module loads but doesn't appear in Qorus apps)
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - getRecordTypeImpl() signature: must be private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) — NOT returning *AbstractDataProviderType
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Dependency JARs committed (for JNI modules): JAR files in qlib/*/jar/ may be gitignored — use git add -f to ensure they're tracked, otherwise CI compilation fails
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - JAR install rules in CMakeLists.txt for all dependency JARs
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - No workarounds: No TODOs, FIXMEs, stubs, or partially-implemented features
     - Pass
     - Private slapd exercises real packaged functionality offline; all tests/docs enabled. Leap depends on the separately tested SASL initialization fix instead of disabling encrypted SASL.

   * - Exception safety: C++ uses ReferenceHolder for Qore allocations, std::unique_ptr for C++ allocations, *xsink checked after every fallible operation
     - Pass
     - Python context managers own temporary directories, logs, executor and server process. Failed startup/tests and timed-out shutdown all terminate and reap the process group; regression tests cover those paths.

   * - Thread safety: All mutable shared state protected by std::lock_guard<std::mutex> or documented as immutable-after-construction
     - Pass
     - The main thread owns server lifecycle; one reader drains the server log. No mutable global test state is shared; tests verify a 1 MiB log cannot block the server.

   * - Type safety: Strongly-typed code<return(args)> instead of untyped code; static_cast instead of C casts; typed hashdecls for results; enums where appropriate
     - Pass
     - Python subprocess arguments are lists; module artifacts and path cardinality are validated. No new C++ or Qore types.

   * - Performance: No O(n²) where O(n) is possible; no unnecessary copies; coordinate descent uses incremental residuals not full matrix multiply
     - Pass
     - Server readiness uses log events with select and a deadline; no sleep or polling loop. Native/AOT modules load once per suite; log draining is asynchronous.

   * - Error handling: All inputs validated (dimensions, empty data, unfitted models); C++ I/O handles EAGAIN/EINTR if applicable
     - Pass
     - Missing/duplicate native/AOT artifacts fail. Startup EOF/deadline and failure cleanup are tested. All compiler, CLI and test commands have checked return codes and bounded deadlines.

   * - Documentation: Doxygen @param, @return, @throw on all public methods; @par Example with realistic business scenarios; @note for important caveats
     - Pass
     - RPM README contains build and installed-test commands, SDK example, package contents and explicit Leap DIGEST-MD5 deployment guidance; spec changelog records behavior.

   * - QPP flags: [flags=CONSTANT] on methods that never throw; [flags=RET_VALUE_ONLY] on methods that throw but have no side effects
     - N/A
     - Packaging and Python fixture changes introduce no Qore module, QPP/C++ implementation, Qore test declarations, DataProvider or Java dependency.

   * - Security: No user-controlled format strings; no buffer overflows; bounds checking on array indices; no credentials in code
     - Pass
     - Unprivileged loopback fixture uses a private temporary directory and verified ephemeral TLS identity. Host LDAP/SASL/module overrides are cleared. Only fixture credentials are embedded; CLI password-redaction regression passes.

   * - Correctness: Algorithms verified against reference implementations; edge cases tested (empty data, single sample, all-zero features)
     - Pass
     - Candidate 2 builds on Fedora/Leap/EL10 against core15. Each installed runtime and SDK passes seven fixture regressions, 86 LDAP cases/501 assertions, all seven CLI help commands, SASL and verified StartTLS. SDK qcc example passes; seven actual installed HTML indexes exist; rpm -V passes.
