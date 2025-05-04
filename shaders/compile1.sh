#!/bin/env bash

# TODO: online compile
# https://github.com/google/shaderc/blob/main/examples/online-compile/main.cc

source ./env
mkdir -p ${OUTPUT_DIR}

${GLSLC} -fshader-stage=vert shader1.vert.glsl -o ${OUTPUT_DIR}/shader1.vert.spirv
${GLSLC} -fshader-stage=vert shader2.vert.glsl -o ${OUTPUT_DIR}/shader2.vert.spirv
${GLSLC} -fshader-stage=frag shader1.frag.glsl -o ${OUTPUT_DIR}/shader1.frag.spirv