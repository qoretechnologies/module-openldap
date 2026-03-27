cmake_minimum_required(VERSION 3.5...3.31)

project(qore-openldap-module)

set (VERSION_MAJOR 2)
set (VERSION_MINOR 0)
set (VERSION_PATCH 0)

if (${VERSION_PATCH})
    set(PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
else()
    set(PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
endif()

if (POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

include(CheckCXXCompilerFlag)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
find_package(Qore 2.3 REQUIRED)

if (APPLE)
    set(CMAKE_FIND_FRAMEWORK NEVER)
endif()

find_package(OpenLDAP REQUIRED)

# Find Cyrus SASL (optional, for SASL authentication support)
find_path(SASL2_INCLUDE_DIR sasl/sasl.h)
find_library(SASL2_LIBRARY NAMES sasl2)
if(SASL2_INCLUDE_DIR AND SASL2_LIBRARY)
    message(STATUS "Found Cyrus SASL: ${SASL2_LIBRARY}")
    set(HAVE_SASL2 TRUE)
else()
    message(STATUS "Cyrus SASL not found - SASL bind support requires sasl/sasl.h header")
endif()

if (DEFINED ENV{OpenLDAP_LIB_R})
    set(OpenLDAP_LIB_R $ENV{OpenLDAP_LIB_R})
endif()
message(STATUS "found lib ${OpenLDAP_LIB_R}")

# Check for C++11.
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

check_cxx_compiler_flag(-fvisibility=hidden HAVE_GCC_VISIBILITY)

configure_file(${CMAKE_SOURCE_DIR}/cmake/config.h.cmake config.h)

set(CPP_SRC src/openldap-module.cpp)
set(QPP_SRC src/QC_LdapClient.qpp)
set(module_name openldap)

set(QORE_DOX_TMPL_SRC
  docs/mainpage.doxygen.tmpl
)

qore_wrap_qpp_value(QPP_CPP_SRC METALIST QPP_META_SRC ${QPP_SRC})

add_library(${module_name} MODULE ${QPP_CPP_SRC} ${CPP_SRC})
target_include_directories(${module_name} PUBLIC ${OpenLDAP_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src)
if(HAVE_SASL2)
    target_include_directories(${module_name} PUBLIC ${SASL2_INCLUDE_DIR})
endif()

if (DEFINED ENV{DOXYGEN_EXECUTABLE})
    set(DOXYGEN_EXECUTABLE $ENV{DOXYGEN_EXECUTABLE})
endif()

qore_external_binary_module(${module_name} "${PROJECT_VERSION}" ${OpenLDAP_LIB_R})

# Install user modules
qore_external_user_module("qlib/LdapHelper.qm" "")
qore_external_user_module("qlib/LdapConnectionPool.qm" "")
qore_external_user_module("qlib/LdapAsync.qm" "")
qore_external_user_module("qlib/LdifHelper.qm" "")
qore_external_user_module("qlib/LdapActiveDirectory.qm" "")

# Install bin scripts
install(PROGRAMS
    bin/qldapadd
    bin/qldapdelete
    bin/qldapmodify
    bin/qldappasswd
    bin/qldapsearch
    DESTINATION bin
)

qore_dist("${PROJECT_VERSION}")

# install qpp metadata files
if (COMMAND qore_install_qpp_metadata)
    qore_install_qpp_metadata(${module_name} ${QPP_META_SRC})
else()
    install(FILES ${QPP_META_SRC}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/qore/metadata)
endif()

qore_config_info()

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_SRC ${QORE_DOX_TMPL_SRC})
    add_custom_target(QORE_MOD_DOX_FILES DEPENDS ${QORE_DOX_SRC})
    add_dependencies(docs-module QORE_MOD_DOX_FILES)
endif()
