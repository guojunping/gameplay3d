/****************************************************************************************

   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.

****************************************************************************************/

#include "maxextension.h"

static FbxManager* gFbxManager = NULL;
static bool        gProcessedOnce = false;
static int         gUniqueNameSuffix = 0;


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Plugin declaration and initialization methods
class MaxCustomTransformOverwritePlugin : public FbxPlugin
{
	FBXSDK_PLUGIN_DECLARE(MaxCustomTransformOverwritePlugin);

protected:
	explicit MaxCustomTransformOverwritePlugin(const FbxPluginDef& pDefinition, FbxModule pFbxModule) : FbxPlugin(pDefinition, pFbxModule)
	{
	}

	// Implement kfbxmodules::FbxPlugin
	virtual bool SpecificInitialize()
	{
		gFbxManager = GetData().mSDKManager;
		return true;
	}

	virtual bool SpecificTerminate()
	{
		return true;
	}
};

FBXSDK_PLUGIN_IMPLEMENT(MaxCustomTransformOverwritePlugin);

// FBX Interface
extern "C"
{
    // The DLL is owner of the plug-in
    static MaxCustomTransformOverwritePlugin* sPlugin = NULL;

    // This function will be called when an application will request the plug-in
    FBXSDK_DLLEXPORT void FBXPluginRegistration(FbxPluginContainer& pContainer, FbxModule pFbxModule)
    {
        if( sPlugin == NULL )
        {
            // Create the plug-in definition which contains the information about the plug-in
            FbxPluginDef sPluginDef;
            sPluginDef.mName = "MaxCustomXFormPlugin";
            sPluginDef.mVersion = "1.0";

            // Create an instance of the plug-in.  The DLL has the ownership of the plug-in
            sPlugin = MaxCustomTransformOverwritePlugin::Create(sPluginDef, pFbxModule);

            // Register the plug-in
            pContainer.Register(*sPlugin);
        }
    }

	FBXSDK_MAX_EXTENSION_DECLARE();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Max Extension Export Callbacks
bool MaxExt_ExportHandled(INode* pMaxObject)
{
	// In this example, we aren't replacing any objects in the fbx scene, so we don't have to
	// prevent the Max plugin from translating Max objects into fbx objects.
	return false;
}

void MaxExt_ExportBegin(FbxScene* pFbxScene, INode* pMaxRootNode)
{
	// In this example, we will create our custom data during the export process.
    // So there is nothing to do here.
}

bool MaxExt_ExportProcess(FbxObject** pOutputFbxObject, ReferenceTarget* pInputObject, FbxScene* pFbxScene)
{
    // In this example, we replace the current FbxNodeAttribute using a FbxMarker with the sphere look.
    // Because this function is called several times during the export, we need to figure out what 
    // we are processing and take action only on the objects of interest.

    // validate the inputs. 
    // pOutputFbxObject == NULL && pFbxScene == NULL -> we have been called for a "can we support the conversion
    // of pInputObject ? 
    if (pInputObject == NULL || (pOutputFbxObject != NULL && pFbxScene == NULL))
        return false;

    // Test the conditions that will allow us to validate that we can convert the incoming object
    if (pInputObject->SuperClassID() != HELPER_CLASS_ID)
        return false;

    // Did we get called only to answer if we support the conversion or not?
    if (pOutputFbxObject == NULL && pFbxScene == NULL)
        return true;

    if (*pOutputFbxObject == NULL)
        // we did not receive a valid object for our purpose
        return false;

    FbxNode* lNode = FbxCast<FbxNode>(*pOutputFbxObject);
    if (lNode == NULL)
        // again, wrong object!
        return false;

    // make sure we name the node attribute with a unique name.
    FbxString name = FbxString("MyNodeAttribute") + FbxString(gUniqueNameSuffix++);
    FbxMarker* lMarker = FbxMarker::Create(pFbxScene, name.Buffer());
    lMarker->Look.Set(FbxMarker::eSphere);

    FbxNodeAttribute* lNA = lNode->SetNodeAttribute(lMarker);
    if (lNA)
    {
        // since we are replacing the node attribute, we destroy what is already there...
        lNA->Destroy();
    }

    // let the caller know that we handled the received 'pInputObject' object
    return true;
}

void MaxExt_ExportTranslated(FbxObject* pFbxObject, INode* pMaxObject)
{
	// Since we aren't replacing objects in the scene hierarchy in this example, we don't have
	// anything to do with this information.
}

void MaxExt_ExportEnd(FbxScene* pFbxScene, INode* pMaxRootNode)
{
	//Since we process all our data in the MaxExt_ExportProcess callback, we have nothing to do here.
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Max Extension Import Callbacks
bool MaxExt_ImportHandled(FbxObject* pFbxObject)
{
	// In this example, we want to intercept the processing pass only.
    // So we return false and let the caller process the hierarchy as usual.
	return false;
}

void MaxExt_ImportBegin(FbxScene* pFbxScene, INode* pMaxRootNode)
{
	// In this example we have nothing to do before we translate the FBX scene into
    // Max data.
}

bool MaxExt_ImportProcess(ReferenceTarget** pOutputObject, FbxObject* pInputFbxObject, bool pIsAnInstance, bool pMerge)
{
    // Here we want to process only FbxMarker objects. We will create torus object.
    // We ignore any existing hierarchy.

    // Validate the input
    if (pInputFbxObject == NULL)
        return false;

    FbxNode* lNode = FbxCast<FbxNode>(pInputFbxObject);
    if (lNode == NULL || lNode->GetNodeAttribute()==NULL || FbxCast<FbxMarker>(lNode->GetNodeAttribute())==NULL)
        // not the expected object
        return false;
    
    // Did we get called only to answer if we support the conversion or not?
    if (pMerge == false && pIsAnInstance == false && pOutputObject == NULL)
    {
        // Yes we did. And since we already validated all the conditions we can answer
        return true;
    }

    // We have everything to successfully create a torus object...
    Object* obj = static_cast<Object*>(GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, Class_ID(TORUS_CLASS_ID,0))); 
    if (obj == NULL)
        return false;

    // finally, we return the object!
    *pOutputObject = (ReferenceTarget*) obj;
    return true;
}

void MaxExt_ImportTranslated(FbxObject* pFbxObject, INode* pMaxObject)
{
	// In this example, this doesn't interest us. A typical usage would be to
	// record this information to able to re-make connections and such.
}

void MaxExt_ImportEnd(FbxScene* pFbxScene, INode* pMaxRootNode)
{
    // In this example, this doesn't interest us either. Every thing has been processed in
    // the MaxExt_ImportProcess callback.
}


