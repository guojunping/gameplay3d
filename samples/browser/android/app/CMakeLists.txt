# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.


include_directories(
            ${ANDROID_NDK}/sources/android/native_app_glue
            ../../../../external-deps/include
            ../../../../gameplay/src
            )

#add_library(lua_lib STATIC IMPORTED)
#set_target_properties(lua_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/liblua.a)

#add_library(base64_lib STATIC IMPORTED)
#set_target_properties(base64_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libbase64.a)

#add_library(BulletCollision_lib STATIC IMPORTED)
#set_target_properties(BulletCollision_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libBulletCollision.a)

#add_library(BulletDynamics_lib STATIC IMPORTED)
#set_target_properties(BulletDynamics_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libBulletDynamics.a)

#add_library(json_lib STATIC IMPORTED)
#set_target_properties(json_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libjson.a)

#add_library(LinearMath_lib STATIC IMPORTED)
#set_target_properties(LinearMath_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libLinearMath.a)

#add_library(ogg_lib STATIC IMPORTED)
#set_target_properties(ogg_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libogg.a)

#add_library(png_lib STATIC IMPORTED)
#set_target_properties(png_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libpng.a)

#add_library(tinyxml2_lib STATIC IMPORTED)
#set_target_properties(tinyxml2_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libtinyxml2.a)

#add_library(vorbis_lib STATIC IMPORTED)
#set_target_properties(vorbis_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libvorbis.a)

#add_library(vorbisenc_lib STATIC IMPORTED)
#set_target_properties(vorbisenc_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libvorbisenc.a)

#add_library(vorbisfile_lib STATIC IMPORTED)
#set_target_properties(vorbisfile_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libvorbisfile.a)

#add_library(z_lib STATIC IMPORTED)
#set_target_properties(z_lib PROPERTIES IMPORTED_LOCATION ../../../../external-deps/lib/android/${ANDROID_ABI}/libz.a)

add_library(gameplay-deps_lib STATIC IMPORTED)
set_target_properties(gameplay-deps_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../../../../external-deps/lib/android/${ANDROID_ABI}/libgameplay-deps.a)

add_library(gameplay_lib STATIC IMPORTED)
set_target_properties(gameplay_lib PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../../../../gameplay/libgameplay.a)


message(${CMAKE_SOURCE_DIR})
add_library(sample-browser
             SHARED
              ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
              ../../../../gameplay/src/gameplay-main-android.cpp
              ../../src/FirstPersonCamera.cpp
              ../../src/Grid.cpp
              ../../src/Sample.cpp
              ../../src/SamplesGame.cpp
              ../../src/Audio3DSample.cpp
              ../../src/AudioSample.cpp
              ../../src/BillboardSample.cpp
              ../../src/FontSample.cpp
              ../../src/FormsSample.cpp
              ../../src/GestureSample.cpp
              ../../src/GamepadSample.cpp
              ../../src/InputSample.cpp
              ../../src/LightSample.cpp
              ../../src/MeshBatchSample.cpp
              ../../src/MeshPrimitiveSample.cpp
              ../../src/ParticlesSample.cpp
              ../../src/PhysicsCollisionObjectSample.cpp
              ../../src/PostProcessSample.cpp
              ../../src/SceneCreateSample.cpp
              ../../src/SceneLoadSample.cpp
              ../../src/SpriteBatchSample.cpp
              ../../src/SpriteSample.cpp
              ../../src/TerrainSample.cpp
              ../../src/TextureSample.cpp
              ../../src/TriangleSample.cpp
              ../../src/WaterSample.cpp)


# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

find_library( # Sets the name of the path variable.
              opensles-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              OpenSLES )



#设置编译指令
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

#如果当前系统版本过低设置拒绝编译
if (${ANDROID_PLATFORM_LEVEL} LESS 11)
  message(FATAL_ERROR "OpenGL 2 is not supported before API level 11 (currently using ${ANDROID_PLATFORM_LEVEL}).")
  return()
elseif (${ANDROID_PLATFORM_LEVEL} LESS 18)
  add_definitions("-DDYNAMIC_ES3")
  set(OPENGL_LIB GLESv2)
else ()
  set(OPENGL_LIB GLESv3)
endif (${ANDROID_PLATFORM_LEVEL} LESS 11)


set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries(sample-browser
                       android
                       ${OPENGL_LIB}
                       ${log-lib}
                       ${opensles-lib}
                       EGL
                       gameplay_lib
                       gameplay-deps_lib
                       #lua_lib
                       #base64_lib
                       #BulletCollision_lib
                       #BulletDynamics_lib
                       #json_lib
                       #LinearMath_lib
                       #ogg_lib
                       #png_lib
                       #tinyxml2_lib
                       #vorbis_lib
                       #vorbisenc_lib
                       #vorbisfile_lib
                       #z_lib
                       )