/****************************************************************************************

   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.

****************************************************************************************/

#include "mayaextension.h"

#include <maya/MDagPath.h>
#include <maya/MFnPhongShader.h>
#include <maya/MColor.h>

static FbxManager* gFbxManager = NULL;
static bool        gProcessedOnce = false;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Custom Material declaration
#define MY_CUSTOM_SURFACE_CLASS_NAME     "MyCustomSurfaceClass"
#define MY_CUSTOM_SURFACE_CLASS_SUBTYPE	 "MyCustomSurfaceClass" // need to match the SURFACE_CLASS_NAME so the class can be recognized
                                                                // by the FBX reader.

// Define a custom material object. For the purpose of this sample, this class will simply contains a Diffuse color property.
class MyCustomSurface : public FbxSurfaceMaterial
{
    FBXSDK_OBJECT_DECLARE(MyCustomSurface, FbxSurfaceMaterial);

protected:
    virtual void ConstructProperties(bool pForceSet)
    {
	    ParentClass::ConstructProperties(pForceSet);
        Diffuse.StaticInit(this, "Diffuse", FbxColor3DT, FbxDouble3(0,0,0), pForceSet);
    }

public:
    FbxPropertyT<FbxDouble3> Diffuse;
};

FBXSDK_OBJECT_IMPLEMENT(MyCustomSurface);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Plugin declaration and initialization methods
class MayaCustomSurfacePlugin : public FbxPlugin
{
	FBXSDK_PLUGIN_DECLARE(MayaCustomSurfacePlugin);

protected:
	explicit MayaCustomSurfacePlugin(const FbxPluginDef& pDefinition, FbxModule pFbxModule) : FbxPlugin(pDefinition, pFbxModule)
	{
	}

	// Implement kfbxmodules::FbxPlugin
	virtual bool SpecificInitialize()
	{
		gFbxManager = GetData().mSDKManager;

        // Register MyCustomSurfacer class with the plug-in's manager
        gFbxManager->RegisterFbxClass(MY_CUSTOM_SURFACE_CLASS_NAME,	FBX_TYPE(MyCustomSurface),FBX_TYPE(FbxSurfaceMaterial), FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MATERIAL, MY_CUSTOM_SURFACE_CLASS_SUBTYPE);
		return true;
	}

	virtual bool SpecificTerminate()
	{
		// Unregister MyCustomSurface class with the plug-in's manager
		gFbxManager->UnregisterFbxClass(FBX_TYPE(MyCustomSurface));

		return true;
	}
};

FBXSDK_PLUGIN_IMPLEMENT(MayaCustomSurfacePlugin);

// FBX Interface
extern "C"
{
    // The DLL is owner of the plug-in
    static MayaCustomSurfacePlugin* sPlugin = NULL;

    // This function will be called when an application will request the plug-in
    FBXSDK_DLLEXPORT void FBXPluginRegistration(FbxPluginContainer& pContainer, FbxModule pFbxModule)
    {
        if( sPlugin == NULL )
        {
            // Create the plug-in definition which contains the information about the plug-in
            FbxPluginDef sPluginDef;
            sPluginDef.mName = "MayaCustomSurfacePlugin";
            sPluginDef.mVersion = "1.0";

            // Create an instance of the plug-in.  The DLL has the ownership of the plug-in
            sPlugin = MayaCustomSurfacePlugin::Create(sPluginDef, pFbxModule);

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
    // In this example, we replace the Maya phong shader with our custom material.
    // Because this function is called several times during the export, we need to figure out what 
    // we are processing and take action only on the objects of interest.

    // validate the inputs
    if (pOutputFbxObject == NULL || pInputObject.isNull() || pFbxScene == NULL)
        return false;

    // If *pOutputFbxObject is NULL, the caller expect us to create an object and return it back.
    // In this case, we replace all the Phong materials we receive with our own with a white color.
    if (pInputObject.hasFn(MFn::kPhong))
    {
        // Make sure our custom object class was correctly registered
		FbxClassId MyCustomSurfaceDataClassId = gFbxManager->FindClass(MY_CUSTOM_SURFACE_CLASS_NAME);

        // if not, we notify the caller that we cannot handle the received object
		if( !MyCustomSurfaceDataClassId.IsValid() ) return false;

		// Create the custom object to hold the custom data
		MyCustomSurface* MyCustomSurfaceInstance = MyCustomSurface::Create(pFbxScene, "My Custom Surface Instance");
        MyCustomSurfaceInstance->Diffuse.Set(FbxDouble3(1, 1, 1));

        *pOutputFbxObject = FbxCast<FbxObject>(MyCustomSurfaceInstance);

        // It is important that the created object gets its proper name otherwise the Maya plug-in may be fooled
        // and could not understand that a given material already exists.
        MString lShaderName;
        MStatus lStatus;

        MFnDependencyNode lDependencyInterface (pInputObject, &lStatus);
        lShaderName = lDependencyInterface.name (&lStatus);
        (*pOutputFbxObject)->SetName(lShaderName.asUTF8());

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
    // Validate the input
    if (pInputFbxObject == NULL)
        return false;

    if (pIsAnInstance)
    {
        // we have been called with an instance...actually this block simply shows how we would check and
        // handle the case of an instance. But this case can never happen with the current Maya plug-in and
        // the material objects (pIsAnInstance is always false)
        if (gProcessedOnce)
            return true;
        else
            gProcessedOnce = true;
    }

    // check that we have received an object that derives from the FbxSurfaceMaterial class.
    FbxSurfaceMaterial* lSurfaceMat = FbxCast<FbxSurfaceMaterial>(pInputFbxObject);
    if (lSurfaceMat == NULL)
        // we did not: return immediately and let the caller know that we did not process the input object
        return false;

    // Now that we validated that the input object is some sort of SurfaceMaterial, we make sure that 
    // it is our own custom material.
    if (!lSurfaceMat->Is<MyCustomSurface>())
        // nope! again, we return immediately
        return false;

    // All right! We have our data object...
    MyCustomSurface* lCustSurface = (MyCustomSurface*)lSurfaceMat;

    // get the only attribute of our custom surface...
    FbxDouble3 lFbxColor = lCustSurface->Diffuse.Get();
    MColor lColor ((float)lFbxColor[0], (float)lFbxColor[1], (float)lFbxColor[2]);

    // Let's create a PhongShader and give it the Diffuse color of our custom object.
    MStatus        lStatus;
    MFnPhongShader lTempMaterial;

    if (pMerge)
    {
        // we are importing in a merge-back operation. 
        if (pOutputObject.isNull())
        {
            // there is a problem! pOutputObject must be non Null
            return false;
        }

        // let's see if we have a Phong shader interface
        MFnPhongShader lPhong(pOutputObject, &lStatus);
        if (lStatus == MStatus::kSuccess)
        {
            // Yep! Let's set the color
            lPhong.setColor(lColor);
        }
    }
    else
    {
        // too bad if pOutputObject was pointing to something valid... we are creating a new phong 
        // anyway!
        pOutputObject = lTempMaterial.create (true, &lStatus);
        lTempMaterial.setColor (lColor);
    }
    return (lStatus == MStatus::kSuccess);
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


