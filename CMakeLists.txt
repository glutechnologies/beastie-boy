cmake_minimum_required(VERSION 3.16)

project(beastie-boy C)

include(CheckCSourceCompiles)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(ALLOW_UNSUPPORTED_VPP "Allow unsupported detected VPP API series" OFF)
set(SUPPORTED_VPP_SERIES "2506;2510;2602" CACHE STRING
    "Supported VPP API series as a semicolon-separated list")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

function(bb_resolve_pkg module_prefix)
  set(options REQUIRED)
  set(one_value_args HEADER LIBRARY)
  set(multi_value_args PKG_NAMES HEADER_NAMES LIB_NAMES)
  cmake_parse_arguments(BB "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

  set(found FALSE)
  foreach(pkg_name IN LISTS BB_PKG_NAMES)
    pkg_check_modules(${module_prefix} QUIET ${pkg_name})
    if(${module_prefix}_FOUND)
      set(found TRUE)
      break()
    endif()
  endforeach()

  if(NOT found)
    find_path(${module_prefix}_INCLUDE_DIR
      NAMES ${BB_HEADER_NAMES}
      PATH_SUFFIXES include)
    find_library(${module_prefix}_LIBRARY_FILE
      NAMES ${BB_LIB_NAMES}
      PATH_SUFFIXES lib lib64)

    if(${module_prefix}_INCLUDE_DIR AND ${module_prefix}_LIBRARY_FILE)
      set(${module_prefix}_FOUND TRUE PARENT_SCOPE)
      set(${module_prefix}_INCLUDE_DIRS "${${module_prefix}_INCLUDE_DIR}" PARENT_SCOPE)
      set(${module_prefix}_LINK_LIBRARIES "${${module_prefix}_LIBRARY_FILE}" PARENT_SCOPE)
      set(${module_prefix}_VERSION "" PARENT_SCOPE)
      return()
    endif()
  else()
    set(${module_prefix}_FOUND TRUE PARENT_SCOPE)
    set(${module_prefix}_INCLUDE_DIRS "${${module_prefix}_INCLUDE_DIRS}" PARENT_SCOPE)
    set(${module_prefix}_LINK_LIBRARIES "${${module_prefix}_LINK_LIBRARIES}" PARENT_SCOPE)
    set(${module_prefix}_VERSION "${${module_prefix}_VERSION}" PARENT_SCOPE)
    return()
  endif()

  if(BB_REQUIRED)
    message(FATAL_ERROR
      "Could not find dependency for ${module_prefix}. "
      "Tried pkg-config names: ${BB_PKG_NAMES}; headers: ${BB_HEADER_NAMES}; libraries: ${BB_LIB_NAMES}")
  endif()
endfunction()

bb_resolve_pkg(PCAP REQUIRED
  PKG_NAMES libpcap pcap
  HEADER_NAMES pcap/pcap.h
  LIB_NAMES pcap)

bb_resolve_pkg(MEMIF REQUIRED
  PKG_NAMES libmemif memif
  HEADER_NAMES libmemif.h
  LIB_NAMES memif)

bb_resolve_pkg(VAPI REQUIRED
  PKG_NAMES vapiclient vpp-api-client
  HEADER_NAMES vapi/vapi.h vapi/vpe.api.vapi.h
  LIB_NAMES vapiclient)

set(VPP_API_VERSION "unknown")
if(DEFINED VAPI_VERSION AND NOT VAPI_VERSION STREQUAL "")
  set(VPP_API_VERSION "${VAPI_VERSION}")
endif()
if(VPP_API_VERSION STREQUAL "unknown" OR VPP_API_VERSION STREQUAL "")
  find_program(DPKG_QUERY_EXECUTABLE dpkg-query)
  if(DPKG_QUERY_EXECUTABLE)
    foreach(vpp_pkg IN ITEMS vpp-dev vpp libvppinfra-dev libvppinfra)
      execute_process(
        COMMAND ${DPKG_QUERY_EXECUTABLE} -W -f=${Status} ${vpp_pkg}
        OUTPUT_VARIABLE dpkg_status
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(dpkg_status STREQUAL "install ok installed")
        execute_process(
          COMMAND ${DPKG_QUERY_EXECUTABLE} -W -f=${Version} ${vpp_pkg}
          OUTPUT_VARIABLE dpkg_version
          ERROR_QUIET
          OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT dpkg_version STREQUAL "")
          set(VPP_API_VERSION "${dpkg_version}")
          break()
        endif()
      endif()
    endforeach()
  endif()
endif()

set(VPP_API_SERIES "0")
if(VPP_API_VERSION MATCHES "^([0-9]+)\\.([0-9]+)")
  set(VPP_MINOR_2D "${CMAKE_MATCH_2}")
  string(LENGTH "${VPP_MINOR_2D}" VPP_MINOR_LEN)
  if(VPP_MINOR_LEN EQUAL 1)
    set(VPP_MINOR_2D "0${VPP_MINOR_2D}")
  endif()
  set(VPP_API_SERIES "${CMAKE_MATCH_1}${VPP_MINOR_2D}")
endif()

if(NOT VPP_API_SERIES STREQUAL "0")
  set(VPP_SERIES_SUPPORTED FALSE)
  foreach(series IN LISTS SUPPORTED_VPP_SERIES)
    if(series STREQUAL VPP_API_SERIES)
      set(VPP_SERIES_SUPPORTED TRUE)
    endif()
  endforeach()
  if(NOT VPP_SERIES_SUPPORTED AND NOT ALLOW_UNSUPPORTED_VPP)
    message(FATAL_ERROR
      "Detected unsupported VPP series ${VPP_API_SERIES} (version: ${VPP_API_VERSION}). "
      "Supported: ${SUPPORTED_VPP_SERIES}")
  endif()
endif()

set(LIBMEMIF_HEADER_CANDIDATE "")
foreach(memif_inc IN LISTS MEMIF_INCLUDE_DIRS)
  if(EXISTS "${memif_inc}/libmemif.h")
    set(LIBMEMIF_HEADER_CANDIDATE "${memif_inc}/libmemif.h")
    break()
  endif()
endforeach()
if(LIBMEMIF_HEADER_CANDIDATE STREQUAL "")
  message(FATAL_ERROR "Could not locate libmemif.h in include paths: ${MEMIF_INCLUDE_DIRS}")
endif()

file(READ "${LIBMEMIF_HEADER_CANDIDATE}" LIBMEMIF_HEADER_TEXT)
if(NOT LIBMEMIF_HEADER_TEXT MATCHES "int memif_create \\(memif_conn_handle_t \\*conn, memif_conn_args_t \\*args,")
  message(FATAL_ERROR "libmemif API compatibility check failed: memif_create signature missing")
endif()
if(NOT LIBMEMIF_HEADER_TEXT MATCHES "int memif_create_socket \\(memif_socket_handle_t \\*sock,")
  message(FATAL_ERROR "libmemif API compatibility check failed: memif_create_socket signature missing")
endif()
if(NOT LIBMEMIF_HEADER_TEXT MATCHES "int memif_poll_event \\(memif_socket_handle_t sock, int timeout\\);")
  message(FATAL_ERROR "libmemif API compatibility check failed: memif_poll_event signature missing")
endif()
if(NOT LIBMEMIF_HEADER_TEXT MATCHES "int memif_cancel_poll_event \\(memif_socket_handle_t sock\\);")
  message(FATAL_ERROR "libmemif API compatibility check failed: memif_cancel_poll_event signature missing")
endif()

set(VAPI_HEADER_OK FALSE)
foreach(vapi_inc IN LISTS VAPI_INCLUDE_DIRS)
  if(EXISTS "${vapi_inc}/vapi/vapi.h" AND EXISTS "${vapi_inc}/vapi/vpe.api.vapi.h")
    set(VAPI_HEADER_OK TRUE)
    break()
  endif()
endforeach()
if(NOT VAPI_HEADER_OK)
  message(FATAL_ERROR "libvapi API headers not found in include paths: ${VAPI_INCLUDE_DIRS}")
endif()

if(NOT VAPI_LINK_LIBRARIES)
  message(FATAL_ERROR "libvapi library could not be resolved")
endif()

execute_process(
  COMMAND git -C ${CMAKE_SOURCE_DIR} describe --tags --always --dirty
  OUTPUT_VARIABLE APP_GIT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
if(APP_GIT_VERSION STREQUAL "")
  set(APP_GIT_VERSION "unknown")
endif()

set(APP_PACKAGE_VERSION "${APP_GIT_VERSION}")
string(REGEX REPLACE "^v" "" APP_PACKAGE_VERSION "${APP_PACKAGE_VERSION}")
string(REGEX REPLACE "-" "." APP_PACKAGE_VERSION "${APP_PACKAGE_VERSION}")
string(REGEX REPLACE "[^A-Za-z0-9.+~]" "." APP_PACKAGE_VERSION "${APP_PACKAGE_VERSION}")
if(NOT APP_PACKAGE_VERSION MATCHES "^[0-9]")
  set(APP_PACKAGE_VERSION "0.0.0+git.${APP_PACKAGE_VERSION}")
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/git_version.h.in
  ${CMAKE_BINARY_DIR}/generated/git_version.h
  @ONLY)

set(COMMON_SOURCES
  common/common.c
  common/table.c)

set(BEASTIE_SOURCES
  beastie/beastie_log.c
  beastie/cli_options.c
  beastie/commands/capture_cmd.c
  beastie/main.c
  beastie/pcap_writer.c)

set(BOY_SOURCES
  boy/boy_log.c
  boy/cli_options.c
  boy/collectors.c
  boy/commands/memif_cmd.c
  boy/commands/phy_cmd.c
  boy/commands/span_cmd.c
  boy/commands/version_cmd.c
  boy/main.c
  boy/vpp_client.c)

add_executable(beastie ${BEASTIE_SOURCES} ${COMMON_SOURCES})
target_include_directories(beastie PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/common
  ${CMAKE_SOURCE_DIR}/beastie
  ${CMAKE_BINARY_DIR}/generated
  ${PCAP_INCLUDE_DIRS}
  ${MEMIF_INCLUDE_DIRS})
target_compile_options(beastie PRIVATE -Wall)
target_link_libraries(beastie PRIVATE Threads::Threads ${MEMIF_LINK_LIBRARIES} ${PCAP_LINK_LIBRARIES})

add_executable(boy ${BOY_SOURCES} ${COMMON_SOURCES})
target_include_directories(boy PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/common
  ${CMAKE_SOURCE_DIR}/boy
  ${CMAKE_BINARY_DIR}/generated
  ${PCAP_INCLUDE_DIRS}
  ${VAPI_INCLUDE_DIRS})
target_compile_options(boy PRIVATE -Wall)
target_compile_definitions(boy PRIVATE
  BOY_VPP_API_SERIES=${VPP_API_SERIES}
  BOY_VPP_API_VERSION_STR="${VPP_API_VERSION}")
target_link_libraries(boy PRIVATE Threads::Threads ${PCAP_LINK_LIBRARIES} ${VAPI_LINK_LIBRARIES})

install(TARGETS beastie boy
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES ${CMAKE_SOURCE_DIR}/README.md
  DESTINATION ${CMAKE_INSTALL_DOCDIR})

set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_PACKAGE_NAME "beastie-boy")
set(CPACK_PACKAGE_VENDOR "beastie-boy")
set(CPACK_PACKAGE_CONTACT "unknown")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Beastie and Boy utilities for VPP memif and SPAN workflows")
set(CPACK_PACKAGE_FILE_NAME
  "${CPACK_PACKAGE_NAME}-${APP_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_VERSION "${APP_PACKAGE_VERSION}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/README.md")

include(CPack)
