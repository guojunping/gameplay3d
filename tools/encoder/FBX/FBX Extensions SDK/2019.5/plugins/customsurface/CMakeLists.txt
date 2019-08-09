SET(CMAKE_MIN_REQ_VERSION 2.4)
cmake_minimum_required(VERSION ${CMAKE_MIN_REQ_VERSION})

INCLUDE("../CMakeSettings.txt")

IF(FBX_MAYA)
    SET(FBX_TARGET_SOURCE 
        mayacustomsurfaceplugin.cxx
        mayaextension.h
    )
ELSEIF(FBX_3DSMAX)
    SET(FBX_TARGET_SOURCE 
        maxcustomsurfaceplugin.cxx
        maxextension.h
    )
ENDIF()

FBX_PROJECT(${FBXEXT_TARGET}customsurface .)
ADD_LIBRARY(${FBX_TARGET_NAME} SHARED ${FBX_TARGET_SOURCE})

SET_SAMPLES_GLOBAL_FLAGS()
