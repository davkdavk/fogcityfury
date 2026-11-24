cmake_minimum_required(VERSION 3.25)
project(fogcityfurysrc CXX)
set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(FCF_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/build/gen" CACHE PATH "Generated Xenos shader directory")

function(add_fcf_xex target)
  add_executable(${target} "${REPO_ROOT}/src/game.cpp" "${CMAKE_CURRENT_LIST_DIR}/runtime_compat.cpp")
  set_target_properties(${target} PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
  target_compile_definitions(${target} PRIVATE FCF_GFX_XSDK ${ARGN})
  target_compile_options(${target} PRIVATE -Wall -Wno-unused-function -Wno-misleading-indentation)
  target_include_directories(${target} PRIVATE "${REPO_ROOT}/src" "${FCF_SHADER_DIR}")
endfunction()

set(FCF_XENOS_LOD_NEAR 120 CACHE STRING "Cell LOD0-to-LOD1 distance in metres")
set(FCF_XENOS_LOD_FAR 300 CACHE STRING "Cell LOD1-to-LOD2 distance in metres")
set(FCF_XENOS_LOD_ULTRA 600 CACHE STRING "Cell LOD2-to-LOD3 distance in metres")
set(FCF_XENOS_ROAD_LOD0 120 CACHE STRING "Road-cell LOD0 distance in metres")

# The game: one title. Its shaders are the 360's own (xenon/lean_shaders.py, made by build_shaders.sh); FCF_PERF_NOISE
# makes the noise textures they read, FCF_XENOS_LEAN draws with them, FCF_XENOS_CULL/LOD draw the world in cells.
# (The 2.x bring-up's diagnostic titles - flat, noise, water, cull, draw-mask, sky and Hi-Z tests - were removed in 2.25.)
add_fcf_xex(lean FCF_PERF_NOISE FCF_XENOS_LEAN FCF_XENOS_CULL FCF_XENOS_LOD FCF_XENOS_LOD_NEAR=${FCF_XENOS_LOD_NEAR} FCF_XENOS_LOD_FAR=${FCF_XENOS_LOD_FAR} FCF_XENOS_LOD_ULTRA=${FCF_XENOS_LOD_ULTRA} FCF_XENOS_ROAD_LOD0=${FCF_XENOS_ROAD_LOD0})
xsdk_add_xex(lean TITLE_ID 0x46434C31 NAME "Fog City Fury" VERSION 2.2.9.0)
