set(PXR_PREFIX rpr/imaging)
set(PXR_PACKAGE ipc)

set(BUILD_SHARED_LIBS_backup ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(thirdparty/tinyProcess)
set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_backup})

find_package(ZeroMQ REQUIRED)
find_package(cppzmq REQUIRED)

set(USD_LIBRARIES
    vt
    arch
    tf
    plug
    sdf)

# if(NOT Houdini_FOUND)
#     add_definitions(-DBOOST_ALL_DYN_LINK)
#     link_directories(${pxr_DIR}/lib)
# endif()

pxr_library(ipc
    DISABLE_PRECOMPILED_HEADERS

    PUBLIC_CLASSES
        server
        client
        tokens
        debugCodes

    LIBRARIES
        ${USD_LIBRARIES}
        tiny-process-library
        libzmq-static
        cppzmq)

set(PLUG_INFO_ROOT "../..")
set(PLUG_INFO_LIBRARY_PATH "../ipc${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(PLUG_INFO_RESOURCE_PATH "ipc/resources")
configure_file(plugInfo.json ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json)

install(
    FILES viewer.py ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json
    DESTINATION lib/rpr/ipc/resources)
install(
    FILES 
    DESTINATION lib/rpr/ipc/resources)
