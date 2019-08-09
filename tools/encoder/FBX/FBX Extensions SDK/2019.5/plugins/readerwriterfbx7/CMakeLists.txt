SET(CMAKE_MIN_REQ_VERSION 2.4)
cmake_minimum_required(VERSION ${CMAKE_MIN_REQ_VERSION})

INCLUDE("../CMakeSettings.txt")

SET(FBX_TARGET_SOURCE 
    fbx7plugin.cxx
    fbxreaderfbx7.cxx
    fbxreaderfbx7.h
    fbxwriterfbx7.cxx
    fbxwriterfbx7.h
)

FBX_PROJECT(${FBXEXT_TARGET}readerwriterfbx7 .)
ADD_LIBRARY(${FBX_TARGET_NAME} SHARED ${FBX_TARGET_SOURCE})

SET_SAMPLES_GLOBAL_FLAGS()
