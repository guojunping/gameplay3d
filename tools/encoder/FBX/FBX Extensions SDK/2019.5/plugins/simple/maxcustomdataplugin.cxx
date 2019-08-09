/****************************************************************************************

   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.

****************************************************************************************/

#include "maxextension.h"

#define MY_CUSTOM_CLASS_NAME	"MyCustomClass"
#define MY_CUSTOM_CLASS_TYPE	"MyCustomType"
#define MY_CUSTOM_CLASS_SUBTYPE	"MyCustomSubType"

static FbxManager* gFbxManager = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Custom Object declaration
class MyCustomObject : public FbxObject
{
	FBXSDK_OBJECT_DECLARE(MyCustomObject, FbxObject);
};

FBXSDK_OBJECT_IMPLEMENT(MyCustomObject);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Plugin declaration and initialization methods
class MaxCustomDataPlugin : public FbxPlugin
{
	FBXSDK_PLUGIN_DECLARE(MaxCustomDataPlugin);

protected:
	explicit MaxCustomDataPlugin(const FbxPluginDef& pDefinition, FbxModule pFbxModule) : FbxPlugin(pDefinition, pFbxModule)
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

FBXSDK_PLUGIN_IMPLEMENT(MaxCustomDataPlugin);

// FBX Interface
extern "C"
{
    //The DLL is owner of the plug-in
    static MaxCustomDataPlugin* sPlugin = NULL;

    //This function will be called when an application will request the plug-in
    FBXSDK_DLLEXPORT void FBXPluginRegistration(FbxPluginContainer& pContainer, FbxModule pFbxModule)
    {
        if( sPlugin == NULL )
        {
            //Create the plug-in definition which contains the information about the plug-in
            FbxPluginDef sPluginDef;
            sPluginDef.mName = "MaxExtensionSimplePlugin";
            sPluginDef.mVersion = "1.0";

            //Create an instance of the plug-in.  The DLL has the ownership of the plug-in
            sPlugin = MaxCustomDataPlugin::Create(sPluginDef, pFbxModule);

            //Register the plug-in
            pContainer.Register(*sPlugin);
        }
    }

	FBXSDK_MAX_EXTENSION_DECLARE();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Max Extension Export Functions
bool MaxExt_ExportHandled(INode* pMaxObject)
{
	//Called for each Max object that *is going to* be translated into fbx common data.
	//Sometimes, for different reasons, this might be called more than once for the same object.
	//
	//The purpose of this function is to tell the plugin if we are going to handle the creation
	//of this Max object in this extension plugin or if we let the plugin work as usual.
	//
	//So returning TRUE means the corresponding fbx object for this particuliar Max object won't
	//be created, and we'll have to create it ourselves, so we say "we handle it". Otherwise,
	//returning FALSE will simply result in the usual behavior, which means the plugin will
	//translate the Max object into the corresponding fbx object, if it knows how-to, of course.
	return false;
}

void MaxExt_ExportBegin(FbxScene* pFbxScene, INode* pMaxRootNode)
{
	//Called before we export anything into the fbx scene.
	//
	//We could choose to create our custom data now or wait at the end of the export
	//depending if that custom data replaces objects in the fbx scene hierarchy.
}

void MaxExt_ExportTranslated(FbxObject* pFbxObject, INode* pMaxObject)
{
	//Called after the Max scene has been translated into an fbx scene, for each
	//object that was convertable. In other words, objects unknown or unsupported
	//by the FbxSdk won't be called in this function.
	//
	//The purpose of this function is to tell the extension plugin what is the corresponding
	//fbx object for that particuliar Max object. This is especially useful when connections
	//need to be preserved between plugin and extension plugin fbx objects.
}

bool MaxExt_ExportProcess(FbxObject** pOutputFbxObject, ReferenceTarget* pInputObject, FbxScene* pFbxScene)
{
    //In this example, we want to perform a post-export process so we notify the caller that
    //we do not whish to intervene at this moment.
    return false;
}

void RecursiveIterator(FbxScene* pFbxScene, INode* pMaxNode)
{
	if( pMaxNode )
	{
		Object* lMaxGeom = pMaxNode->EvalWorldState(0).obj;
		if( lMaxGeom && lMaxGeom->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0)) )
		{
			//We found a mesh, lets store custom data for each vertex :)
			//Obviously, for the simplicity of this example, we generate random data ourselves, but in
			//the real world, one could get data from the Max scene or from some other module/plugin.
			TriObject* lTri = (TriObject*)lMaxGeom->ConvertToType(0, Class_ID(TRIOBJ_CLASS_ID, 0));
			int VertexCount = lTri->mesh.getNumVerts();

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

		for( int i = 0, c = pMaxNode->NumberOfChildren(); i < c; ++i )
		{
			RecursiveIterator(pFbxScene, pMaxNode->GetChildNode(i));
		}
	}
}

void MaxExt_ExportEnd(FbxScene* pFbxScene, INode* pMaxRootNode)
{
	//Called after the scene has been fully translated into fbx. This is the last function called
	//before the extension plugin is done with the export process. Any clean-up or last minute
	//modification to the scene must be done now.
	if( !pMaxRootNode ) return;

	RecursiveIterator(pFbxScene, pMaxRootNode);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Max Extension Import Functions
bool MaxExt_ImportHandled(FbxObject* pFbxObject)
{
	//This is called by the importer everytime its about to translate the FBX object
	//into a Max object. If the extension plugin should handle the creation, then
	//it needs to answer TRUE to this call.
	return false;
}

void MaxExt_ImportBegin(FbxScene* pFbxScene, INode* pMaxRootNode)
{
	//This is called the the beginning of the import process,
	//allowing us to act on the FBX scene before its loaded from the file.
}

bool MaxExt_ImportProcess(ReferenceTarget** pOutputObject, FbxObject* pInputFbxObject, bool pIsAnInstance, bool pMerge)
{
    //In this example, we want to perform a post-import process so we notify the caller that
    //we do not wish to intervene at this moment.
    return false;
}

void MaxExt_ImportTranslated(FbxObject* pFbxObject, INode* pMaxObject)
{
	//This is called everytime an FBX object got converted into a Max object
	//while we are traversing the scene during the translation process.
}

void MaxExt_ImportEnd(FbxScene* pFbxScene, INode* pMaxRootNode)
{
	//In this example, we will try to retrieve our custom data to see if its there
	bool Result = false;

	//Make sure our custom object class was correctly registred
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
				//We can also alterate the Max scene, its all available!
				Result = true;
			}
		}
	}
}


