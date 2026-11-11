# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
#
# Builds mdsrun against the melonDS core (source unpacked into ./melonDS by the Dockerfile).
cmake_minimum_required(VERSION 3.16)
project(mdsrun C CXX)

set(BUILD_QT_SDL OFF CACHE BOOL "" FORCE)
set(ENABLE_JIT OFF CACHE BOOL "" FORCE)
set(ENABLE_OGLRENDERER OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB OFF CACHE BOOL "" FORCE)
set(ENABLE_LTO_RELEASE OFF CACHE BOOL "" FORCE)
add_subdirectory(melonDS)

add_executable(mdsrun mdsrun.cpp)
target_link_libraries(mdsrun PRIVATE core)
