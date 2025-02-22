# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

#------------------------------------------------------------------------------
# Configure and install libonnx ...
#------------------------------------------------------------------------------

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
set(OV_ONNX_NAMESPACE ngraph_onnx)

if(NOT DEFINED ONNX_USE_MSVC_STATIC_RUNTIME)
    set(ONNX_USE_MSVC_STATIC_RUNTIME OFF)
endif()

if(FORCE_FRONTENDS_USE_PROTOBUF)
    set(ONNX_USE_LITE_PROTO_DEFAULT OFF)
else()
    set(ONNX_USE_LITE_PROTO_DEFAULT ON)
endif()

set(ONNX_USE_PROTOBUF_SHARED_LIBS CACHE BOOL "Use dynamic protobuf by ONNX library" FORCE)
set(ONNX_NAMESPACE ${OV_ONNX_NAMESPACE})
set(ONNX_USE_LITE_PROTO ${ONNX_USE_LITE_PROTO_DEFAULT} CACHE BOOL "Use protobuf lite for ONNX library" FORCE)
set(ONNX_ML ON CACHE BOOL "Use ONNX ML" FORCE)
if(CMAKE_CROSSCOMPILING)
    set(ONNX_CUSTOM_PROTOC_EXECUTABLE ${SYSTEM_PROTOC})
endif()

# build targets

function(ov_onnx_build_static)
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(onnx EXCLUDE_FROM_ALL)
endfunction()

ov_onnx_build_static()

target_include_directories(onnx SYSTEM PRIVATE "${Protobuf_INCLUDE_DIRS}")
target_include_directories(onnx_proto SYSTEM PRIVATE "${Protobuf_INCLUDE_DIRS}")

ov_disable_all_warnings(onnx onnx_proto)

# install

ov_install_static_lib(onnx core)
ov_install_static_lib(onnx_proto core)

# WA for ONNX: protobuf must be in the same export set of ONNX targets
if(NOT BUILD_SHARED_LIBS)
    if(ONNX_USE_LITE_PROTO)
        install(TARGETS ${Protobuf_LITE_LIBRARIES} EXPORT ONNXTargets
                ARCHIVE DESTINATION ${IE_CPACK_ARCHIVE_PATH} COMPONENT core)
    else()
        install(TARGETS ${Protobuf_LIBRARIES} EXPORT ONNXTargets
                ARCHIVE DESTINATION ${IE_CPACK_ARCHIVE_PATH} COMPONENT core)
    endif()
endif()
