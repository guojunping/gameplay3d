SET(CMAKE_MIN_REQ_VERSION 2.4)
cmake_minimum_required(VERSION ${CMAKE_MIN_REQ_VERSION})

INCLUDE("../CMakeSettings.txt")

SET(FBX_TARGET_SOURCE 
    MyOwnReader.cxx
    MyOwnReader.h
    MyOwnWriter.cxx
    MyOwnWriter.h
    myreaderwriterplugin.cxx
)

FBX_PROJECT(${FBXEXT_TARGET}readerwritercustom .)
ADD_LIBRARY(${FBX_TARGET_NAME} SHARED ${FBX_TARGET_SOURCE})

SET_SAMPLES_GLOBAL_FLAGS()
