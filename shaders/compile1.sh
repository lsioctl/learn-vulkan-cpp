#!/bin/env bash

# TODO: online compile
# https://github.com/google/shaderc/blob/main/examples/online-compile/main.cc

source ./env
mkdir -p ${OUTPUT_DIR}

${GLSLC} -fshader-stage=vert shader1.vert.glsl -o ${OUTPUT_DIR}/shader1.vert.spirv
${GLSLC} -fshader-stage=vert shader2.vert.glsl -o ${OUTPUT_DIR}/shader2.vert.spirv
${GLSLC} -fshader-stage=vert shader3.vert.glsl -o ${OUTPUT_DIR}/shader3.vert.spirv
${GLSLC} -fshader-stage=vert shader4.vert.glsl -o ${OUTPUT_DIR}/shader4.vert.spirv
${GLSLC} -fshader-stage=vert shader5.vert.glsl -o ${OUTPUT_DIR}/shader5.vert.spirv
${GLSLC} -fshader-stage=frag shader1.frag.glsl -o ${OUTPUT_DIR}/shader1.frag.spirv
${GLSLC} -fshader-stage=frag shader2.frag.glsl -o ${OUTPUT_DIR}/shader2.frag.spirv
${GLSLC} -fshader-stage=frag shader3.frag.glsl -o ${OUTPUT_DIR}/shader3.frag.spirv