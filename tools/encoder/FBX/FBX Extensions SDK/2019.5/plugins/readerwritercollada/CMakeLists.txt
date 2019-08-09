SET(CMAKE_MIN_REQ_VERSION 2.4)
cmake_minimum_required(VERSION ${CMAKE_MIN_REQ_VERSION})

INCLUDE("../CMakeSettings.txt")

SET(FBX_TARGET_SOURCE 
    colladaplugin.cxx
    fbxcolladaanimationelement.cxx
    fbxcolladaanimationelement.h
    fbxcolladaelement.cxx
    fbxcolladaelement.h
    fbxcolladaiostream.cxx
    fbxcolladaiostream.h
    fbxcolladanamespace.cxx
    fbxcolladanamespace.h
    fbxcolladatokens.h
    fbxcolladautils.cxx
    fbxcolladautils.h
    fbxreadercollada14.cxx
    fbxreadercollada14.h
    fbxwritercollada14.cxx
    fbxwritercollada14.h
)

FBX_PROJECT(${FBXEXT_TARGET}readerwritercollada .)
ADD_LIBRARY(${FBX_TARGET_NAME} SHARED ${FBX_TARGET_SOURCE})

SET_SAMPLES_GLOBAL_FLAGS()
