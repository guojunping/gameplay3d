/****************************************************************************************

   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.

****************************************************************************************/

#include "mayaextension.h"
#include <maya/MFnTransform.h>

static FbxManager* gFbxManager = NULL;
static bool        gProcessedOnce = false;
static int         gUniqueNameSuffix = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Plugin declaration and initialization methods
class MayaCustomTransformOverwritePlugin : public FbxPlugin
{
	FBXSDK_PLUGIN_DECLARE(MayaCustomTransformOverwritePlugin);

protected:
	explicit MayaCustomTransformOverwritePlugin(const FbxPluginDef& pDefinition, FbxModule pFbxModule) : FbxPlugin(pDefinition, pFbxModule)
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

FBXSDK_PLUGIN_IMPLEMENT(MayaCustomTransformOverwritePlugin);

// FBX Interface
extern "C"
{
    // The DLL is owner of the plug-in
    static MayaCustomTransformOverwritePlugin* sPlugin = NULL;

    // This function will be called when an application will request the plug-in
    FBXSDK_DLLEXPORT void FBXPluginRegistration(FbxPluginContainer& pContainer, FbxModule pFbxModule)
    {
        if( sPlugin == NULL )
        {
            // Create the plug-in definition which contains the information about the plug-in
            FbxPluginDef sPluginDef;
            sPluginDef.mName = "MayaCustomXFormPlugin";
            sPluginDef.mVersion = "1.0";

            // Create an instance of the plug-in.  The DLL has the ownership of the plug-in
            sPlugin = MayaCustomTransformOverwritePlugin::Create(sPluginDef, pFbxModule);

            // Register the plug-in
            pContainer.Register(*sPlugin);
        }
    }

	FBXSDK_MAYA_EXTENSION_DECLARE();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Maya Extension Export Callbacks
bool MayaExt_ExportHandled(MObject& pMayaObject)
{
	// In this example, we aren't replacing any objects in the fbx scene, so we don't have to
	// prevent the Maya plugin from translating Maya objects into fbx objects.
	return false;
}

void MayaExt_ExportBegin(FbxScene* pFbxScene)
{
	// In this example, we will create our custom data during the export process.
    // So there is nothing to do here.
}

void MayaExt_ExportTranslated(FbxObject* pFbxObject, MObject& pMayaObject)
{
	// Since we aren't replacing objects in the scene hierarchy in this example, we don't have
	// anything to do with this information.
}

bool MayaExt_ExportProcess(FbxObject** pOutputFbxObject, MObject& pInputObject, FbxScene* pFbxScene)
{
    // In this example, we replace the current FbxNodeAttribute using a FbxMarker with the sphere look.
    // Because this function is called several times during the export, we need to figure out what 
    // we are processing and take action only on the objects of interest.

    // validate the inputs
    if (pOutputFbxObject == NULL || pInputObject.isNull() || pFbxScene == NULL)
        return false;

    if (pInputObject.hasFn(MFn::kTransform))
    {
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

    // let the caller know that we did not handle the received 'pInputObject' object
    return false;
}

void MayaExt_ExportEnd(FbxScene* pFbxScene)
{
	//Since we process all our data in the MayaExt_ExportProcess callback, we have nothing to do here.
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Maya Extension Import Callbacks
bool MayaExt_ImportHandled(FbxObject* pFbxObject)
{
	// In this example, we want to intercept the processing pass only. If we return true here, we tell the 
    // caller that we are responsible for creating the geometry associated with pFbxObject which we don't.
    // So we return false and let the caller process the geometry as usual.
	return false;
}

void MayaExt_ImportBegin(FbxScene* pFbxScene)
{
	// In this example we have nothing to do before we translate the FBX scene into
    // Maya data.
}

bool MayaExt_ImportProcess(MObject& pOutputObject, FbxObject* pInputFbxObject, bool pIsAnInstance, bool pMerge)
{
    // Here we want to process only FbxMarker objects. We will create a transform with the "Display Handle" and
    // "Display Local Axis" enabled. We ignore any existing hierarchy.

    if (pInputFbxObject == NULL && !pOutputObject.isNull())
        // invalid data
        return false;

    FbxNode* lNode = FbxCast<FbxNode>(pInputFbxObject);
    if (lNode == NULL || lNode->GetNodeAttribute()==NULL || FbxCast<FbxMarker>(lNode->GetNodeAttribute())==NULL)
        // not the expected object
        return false;
    
    MStatus      lStatus;
    MFnTransform lTransform;
    MObject      lTempObj;

    lTempObj = lTransform.create (MObject::kNullObj, &lStatus);
    if (!lStatus)
    {
        lStatus.perror ("Cannot create Transform");
        return false;
    }
    
    FbxString lString = lNode->GetNameWithoutNameSpacePrefix();
    lTransform.setName (lString.Buffer());

    // some Mel
    MString lMayaCommand;

    lMayaCommand = "setAttr "+lTransform.name()+".displayHandle 1";
    MGlobal::executeCommand( lMayaCommand, lStatus );

    lMayaCommand = "setAttr "+lTransform.name()+".displayLocalAxis 1";
    MGlobal::executeCommand( lMayaCommand, lStatus );

    // finally, we return the object!
    pOutputObject = lTempObj;
    return true;
}

void MayaExt_ImportTranslated(FbxObject* pFbxObject, MObject& pMayaObject)
{
	// In this example, this doesn't interest us. A typical usage would be to
	// record this information to able to re-make connections and such.
}

void MayaExt_ImportEnd(FbxScene* pFbxScene)
{
    // In this example, this doesn't interest us either. Every thing has been processed in
    // the MayaExt_ImportProcess callback.
}


