================================================================================

                                     README

                       Autodesk FBX Extensions SDK 2019
                       ----------------------------------


Welcome to the FBX Extensions SDK readme! This document will guide you through
steps on how to use the published FBX and Collada readers and writers, as well
as compiling your own extension plug-ins.

For more information, please visit us at http://www.autodesk.com/fbx/

To join the FBX Beta Program, please visit the Autodesk Feedback Community site
at http://beta.autodesk.com

Sincerely,
the Autodesk FBX team

================================================================================



TABLE OF CONTENTS
-----------------

    1. What Is The Purpose Of This SDK?
    2. New And Deprecated Features
    3. Fixed And Known Issues
    4. How-to Use
    5. Release Notes From Previous Releases
    6. Legal Disclaimer 



1. WHAT IS THE PURPOSE OF THIS SDK?
-----------------------------------

The FBX Extension SDK allows developers to write extensions to the FBX plug-ins
in a modular, plug-in like fashion. For example, it enables developers to add
or remove content in FBX files produced by FBX plug-ins (such as the FBX Maya
plug-in) using callbacks. See section 4 for a list of supported FBX plug-ins or
applications. The sample programs starting with Maya, 3dsMax and MotionBuilder
shows how to write this kind of extension. A template is also provided for each.

Another kind of extension can be created, that doesn't use the callbacks
mechanics as explained above. It open the doors to add new custom file
reader/writer to the supported applications. It now becomes possible to
instantiate an FBX object of FbxReader or FbxWriter type that implements their
own file I/O routines. Along with this, we offer the source code that
implements the FBX readers and writers, as well as the Collada ones. For
instance, developers can now see how we generate FBX files from an FBX scene
produced by an FBX plug-in.

In fact, any FBX SDK call can be made with an FbxPlugin object. The FbxPlugin
class is available in the standard FBX SDK, and can be loaded via the manager
call FbxManager::LoadPluginsDirectory(plugins location). This SDK was separated
from the FBX SDK simply because we offer source code, as well as library file
to link against for each application supported. This SDK is absolutely not
necessary if you write FBX plug-ins that do not interact with the applications
specified in section 4.



2. NEW AND DEPRECATED FEATURES
------------------------------

2.1 New Features

    * Replaced the Visual Studio projects/Makefiles with CMakeList.txt.
      Projects now need to be generated using CMake.
      
    * The Windows package provides the libraries to be used with VS2015.

2.2 Deprecated Features

    * No deprecated features in this version.



3. FIXED AND KNOWN ISSUES
-------------------------

3.1 Fixed Issues

    * No fixed issues in this version.

3.2 Known Issues

    * No known issues in this version.



4. HOW-TO USE
-------------

Note: A C++ compiler for your operating system will be required. For instance,
      on Microsoft Windows, you can use Microsoft Visual Studio 2012.
      
4.1 How-to use the FBX Extension SDK with Autodesk Applications
   
    1) Download and install this FBX Extension SDK. 
       Make sure you install the FBX Extension SDK that matches the version 
       of the FBX plug-in used by Maya or 3dsMax applications. The plug-in 
       version can be found in the FBX plug-in Import or Export dialogs under
       the "Information" tab.
       
       For example, if the description on the plug-in dialog shows 
       "FBX Plug-in version: 2016.1.2 Release (238216)" then, this FBX Extension SDK 
       installer must be prefixed: fbx20161_2_
       
    2) It is possible that you need to download and install the corresponding 
       application devkit in order to have access to the include and lib folders. 
    
    3) Open the "plugins" folder under the installation folder of the FBX
       Extension SDK, and choose your favorite sample program to try out.
       You need CMake on your system to generate the actual build instructions 
       for the sample. On Windows, the following statements can be used:
       
            cmake -G "Visual Studio 11 Win64" -DTARGET_PRODUCT="3ds Max 2017" .
            cmake -G "Visual Studio 12 Win64" -DTARGET_PRODUCT="Maya2018" .
       
       On Unix machines (Linux and MacOS) only the Makefile generator has been tested
       and is officially supported:
              
            cmake -G "Unix Makefiles" -DTARGET_PRODUCT="Maya2018" .
            
       Once the generation is completed, you can open the project template you wish
       to compile. The projects rely on some environment variables to locate extra
       includes and library files. For Maya target, it will look for MAYAPATH while 
       for 3dsMax, it will look for ADSK_3DSMAX_SDK_XXXX (were XXXX) is the version 
       number (with the example above: ADSK_3DSMAX_SDK_2017). Typically these 
       variables are set when installing the respective products. If, for some reason,
       they are not, you must define them manually.

    4) Copy the build result output file to the correct location as described below, 
       this is where the FBX plug-ins will search for FBX extensions at startup:
       
       3dsMax:        <3dsMax install folder>/stdplugs/FBX/
       Maya:          <Maya install folder>/plug-ins/fbx/plug-ins/FBX/
       MotionBuilder: <MB install folder>/bin/<win32 or x64>/plugins/FBX/
       
       Please note that you might need to manually create the "FBX" folder at
       these locations, since by default they do not exist. The basics is that
       FBX extensions shall be placed along the FBX plug-ins under a folder
       named "FBX". In other words, the locations given above are relative to
       the default installation folder of plug-ins for each application. The
       "FBX" folder is also case-sensitive, especially true on the Linux and
       MacOS platforms.

       Optionally, you can set the following environment paths to include an
       additional search path for your extension plug-ins, defined as such for
       each application:
       
       3dsMax:          MAX_FBX_EXTENSION_PATH
       Maya:            MAYA_FBX_EXTENSION_PATH
       MotionBuilder:   MOBU_FBX_EXTENSION_PATH
       
4.2 How-to use the FBX Extension SDK with your own application

    1) Download and install this FBX Extension SDK.
    
    2) Download and install the FBX SDK.
    
    3) Upon invoking the creation of the FBX Manager in your software, call the
       function FbxManager::LoadPluginsDirectory(extensions location) to load
       your FBX extensions.



5. RELEASE NOTES FROM PREVIOUS RELEASES
---------------------------------------

2017.1

    * Cleaned up the samples projects/makefiles and updated this readme.

2014.0

    * Some projects failed to compile depending on their location on disk,
      because the file path was too long. All the project file names were
      shortned to fix this.

    * Project settings for all Windows project files were updated to be much
      simpler.

2013.2

    * Added a new callback function to allow users to override unknown objects
      from the FBX export/import.
      
    * Added new sample code to demonstrate the power of extensions.

2013.1

    * It is now possible to set and environment variable to include an
      additional search path for extension plug-ins for supported applications.
      Please refer to the documentation below for more information.

2012.1

    * Added MotionBuilder in the list of applications that support FBX
      Extensions. Please note that MotionBuilder libraries, header files and
      sample codes are shipped with the MotionBuilder OpenReality SDK.
      
2010.2

    * Added extensions callbacks support in FBX 3dsMax and Maya plug-ins.
    
2009.3

    * Introduced the FBX Extension SDK for the first time!



6. LEGAL DISCLAIMER
-------------------

Autodesk and FBX are registered trademarks or trademarks of Autodesk, Inc., in
the USA and/or other countries. All other brand names, product names, or trade-
marks belong to their respective holders.

                       Copyright (C) 2019 Autodesk, Inc.
                              All Rights Reserved

================================================================================
