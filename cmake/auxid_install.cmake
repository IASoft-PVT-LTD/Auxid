# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(AUXID_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Auxid")
set(AUXID_MODULE_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/auxid/ixx")

install(TARGETS libauxid
    EXPORT AuxidTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET auxid_interfaces
        DESTINATION ${AUXID_MODULE_INSTALL_DIR}
)

install(FILES
    "${AUXID_ROOT}/include/auxid/api.hpp"
    "${AUXID_ROOT}/include/auxid/macros.hpp"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/auxid
)

install(FILES
    "${AUXID_ROOT}/cmake/auxid_setup_project.cmake"
    "${AUXID_ROOT}/cmake/auxid_sanitizers.cmake"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/auxid/cmake
)

install(CODE "
set(_auxid_module_dir \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/auxid/ixx\")
file(GLOB _auxid_ixx_files \"\${_auxid_module_dir}/*.ixx\")
foreach(_auxid_ixx IN LISTS _auxid_ixx_files)
    get_filename_component(_auxid_name \"\${_auxid_ixx}\" NAME_WE)
    file(RENAME \"\${_auxid_ixx}\" \"\${_auxid_module_dir}/\${_auxid_name}.cppm\")
endforeach()
if(EXISTS \"\${_auxid_module_dir}/auxid-test.cppm\")
    file(REMOVE \"\${_auxid_module_dir}/auxid-test.cppm\")
endif()
")

install(EXPORT AuxidTargets
    FILE AuxidTargets.cmake
    NAMESPACE Auxid::
    DESTINATION ${AUXID_INSTALL_CMAKEDIR}
)

configure_package_config_file(
    "${AUXID_ROOT}/cmake/AuxidConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AuxidConfig.cmake"
    INSTALL_DESTINATION ${AUXID_INSTALL_CMAKEDIR}
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_DATADIR AUXID_INSTALL_CMAKEDIR
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AuxidConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AuxidConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AuxidConfigVersion.cmake"
    DESTINATION ${AUXID_INSTALL_CMAKEDIR}
)
