/****************************************************************************************

   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.

****************************************************************************************/

#include "MyOwnReader.h"
#include "MyOwnWriter.h"

#define PLUGIN_NAME			"CustomReaderWriter"
#define PLUGIN_VERSION		"1.0"
#define PLUGIN_EXTENSION	"abc"

// Create your own writer.
// And your writer will get a pPluginID and pSubID. 
FbxWriter* CreateMyOwnWriter(FbxManager& pManager, FbxExporter& pExporter, int pSubID, int pPluginID)
{
	FbxWriter* lWriter = FbxNew< MyOwnWriter >(pManager, pPluginID);
	lWriter->SetIOSettings(pExporter.GetIOSettings());
	return lWriter;
}

// Get extension, description or version info about MyOwnWriter
void* GetMyOwnWriterInfo(FbxWriter::EInfoRequest pRequest, int pId)
{
    static const char* sExt[] = {PLUGIN_EXTENSION, 0};
    static const char* sDesc[] = {PLUGIN_NAME"Writer", 0};

    switch( pRequest )
    {
		case FbxWriter::eInfoExtension:	return sExt;
		case FbxWriter::eInfoDescriptions:	return sDesc;
		case FbxWriter::eInfoVersions:		return 0;
		default:							return 0;
    }
}

void FillOwnWriterIOSettings(FbxIOSettings& pIOS)
{
    // Here you can write your own FbxIOSettings and parse them.
    FbxProperty FBXExtentionsSDKGroup = pIOS.GetProperty(EXP_FBX_EXT_SDK_GRP);
    if( !FBXExtentionsSDKGroup.IsValid() ) return;

    FbxProperty IOPluginGroup = pIOS.AddPropertyGroup(FBXExtentionsSDKGroup, PLUGIN_NAME, FbxStringDT, PLUGIN_NAME);
    if( IOPluginGroup.IsValid() )
    {
        //Add your plugin export options here...
        //Example:
        bool Default_True = true;
        pIOS.AddProperty(IOPluginGroup, "Test", FbxBoolDT, "Test", &Default_True);
    }
}


// Creates a MyOwnReader in the Sdk Manager
FbxReader* CreateMyOwnReader(FbxManager& pManager, FbxImporter& pImporter, int pSubID, int pPluginID)
{
	FbxReader* lReader = FbxNew< MyOwnReader >(pManager, pPluginID);
	lReader->SetIOSettings(pImporter.GetIOSettings());
	return lReader;
}

// Get extension, description or version info about MyOwnReader
void *GetMyOwnReaderInfo(FbxReader::EInfoRequest pRequest, int pId)
{
    static const char* sExt[] = {PLUGIN_EXTENSION, 0};
    static const char* sDesc[] = {PLUGIN_NAME"Reader", 0};

    switch( pRequest )
    {
		case FbxReader::eInfoExtension:	return sExt;
		case FbxReader::eInfoDescriptions:	return sDesc;
		default:							return 0;
    }
}

void FillOwnReaderIOSettings(FbxIOSettings& pIOS)
{    
    // Here you can write your own FbxIOSettings and parse them.
    FbxProperty FBXExtentionsSDKGroup = pIOS.GetProperty(IMP_FBX_EXT_SDK_GRP);
    if( !FBXExtentionsSDKGroup.IsValid() ) return;

    FbxProperty IOPluginGroup = pIOS.AddPropertyGroup(FBXExtentionsSDKGroup, PLUGIN_NAME, FbxStringDT, PLUGIN_NAME);
    if( IOPluginGroup.IsValid() )
    {
        //Add your plugin import options here...
        //Example:
        bool Default_True = true;
        pIOS.AddProperty(IOPluginGroup, "Test", FbxBoolDT, "Test", &Default_True);
    }
}


class MyOwnWriterReaderPlugin : public FbxPlugin
{
    FBXSDK_PLUGIN_DECLARE(MyOwnWriterReaderPlugin);

protected:
    explicit MyOwnWriterReaderPlugin(const FbxPluginDef& pDefinition, FbxModule pFbxModule) : FbxPlugin(pDefinition, pFbxModule)
    {
    }

    // Implement kfbxmodules::FbxPlugin
    virtual bool SpecificInitialize()
    {
        int FirstPluginID, RegistredCount;
        GetData().mSDKManager->GetIOPluginRegistry()->RegisterReader(CreateMyOwnReader, GetMyOwnReaderInfo, FirstPluginID, RegistredCount, FillOwnReaderIOSettings);
        GetData().mSDKManager->GetIOPluginRegistry()->RegisterWriter(CreateMyOwnWriter, GetMyOwnWriterInfo, FirstPluginID, RegistredCount, FillOwnWriterIOSettings);
        return true;
    }

    virtual bool SpecificTerminate()
    {
        return true;
    }
};

FBXSDK_PLUGIN_IMPLEMENT(MyOwnWriterReaderPlugin);

// FBX Interface
extern "C"
{
    //The DLL is owner of the plug-in
    static MyOwnWriterReaderPlugin* sPlugin = NULL;

    //This function will be called when an application will request the plug-in
	FBXSDK_DLLEXPORT void FBXPluginRegistration(FbxPluginContainer& pContainer, FbxModule pFbxModule)
    {
        if( sPlugin == NULL )
        {
            //Create the plug-in definition which contains the information about the plug-in
            FbxPluginDef sPluginDef;
            sPluginDef.mName = PLUGIN_NAME"Plugin";
            sPluginDef.mVersion = PLUGIN_VERSION;

            //Create an instance of the plug-in.  The DLL has the ownership of the plug-in
            sPlugin = MyOwnWriterReaderPlugin::Create(sPluginDef, pFbxModule);

            //Register the plug-in
            pContainer.Register(*sPlugin);
        }
    }
}


