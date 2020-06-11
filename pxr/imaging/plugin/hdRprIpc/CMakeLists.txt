set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hdRprIpc)

set(OptLibs ${ARGN})
set(OptBin ${ARGN})

set(_sep ${PXR_RESOURCE_FILE_SRC_DST_SEPARATOR})
if(HoudiniUSD_FOUND)
    list(APPEND OptLibs Houdini)
endif(HoudiniUSD_FOUND)

if(USDMonolithic_FOUND)
    set(USD_LIBRARIES usd_ms)
else()
    set(USD_LIBRARIES
        trace
        tf
        vt
        gf
        hf
        hd
        sdf
        usd
        usdGeom)
endif()

pxr_plugin(hdRprIpc
    DISABLE_PRECOMPILED_HEADERS

    LIBRARIES
        ${USD_LIBRARIES}
        ipc

        ${OptLibs}

    PRIVATE_CLASSES
        rendererPlugin
        renderDelegate
        renderThread
        renderPass
        layer
        mesh
        # material
        # light
        renderBuffer

    PRIVATE_HEADERS
        renderParam.h

    RESOURCE_FILES
        plugInfo.json
)

GroupSources(hdRprIpc)
