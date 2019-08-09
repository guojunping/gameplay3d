SET(CMAKE_MIN_REQ_VERSION 2.4)
cmake_minimum_required(VERSION ${CMAKE_MIN_REQ_VERSION})

INCLUDE("../CMakeSettings.txt")

IF(FBX_MAYA)
    SET(FBX_TARGET_SOURCE 
        mayaextension.h 
        mayacustomdataplugin.cxx
    )
ELSEIF(FBX_3DSMAX)
    SET(FBX_TARGET_SOURCE 
        maxextension.h
        maxcustomdataplugin.cxx
    )
ENDIF()

FBX_PROJECT(${FBXEXT_TARGET}simple .)
ADD_LIBRARY(${FBX_TARGET_NAME} SHARED ${FBX_TARGET_SOURCE})

SET_SAMPLES_GLOBAL_FLAGS()
