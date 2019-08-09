/****************************************************************************************

   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.

****************************************************************************************/

#include "mayaextension.h"

#include <maya/MDagPath.h>
#include <maya/MFnMesh.h>

static FbxManager* gFbxManager = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Custom Object declaration
#define MY_CUSTOM_CLASS_NAME	"MyCustomClass"
#define MY_CUSTOM_CLASS_TYPE	"MyCustomType"
#define MY_CUSTOM_CLASS_SUBTYPE	"MyCustomSubType"

class MyCustomObject : public FbxObject
{
	FBXSDK_OBJECT_DECLARE(MyCustomObject, FbxObject);
};

FBXSDK_OBJECT_IMPLEMENT(MyCustomObject);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Plugin declaration and initialization methods
class MayaCustomDataPlugin : public FbxPlugin
{
	FBXSDK_PLUGIN_DECLARE(MayaCustomDataPlugin);

protected:
	explicit MayaCustomDataPlugin(const FbxPluginDef& pDefinition, FbxModule pFbxModule) : FbxPlugin(pDefinition, pFbxModule)
	{
	}

	// Implement kfbxmodules::FbxPlugin
	virtual bool SpecificInitialize()
	{
		//Register MyCustomObject class with the plug-in's manager
		gFbxManager = GetData().mSDKManager;
		gFbxManager->RegisterFbxClass(MY_CUSTOM_CLASS_NAME, FBX_TYPE(MyCustomObject), FBX_TYPE(FbxObject), MY_CUSTOM_CLASS_TYPE, MY_CUSTOM_CLASS_SUBTYPE);
		return true;
	}

	virtual bool SpecificTerminate()
	{
		//Unregister MyCustomObject class with the plug-in's manager
		gFbxManager->UnregisterFbxClass(FBX_TYPE(MyCustomObject));
		return true;
	}
};

FBXSDK_PLUGIN_IMPLEMENT(MayaCustomDataPlugin);

// FBX Interface
extern "C"
{
    //The DLL is owner of the plug-in
    static MayaCustomDataPlugin* sPlugin = NULL;

    //This function will be called when an application will request the plug-in
    FBXSDK_DLLEXPORT void FBXPluginRegistration(FbxPluginContainer& pContainer, FbxModule pFbxModule)
    {
        if( sPlugin == NULL )
        {
            //Create the plug-in definition which contains the information about the plug-in
            FbxPluginDef sPluginDef;
            sPluginDef.mName = "MayaExtensionSimplePlugin";
            sPluginDef.mVersion = "1.0";

            //Create an instance of the plug-in.  The DLL has the ownership of the plug-in
            sPlugin = MayaCustomDataPlugin::Create(sPluginDef, pFbxModule);

            //Register the plug-in
            pContainer.Register(*sPlugin);
        }
    }

	FBXSDK_MAYA_EXTENSION_DECLARE();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Maya Extension Export Callbacks
bool MayaExt_ExportHandled(MObject& pMayaObject)
{
	//In this example, we aren't replacing any objects in the fbx scene, so we don't have to
	//prevent the Maya plugin from translating Maya objects into fbx objects.
	return false;
}

void MayaExt_ExportBegin(FbxScene* pFbxScene)
{
	//In this example, we will create our custom data at the end of the export.
}

void MayaExt_ExportTranslated(FbxObject* pFbxObject, MObject& pMayaObject)
{
	//Since we aren't replacing objects in the scene hierarchy in this example, we don't have
	//anything to do with this information.
}


bool MayaExt_ExportProcess(FbxObject** pOutputFbxObject, MObject& pInputObject, FbxScene* pFbxScene)
{
    //In this example, we want to perform a post-export process so we notify the caller that
    //we do not wish to intervene at this moment.
    return false;
}

void MayaExt_ExportEnd(FbxScene* pFbxScene)
{
	//Since we didn't export our custom data in the pre-export callback, we'll have to do it now.
	//This is the simplest solution for simple plugin-of-plugin like this example.

	//Loop through Maya scene and find meshes
	MStatus	Status;
	MItDag	DagIterator(MItDag::kDepthFirst, MFn::kInvalid, &Status);
    if( !Status ) return;

	MDagPath	DagPath;
	int			VertexCount;
	for( ; !DagIterator.isDone(); DagIterator.next() )
	{
		Status = DagIterator.getPath(DagPath);
        if( !Status ) continue;

		MObject MayaObject(DagPath.node());
		MFnMesh MayaMesh(MayaObject, &Status);
		if( Status )
		{
			//We found a mesh, lets store custom data for each vertex :)
			//Obviously, for the simplicity of this example, we generate random data ourselves, but in
			//the real world, one could get data from the Maya scene or from some other module/plugin.
			VertexCount = MayaMesh.numVertices();

			//Make sure our custom object class was correctly registred
			FbxClassId MyCustomDataClassId = gFbxManager->FindClass(MY_CUSTOM_CLASS_NAME);
			if( !MyCustomDataClassId.IsValid() ) return;

			//Create the custom object to hold the custom data
			MyCustomObject* MyCustomObjectInstance = MyCustomObject::Create(pFbxScene, "My Custom Object Instance");

			//Fill our custom object with some properties, and set value to the vertex's index
			for( int Iter = 0; Iter < VertexCount; ++Iter )
			{
				float		Value = (float)Iter+1;
				FbxString	PropertyName = "CustomData" + FbxString(Iter+1);
				FbxProperty	NewProperty = FbxProperty::Create(MyCustomObjectInstance, FbxFloatDT, PropertyName.Buffer());
				if( NewProperty.IsValid() ) NewProperty.Set(Value);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Maya Extension Import Callbacks
bool MayaExt_ImportHandled(FbxObject* pFbxObject)
{
	//In this example, for simplicity we aren't demonstrating object replacement.
	return false;
}

void MayaExt_ImportBegin(FbxScene* pFbxScene)
{
	//For this example, we need the FBX loaded to retrieve our data, so we wait for now.
}

bool MayaExt_ImportProcess(MObject& pOutputObject, FbxObject* pInputFbxObject, bool pIsAnInstance, bool pMerge)
{
    //In this example, we want to perform a post-import process so we notify the caller that
    //we do not wish to intervene at this moment.
    return false;
}
void MayaExt_ImportTranslated(FbxObject* pFbxObject, MObject& pMayaObject)
{
	//In this example, this doesn't interest us. A typical usage would be to
	//record this information to able to re-make connections and such.
}

void MayaExt_ImportEnd(FbxScene* pFbxScene)
{
	//In this example, we will try to retrieve our custom data to see if its there
	bool Result = false;

	//Make sure our custom object classes were correctly registred
	FbxClassId MyCustomDataClassId = gFbxManager->FindClass(MY_CUSTOM_CLASS_NAME);
	if( !MyCustomDataClassId.IsValid() ) return;
    
	//Search through the FBX scene to find our custom objects
	int Iter, Count = pFbxScene->GetSrcObjectCount<MyCustomObject>();
	for( Iter = 0; Iter < Count; ++Iter )
	{
		MyCustomObject* MyCustomObjectInstance = pFbxScene->GetSrcObject<MyCustomObject>();
		if( MyCustomObjectInstance )
		{
			if( MyCustomObjectInstance->FindProperty("CustomData1").IsValid() )
			{
				//...we successfully retrieved our custom data, yay! :)
				//Here we could loop to find all the properties that we wrote, its limitless...
				//We can also alterate the Maya scene, its all available!
				Result = true;
			}
		}
	}
}


