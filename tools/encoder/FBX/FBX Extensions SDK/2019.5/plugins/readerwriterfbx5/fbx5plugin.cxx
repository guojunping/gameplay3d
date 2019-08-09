/****************************************************************************************

   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.

****************************************************************************************/

#include "fbxreaderfbx5.h"

#define PLUGIN_NAME			"CustomFBX5"
#define PLUGIN_VERSION		"1.0"
#define PLUGIN_EXTENSION	"fbx"

FbxReader* CreateReader(FbxManager& pManager, FbxImporter& pImporter, int pSubID, int pPluginID)
{
	FbxReader* lReader = FbxNew< FbxReaderFbx5 >(pManager, pImporter, pPluginID, FbxStatusGlobal::GetRef());
	lReader->SetIOSettings(pImporter.GetIOSettings());
	return lReader;
}

void* GetInfoReader(FbxReader::EInfoRequest pRequest, int pId)
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

void GetReaderIOSettings(FbxIOSettings& pIOS)
{
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

class Fbx5Plugin : public FbxPlugin
{
	FBXSDK_PLUGIN_DECLARE(Fbx5Plugin);

protected:
	explicit Fbx5Plugin(const FbxPluginDef& pDefinition, FbxModule pFbxModule) : FbxPlugin(pDefinition, pFbxModule)
	{
	}

	// Implement kfbxmodules::FbxPlugin
	virtual bool SpecificInitialize()
	{
		int FirstPluginID, RegistredCount;
		GetData().mSDKManager->GetIOPluginRegistry()->RegisterReader(CreateReader, GetInfoReader, FirstPluginID, RegistredCount, GetReaderIOSettings);
		return true;
	}

	virtual bool SpecificTerminate()
	{
		return true;
	}
};

FBXSDK_PLUGIN_IMPLEMENT(Fbx5Plugin);

// FBX Interface
extern "C"
{
    //The DLL is owner of the plug-in
    static Fbx5Plugin* sPlugin = NULL;

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
            sPlugin = Fbx5Plugin::Create(sPluginDef, pFbxModule);

            //Register the plug-in
            pContainer.Register(*sPlugin);
        }
    }
}


