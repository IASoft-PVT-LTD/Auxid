# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED AUXID_SRC_DIR)
    message(FATAL_ERROR "auxid_lint_ban_list: AUXID_SRC_DIR not set.")
endif()

if(NOT IS_DIRECTORY "${AUXID_SRC_DIR}")
    message(FATAL_ERROR "auxid_lint_ban_list: AUXID_SRC_DIR='${AUXID_SRC_DIR}' is not a directory.")
endif()

get_filename_component(_auxid_root "${AUXID_SRC_DIR}" DIRECTORY)

file(GLOB_RECURSE _scan_files
    LIST_DIRECTORIES false
    "${AUXID_SRC_DIR}/cpp/*.cpp"
    "${AUXID_SRC_DIR}/ixx/*.ixx"
    "${_auxid_root}/include/auxid/*.hpp"
    "${_auxid_root}/tests/cpp/*.cpp"
    "${_auxid_root}/tests/cpp/**/*.cpp"
)

set(_banned_tokens
    "std::unordered_map"
    "std::unordered_set"
    "std::list"
    "std::map"
    "std::set"
    "std::multimap"
    "std::multiset"
    "std::shared_ptr"
    "std::make_shared"
    "std::allocate_shared"
    "std::weak_ptr"
    "std::function"
    "<iostream>"
    "std::cout"
    "std::cerr"
    "std::clog"
    "dynamic_cast"
    "typeid"
)

set(_violations "")

foreach(_file ${_scan_files})
    file(READ "${_file}" _contents)

    string(REGEX REPLACE "\r\n" "\n" _contents "${_contents}")
    string(REPLACE "\n" ";" _lines "${_contents}")

    set(_lineno 0)
    foreach(_line IN LISTS _lines)
        math(EXPR _lineno "${_lineno} + 1")

        foreach(_tok IN LISTS _banned_tokens)
            string(FIND "${_line}" "${_tok}" _hit)
            if(NOT _hit EQUAL -1)
                list(APPEND _violations "${_file}:${_lineno}: banned token '${_tok}'")
            endif()
        endforeach()

        if(_line MATCHES "(^|[^a-zA-Z0-9_])throw[ \t]*[a-zA-Z_(;{]")
            list(APPEND _violations "${_file}:${_lineno}: banned token 'throw'")
        endif()

        if(_line MATCHES "(^|[^a-zA-Z0-9_])try([^a-zA-Z0-9_]|$)")
            list(APPEND _violations "${_file}:${_lineno}: banned token 'try'")
        endif()

        if(_line MATCHES "(^|[^a-zA-Z0-9_])catch([^a-zA-Z0-9_]|$)")
            list(APPEND _violations "${_file}:${_lineno}: banned token 'catch'")
        endif()
    endforeach()
endforeach()

if(_violations)
    message("Rigid C++ ban-list violations:")
    foreach(_v IN LISTS _violations)
        message("  ${_v}")
    endforeach()
    list(LENGTH _violations _n)
    message(FATAL_ERROR "auxid_lint_ban_list: ${_n} violation(s) found.")
endif()

message(STATUS "auxid_lint_ban_list: clean (${_scan_files})")
