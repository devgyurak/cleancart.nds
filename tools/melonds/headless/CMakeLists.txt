# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
#
# Builds mdsrun against the melonDS core (source unpacked into ./melonDS by the Dockerfile).
cmake_minimum_required(VERSION 3.16)
project(mdsrun C CXX)

set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_OGLRENDERER ON CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
set(ENABLE_LTO_RELEASE OFF CACHE BOOL "" FORCE)
add_subdirectory(melonDS)
# The GL sources include glad through MELONDS_GL_HEADER; the default in
# PlatformOGL.h expands to an over-quoted name, so give it plainly.
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/melonDS/src)
target_compile_definitions(core PUBLIC MELONDS_GL_HEADER=\"frontend/glad/glad.h\")

# glad comes with the melonDS source; the GL renderers resolve through it.
add_executable(mdsrun mdsrun.cpp melonDS/src/frontend/glad/glad.c)
target_link_libraries(mdsrun PRIVATE core EGL ${CMAKE_DL_LIBS})
