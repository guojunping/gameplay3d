/****************************************************************************************

   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.

****************************************************************************************/

#include "maxextension.h"
#include <stdmat.h>

static FbxManager* gFbxManager = NULL;
static bool        gProcessedOnce = false;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Custom Material declaration
#define MY_CUSTOM_SURFACE_CLASS_NAME     "MyCustomSurfaceClass1"
#define MY_CUSTOM_SURFACE_CLASS_SUBTYPE	 "MyCustomSurfaceClass1" // need to match the SURFACE_CLASS_NAME so the class can be recognized
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
class MaxCustomSurfacePlugin : public FbxPlugin
{
	FBXSDK_PLUGIN_DECLARE(MaxCustomSurfacePlugin);

protected:
	explicit MaxCustomSurfacePlugin(const FbxPluginDef& pDefinition, FbxModule pFbxModule) : FbxPlugin(pDefinition, pFbxModule)
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

FBXSDK_PLUGIN_IMPLEMENT(MaxCustomSurfacePlugin);

// FBX Interface
extern "C"
{
    // The DLL is owner of the plug-in
    static MaxCustomSurfacePlugin* sPlugin = NULL;

    // This function will be called when an application will request the plug-in
    FBXSDK_DLLEXPORT void FBXPluginRegistration(FbxPluginContainer& pContainer, FbxModule pFbxModule)
    {
        if( sPlugin == NULL )
        {
            // Create the plug-in definition which contains the information about the plug-in
            FbxPluginDef sPluginDef;
            sPluginDef.mName = "MaxCustomSurfacePlugin";
            sPluginDef.mVersion = "1.0";

            // Create an instance of the plug-in.  The DLL has the ownership of the plug-in
            sPlugin = MaxCustomSurfacePlugin::Create(sPluginDef, pFbxModule);

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
	// prevent the Maya plugin from translating Maya objects into fbx objects.
	return false;
}

void MaxExt_ExportBegin(FbxScene* pFbxScene, INode* pMaxRootNode)
{
	// In this example, we will create our custom data during the export process.
    // So there is nothing to do here.
}

void MaxExt_ExportTranslated(FbxObject* pFbxObject, INode* pMaxObject)
{
	// Since we aren't replacing objects in the scene hierarchy in this example, we don't have
	// anything to do with this information.
}

bool MaxExt_ExportProcess(FbxObject** pOutputFbxObject, ReferenceTarget* pInputObject, FbxScene* pFbxScene)
{
    // In this example, we replace the 3dsMax phong material with our custom material.
    // Because this function is called several times during the export, we need to figure out what 
    // we are processing and take action only on the objects of interest.

    // validate the inputs
    // pOutputFbxObject == NULL && pFbxScene == NULL -> we have been called for a "can we support the conversion
    // of pInputObject ? 
    if (pInputObject == NULL || (pOutputFbxObject != NULL && pFbxScene == NULL))
        return false;

    // Test the conditions that will allow us to validate that we can convert the incoming object
    StdMat* lMtl = NULL;
    if (pInputObject->SuperClassID() != MATERIAL_CLASS_ID)
        return false;

    lMtl = static_cast<StdMat*>(pInputObject);
    if (lMtl == NULL)
        return false;

    // we only want to process simple phong materials
    if ( (lMtl->GetShading() != SHADE_PHONG) || lMtl->IsMultiMtl() )
        return false;

    // Make sure our custom object class was correctly registered
	FbxClassId MyCustomSurfaceDataClassId = gFbxManager->FindClass(MY_CUSTOM_SURFACE_CLASS_NAME);

    // if not, we notify the caller that we cannot handle the received object
	if( !MyCustomSurfaceDataClassId.IsValid() ) return false;

    // Did we get called only to answer if we support the conversion or not?
    if (pOutputFbxObject == NULL && pFbxScene == NULL)
    {
        // Yes we did. And since we already validated all the conditions we can answer
        return true;
    }

    // If *pOutputFbxObject is NULL, the caller expect us to create an object and return it back.
    // In this case, we replace all the Phong materials we receive with our own with a white color.
    
    // Create the custom object to hold the custom data
	MyCustomSurface* MyCustomSurfaceInstance = MyCustomSurface::Create(pFbxScene, "My Custom Surface Instance");
    MyCustomSurfaceInstance->Diffuse.Set(FbxDouble3(1, 1, 1));

    *pOutputFbxObject = FbxCast<FbxObject>(MyCustomSurfaceInstance);

    // It is important that the created object gets its proper name otherwise the 3dsMax plug-in may be fooled
    // and could not understand that a given material already exists.
    char* _s;
    MSTR name = lMtl->GetName();
#if defined(_UNICODE)
    FbxWCToUTF8(name, _s);
#else
	FbxAnsiToUTF8(name, _s);
#endif
    FbxAutoFreePtr<char> lGuard(_s);

    (*pOutputFbxObject)->SetName(_s);

    // let the caller know that we handled the received 'pInputObject' object
    return true;
}

void MaxExt_ExportEnd(FbxScene* pFbxScene, INode* pMaxRootNode)
{
	//Since we process all our data in the MaxExt_ExportProcess callback, we have nothing to do here.
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Max Extension Import Callbacks
bool MaxExt_ImportHandled(FbxObject* pFbxObject)
{
	// In this example, we want to intercept the processing pass only. If we return true here, we tell the 
    // caller that we are responsible for creating the geometry associated with pFbxObject which we don't.
    // So we return false and let the caller process the geometry as usual.
	return false;
}

void MaxExt_ImportBegin(FbxScene* pFbxScene, INode* pMaxRootNode)
{
	// In this example we have nothing to do before we translate the FBX scene into
    // 3dsMax data.
}

bool MaxExt_ImportProcess(ReferenceTarget** pOutputObject, FbxObject* pInputFbxObject, bool pIsAnInstance, bool pMerge)
{
    // Validate the input
    if (pInputFbxObject == NULL)
        return false;

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

    // Did we get called only to answer if we support the conversion or not?
    if (pMerge == false && pIsAnInstance == false && pOutputObject == NULL)
    {
        // Yes we did. And since we already validated all the conditions we can answer
        return true;
    }

    if (pIsAnInstance)
    {
        // we have been called with an instance...actually this block simply shows how we would check and
        // handle the case of an instance. But this case can never happen with the current 3dsMax plug-in and
        // the material objects (pIsAnInstance is always false)
        if (gProcessedOnce)
            return true;
        else
            gProcessedOnce = true;
    }

    // All right! We have our data object...
    MyCustomSurface* lCustSurface = (MyCustomSurface*)lSurfaceMat;

    // get the only attribute of our custom surface...
    FbxDouble3 lFbxColor = lCustSurface->Diffuse.Get();
    Color lColor;
    lColor.r = (float)lFbxColor[0];
    lColor.g = (float)lFbxColor[1];
    lColor.b = (float)lFbxColor[2];

    // and its name
    FbxString lString = lCustSurface->GetNameWithoutNameSpacePrefix();
    TCHAR* _s;
#if defined(_UNICODE)
    FbxUTF8ToWC(lString.Buffer(), _s);
#else
	FbxUTF8ToAnsi(lString.Buffer(), _s);
#endif
    FbxAutoFreePtr<TCHAR> lsGuard(_s);

    bool ret = false;
    StdMat* lMtl = NULL;
    if (pMerge)
    {
        // we are importing in a merge-back operation. 
        if (pOutputObject == NULL || *pOutputObject == NULL)
        {
            // there is a problem! pOutputObject must be non Null
            return false;
        }

        // let's see if we have a Phong shader interface
        if ((*pOutputObject)->SuperClassID() == MATERIAL_CLASS_ID)
        {
            lMtl = static_cast<StdMat*>((*pOutputObject));
            if (lMtl == NULL)
                return false;

            // we only want to process simple phong materials
            if ( (lMtl->GetShading() != SHADE_PHONG) || lMtl->IsMultiMtl() )
                return false;

            lMtl->SetDiffuse(lColor, 0);
            ret = true;
        }
    }
    else
    {
        // too bad if pOutputObject was pointing to something valid... we are creating a new phong 
        // anyway and give it the Diffuse color of our custom object.
        lMtl = NewDefaultStdMat();
        if (lMtl)
        {
            lMtl->SetShading(SHADE_PHONG);
            lMtl->SetName(_s); 
            lMtl->SetDiffuse(lColor, 0);

            *pOutputObject = (ReferenceTarget*)lMtl;
            ret = true;
        }
    }

    return ret;
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


