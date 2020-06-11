project(hdRpr)

cmake_minimum_required(VERSION 3.12)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/defaults
                      ${CMAKE_SOURCE_DIR}/cmake/modules
                      ${CMAKE_SOURCE_DIR}/cmake/macros)

include(Options)
include(ProjectDefaults)
include(Packages)

# This has to be defined after Packages is included, because it relies on the
# discovered path to the python executable.
set(PXR_PYTHON_SHEBANG "${PYTHON_EXECUTABLE}"
    CACHE
    STRING
    "Replacement path for Python #! line."
)

# CXXDefaults will set a variety of variables for the project.
# Consume them here. This is an effort to keep the most common
# build files readable.
include(CXXDefaults)
add_definitions(${_PXR_CXX_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${_PXR_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")

include(Public)

set(CMAKE_CXX_STANDARD 14)

STRING(REPLACE "/O2" "/Od" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} CACHE STRING "compile flags" FORCE)

install(
    CODE
    "FILE(WRITE \"${CMAKE_INSTALL_PREFIX}/plugin/plugInfo.json\"
    \"{
    \\\"Includes\\\": [ \\\"usd/*/resources/\\\", \\\"../lib/rpr/*/*/\\\" ]
}\")")

add_custom_target(shared_libs)
if(HoudiniUSD_FOUND)
    install(
        CODE
        "FILE(WRITE \"${CMAKE_INSTALL_PREFIX}/houdini/dso/usd_plugins/plugInfo.json\"
        \"{
        \\\"Includes\\\": [ \\\"../../../plugin/\\\" ]
    }\")")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(rpr/imaging/ipc)
add_subdirectory(pxr/imaging/plugin/hdRpr)
add_subdirectory(pxr/imaging/plugin/hdRprIpc)

install(FILES README.md DESTINATION .)
install(FILES INSTALL.md DESTINATION .)
install(FILES LICENSE.md DESTINATION .)
