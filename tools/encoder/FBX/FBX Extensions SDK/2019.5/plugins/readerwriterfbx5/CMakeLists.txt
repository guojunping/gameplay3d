SET(CMAKE_MIN_REQ_VERSION 2.4)
cmake_minimum_required(VERSION ${CMAKE_MIN_REQ_VERSION})

INCLUDE("../CMakeSettings.txt")

SET(FBX_TARGET_SOURCE 
    fbx5plugin.cxx
    fbxreaderfbx5.cxx
    fbxreaderfbx5.h
)

FBX_PROJECT(${FBXEXT_TARGET}readerwriter5 .)
ADD_LIBRARY(${FBX_TARGET_NAME} SHARED ${FBX_TARGET_SOURCE})

SET_SAMPLES_GLOBAL_FLAGS()
