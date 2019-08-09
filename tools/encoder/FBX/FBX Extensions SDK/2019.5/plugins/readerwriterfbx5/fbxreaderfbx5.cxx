/****************************************************************************************
 
   Copyright (C) 2016 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/fileio/fbxglobalsettings.h>
#include <fbxsdk/fileio/fbx/fbxreaderfbx5.h>
#include <fbxsdk/scene/animation/fbxanimcurvefilters.h>

#include <fbxsdk/scene/animation/fbxanimutilities.h>
#include <fbxsdk/utils/fbxscenecheckutility.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

static FbxLayerElement::EMappingMode		ConvertMappingModeToken(const char* pToken);
static FbxLayerElement::EReferenceMode		ConvertReferenceModeToken(const char* pToken);
static FbxLayerElementTexture::EBlendMode	ConvertBlendModeToken(const char* pToken);
static FbxConstraint::EType					ConvertConstraintToken(const char* pToken);

//
// Local class
//
class Fbx5ObjectTypeInfo
{
	public:
		FbxString mType;
		FbxString mSubType;
		FbxString mName;
};

//
// Local utility functions
//
static int FindTypeIndex(FbxString& pTypeName, FbxArray<Fbx5ObjectTypeInfo*>& pTypeList);

static void ForceFileNameExtensionToTif(FbxString& FileName)
{
	FbxString fext = FbxPathUtils::GetExtensionName(FileName);
	fext = fext.Upper();
	if (fext != "TIF" && fext != "TIFF")
	{
		// delete the file (we have a copy in memory and we are
		// going to write a TIFF file instead.
		FbxFileUtils::Delete(FileName);

		// change FileName so it's extension becomes tif
		int ext = FileName.ReverseFind('.');
		if (ext != -1)
			FileName = FileName.Left(ext) + ".tif";
	}
}


FbxReaderFbx5::FbxReaderFbx5(FbxManager& pManager, FbxImporter& pImporter, int pID, FbxStatus& pStatus) :
    FbxReader(pManager, pID, pStatus),
    mFileObject(NULL),
    mImporter(pImporter),
    mSceneInfo(NULL),
    mAnimLayer(NULL)
{
	mNodeArrayName.SetCaseSensitive(true);
	mTargetArrayName.SetCaseSensitive(true);
	mUpNodeArrayName.SetCaseSensitive(true);
	mCameraBackgroundArrayName.SetCaseSensitive(true);
	SetIOSettings(pImporter.GetIOSettings());
}


FbxReaderFbx5::~FbxReaderFbx5()
{
    mAnimLayer = NULL;
    if (mFileObject)
    {
        FileClose();
    }

    FBX_SAFE_DESTROY(mSceneInfo);

    FbxArrayDelete(mTakeInfo);
}


bool FbxReaderFbx5::FileOpen(char* pFileName, bool pIgnoredArg)
{
	return FileOpen(pFileName);
}

bool FbxReaderFbx5::FileOpen(char* pFileName)
{
	bool lCheckCRC = false;
	bool lParse = false;

	mData->Reset();

    if (!mFileObject)
    {
        FBX_ASSERT(GetStatus());
		mFileObject = FbxIO::Create(FbxIO::BinaryNormal, GetStatus());       
        mFileObject->CacheSize(IOS_REF.GetIntProp(IMP_CACHE_SIZE, 8));
    }

    FbxString lFullName = FbxPathUtils::Bind(FbxGetCurrentWorkPath(), pFileName); 
    FbxIOFileHeaderInfo* lFHI = mImporter.GetFileHeaderInfo();
    FbxIOFileHeaderInfo lFileHeaderInfo;
    FBX_ASSERT(lFHI != NULL);
    if (lFHI == NULL) lFHI = &lFileHeaderInfo;

    if (!mFileObject->ProjectOpen(lFullName.Buffer(), this, lCheckCRC, lParse, lFHI))
    {
        return false;
    }
	else
	{
		// Get the default render resolution from the header
        if( lFHI->mDefaultRenderResolution.mResolutionW && lFHI->mDefaultRenderResolution.mResolutionH && lFHI->mDefaultRenderResolution.mResolutionMode.GetLen())
        {
            SetDefaultRenderResolution(lFHI->mDefaultRenderResolution.mCameraName.Buffer(), lFHI->mDefaultRenderResolution.mResolutionMode.Buffer(),
                lFHI->mDefaultRenderResolution.mResolutionW, lFHI->mDefaultRenderResolution.mResolutionH);
        }
	}

    return true;
}

bool FbxReaderFbx5::FileOpen(FbxFile * pFile)
{
	bool lCheckCRC = false;
	bool lParse = false;

	mData->Reset();

    if (!mFileObject)
    {
        FBX_ASSERT(GetStatus());
		mFileObject = FbxIO::Create(FbxIO::BinaryNormal, GetStatus());       
        mFileObject->CacheSize(IOS_REF.GetIntProp(IMP_CACHE_SIZE, 8));
    }

    FbxIOFileHeaderInfo lFHI;
    
    if (!mFileObject->ProjectOpen(pFile, this, lCheckCRC, lParse, &lFHI))
    {
        return false;
    }
	else
	{
		// Get the default render resolution from the header
		if( lFHI.mDefaultRenderResolution.mResolutionW && lFHI.mDefaultRenderResolution.mResolutionH && lFHI.mDefaultRenderResolution.mResolutionMode.GetLen())
		{
			SetDefaultRenderResolution(lFHI.mDefaultRenderResolution.mCameraName.Buffer(), lFHI.mDefaultRenderResolution.mResolutionMode.Buffer(),
				lFHI.mDefaultRenderResolution.mResolutionW, lFHI.mDefaultRenderResolution.mResolutionH);
		}
	}
    return true;
}

bool FbxReaderFbx5::FileOpen(FbxStream * pStream, void* pStreamData)
{
	bool lCheckCRC = false;
	bool lParse = false;

	mData->Reset();

    if (!mFileObject)
    {
        FBX_ASSERT(GetStatus());
		mFileObject = FbxIO::Create(FbxIO::BinaryNormal, GetStatus());       
        mFileObject->CacheSize(IOS_REF.GetIntProp(IMP_CACHE_SIZE, 8));
    }

    FbxIOFileHeaderInfo lFHI;
    
    if (!mFileObject->ProjectOpen(pStream, pStreamData, this, lCheckCRC, lParse, &lFHI))
    {
        return false;
    }
	else
	{
		// Get the default render resolution from the header
		if( lFHI.mDefaultRenderResolution.mResolutionW && lFHI.mDefaultRenderResolution.mResolutionH && lFHI.mDefaultRenderResolution.mResolutionMode.GetLen())
		{
			SetDefaultRenderResolution(lFHI.mDefaultRenderResolution.mCameraName.Buffer(), lFHI.mDefaultRenderResolution.mResolutionMode.Buffer(),
				lFHI.mDefaultRenderResolution.mResolutionW, lFHI.mDefaultRenderResolution.mResolutionH);
		}
	}
    return true;
}

bool FbxReaderFbx5::FileClose()
{
    if (!mFileObject)
    {
        GetStatus().SetCode(FbxStatus::eFailure);
        return false;
    }

    if (!mFileObject->ProjectClose())
    {
        return false;
    }
    else
    {
        FBX_SAFE_DELETE(mFileObject);
        return true;    
    }       
}

    
bool FbxReaderFbx5::IsFileOpen()
{
    return mFileObject != NULL;
}

void FbxReaderFbx5::SetEmbeddingExtractionFolder(const char* pExtractFolder)
{
	mFileObject->SetEmbeddingExtractionFolder(pExtractFolder);
}

FbxReaderFbx5::EImportMode FbxReaderFbx5::GetImportMode()
{
    FBX_ASSERT(mFileObject);
    
	if (mFileObject->IsEncrypted())
	{
		return eENCRYPTED;
	}
    else if (mFileObject->IsBinary())
    {
        return eBINARY;
    }
    else
    {
        return eASCII;
    }
}

void FbxReaderFbx5::GetVersion(int& pMajor, int& pMinor, int& pRevision)
{
    FBX_ASSERT(mFileObject);

	FbxIO::ProjectConvertVersionNumber(mFileObject->ProjectGetSectionVersion(FBX_MAIN_SECTION), 
									  pMajor, 
									  pMinor, 
									  pRevision);

}

bool FbxReaderFbx5::GetReadOptions(bool pParseFileAsNeeded)
{
    return GetReadOptions(NULL, pParseFileAsNeeded);
}

bool FbxReaderFbx5::Read(FbxDocument* pDocument)
{
	if (!pDocument)
	{
        GetStatus().SetCode(FbxStatus::eFailure, "Invalid document handle");
        return false;
	}

	FbxScene* lScene = FbxCast<FbxScene>(pDocument);

	if (!lScene)
	{
        GetStatus().SetCode(FbxStatus::eFailure, "Document not supported");
        return false;
	}

	// note: sprintf() use the locale to find the decimal separator
	// French, Italian, German, ... use the comma as decimal separator
	// so we need a way to be un-localized into writing/reading our files formats

	// force usage of a period as decimal separator
	char lPrevious_Locale_LCNUMERIC[100]; memset(lPrevious_Locale_LCNUMERIC, 0, 100);
	FBXSDK_strcpy(lPrevious_Locale_LCNUMERIC, 100, setlocale(LC_NUMERIC, 0  ));	// query current setting for LC_NUMERIC
	char *lCurrent_Locale_LCNUMERIC  = setlocale(LC_NUMERIC, "C");  // set locale using period as decimal separator

    bool result = Read(*lScene, NULL);

	// set numeric locale back
	setlocale(LC_NUMERIC, lPrevious_Locale_LCNUMERIC);

	return result;
}


bool FbxReaderFbx5::GetReadOptions(FbxIO* pFbx, bool pParseFileAsNeeded)
{
    FbxIO* lInternalFbx = NULL;
    bool lResult = true;

	if (pFbx)
	{
		lInternalFbx = mFileObject;
		mFileObject = pFbx;
	}
	else if (mFileObject)
	{
	}
	else
	{   
        GetStatus().SetCode(FbxStatus::eFailure, "File not opened");
		lResult = false;
	}

	if (lResult)
    {
		if (mFileObject->ProjectGetCurrentSection() == FBX_NO_SECTION)
		{
			int  lDummy;
			bool lFound = ReadOptionsInExtensionSection(lDummy);

			if (!lFound)
			{
				if (pParseFileAsNeeded)
				{
					lResult = mFileObject->ProjectOpenMainSection();

					if (lResult)
					{
						SetIsBeforeVersion6WithMainSection(false);

						ReadOptionsInMainSection();
						mFileObject->ProjectCloseSection();
						WriteOptionsInExtensionSection();
					}
				}
				else
				{
					lResult = false;
				}
			}
			else
			{
				SetIsBeforeVersion6WithMainSection(true);
			}
		}
		else
		{
			if (pParseFileAsNeeded)
			{
				ReadOptionsInMainSection();
			}
			else
			{
				lResult = false;
			}
		}
    }

    if (pFbx)
    {
        mFileObject = lInternalFbx;
    }   

	return lResult;
}


bool FbxReaderFbx5::Read(FbxScene& pScene, FbxIO* pFbx)
{
    FbxIO*			lInternalFbx = NULL;
    bool			lResult = true;
	bool			lDontResetPosition = false;


	mObjectMap.Clear();

	if (pFbx)
	{
		lInternalFbx = mFileObject;
		mFileObject = pFbx;

		lDontResetPosition = true;			
	}
	else if (mFileObject)
	{
	}
	else
	{   
        GetStatus().SetCode(FbxStatus::eFailure, "File not opened");
		lResult = false;
	}

	if (lResult)
	{   
		if (mFileObject->ProjectGetCurrentSection() == FBX_NO_SECTION)
		{
			if (!mFileObject->ProjectOpenMainSection())
			{
                GetStatus().SetCode(FbxStatus::eInvalidFile, "File is corrupted %s", mFileObject->GetFilename());
                lResult = false;
			}
			SetIsBeforeVersion6WithMainSection(false);
		}
		else
		{
			if( !lDontResetPosition ) mFileObject->FieldReadResetPosition();
		}
	}

    if (lResult)
    {
		if( !lDontResetPosition )
		{
            if (!ReadPassword( IOS_REF.GetStringProp(IMP_FBX_PASSWORD, FbxString("")) ))
			{
                GetStatus().SetCode(FbxStatus::ePasswordError, "Wrong password");
				lResult = false;
			}
		}
    }

    pScene.Clear();
    mNodeArrayName.Clear();
    mTargetArrayName.Clear();
    mUpNodeArrayName.Clear();
	mCameraBackgroundArrayName.Clear();

    FbxArray<Fbx5ObjectTypeInfo*> lObjectDefinitionContent;

    if (lResult)
    {	
		lResult = ReadDefinitionSection(pScene, lObjectDefinitionContent);
    }

    if (lResult)
    {
        ReadCameraSwitcher(pScene);

        // Always read the nodes.
		mFileObject->FieldReadResetPosition();

		if (!mFileObject->IsBeforeVersion6())
		{
			lResult = ReadObjectSection(pScene, lObjectDefinitionContent);
		}
		else
		{
			lResult = ReadNode();
		}    

		// make sure the root node is registered as Model::Scene, so
		// it match the System object of MB.
		if (pScene.GetRootNode())
		{
			mObjectMap.Remove(mObjectMap.Find("Model::Scene"));
			mObjectMap.Add("Model::Scene",pScene.GetRootNode());
		}
    }

    if (lResult)
    {
        // Read the node hierarchy and load it in the entity.
        lResult = ReadHierarchy(*pScene.GetRootNode()); 

		// The above function does nothing on V6 files so let's try 
		// to give the PRODUCER cameras to the globalCameraSettings object.
		// Actually we are going to copy the camera attributes.		
		if (lResult && !mFileObject->IsBeforeVersion6())
		{		
			int i;
			FbxArray<int> cameraSwitcherId;
            FbxArray<int> lProducerCamerasId;

			// scan all the nodes and only process cameras
			for (i = 0; i < (int) mNodeArrayName.GetCount(); i++)
			{
				FbxNode* lCurrentNode = (FbxNode*) mNodeArrayName[i];
				FbxCamera* cam = lCurrentNode->GetCamera(); // ignore anything that is not a camera
				if (cam)
				{
					// the CopyProducerCameras can ignore the cam argument if the name is not one of the
					// producer cameras
					if (pScene.GlobalCameraSettings().CopyProducerCamera(lCurrentNode->GetNameWithoutNameSpacePrefix(), cam))
                    {
                        // the PRODUCER cameras must not exist in the scene. Since we already copied them to the GlobalCameraSetting,
                        // we can destroy them
                        lProducerCamerasId.Add(i); // delay the removal from mNodeArrayName
                    }
				}

				FbxCameraSwitcher* cams = lCurrentNode->GetCameraSwitcher(); // ignore anything that is not a camera switcher
				if (cams)
				{					
					// The last camera switcher that we have processed, will become the
					// actual cameraSwitcher in the scene therefore it will not be deleted. 
					// Previous Camera Switcher will be deleted in SetCameraSwitcher
					
					pScene.GlobalCameraSettings().SetCameraSwitcher(cams);
				}
			}

            // actually destroy the producer cameras
            for (i = lProducerCamerasId.GetCount()-1; i >= 0; i--)
            {
                FbxNode* lProdCam = (FbxNode*) mNodeArrayName[ lProducerCamerasId.GetAt(i) ];
                mNodeArrayName.RemoveFromIndex(lProducerCamerasId.GetAt(i));
                mObjectMap.Remove(mObjectMap.Find(lProdCam->GetName()));
                lProdCam->Destroy();
            }
			
		}

	}
	
	if (lResult)
	{
		// Resolve Camera Background Nodes
		lResult = ResolveCameraBackgrounds(pScene);
    }

    if (lResult)
    {
        //mImporter.ProgressUpdate(NULL, "Removing duplicate textures", "", 0);
        RemoveDuplicateTextures(pScene);

        //mImporter.ProgressUpdate(NULL, "Removing duplicate materials", "", 0);
        RemoveDuplicateMaterials(pScene);

        // Import the textures.
        // Must be called after ReadNode() to set texture file names.
        
        if (IOS_REF.GetBoolProp(IMP_FBX_TEXTURE, true) )
        {
            //mImporter.ProgressUpdate(NULL, "Retrieving medias", "", 0);
            ReadMedia(pScene);
        }
    }

    if (lResult)
    {
        if (IOS_REF.GetBoolProp(IMP_FBX_GOBO, true) )
        {
            //mImporter.ProgressUpdate(NULL, "Retrieving gobos", "", 0);
            ReadGobo(pScene);
        }
	
        if (IOS_REF.GetBoolProp(IMP_FBX_CHARACTER, true))
        {
            //mImporter.ProgressUpdate(NULL, "Retrieving characters", "", 0);
            ReadCharacter(pScene);
		}

		// Read the "SceneGenericPersistence" section (version 5 and lower)
		ReadSceneGenericPersistenceSection(pScene);

        if (IOS_REF.GetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true) )
        {
			if (!mFileObject->IsBeforeVersion6())
			{
				if (mFileObject->FieldReadBegin("Version5"))
				{
					if (mFileObject->FieldReadBlockBegin())
					{
						//mImporter.ProgressUpdate(NULL, "Retrieving global settings", "", 0);
						ReadGlobalLightSettings(pScene);
						ReadGlobalTimeSettings(pScene); 
						ReadGlobalCameraSettings(pScene);
					
						mFileObject->FieldReadBlockEnd();
					}
					mFileObject->FieldReadEnd();
				}
			}
			else
			{
				//mImporter.ProgressUpdate(NULL, "Retrieving global settings", "", 0);
				ReadGlobalLightSettings(pScene);
				ReadGlobalCameraAndTimeSettings(pScene);
			}
        }
    }
        
	if (lResult)
	{
		if (!mFileObject->IsBeforeVersion6())
		{
			ReadConnectionSection();
		}		
	}

    if (lResult)
    {
        // Import the animation.
        if (IOS_REF.GetBoolProp(IMP_ANIMATION, true)  && mTakeInfo.GetCount() > 0)
        {
            lResult = ReadAnimation (pScene);
        }
    }

	if (lResult)
	{
		if (!IOS_REF.GetBoolProp(IMP_RELAXED_FBX_CHECK, false))
		{
			FbxSceneCheckUtility lSceneCheck(&pScene, &mStatus);
			lResult = lSceneCheck.Validate();
		}
	}

	//Post-process
	if (lResult)
	{
		if (!mFileObject->IsBeforeVersion6())
		{
			pScene.BuildMaterialLayersDirectArray();
			pScene.BuildTextureLayersDirectArray();
			pScene.FixInheritType(pScene.GetRootNode());
		}

        // make sure we are always using the new material system
        FbxMaterialConverter lConv( mManager );
        lConv.ConnectTexturesToMaterials( pScene );

		ReorderCameraSwitcherIndices(pScene);
        //let's rename here
        FbxRenamingStrategyFbx5 lRenaming;
        lRenaming.DecodeScene(&pScene);

		//Normally, we would have put this code in ReadLight, but since the properties are read
		//after ReadLight, they wouldn't be initialized with file value. So we have no choice
		//to process this after the file is done reading properties.
		for( int i = 0, lCount = pScene.GetSrcObjectCount<FbxLight>(); i < lCount; i++ )
		{
			FbxLight* lLight = pScene.GetSrcObject<FbxLight>(i);
			if( lLight )
			{
				//In file version 7.3, HotSpot became InnerAngle
				FbxProperty lHotSpot = lLight->FindProperty("HotSpot");
				if( lHotSpot.IsValid() ) lLight->InnerAngle.CopyValue(lHotSpot);

				//In file version 7.3, ConeAngle became OutAngle
				FbxProperty lConeAngle = lLight->FindProperty("Cone angle");
				if( lConeAngle.IsValid() ) lLight->OuterAngle.CopyValue(lConeAngle);
			}
		}

		// Let's make sure that the Roll bones values in memory are expressed using the v7.3 design (i.e complementary values)
		for (int c = 0, lCount = pScene.GetSrcObjectCount<FbxCharacter>(); c < lCount; c++)
		{
			FbxCharacter* lCharacter = pScene.GetSrcObject<FbxCharacter>(c);
			if ( lCharacter )
			{
				lCharacter->SetVersion(0); // force conversion from legacy values to current.
				lCharacter->SetValuesFromLegacyLoad();
			}
		}

		//During FBX 2014.1 release, we changed FbxReference to be FbxObject* type rather than void*.
		//Some reference properties in constraints were connecting to the Dst of the property rather than the Src.
		//This was changed so that all reference properties are used the same way; via Src connections.
		//Because of this, for all files 2014 and lower, we need to make sure this is respected.
		//Gather specific reference properties that changed from Dst to Src connections:
		FbxArray<FbxProperty> lProperties;
		for( int i = 0, c = pScene.GetSrcObjectCount<FbxConstraint>(); i < c; ++i )
		{
			FbxConstraint* lConstraint = pScene.GetSrcObject<FbxConstraint>(i);
			switch( lConstraint->GetConstraintType() )
			{
				case FbxConstraint::eAim:
					lProperties.Add(FbxCast<FbxConstraintAim>(lConstraint)->ConstrainedObject);
					break;

				case FbxConstraint::eParent:
					lProperties.Add(FbxCast<FbxConstraintParent>(lConstraint)->ConstrainedObject);
					break;

				case FbxConstraint::ePosition:
					lProperties.Add(FbxCast<FbxConstraintPosition>(lConstraint)->ConstrainedObject);
					break;

				case FbxConstraint::eRotation:
					lProperties.Add(FbxCast<FbxConstraintRotation>(lConstraint)->ConstrainedObject);
					break;

				case FbxConstraint::eScale:
					lProperties.Add(FbxCast<FbxConstraintScale>(lConstraint)->ConstrainedObject);
					break;

				case FbxConstraint::eSingleChainIK:
					lProperties.Add(FbxCast<FbxConstraintSingleChainIK>(lConstraint)->FirstJointObject);
					lProperties.Add(FbxCast<FbxConstraintSingleChainIK>(lConstraint)->EndJointObject);
					break;

				default:
					break;
			}
		}

		//Now iterate through all gathered properties and switch Dst to Src connections
		for( int i = 0, c = lProperties.Size(); i < c; ++i )
		{
			FbxPropertyT<FbxReference> lRefProperty = lProperties[i];
			FBX_ASSERT_MSG(lRefProperty.GetDstObjectCount() <= 1, "There's more than one destination object, so we can't be sure we're picking the right one!");
			FbxObject* lRefObj = lRefProperty.GetDstObject();
			if( lRefObj )
			{
				lRefProperty.DisconnectDstObject(lRefObj);
				lRefProperty = lRefObj;
			}
		}

		//Producer cameras exported by MB stored their position in Camera.Position property, so transfer it to the LclTranslation of the first node
		#define COPY_POSITION_TO_TRANSLATION(camera)\
			{FbxNode* node = camera ? camera->GetNode() : NULL;\
			if( node && node->LclTranslation.GetCurveNode() == NULL ) node->LclTranslation = camera->Position;}

		FbxGlobalCameraSettings& lGCS = pScene.GlobalCameraSettings();
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerPerspective());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerBack());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerBottom());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerFront());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerLeft());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerRight());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerTop());
	}

    if (pFbx)
    {
        mFileObject = lInternalFbx;
    }   

	FbxArrayDelete(lObjectDefinitionContent);	

    if(!IOS_REF.GetBoolProp(IMP_RELAXED_FBX_CHECK, false))
    {
	    // if at some point the eInvalidFile error has been raised inside a block that still returned true,
	    // we should consider the file corrupted
	    if (lResult && mStatus.GetCode() == FbxStatus::eInvalidFile)
		    lResult = false;
    }
    return lResult; 
}


void FbxReaderFbx5::ReadOptionsInMainSection()
{
	mFileObject->FieldReadResetPosition();

    if (mFileObject->IsPasswordProtected())
    {
        IOS_REF.SetBoolProp(IMP_FBX_PASSWORD_ENABLE, true);
    }
    else
    {
        IOS_REF.SetBoolProp(IMP_FBX_PASSWORD_ENABLE, false);
    }

	int lContentCount;
	
	lContentCount = mFileObject->FieldGetInstanceCount(FIELD_KFBXNODE_MODEL);

    IOS_REF.SetIntProp(IMP_FBX_MODEL_COUNT, lContentCount);

	while (mFileObject->FieldReadBegin(FIELD_KFBXNODE_MODEL))
	{
		FbxString lModelName = mFileObject->FieldReadC();
		mFileObject->FieldReadEnd();

		FbxString lModelNameNoPrefix = lModelName.Mid(lModelName.ReverseFind(':') + 1);

		if (lModelNameNoPrefix.Compare("~fbxexport~") == 0)
		{
            IOS_REF.SetBoolProp(IMP_FBX_TEMPLATE, true);
			break;
		}
	}

	lContentCount = mFileObject->FieldGetInstanceCount("Device");

    IOS_REF.SetIntProp(IMP_FBX_DEVICE_COUNT, lContentCount);
	
	lContentCount = mFileObject->FieldGetInstanceCount("CHARACTER");
    
    IOS_REF.SetIntProp(IMP_FBX_CHARACTER_COUNT, lContentCount);

	lContentCount = mFileObject->FieldGetInstanceCount("ACTOR");
    
    IOS_REF.SetIntProp(IMP_FBX_ACTOR_COUNT, lContentCount);

	lContentCount = 0;

	if (mFileObject->FieldReadBegin("Constraints"))
	{
		if (mFileObject->FieldReadBlockBegin())
		{
			while (mFileObject->FieldReadBegin("Group"))
			{
				if (mFileObject->FieldReadBlockBegin())
				{
					lContentCount += mFileObject->FieldGetInstanceCount("Constraint");

					mFileObject->FieldReadBlockEnd();
				}
				mFileObject->FieldReadEnd();
			}

			mFileObject->FieldReadBlockEnd();
		}
		mFileObject->FieldReadEnd();
	}

    IOS_REF.SetIntProp(IMP_FBX_CONSTRAINT_COUNT, lContentCount);

	lContentCount = 0;

    if (mFileObject->FieldReadBegin(FIELD_MEDIA_MEDIA)) 
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			if (mFileObject->FieldReadBegin(FIELD_MEDIA_VIDEO))
			{
				if (mFileObject->FieldReadBlockBegin())
				{
					while (mFileObject->FieldReadBegin(FIELD_MEDIA_VIDEO))
					{
						if (mFileObject->FieldReadBlockBegin())
						{
							FbxString lNewMediaType = mFileObject->FieldReadC(FIELD_MEDIA_TYPE);

							if (lNewMediaType.Compare(TOKEN_MEDIA_CLIP) == 0)
							{
								lContentCount++;
							}

							mFileObject->FieldReadBlockEnd();
						}
						mFileObject->FieldReadEnd();
					}

					mFileObject->FieldReadBlockEnd();
				}
				mFileObject->FieldReadEnd();
			}

			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    IOS_REF.SetIntProp(IMP_FBX_MEDIA_COUNT, lContentCount);

	ReadTakeOptions();

	ReadOptionsInGenericSection();

	mFileObject->FieldReadResetPosition();
}


void FbxReaderFbx5::ReadTakeOptions()
{
    FbxArrayDelete(mTakeInfo); 

	FbxString lString;

    IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString("") );
    
    //Retrieve fbx info for getting some takes infos out
    if (mFileObject->FieldReadBegin ("Takes")) 
    {
		bool lCurrentTakeFound = false; // Used to validate if the CurrentTake name is valid

        if (mFileObject->FieldReadBlockBegin())
		{
            IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString(mFileObject->FieldReadC ("Current")) );

			while (mFileObject->FieldReadBegin ("Take")) 
			{
				FbxTakeInfo* lNewInfo = FbxNew< FbxTakeInfo >();

				lNewInfo->mName = mFileObject->FieldReadC();

				if (mFileObject->FieldReadBlockBegin ()) 
				{
					if (mFileObject->FieldReadBegin ("FileName")) 
					{
						FbxStatus lStatus; // use a local status since the only error we would get here is
						                   // that we cannot open take001.tak and we don't want to overwite
										   // the mStatus because it may contain important info (like: file corrupted)
						FbxIO lTakeFbxObject(FbxIO::BinaryNormal, lStatus);

						FbxString lTakeFileName;
						FbxString lFullFileName;
              
						lTakeFileName = mFileObject->FieldReadC ();
						mFileObject->FieldReadEnd ();
                    
						// Open the file and read the take
						lFullFileName = mFileObject->GetFullFilePath (lTakeFileName.Buffer ());

						if( lTakeFbxObject.ProjectOpenDirect(lFullFileName.Buffer(), this) )
						{
							lNewInfo->mDescription = lTakeFbxObject.FieldReadC ("Comments");
							lNewInfo->mLocalTimeSpan = lTakeFbxObject.FieldReadTS("LocalTime");
							lNewInfo->mReferenceTimeSpan = lTakeFbxObject.FieldReadTS("ReferenceTime");
							lTakeFbxObject.ProjectClose();
						}
						else if( mFileObject->IsEmbedded() )
						{
							lNewInfo->mDescription = mFileObject->FieldReadC ("Comments");
							lNewInfo->mLocalTimeSpan = mFileObject->FieldReadTS("LocalTime");
							lNewInfo->mReferenceTimeSpan = mFileObject->FieldReadTS("ReferenceTime");
						}
					} 
					else 
					{
						lNewInfo->mDescription = mFileObject->FieldReadC ("Comments");
						lNewInfo->mLocalTimeSpan = mFileObject->FieldReadTS("LocalTime");
						lNewInfo->mReferenceTimeSpan = mFileObject->FieldReadTS("ReferenceTime");
					}

					mFileObject->FieldReadBlockEnd ();
				}

				// New name generation of takes 
				lNewInfo->mImportName = lNewInfo->mName;

                mTakeInfo.Add (lNewInfo);
				

				mFileObject->FieldReadEnd ();
				
                if( IOS_REF.GetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString("")) == lNewInfo->mName )
				{
					lCurrentTakeFound = true;
				}
			}
			mFileObject->FieldReadBlockEnd ();
		}
        mFileObject->FieldReadEnd ();

		// If current take name is invalid take first take name 
		if( !lCurrentTakeFound )
		{
			if( mTakeInfo.GetCount() > 0 )
			{
                IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString(mTakeInfo[0]->mName) );
			}
			else
			{
                IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString("") );
			}
		}
    }
}

void FbxReaderFbx5::ReadOptionsInGenericSection()
{
	if (mFileObject->FieldReadBegin("SceneGenericPersistence"))
	{
		if (mFileObject->FieldReadBlockBegin())
		{
            if (mSceneInfo) mSceneInfo->Destroy();
			mSceneInfo = ReadSceneInfo();

			mFileObject->FieldReadBlockEnd();
		}
		mFileObject->FieldReadEnd();
	}
}

bool FbxReaderFbx5::ReadOptionsInExtensionSection(int& pSectionIndex)
{
	bool lFound = false;
	int i, lCount = mFileObject->ProjectGetExtensionSectionCount();

	for (i = lCount - 1; i >= 0; i--)
	{
		if (mFileObject->ProjectOpenExtensionSection(i))
		{
			if (mFileObject->FieldReadBegin(FIELD_SUMMARY))
			{
				lFound = true;
				pSectionIndex = i;
				break;
			}
			else
			{
				mFileObject->ProjectCloseSection();
			}
		}
	}

	if (!lFound)
	{
		return false;
	}

	if (mFileObject->FieldReadBlockBegin())
	{
		int lFooterVersion = mFileObject->FieldReadI(FIELD_SUMMARY_VERSION, 100);

        IOS_REF.SetBoolProp(IMP_FBX_TEMPLATE, (bool)mFileObject->FieldReadB(FIELD_SUMMARY_TEMPLATE) );
        IOS_REF.SetBoolProp(IMP_FBX_PASSWORD_ENABLE, (bool)mFileObject->FieldReadB(FIELD_SUMMARY_PASSWORD_PROTECTION) );

		if (mFileObject->FieldReadBegin(FIELD_SUMMARY_CONTENT_COUNT))
		{
			if (mFileObject->FieldReadBlockBegin())
			{
                IOS_REF.SetIntProp(IMP_FBX_MODEL_COUNT,      (int)mFileObject->FieldReadI(FIELD_SUMMARY_CONTENT_COUNT_MODEL)       );
                IOS_REF.SetIntProp(IMP_FBX_DEVICE_COUNT,     (int)mFileObject->FieldReadI(FIELD_SUMMARY_CONTENT_COUNT_DEVICE)      );
                IOS_REF.SetIntProp(IMP_FBX_CHARACTER_COUNT,  (int)mFileObject->FieldReadI(FIELD_SUMMARY_CONTENT_COUNT_CHARACTER)   );
                IOS_REF.SetIntProp(IMP_FBX_ACTOR_COUNT,      (int)mFileObject->FieldReadI(FIELD_SUMMARY_CONTENT_COUNT_ACTOR)       );
                IOS_REF.SetIntProp(IMP_FBX_CONSTRAINT_COUNT, (int)mFileObject->FieldReadI(FIELD_SUMMARY_CONTENT_COUNT_CONSTRAINT)  );
                IOS_REF.SetIntProp(IMP_FBX_MEDIA_COUNT,      (int)mFileObject->FieldReadI(FIELD_SUMMARY_CONTENT_COUNT_MEDIA)       );

				mFileObject->FieldReadBlockEnd();
			}
			mFileObject->FieldReadEnd();
		}

		if (lFooterVersion >= 101)
		{
			// Scene info, added in version 101 of FIELD_SUMMARY_VERSION
            if (mSceneInfo) mSceneInfo->Destroy();
            mSceneInfo = ReadSceneInfo();
		}

		ReadTakeOptions();

		mFileObject->FieldReadBlockEnd();
	}
	mFileObject->FieldReadEnd();

	mFileObject->ProjectCloseSection();

	return true;
}


bool FbxReaderFbx5::WriteOptionsInExtensionSection(bool pOverwriteLastExtensionSection)
{
	if (!mFileObject->ProjectCreateExtensionSection(pOverwriteLastExtensionSection))
	{
		// Clear last status if function failed.
		// It is necessary because a file can possibly be opened in read and write mode over the network even if it is read only.
		// It has been observed when opening a read only file on Linux over a network using Samba.
		// Clearing the error prevents file retrieval to fail at a later step.
        GetStatus().Clear();
		return false;
	}

	mFileObject->FieldWriteBegin(FIELD_SUMMARY);
	mFileObject->FieldWriteBlockBegin();

		// Version 100: original version
		// Version 101: 
		//		- added the scene info
		//		- added block version number for FIELD_SUMMARY_TAKES		
		mFileObject->FieldWriteI(FIELD_SUMMARY_VERSION, 101);

        mFileObject->FieldWriteB(FIELD_SUMMARY_TEMPLATE, IOS_REF.GetBoolProp(IMP_FBX_TEMPLATE, false));
        mFileObject->FieldWriteB(FIELD_SUMMARY_PASSWORD_PROTECTION, IOS_REF.GetBoolProp(IMP_FBX_PASSWORD_ENABLE, false));

		mFileObject->FieldWriteBegin(FIELD_SUMMARY_CONTENT_COUNT);
		mFileObject->FieldWriteBlockBegin();	

		mFileObject->FieldWriteS(FIELD_SUMMARY_VERSION, 100);

		mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_MODEL,      IOS_REF.GetIntProp(IMP_FBX_MODEL_COUNT,      0 ));
		mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_DEVICE,     IOS_REF.GetIntProp(IMP_FBX_DEVICE_COUNT,     0 ));
		mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_CHARACTER,  IOS_REF.GetIntProp(IMP_FBX_CHARACTER_COUNT,  0 ));
		mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_ACTOR,      IOS_REF.GetIntProp(IMP_FBX_ACTOR_COUNT,      0 ));
		mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_CONSTRAINT, IOS_REF.GetIntProp(IMP_FBX_CONSTRAINT_COUNT, 0 ));
		mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_MEDIA,      IOS_REF.GetIntProp(IMP_FBX_MEDIA_COUNT,      0 ));

		mFileObject->FieldWriteBlockEnd();
		mFileObject->FieldWriteEnd();

		// Scene info, added in version 101 of FIELD_SUMMARY_VERSION

        if (mSceneInfo)
		{
			WriteSceneInfo(mSceneInfo);
		}

	    mFileObject->FieldWriteBegin(FIELD_SUMMARY_TAKES);
		mFileObject->FieldWriteBlockBegin();	

			// Well, it's never to late to do correctly...
			// The FIELD_SUMMARY_TAKES_VERSION field did not exist in
			// the initial block. It has been added in v101 of FIELD_SUMMARY_VERSION.
			// v100 has been left to represent the "original" block.
			//
			// Version 101: added the take thumbnail
			mFileObject->FieldWriteI(FIELD_SUMMARY_TAKES_VERSION, 101);

            FbxString lTakeName = IOS_REF.GetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString("") );

			mFileObject->FieldWriteC(FIELD_SUMMARY_TAKES_CURRENT, lTakeName.Buffer());

            int i, lCount = mTakeInfo.GetCount();

			for (i = 0; i < lCount; i++)
			{
                FbxTakeInfo* lTakeInfo = mTakeInfo[i];

				mFileObject->FieldWriteBegin(FIELD_SUMMARY_TAKES_TAKE);
				mFileObject->FieldWriteC(lTakeInfo->mName.Buffer());
				mFileObject->FieldWriteBlockBegin();

					if (!lTakeInfo->mDescription.IsEmpty())
					{
						mFileObject->FieldWriteC(FIELD_SUMMARY_TAKES_TAKE_COMMENT, lTakeInfo->mDescription.Buffer());
					}

					mFileObject->FieldWriteTS(FIELD_SUMMARY_TAKES_TAKE_LOCAL_TIME, lTakeInfo->mLocalTimeSpan);
					mFileObject->FieldWriteTS(FIELD_SUMMARY_TAKES_TAKE_REFERENCE_TIME, lTakeInfo->mReferenceTimeSpan);

					if (mSceneInfo && mSceneInfo->GetSceneThumbnail())
					{
						WriteThumbnail(mSceneInfo->GetSceneThumbnail());
					}

				mFileObject->FieldWriteBlockEnd();
				mFileObject->FieldWriteEnd();
			}

		mFileObject->FieldWriteBlockEnd();
		mFileObject->FieldWriteEnd();

	mFileObject->FieldWriteBlockEnd();
	mFileObject->FieldWriteEnd();

	mFileObject->ProjectCloseSection();

	return true;
}

bool FbxReaderFbx5::WriteThumbnail(FbxThumbnail* pThumbnail)
{
	if (pThumbnail->GetSize() != FbxThumbnail::eNotSet)
	{
		// This is a non-empty thumbnail, so save it
		FbxUChar* lImagePtr   = pThumbnail->GetThumbnailImage();
		unsigned long lSize = pThumbnail->GetSizeInBytes();
		unsigned long i;

		mFileObject->FieldWriteBegin(FIELD_THUMBNAIL);
		mFileObject->FieldWriteBlockBegin();

		mFileObject->FieldWriteI(FIELD_THUMBNAIL_VERSION, 100);
		mFileObject->FieldWriteI(FIELD_THUMBNAIL_FORMAT, pThumbnail->GetDataFormat());
		mFileObject->FieldWriteI(FIELD_THUMBNAIL_SIZE,   pThumbnail->GetSize());

		// hard code an encoding of "0" for "raw data". In future version, encoding
		// will indicate if the file is stored with OpenEXR or RAW.
		mFileObject->FieldWriteI(FIELD_THUMBNAIL_ENCODING, 0);

		mFileObject->FieldWriteBegin(FIELD_THUMBNAIL_IMAGE);

	    for (i=0; i<lSize; i++) 
        {
			mFileObject->FieldWriteI(lImagePtr[i]);
		}

		mFileObject->FieldWriteEnd();

		mFileObject->FieldWriteBlockEnd();
		mFileObject->FieldWriteEnd();
	}
	
	return true;
}


FbxDocumentInfo* FbxReaderFbx5::ReadSceneInfo(FbxString& pType)
{
	FbxDocumentInfo* lSceneInfo = NULL;
	if (pType.CompareNoCase("UserData") == 0)
	{
		lSceneInfo = FbxDocumentInfo::Create(&mManager, "");

		/*int lVersion = */ mFileObject->FieldReadI(FIELD_SCENEINFO_VERSION);

		// Read the scene thumbnail
		lSceneInfo->SetSceneThumbnail(ReadThumbnail());

		// Read the Meta-Data
		if (mFileObject->FieldReadBegin(FIELD_SCENEINFO_METADATA))
		{
			if (mFileObject->FieldReadBlockBegin())
			{
				/*int lVersion = */ mFileObject->FieldReadI(FIELD_SCENEINFO_METADATA_VERSION);

				lSceneInfo->mTitle		= mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_TITLE);
				lSceneInfo->mSubject	= mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_SUBJECT);
				lSceneInfo->mAuthor		= mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_AUTHOR);
				lSceneInfo->mKeywords	= mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_KEYWORDS);
				lSceneInfo->mRevision	= mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_REVISION);
				lSceneInfo->mComment	= mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_COMMENT);

				mFileObject->FieldReadBlockEnd();
			}					
			mFileObject->FieldReadEnd();
		}
	}

	return lSceneInfo;
}

FbxDocumentInfo* FbxReaderFbx5::ReadSceneInfo()
{
	FbxDocumentInfo* lSceneInfo = NULL;

	if (mFileObject->FieldReadBegin(FIELD_SCENEINFO))
	{
		// There should be only one block of this type.
		// So read only the first one
		if (mFileObject->FieldReadBlockBegin())
		{
			FbxString lType = mFileObject->FieldReadS(FIELD_SCENEINFO_TYPE);
			lSceneInfo = ReadSceneInfo(lType);
			mFileObject->FieldReadBlockEnd();
		}		
		mFileObject->FieldReadEnd();
	}

	return lSceneInfo;
}

void FbxReaderFbx5::WriteSceneInfo(FbxDocumentInfo* pSceneInfo)
{
	if (!pSceneInfo) return;

	mFileObject->FieldWriteBegin(FIELD_SCENEINFO);
	mFileObject->FieldWriteS("SceneInfo::GlobalInfo");
	{
		mFileObject->FieldWriteBlockBegin();
		{
			mFileObject->FieldWriteS(FIELD_SCENEINFO_TYPE,"UserData");
			mFileObject->FieldWriteI(FIELD_SCENEINFO_VERSION, 100);

			// Thumbnail
			if (pSceneInfo->GetSceneThumbnail())
			{
				WriteThumbnail(pSceneInfo->GetSceneThumbnail());
			}

			// Meta-Data
			mFileObject->FieldWriteBegin(FIELD_SCENEINFO_METADATA);
			mFileObject->FieldWriteBlockBegin();
			{
				mFileObject->FieldWriteI(FIELD_SCENEINFO_METADATA_VERSION,  100);
				mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_TITLE,    pSceneInfo->mTitle.Buffer());
				mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_SUBJECT,  pSceneInfo->mSubject.Buffer());
				mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_AUTHOR,   pSceneInfo->mAuthor.Buffer());
				mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_KEYWORDS, pSceneInfo->mKeywords.Buffer());
				mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_REVISION, pSceneInfo->mRevision.Buffer());
				mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_COMMENT,  pSceneInfo->mComment.Buffer());
			}
			mFileObject->FieldWriteBlockEnd();
			mFileObject->FieldWriteEnd();
		}
		mFileObject->FieldWriteBlockEnd();
	}
	mFileObject->FieldWriteEnd();
}

void FbxReaderFbx5::SetIsBeforeVersion6WithMainSection(bool pOpenMainSection)
{
	// This is a patch for FBX File version 5.8.0
	// Since some of those files are v5 format, and 
	// some of the files created for Siggraph 2004 was v6 format
	// we need to look at the content of the file to be sure
	// if it's a v5 or v6. 
	// This processing is not done for the other version numbers.
	int lMajor, lMinor, lRevision;

	GetVersion(lMajor, lMinor, lRevision);

	if (lMajor == 5 && lMinor == 8 && lRevision == 0)
	{
		bool lMainSectionOpened = pOpenMainSection ? false : true;

		// Look for "Definitions" field in the main section
		if (pOpenMainSection)
		{
			lMainSectionOpened = mFileObject->ProjectOpenMainSection();
		}

		if (lMainSectionOpened)
		{
			if (mFileObject->FieldGetInstanceCount("Definitions") == 0)
			{
				// No field definition, so it's a v5 file format
				mFileObject->SetIsBeforeVersion6(true);
			}
			else
			{
				mFileObject->SetIsBeforeVersion6(false);
			}
		}

		if (pOpenMainSection && lMainSectionOpened)
		{
			mFileObject->ProjectCloseSection();
		}
	}
	else if (lMajor == 5 && lMinor < 8)
	{
		// Note: The FBX SDK will always report a file version >= 5.0
		// See the file history for more details
		mFileObject->SetIsBeforeVersion6(true);
	}
	else if(lMajor < 5)
	{
		mFileObject->SetIsBeforeVersion6(true);
	}
    else
    {
        mFileObject->SetIsBeforeVersion6(false);
    }
}

bool FbxReaderFbx5::ReadDefinitionSection(FbxScene& pScene, FbxArray<Fbx5ObjectTypeInfo*>& pObjectContent)
{
	bool lRet = true;

	if (mFileObject->IsBeforeVersion6()) return lRet;

	if (mFileObject->FieldReadBegin(FIELD_OBJECT_DEFINITION))
    {
		if(mFileObject->FieldReadBlockBegin())
        {
			while (mFileObject->FieldReadBegin(FIELD_OBJECT_DEFINITION_OBJECT_TYPE))
			{
				FbxString lType = mFileObject->FieldReadC();

				if (FindTypeIndex(lType, pObjectContent) == -1)
				{
					Fbx5ObjectTypeInfo* lTypeInfo = FbxNew< Fbx5ObjectTypeInfo >();
					lTypeInfo->mType = lType;

					pObjectContent.Add(lTypeInfo);
				}

				mFileObject->FieldReadEnd();
			}

			mFileObject->FieldReadBlockEnd();			
		}
		mFileObject->FieldReadEnd();
	}
	
	return lRet;
}

bool FbxReaderFbx5::ReadObjectSection(FbxScene& pScene, FbxArray<Fbx5ObjectTypeInfo*>& pObjectContent)
{
	bool lRet = true;

	if (mFileObject->FieldReadBegin(FIELD_OBJECT_PROPERTIES))
    {
		if (mFileObject->FieldReadBlockBegin())
        {			
			int i, lTypeCount = pObjectContent.GetCount();

			// Read all the objects defined in the definition section
			for (i=0; i<lTypeCount; i++)
			{
				FbxString lObjectType = pObjectContent[i]->mType;

				while (mFileObject->FieldReadBegin(lObjectType.Buffer()))
				{
					FbxString lObjectUniqueId	 = mFileObject->FieldReadC();
					FbxString lObjectName      = FbxObject::StripPrefix(lObjectUniqueId);
					FbxString lObjectSubType = mFileObject->FieldReadC();

					if (mFileObject->FieldReadBlockBegin())
					{
						ReadObject(pScene, lObjectType, lObjectSubType, lObjectName, lObjectUniqueId);

						mFileObject->FieldReadBlockEnd();
					}

					mFileObject->FieldReadEnd();
				}
			}

            mFileObject->FieldReadBlockEnd();
        }
		mFileObject->FieldReadEnd();
    }

	return lRet;
}

bool FbxReaderFbx5::ReadObject(FbxScene& pScene, FbxString& pObjectType, FbxString& pObjectSubType, FbxString& pObjectName, FbxString& pObjectUniqueId)
{
	FbxStatus lPrevStatus = mStatus; // Because the file can still be read even if errors are
	// encountered, we remember the current status (in case of important error such as: file corrupted")

	if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_SCENEINFO)
	{
		// Read a SceneInfo
		pScene.SetSceneInfo(ReadSceneInfo(pObjectSubType));
	}
	else
	if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MODEL)
	{
		// Read a FbxNode
		FbxNode* lNode = FbxNode::Create(&mManager, pObjectName.Buffer());            
		pScene.AddNode(lNode);

		mNodeArrayName.Add (const_cast<char*>(lNode->GetName()), (FbxHandle)lNode);
		
		ReadNode(*lNode);
		mObjectMap.Add(pObjectUniqueId.Buffer(),lNode );
	}
	else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MATERIAL)
	{
		// Read a FbxSurfaceMaterial
		FbxSurfacePhong* lMaterial = FbxSurfacePhong::Create(&mManager, pObjectName.Buffer());            
            
		ReadSurfaceMaterial(*lMaterial);
		mObjectMap.Add(pObjectUniqueId.Buffer(),lMaterial );
		
		pScene.AddMaterial(lMaterial);
	}
	else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TEXTURE)
	{
		// Read a FbxFileTexture		
		FbxFileTexture* lTexture = FbxFileTexture::Create(&mManager, pObjectName.Buffer());            
            
		ReadTexture(*lTexture);
		mObjectMap.Add(pObjectUniqueId.Buffer(),lTexture );

		pScene.AddTexture(lTexture);
	}
	else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_VIDEO)
	{
		// Read a FbxVideo
		FbxVideo* lVideo = FbxVideo::Create(&mManager, pObjectName.Buffer());            
            
		ReadVideo(*lVideo);
		
		mObjectMap.Add(pObjectUniqueId.Buffer(),lVideo);

		pScene.AddVideo(lVideo);
	}
	else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_DEFORMER)
	{
		// Read a FbxDeformer
		if (pObjectSubType == "Skin")
		{
			FbxSkin* lSkin = FbxSkin::Create(&mManager, pObjectName.Buffer());
            
			ReadSkin(*lSkin);
			pScene.ConnectSrcObject(lSkin);
		
			mObjectMap.Add(pObjectUniqueId.Buffer(),lSkin);
		}
		else if (pObjectSubType == "Cluster")
		{
			FbxCluster* lCluster = FbxCluster::Create(&mManager, pObjectName.Buffer());
            
			ReadCluster(*lCluster);
			pScene.ConnectSrcObject(lCluster);
			mObjectMap.Add(pObjectUniqueId.Buffer(),lCluster );
		}
	}
	else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_POSE)
	{
		// Read bind pose, rest pose and character pose
		if (pObjectSubType == "CharacterPose")
		{
			// CharacterPose are written in v5 file format for the moment.
			// So we should not be here, But here is the code that will do the job
			int lCharacterPoseIndex = pScene.CreateCharacterPose(pObjectName.Buffer());

			if (lCharacterPoseIndex != -1)
			{
				FbxCharacterPose* lCharacterPose = pScene.GetCharacterPose(lCharacterPoseIndex);
				
				if (!ReadCharacterPose(*lCharacterPose))
				{
					pScene.DestroyCharacterPose(lCharacterPoseIndex);
				}
			}
		}
		else if (pObjectSubType == "BindPose" || pObjectSubType == "RestPose")
		{
			bool isBindPose = pObjectSubType == "BindPose";
			FbxPose* lPose = FbxPose::Create(&mManager, pObjectName.Buffer());
			lPose->SetIsBindPose(isBindPose);

			if (!ReadPose(pScene, lPose, isBindPose))
			{
				lPose->Destroy();
			}
			else
			{
				pScene.AddPose(lPose);
			}
		}
	}
	else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GENERIC_NODE)
	{
		// Read a FbxGenericNode
		FbxGenericNode* lNode = FbxGenericNode::Create(&mManager, pObjectName.Buffer());

		ReadGenericNode(*lNode);
		pScene.AddGenericNode(lNode);

		mObjectMap.Add(pObjectUniqueId.Buffer(),lNode );
	}
	else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONSTRAINT)
	{
		FbxConstraint::EType lType = ConvertConstraintToken(pObjectSubType.Buffer());
		//TO DO -> merge those two sections when the character will be added to the entity

		if (lType == FbxConstraint::eCharacter)
		{
            int lInputType;				
			int lInputIndex;
			int lCharacterIndex = pScene.CreateCharacter(pObjectName.Buffer());

			if (lCharacterIndex != -1)
			{
				FbxCharacter* lCharacter = pScene.GetCharacter(lCharacterIndex);
				ReadCharacter(*lCharacter, lInputType, lInputIndex);				

				mObjectMap.Add(pObjectUniqueId.Buffer(),lCharacter );
			}
		}
		else
		{
			FbxConstraint* lConstraint = NULL;
			if(lType == FbxConstraint::ePosition)
			{
				lConstraint = FbxConstraintPosition::Create(&mManager,  pObjectName.Buffer() );
			}
			else if(lType == FbxConstraint::eRotation)
			{
				lConstraint = FbxConstraintRotation::Create(&mManager,  pObjectName.Buffer() );
			}
			else if(lType == FbxConstraint::eScale)
			{
				lConstraint = FbxConstraintScale::Create(&mManager,  pObjectName.Buffer() );
			}
			else if(lType == FbxConstraint::eParent)
			{
				lConstraint = FbxConstraintParent::Create(&mManager,  pObjectName.Buffer() );
			}
			else if(lType == FbxConstraint::eSingleChainIK)
			{
				lConstraint = FbxConstraintSingleChainIK::Create(&mManager,  pObjectName.Buffer() );
			}
			else if(lType == FbxConstraint::eAim)
			{
				lConstraint = FbxConstraintAim::Create(&mManager,  pObjectName.Buffer() );
			}

			if(lConstraint)
			{
				if (ReadConstraint(*lConstraint))	
				{
					pScene.ConnectSrcObject(lConstraint);
					mObjectMap.Add(pObjectUniqueId.Buffer(),lConstraint );
				}
				else
				{
					lConstraint->Destroy();
				}
			}
		}
	}
	//
	// todo: Actor
	//
	else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONTROLSET_PLUG)
	{
		if(pObjectSubType == "ControlSetPlug")
		{
			int lControlSetPlugIndex = pScene.CreateControlSetPlug(pObjectName.Buffer());

			if(lControlSetPlugIndex != -1)
			{
				FbxControlSetPlug *lPlug = pScene.GetControlSetPlug(lControlSetPlugIndex);

				mFileObject->FieldReadI(FIELD_KFBXGENERICNODE_VERSION,100);

				ReadProperties(lPlug);
				mObjectMap.Add(pObjectUniqueId.Buffer(),lPlug );
			}
		}
	}
	else
	{
		// Read an object of type unknown to the reader in a FbxGenericNode
	}
    
	// the mStatus may have been cleared while reading a valid section but a previous one may have been
	// detected as corrupted so let's return the error we had before processing the object
	if (!mStatus.Error() && lPrevStatus.Error())
		mStatus = lPrevStatus;
        
	return true;
}

bool FbxReaderFbx5::ReadNode ()
{
    int lCount;
    FbxString lModelName;
    FbxNode* lNode;
    FbxString lTemp;
    int lModelCount;

    //
    // Retrieve all Models....
    //

    lModelCount = mFileObject->FieldGetInstanceCount (FIELD_KFBXNODE_MODEL);

    for (lCount=0; lCount < lModelCount; lCount ++)
    {
        if (mFileObject->FieldReadBegin (FIELD_KFBXNODE_MODEL))
        {
            //
            // Read the model name and create a node
            //
            lModelName = FbxObject::StripPrefix(mFileObject->FieldReadC ());

            lNode = FbxNode::Create(&mManager, lModelName.Buffer());            
            mNodeArrayName.Add (const_cast<char*>(lNode->GetName()), (FbxHandle)lNode);

            if (mFileObject->FieldReadBlockBegin ())
            {
                ReadNode(*lNode);
                mFileObject->FieldReadBlockEnd ();

				mObjectMap.Add(lNode->GetName(),lNode);
            }

            mFileObject->FieldReadEnd ();
        }
    }
	FBX_ASSERT_RETURN_VALUE(GetStatus().GetCode() != FbxStatus::eInvalidFile, false);
    return true;
}

bool FbxReaderFbx5::ReadAnimation(FbxScene& pScene)
{
    int i, lCount = mTakeInfo.GetCount();

	bool lResult = true;

    if (mFileObject->FieldReadBegin("Takes")) 
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			while (mFileObject->FieldReadBegin("Take"))
			{
				FbxString lTakeRead = mFileObject->FieldReadC();
				FbxTakeInfo* lTakeInfo = NULL;

				if (!lTakeRead.IsEmpty())
				{
					for (i = 0; i < lCount; i++)
					{
						if (lTakeRead.Compare(mTakeInfo[i]->mName) == 0)
						{
                            lTakeInfo = mTakeInfo[i];
							break;
						}
					}
				}

				if (lTakeInfo && lTakeInfo->mSelect)
				{
					if (mFileObject->FieldReadBlockBegin()) 
					{
						if (ReadTakeAnimation(pScene, lTakeInfo))
						{
							pScene.SetTakeInfo(*lTakeInfo);
						}
						else
						{
							lResult = false;
						}
						mFileObject->FieldReadBlockEnd();
					}
				}
				mFileObject->FieldReadEnd();    
			}
			mFileObject->FieldReadBlockEnd();        
		}
        mFileObject->FieldReadEnd();    
    }

    FbxString lCurrentTakeName = IOS_REF.GetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString("") );
	if( pScene.GetTakeInfo(lCurrentTakeName.Buffer()) )
		pScene.ActiveAnimStackName = lCurrentTakeName;

    return lResult;
}


bool FbxReaderFbx5::ReadTakeAnimation(FbxScene& pScene, FbxTakeInfo* pTakeInfo)
{
	bool lResult = false;

    // Create the FbxAnimStack from the pTakeInfo
    FbxAnimStack* lAnimStack = FbxAnimStack::Create(&pScene, pTakeInfo->mName);
    FBX_ASSERT(lAnimStack != NULL);
    if (!lAnimStack)
        // This is a fatal error!
        return false;

    // Intialize the anim stack with the information stored in the pTakeInfo
    lAnimStack->Reset(pTakeInfo);

    // And add the base layer. This layer must always exist since the rest
    // of the reader will use it to store the animation nodes.
    mAnimLayer = FbxAnimLayer::Create(&pScene, "Layer0");
    lAnimStack->AddMember(mAnimLayer);

	FbxStatus lStatus = GetStatus();
    if (mFileObject->FieldReadBegin ("FileName")) 
    {
        FbxIO lTakeFbxObject(FbxIO::BinaryNormal, GetStatus());

        FbxString lTakeFileName;
        FbxString lFullFileName;
  
        lTakeFileName = mFileObject->FieldReadC();
        mFileObject->FieldReadEnd ();
        
        // Open the file and read the take
        lFullFileName = mFileObject->GetFullFilePath(lTakeFileName.Buffer());

        if( lTakeFbxObject.ProjectOpenDirect(lFullFileName.Buffer(), this) )
        {
            lResult = ReadNodeAnimation(lTakeFbxObject, pScene, pTakeInfo);
            lTakeFbxObject.ProjectClose ();
        }
        else
        {
            if( mFileObject->IsEmbedded() )
            {
                lResult = ReadNodeAnimation(*mFileObject, pScene, pTakeInfo);
            }
            else
            {
                lResult = false;
            }
        }
    } 
    else 
    {
        lResult = ReadNodeAnimation(*mFileObject, pScene, pTakeInfo);
    }

	if (lResult)
	{
		lResult = TimeShiftNodeAnimation(pScene, pTakeInfo);
	}

	// if we already found that the file is corrupted, return this error code
	if (lStatus.GetCode() == FbxStatus::eInvalidFile)
		GetStatus() = lStatus;

	return lResult;
}


bool FbxReaderFbx5::ReadNodeAnimation (FbxIO& pFileObject, FbxScene& pScene, FbxTakeInfo* pTakeInfo)
{
    FbxNode* lNode;
    FbxFileTexture* lTexture;
    FbxSurfaceMaterial* lMaterial;
    FbxString lName;
    FbxString lTemp;
    FbxMultiMap *lTimeWarpSet;

	FbxString lTakeName = pTakeInfo->mImportName;

	lTimeWarpSet = pScene.AddTakeTimeWarpSet(lTakeName.Buffer());
    ReadTimeWarps(pFileObject, *lTimeWarpSet);
    FbxAnimUtilities::SetTimeWarpSet(lTimeWarpSet);

	int lProgress = 0;
	int lModelCount = pFileObject.FieldGetInstanceCount(FIELD_KFBXNODE_MODEL);
	int lTextureCount = pFileObject.FieldGetInstanceCount(FIELD_KFBXNODE_MODEL);
	int lMaterialCount = pFileObject.FieldGetInstanceCount(FIELD_KFBXNODE_MODEL);
	int lTotalCount = lModelCount + lTextureCount + lMaterialCount;

	FbxString lSubTitle;
	lSubTitle += "Retrieving take ";
	lSubTitle += lTakeName;

    if (pScene.GetSceneInfo())
    {
        if (pScene.GetSceneInfo()->GetSceneThumbnail() == NULL)
        {
            FbxThumbnail* lThumbnail = ReadThumbnail();
            pScene.GetSceneInfo()->SetSceneThumbnail(lThumbnail);
         }
    }

    // At this point, we know that the Anim stack corresponding to this pTakeInfo already
    // exists. We can find it and use it from now on.
    FbxAnimStack* lAnimStack = pScene.FindMember<FbxAnimStack>(lTakeName);
    FBX_ASSERT(lAnimStack != NULL);
    if (lAnimStack == NULL)
        // this is a fatal error!
        return false;

    // The anim stack always contain the base layer. We get it now.
    FbxAnimLayer* lAnimLayer = lAnimStack->GetMember<FbxAnimLayer>();
    FBX_ASSERT(lAnimLayer != NULL);
    if (lAnimLayer == NULL)
        // this is a fatal error!
        return false;

    mAnimLayer = lAnimLayer;
    while (pFileObject.FieldReadBegin (FIELD_KFBXNODE_MODEL)) 
    {
		lNode = NULL;
        lName = FbxObject::StripPrefix(pFileObject.FieldReadC());

        if (lName.Compare(FBXSDK_CAMERA_SWITCHER) == 0 || lName == FbxString("Model::") + FbxString(FBXSDK_CAMERA_SWITCHER))
        {
			if (pScene.GlobalCameraSettings().GetCameraSwitcher()) {
				lNode = pScene.GlobalCameraSettings().GetCameraSwitcher()->GetNode();
			}
        }
        else
        {
			lNode = FbxCast<FbxNode>(mObjectMap.Get(mObjectMap.Find(lName)));
        }

        if (lNode) 
        {
            if (pFileObject.FieldReadBlockBegin())
			{
				ReadAnimation(pFileObject, lNode);

				// MotionBuilder 4.01 and earlier versions saved nurb and patch shape channel names 
				// following the template "Shape 0x (Shape)" where x is the index of the shape starting 
				// at 1. Since then, Jori modified shape channels to turn them into animated properties. 
				// As a result, nurb and patch shape channel names are now saved following the template 
				// "<shape name> (Shape)". The FBX SDK keeps the old shape channel naming scheme but has 
				// been modifed to handle the new one and convert shape channel names to the old shape 
				// channel naming scheme.
				if (lNode->GetGeometry())
				{
					if (mFileObject->IsBeforeVersion6())
					{
						lNode->GetGeometry()->CleanShapeChannels(mAnimLayer);
					}
				}

                if(IOS_REF.GetBoolProp(IMP_FBX_MERGE_LAYER_AND_TIMEWARP, false))
				{
					FbxAnimUtilities::MergeLayerAndTimeWarp(lNode, mAnimLayer);
				}

				pFileObject.FieldReadBlockEnd ();
			}
        }

        pFileObject.FieldReadEnd ();
    }

	while (pFileObject.FieldReadBegin (FIELD_KFBXGENERICNODE_GENERICNODE)) 
    {
		FbxGenericNode* lGenericNode;

		lName = FbxObject::StripPrefix(pFileObject.FieldReadC ());

		lGenericNode = pScene.GetGenericNode(lName.Buffer());

		if (lGenericNode)
		{
            if (pFileObject.FieldReadBlockBegin())
			{
				ReadAnimation(pFileObject, lGenericNode);
				pFileObject.FieldReadBlockEnd();
			}			
		}

		pFileObject.FieldReadEnd ();
	}

    while (pFileObject.FieldReadBegin (FIELD_KFBXTEXTURE_TEXTURE)) 
    {
        lName = FbxObject::StripPrefix(pFileObject.FieldReadC());

        lTexture = FbxCast<FbxFileTexture>(pScene.GetTexture(lName.Buffer()));

        if (lTexture) 
        {
            if (pFileObject.FieldReadBlockBegin())
			{
				ReadAnimation(pFileObject, lTexture);

                if(IOS_REF.GetBoolProp(IMP_FBX_MERGE_LAYER_AND_TIMEWARP, false))
				{
					FbxAnimUtilities::MergeLayerAndTimeWarp(lTexture, mAnimLayer);
				}

				pFileObject.FieldReadBlockEnd ();
			}
        }

        pFileObject.FieldReadEnd ();
    }

    while (pFileObject.FieldReadBegin (FIELD_KFBXMATERIAL_MATERIAL)) 
    {
        lName = FbxObject::StripPrefix(pFileObject.FieldReadC ());

        lMaterial = pScene.GetMaterial(lName.Buffer());

        if (lMaterial) 
        {
            if (pFileObject.FieldReadBlockBegin())
			{
				ReadAnimation(pFileObject, lMaterial);
                if(IOS_REF.GetBoolProp(IMP_FBX_MERGE_LAYER_AND_TIMEWARP, false))
				{
					FbxAnimUtilities::MergeLayerAndTimeWarp(lMaterial, mAnimLayer);
				}

				pFileObject.FieldReadBlockEnd ();
			}
        }

        pFileObject.FieldReadEnd ();
    }

	while (pFileObject.FieldReadBegin (FIELD_CONSTRAINT)) 
    {
		FbxConstraint* lConstraint;

		lName = FbxObject::StripPrefix(pFileObject.FieldReadC ());

		lConstraint = pScene.FindSrcObject<FbxConstraint>(lName.Buffer());

		if (lConstraint)
		{
            if (pFileObject.FieldReadBlockBegin())
			{
				ReadAnimation(pFileObject, lConstraint);
				pFileObject.FieldReadBlockEnd();
			}			
		}

		pFileObject.FieldReadEnd ();
	}

    FbxAnimUtilities::SetTimeWarpSet(NULL);

    return true;
}

FbxThumbnail* FbxReaderFbx5::ReadThumbnail()
{
	FbxThumbnail* lRet = NULL;

	if (mFileObject->FieldReadBegin(FIELD_THUMBNAIL))
	{
		FbxThumbnail* lThumbnail = FbxThumbnail::Create(&mManager, "");
		bool           lImageRead = false;

		if (mFileObject->FieldReadBlockBegin())
		{
			mFileObject->FieldReadI(FIELD_THUMBNAIL_VERSION, 100);
			lThumbnail->SetDataFormat((FbxThumbnail::EDataFormat)mFileObject->FieldReadI(FIELD_THUMBNAIL_FORMAT, (int)FbxThumbnail::eRGB_24));
			lThumbnail->SetSize((FbxThumbnail::EImageSize)mFileObject->FieldReadI(FIELD_THUMBNAIL_SIZE, (int)FbxThumbnail::eNotSet));

			// For the moment, do nothing with the encoding; assume it's a raw image
			int lEncoding = mFileObject->FieldReadI(FIELD_THUMBNAIL_ENCODING, 0);

			if (lEncoding == 0)
			{
				if (lThumbnail->GetSize() != FbxThumbnail::eNotSet)
				{
					if (mFileObject->FieldReadBegin(FIELD_THUMBNAIL_IMAGE))
					{
						FbxUChar* lImagePtr   = lThumbnail->GetThumbnailImage();
						unsigned long lSize = lThumbnail->GetSizeInBytes();
						unsigned long i;

						for (i=0; i<lSize; i++) 
						{
							lImagePtr[i] = mFileObject->FieldReadI();
						}

						mFileObject->FieldReadEnd();
					}

					lImageRead = true;
				}
			}

			mFileObject->FieldReadBlockEnd();
		}
		mFileObject->FieldReadEnd();

		if (lImageRead)
		{
			lRet = lThumbnail;
		}
		else
		{
			lThumbnail->Destroy();
		}
	}

	return lRet;
}

bool FbxReaderFbx5::TimeShiftNodeAnimation(FbxScene& pScene, FbxTakeInfo* pTakeInfo)
{
	FbxTime lTimeShift;
	int i, lCount;

    FbxAnimStack* lAnimStack = pScene.FindMember<FbxAnimStack>(pTakeInfo->mImportName.Buffer());
    FBX_ASSERT(lAnimStack != NULL);

	if (pTakeInfo->mImportOffsetType == FbxTakeInfo::eRelative)
	{
		lTimeShift = pTakeInfo->mImportOffset;
	}
	else // if (pTakeInfo->mImportOffsetType == FbxTakeInfo::eAbsolute)
	{
		FbxTimeSpan lAnimationInterval(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);

        // get the animation interval by visiting ALL the nodes in ALL the layers of the
        // selected AnimStack
        for (int l = 0; l < lAnimStack->GetMemberCount<FbxAnimLayer>(); l++)
        {
            FbxAnimLayer* lAnimLayer = lAnimStack->GetMember<FbxAnimLayer>(l);
            for (int n = 0; n < lAnimLayer->GetMemberCount<FbxAnimCurveNode>(); n++)
            {
                FbxAnimCurveNode* lAnimCurveNode = lAnimLayer->GetMember<FbxAnimCurveNode>(n);
                lAnimCurveNode->GetAnimationInterval(lAnimationInterval);
		    }
        }
		lTimeShift = pTakeInfo->mImportOffset - lAnimationInterval.GetStart();
	}

	if (lTimeShift != FBXSDK_TIME_ZERO)
	{
		FbxAnimCurveFilterTSS lOffsetFilter;
		FbxTime lStart(FBXSDK_TIME_MINUS_INFINITE);
		FbxTime lStop(FBXSDK_TIME_INFINITE);

		lOffsetFilter.SetStartTime(lStart);
		lOffsetFilter.SetStopTime(lStop);
		lOffsetFilter.SetShift(lTimeShift);	

		lCount = pScene.GetMemberCount<FbxNode>();
        FbxArray<FbxAnimCurve*> lCurves;

		for (i = 0; i < lCount; i++)
		{
            FbxNode* lNode = pScene.GetMember<FbxNode>(i);

            GetAllAnimCurves(lNode, lAnimStack, lCurves);
            if (lCurves.GetCount() > 0)
            {
                FbxAnimCurve** curves = lCurves.GetArray();
                lOffsetFilter.Apply(curves, lCurves.GetCount());
                lCurves.Clear();
            }
		}

		pTakeInfo->mLocalTimeSpan.SetStart(pTakeInfo->mLocalTimeSpan.GetStart() + lTimeShift);
		pTakeInfo->mLocalTimeSpan.SetStop(pTakeInfo->mLocalTimeSpan.GetStop() + lTimeShift);
	}

	return true;
}


bool FbxReaderFbx5::ReadHierarchy (FbxNode& pRootNode)
{
    bool lStatus;
    
	if (mFileObject->IsBeforeVersion6())
	{
		//
		// Build the node tree
		//
		lStatus = ResolveHierarchy(pRootNode);

		if( lStatus == false )
		{
			return false;
		}

		//
		// Resolve links
		//

        if (IOS_REF.GetBoolProp(IMP_FBX_LINK, true))
		{
			lStatus = ResolveLinks(pRootNode, pRootNode);       

			if( lStatus == false )
			{
				return false;
			}
		}

		//
		// Resolve targets

		lStatus = ResolveTargets(pRootNode);

		if( lStatus == false )
		{
			return false;
		}

		//
		// Resolve Up vector Nodes

		lStatus = ResolveUpNodes(pRootNode);

		if( lStatus == false )
		{
			return false;
		}
	}
    return true;
}


bool FbxReaderFbx5::ResolveHierarchy (FbxNode& pRootNode)
{
    //
    // Sort the mNodeArrayName...
    //
    mNodeArrayName.Sort();

    //
    // For every node in the array
    // 
    int i, lParentCount = (int) mNodeArrayName.GetCount();

    for (i = 0; i < lParentCount; i++)
    {
        //double lPercent = 50.0 * i / lParentCount;
        //mImporter.ProgressUpdate(NULL, "Linking node hierarchy", "", lPercent);

        FbxNode* lParentNode = (FbxNode*) mNodeArrayName[i];
        
        //
        // Link all children 
        //
        int j, lChildrenCount = (int) lParentNode->GetChildNameCount();

        for (j = 0; j < lChildrenCount; j++) 
        {
			char* lChildName = lParentNode->GetChildName(j);

			FbxNode* lChildNode = FindNode(lChildName);
			if (lChildNode != NULL)
			{
				if (lChildNode == lParentNode)
				{
					// technically, in FBX5 names must be unique (case sensitive) therefore, if
					// the incoming file has been tampered with and contains duplicated names, we should abort the import
					// process. Too bad if the in memory scene is incomplete!
					mStatus.SetCode(FbxStatus::eInvalidFile, "Malformed scene graph");
					return false;
				}

                lParentNode->AddChild (lChildNode);
            }
        }
    }

    //
    // Link all orphan nodes with the root node
    // 
    for (i = 0; i < lParentCount; i++)
    {
        //double lPercent = 50.0 + 50.0 * i / lParentCount;
        //mImporter.ProgressUpdate(NULL, "Linking node hierarchy", "", lPercent);

        FbxNode* lCurrentNode = (FbxNode*) mNodeArrayName[i];

        if (lCurrentNode->GetParent() == NULL)
        {
            pRootNode.AddChild(lCurrentNode);
        }
    }

	if (pRootNode.GetChildCount() == 0)
	{
		// most likely the code above have created loops in the scene graph where a top level node has been used
		// as a child of another node thus no "orphan" nodes could be attached to the scene root.
		mStatus.SetCode(FbxStatus::eInvalidFile, "Malformed scene graph");
		return false;
	}
    return true;
}


bool FbxReaderFbx5::ResolveLinks (FbxNode& pRootNode, FbxNode& pCurrentNode)
{
    int j;
    FbxGeometry* lGeometry = pCurrentNode.GetGeometry();
    
    //
    // We link the links only for The Mesh, Nurbs and Patch...
    //
    if (lGeometry)
    {
		int lSkinCount = lGeometry->GetDeformerCount(FbxDeformer::eSkin);

		if (lSkinCount > 0)
		{
			FbxSkin*		lSkin = reinterpret_cast<FbxSkin*>(lGeometry->GetDeformer(0, FbxDeformer::eSkin));
			int				i, lClusterCount;
			FbxCluster*	lCluster;
			FbxString			lName, lAssName;
			FbxNode*		lClusterNode;
			FbxNode*		lAssLinkNode;

			lClusterCount = lSkin->GetClusterCount();

			for (i=0; i<lClusterCount; i++)
			{
				lCluster = lSkin->GetCluster(i);
	        
				lName = lCluster->mBeforeVersion6LinkName;
				lAssName = lCluster->mBeforeVersion6AssociateModelName;
	            
				lClusterNode = pRootNode.FindChild (lName.Buffer ());

				if (lClusterNode) lCluster->SetLink (lClusterNode);
	            
				if (lAssName != "")
				{
					lAssLinkNode = pRootNode.FindChild (lAssName.Buffer ());
	                
					if (lAssLinkNode) 
					{
						lCluster->SetAssociateModel (lAssLinkNode);
					}
				}
			}
		}
    }
    
    for (j = 0; j < pCurrentNode.GetChildCount (); j++)
    {
        ResolveLinks (pRootNode, *pCurrentNode.GetChild (j));
    }

    return true;
}


bool FbxReaderFbx5::ResolveTargets(FbxNode& pRootNode)
{
    mTargetArrayName.Sort();

    int i, lCount = (int) mTargetArrayName.GetCount();

    for (i = 0; i < lCount; i++)
    {
        //double lPercent = 100.0 * i / lCount;
        //mImporter.ProgressUpdate(NULL, "Linking targets", "", lPercent);

        FbxNode* lNode = (FbxNode*) mTargetArrayName[i];
        const char* lTargetName = NULL;
        mTargetArrayName.GetFromIndex(i, &lTargetName);
        lNode->SetTarget(pRootNode.FindChild(lTargetName, true));
    }

    return true;
}

bool FbxReaderFbx5::ResolveUpNodes(FbxNode& pRootNode)
{
    mUpNodeArrayName.Sort();

    int i, lCount = (int) mUpNodeArrayName.GetCount();

    for (i = 0; i < lCount; i++)
    {
        FbxNode* lNode = (FbxNode*)mUpNodeArrayName[i];
        const char* lUpNodeName = NULL;
        mUpNodeArrayName.GetFromIndex(i, &lUpNodeName);
        lNode->SetTargetUp(pRootNode.FindChild(lUpNodeName, true));
    }

    return true;
}

bool FbxReaderFbx5::ResolveCameraBackgrounds(FbxScene& pScene)
{
    mCameraBackgroundArrayName.Sort();

    int i;
	int j;
	int lCCount = (int) mCameraBackgroundArrayName.GetCount();
	int lTCount = pScene.GetTextureCount();

    for (i = 0; i < lCCount; i++)
    {
        FbxCamera* lCamera = (FbxCamera*) mCameraBackgroundArrayName[i];
        const char* lTargetName = NULL;
        mCameraBackgroundArrayName.GetFromIndex(i, &lTargetName);

		for (j = 0; j < lTCount; j++)
		{
			FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(pScene.GetTexture(i));
			if (strcmp(lTexture->GetName(), lTargetName) == 0)
			{
                if( lTexture && lCamera && lCamera->BackgroundTexture.Get() != lTexture )
                {                    
                    lCamera->BackgroundTexture = lTexture;
                }
			}
		}
    }

    return true;
}


bool FbxReaderFbx5::ReadCameraSwitcher( FbxCameraSwitcher& pCameraSwitcher )
{
	pCameraSwitcher.SetDefaultCameraIndex(mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERASWITCHER_CAMERA_ID));    
	pCameraSwitcher.ClearCameraNames();

	if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERASWITCHER_CAMERA_INDEX_NAME))
	{
		FbxUInt lCameraNameCount = mFileObject->FieldReadGetCount();

		while (lCameraNameCount)
		{	
			FbxString lCameraName = FbxObject::StripPrefix(mFileObject->FieldReadS());
			pCameraSwitcher.AddCameraName(lCameraName.Buffer());
			lCameraNameCount--;
		}

		mFileObject->FieldReadEnd();
	}

	return true;
}


void FbxReaderFbx5::ReadCameraSwitcher(FbxScene& pScene)
{
    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERASWITCHER_SWITCHER))
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			if (pScene.GlobalCameraSettings().GetCameraSwitcher()) {
				ReadCameraSwitcher(*pScene.GlobalCameraSettings().GetCameraSwitcher());
			}
			mFileObject->FieldReadBlockEnd();   		
		}
        mFileObject->FieldReadEnd();
    }
}


void FbxReaderFbx5::ReorderCameraSwitcherIndices(FbxScene& pScene)
{
	if( pScene.GlobalCameraSettings().GetCameraSwitcher() )
	{
		int lTakeNodeCount, lKeyCount;
		FbxNode* lCameraSwitcherNode = pScene.GlobalCameraSettings().GetCameraSwitcher()->GetNode();
		FbxCameraSwitcher* lCameraSwitcher = lCameraSwitcherNode->GetCameraSwitcher();
		FbxArray<int> lCameraIndexArray;

		int lCameraIndexCount = lCameraSwitcher->GetCameraNameCount();
		if( lCameraIndexCount == 0 )
		{
			// FiLMBOX may not save camera names associated with indices if there is not animation data in the camera switcher.
			return;
		}

		int lCameraCount = pScene.GetMemberCount<FbxCamera>();
		FBX_ASSERT_MSG(lCameraIndexCount == lCameraCount, "Camera Switcher reference count doesn't match scene camera count!");
		for( int lCameraIndexIter = 0; lCameraIndexIter < lCameraIndexCount; lCameraIndexIter++ )
		{
			bool lFound = false;
			for( int lCameraIter = 0; lCameraIter < lCameraCount; ++lCameraIter )
			{
				FbxCamera* lCamera = pScene.GetMember<FbxCamera>(lCameraIter);
				if (lCamera)
                {
                    // The Camera attribute is not necessarly named (this is more frequent with older
                    // versions of the SDK). Let's check this and name it using the node name the attribute 
                    // is connected to. Note that if the attribute is connected to more than one node, 
                    // we will use the first connection anyway.
                    FbxString lCameraName(lCamera->GetName());
                    if (lCameraName.GetLen() == 0)
                    {
                        FbxNode* lParent = lCamera->GetDstObject<FbxNode>();
                        if (lParent)
                            lCameraName = FbxString(lParent->GetName());
                    }

				    if( !strcmp(lCameraSwitcher->GetCameraName(lCameraIndexIter), lCameraName.Buffer()) )
				    {
					    lFound = true;
					    lCameraIndexArray.Add(lCameraIter + 1); // Camera indices start at 1.
					    break;
				    }
                }
			}

			FBX_ASSERT(lFound == true);
			if( lFound == false )
			{
				lCameraIndexArray.Add(-1);
			}
		}

		lTakeNodeCount = pScene.GetSrcObjectCount<FbxAnimStack>();

		for( int lTakeNodeIter = 0; lTakeNodeIter < lTakeNodeCount; ++lTakeNodeIter )
		{
            FbxAnimStack* lAnimStack = pScene.GetMember<FbxAnimStack>(lTakeNodeIter);
            FBX_ASSERT(lAnimStack != NULL);
            if (!lAnimStack)
                // this is actually a fatal error and should never happen
                continue; 

            FbxAnimLayer* lAnimLayer = lAnimStack->GetMember<FbxAnimLayer>();
            FBX_ASSERT(lAnimLayer != NULL);
            if (!lAnimLayer)
                // this is actually a fatal error and should never happen
                continue; 

            int lCameraIndex, lNewCameraIndex;
			FbxAnimCurve* lAnimCurve = lCameraSwitcher->CameraIndex.GetCurve(lAnimLayer);
            if( lAnimCurve )
			{
                FbxAnimUtilities::CurveIntfce lFCurve(lAnimCurve);
				lCameraIndex = (int) lFCurve.GetValue();
				if( lCameraIndex >= 1 && lCameraIndex <= lCameraIndexCount )
				{
					lNewCameraIndex = lCameraIndexArray[lCameraIndex-1];
					if( lNewCameraIndex != -1 )
					{
						lFCurve.SetValue(float(lNewCameraIndex));
					}
				}

				lKeyCount = lAnimCurve->KeyGetCount();
				for( int lKeyIter = 0; lKeyIter < lKeyCount; ++lKeyIter )
				{
					lCameraIndex = (int) lAnimCurve->KeyGetValue(lKeyIter);
					if( lCameraIndex >= 1 && lCameraIndex <= lCameraIndexCount )
					{
						lNewCameraIndex = lCameraIndexArray[lCameraIndex-1];
						if( lNewCameraIndex != -1 )
						{
							lAnimCurve->KeySetValue(lKeyIter, float(lNewCameraIndex));
						}
					}				
				}
			}
		}
	}
}


void FbxReaderFbx5::ReadGlobalLightSettings(FbxScene& pScene)
{
    pScene.GlobalLightSettings().RestoreDefaultSettings();
    ReadAmbientColor(pScene);
    ReadFogOption(pScene);
    ReadShadowPlane(pScene);
}


void FbxReaderFbx5::ReadShadowPlane(FbxScene& pScene)
{
    int                                lShadowPlaneCount;
    int                                lCount;
    FbxVector4                         lPlaneOrigin;
    FbxVector4                         lPlaneNormal;

    pScene.GlobalLightSettings().RemoveAllShadowPlanes();

    //
    // Determine if there's a ShadowPlanes Section in the File.
    //
    if (mFileObject->FieldReadBegin ( FIELD_KFBXGLOBALLIGHTSETTINGS_SHADOWPLANES ) == false)
    {
        return;
    }

    if (mFileObject->FieldReadBlockBegin())
	{
        //
        // Read the Shadow Plane Block.
        //
        lShadowPlaneCount  = mFileObject->FieldReadI ( FIELD_KFBXGLOBALLIGHTSETTINGS_COUNT );

        if (lShadowPlaneCount <= 0)
        {
            mFileObject->FieldReadBlockEnd ();

            return;
        }

        //
        // Read through all the Shadow Planes in the section...
        //
        for (lCount = 0; lCount < lShadowPlaneCount; lCount ++)
        {
            FbxGlobalLightSettings::ShadowPlane lShadowPlane;

            if (mFileObject->FieldReadBegin ( FIELD_KFBXGLOBALLIGHTSETTINGS_PLANE ))
			{
                mFileObject->FieldRead3D ( (double*) lPlaneOrigin.mData );
                mFileObject->FieldRead3D ( (double*) lPlaneNormal.mData );

                lShadowPlane.mEnable = mFileObject->FieldReadB();
                
                lShadowPlane.mOrigin = lPlaneOrigin;
                lShadowPlane.mNormal = lPlaneNormal;
            
                pScene.GlobalLightSettings().AddShadowPlane ( lShadowPlane );

				mFileObject->FieldReadEnd ();
			}

        }


        pScene.GlobalLightSettings().SetShadowEnable ( mFileObject->FieldReadB (FIELD_KFBXGLOBALLIGHTSETTINGS_USESHADOW) );

        pScene.GlobalLightSettings().SetShadowIntensity ( mFileObject->FieldReadD ( FIELD_KFBXGLOBALLIGHTSETTINGS_SHADOWINTENSITY ) );


		mFileObject->FieldReadBlockEnd ();
	}

    mFileObject->FieldReadEnd ();

}


void FbxReaderFbx5::ReadAmbientColor(FbxScene& pScene)
{
    double lColorVector[4]={0.0,0.0,0.0,0.0};
    FbxColor lColor;

    if (mFileObject->FieldReadBegin ( FIELD_KFBXGLOBALLIGHTSETTINGS_AMBIENTRENDER ) == false)
    {
        return;
    }

    if (mFileObject->FieldReadBlockBegin())
	{
        if (mFileObject->FieldReadBegin( FIELD_KFBXGLOBALLIGHTSETTINGS_AMBIENTLIGHTCOLOR ))
		{
            mFileObject->FieldRead4D ( lColorVector );

			mFileObject->FieldReadEnd (); 
		}

		mFileObject->FieldReadBlockEnd ();
	}

    mFileObject->FieldReadEnd (); 

    lColor.mRed      = lColorVector [0];
    lColor.mGreen    = lColorVector [1];
    lColor.mBlue     = lColorVector [2];
    lColor.mAlpha    = lColorVector [3];

    pScene.GlobalLightSettings().SetAmbientColor(lColor); // this one is obsolete but we still call it to stay sync.
    pScene.GetGlobalSettings().SetAmbientColor(lColor);
}


void FbxReaderFbx5::ReadFogOption(FbxScene& pScene)
{
    FbxColor       lColor;
    double         lColorVector[4]={0.0,0.0,0.0,0.0};

    //
    // Determine if there's a FogOption Section...
    //
    if (mFileObject->FieldReadBegin ( FIELD_KFBXGLOBALLIGHTSETTINGS_FOGOPTIONS ) == false)
    {
        return;
    }  
    
    if (mFileObject->FieldReadBlockBegin())
	{
        pScene.GlobalLightSettings().SetFogEnable(mFileObject->FieldReadB ( FIELD_KFBXGLOBALLIGHTSETTINGS_FOGENABLE ));
        pScene.GlobalLightSettings().SetFogMode((FbxGlobalLightSettings::EFogMode) mFileObject->FieldReadI(FIELD_KFBXGLOBALLIGHTSETTINGS_FOGMODE));
        pScene.GlobalLightSettings().SetFogDensity(mFileObject->FieldReadD(FIELD_KFBXGLOBALLIGHTSETTINGS_FOGDENSITY));
        pScene.GlobalLightSettings().SetFogStart(mFileObject->FieldReadD(FIELD_KFBXGLOBALLIGHTSETTINGS_FOGSTART));
        pScene.GlobalLightSettings().SetFogEnd(mFileObject->FieldReadD(FIELD_KFBXGLOBALLIGHTSETTINGS_FOGEND));

        if (mFileObject->FieldReadBegin( FIELD_KFBXGLOBALLIGHTSETTINGS_FOGCOLOR ))
		{
            mFileObject->FieldRead4D ( lColorVector );

			mFileObject->FieldReadEnd();
		}

        lColor.mRed     = lColorVector [0];
        lColor.mGreen   = lColorVector [1];
        lColor.mBlue    = lColorVector [2];

        pScene.GlobalLightSettings().SetFogColor(lColor);

		mFileObject->FieldReadBlockEnd();
	}

    mFileObject->FieldReadEnd();
}


void FbxReaderFbx5::ReadGobo(FbxScene& pScene)
{
    //
    // Determine if there's a GoboManager Section in the File.
    //
    if (mFileObject->FieldReadBegin ( FIELD_KFBXGOBO_GOBOMANAGER ) == false)
    {
        return;
    }   

    if (mFileObject->FieldReadBlockBegin())
	{
        ReadGoboSection ( pScene );

		mFileObject->FieldReadBlockEnd();
	}

    mFileObject->FieldReadEnd(); 
}

void FbxReaderFbx5::ReadGoboSection(FbxScene& pScene)
{
    FbxArray<FbxGobo*> lGoboArray;
    FbxArray<bool> lGoboMatched;
    int i;
	FbxGobo* lGobo = NULL;

    // Loop through all the gobos.
    while (mFileObject->FieldReadBegin ( FIELD_KFBXGOBO_GOBO ) )
    {
        if (mFileObject->FieldReadBlockBegin())
		{
            char lTmp[]="";
            lGobo = FbxNew< FbxGobo >(lTmp);

            ReadGobo(*lGobo);

            lGoboArray.Add(lGobo);
            lGoboMatched.Add(false);
            
			mFileObject->FieldReadBlockEnd ();
		}

        mFileObject->FieldReadEnd();
    }

    // Loop throught all the gobo references.
    while (mFileObject->FieldReadBegin( FIELD_KFBXGOBO_LIGHTGOBO ) )
    {
        FbxString lLightName = FbxObject::StripPrefix(mFileObject->FieldReadS ());
        FbxLight* lLight = NULL;

        FbxString lGoboName = FbxObject::StripPrefix(mFileObject->FieldReadS ());

        // Find the associated light.
        FbxNode* lNode = pScene.GetRootNode()->FindChild(lLightName.Buffer(), true);

        if (lNode && lNode->GetLight())
        {
            lLight = lNode->GetLight();
        }

        // Find the associated gobo.
        for (i = 0; i < lGoboArray.GetCount(); i++)
        {
            if (strcmp(lGoboArray[i]->mName, lGoboName.Buffer()) == 0)
            {
                lGobo = lGoboArray[i];
                break;
            }
        }

        // Associate gobo with light.
        if (lLight && lGobo)
        {           
			lLight->FileName.Set( lGobo->mFileName.Buffer() );
			lLight->DrawGroundProjection.Set( lGobo->mDrawGroundProjection );
			lLight->DrawVolumetricLight.Set( lGobo->mVolumetricLightProjection );
			lLight->DrawFrontFacingVolumetricLight.Set( lGobo->mFrontVolumetricLightProjection );

            lGoboMatched[i] = true;
        }

        mFileObject->FieldReadEnd ();
    }

    // Remove unmatched gobos if any.
    for (i = 0; i < lGoboArray.GetCount(); i++)
    {
        if (lGoboMatched[i] != true)
        {
			FBX_SAFE_DELETE(lGoboArray[i]);
        }
    }
}


void FbxReaderFbx5::ReadGobo(FbxGobo& pGobo)
{
    pGobo.mName = FbxObject::StripPrefix(mFileObject->FieldReadS(FIELD_KFBXGOBO_GOBONAME));
    pGobo.mFileName = mFileObject->FieldReadS(FIELD_KFBXGOBO_GOBOPATH);            
    
    if (mFileObject->FieldReadBegin(FIELD_KFBXGOBO_DRAWCOMPONENT))
	{
        pGobo.mDrawGroundProjection = mFileObject->FieldReadB();
        pGobo.mVolumetricLightProjection = mFileObject->FieldReadB();
        pGobo.mFrontVolumetricLightProjection = mFileObject->FieldReadB();

		mFileObject->FieldReadEnd();
	}
}

void FbxReaderFbx5::ReadCharacter(FbxScene& pScene)
{
    FbxString lCharacterName;
	int lInputIndex;
	FbxCharacter::EInputType lInputType;
	FbxArray<int> lInputIndices;
	FbxArray<FbxCharacter::EInputType> lInputTypes;
    int lSuffix = 0;

	if (!mFileObject->IsBeforeVersion6()) return;

    while (mFileObject->FieldReadBegin("CHARACTER"))
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			if (mFileObject->FieldReadBegin("CONSTRAINT"))
			{
				lCharacterName = FbxObject::StripPrefix(mFileObject->FieldReadS());
				mFileObject->FieldReadEnd();
			}
			else
			{
				lCharacterName = "Character";

				if (lSuffix > 0)
				{
					lCharacterName += lSuffix;
				}

				lSuffix++;
			}

			int lCharacterIndex = pScene.CreateCharacter(lCharacterName.Buffer());

			if (lCharacterIndex != -1)
			{
				int lTmp;
				FbxCharacter* lCharacter = pScene.GetCharacter(lCharacterIndex);
				ReadCharacter(*lCharacter, lTmp, lInputIndex);				
				lInputIndices.Add(lInputIndex);
				lInputTypes.Add((FbxCharacter::EInputType)lTmp);
			}

			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

	FBX_ASSERT(lInputIndices.GetCount() == pScene.GetCharacterCount());
	FBX_ASSERT(lInputTypes.GetCount() == pScene.GetCharacterCount());	

	int i, lCount = pScene.GetCharacterCount();

	for (i = 0; i < lCount; i++)
	{
		FbxCharacter* lCharacter = pScene.GetCharacter(i);
		lInputIndex = lInputIndices[i];
		lInputType  = lInputTypes[i];

		// File prior 5.6 doesn't have any controlsetplug
		// so we need to manually create them for each character
		if(lCharacter->GetControlSet().GetType() != FbxControlSet::eNone)
		{
			FbxString lCtrlRigName(lCharacter->GetName());
			lCtrlRigName += "_Ctrl";

			lInputIndex = pScene.CreateControlSetPlug( lCtrlRigName.Buffer() );
			FbxControlSetPlug *lCtrlSetPlug = pScene.GetControlSetPlug(lInputIndex); 
			lCharacter->GetControlSet().ToPlug(lCtrlSetPlug);
		}
		
		if (lInputType == FbxCharacter::eInputActor)
		{			
		}
		else if (lInputType == FbxCharacter::eInputCharacter)
		{
			if (lInputIndex >= 0 && lInputIndex < pScene.GetCharacterCount())
			{
				lCharacter->SetInput(FbxCharacter::eInputCharacter, pScene.GetCharacter(lInputIndex));
			}
		}
		else if (lInputType == FbxCharacter::eInputMarkerSet)
		{			
			lCharacter->SetInput(FbxCharacter::eInputMarkerSet, pScene.GetControlSetPlug(lInputIndex));			
		}
	}
}


void FbxReaderFbx5::ReadCharacter(FbxCharacter& pCharacter, int& pInputType, int& pInputIndex)
{
	if (!mFileObject->IsBeforeVersion6())
	{
		// Read the character properties
		ReadProperties(&pCharacter);
	}

    // Need to publish all the character property
	mFileObject->FieldReadB("CHARACTERIZE");
	mFileObject->FieldReadB("LOCK_XFORM");
	mFileObject->FieldReadB("LOCK_PICK");

	if(mFileObject->IsBeforeVersion6())
	{
		mFileObject->FieldReadC("CONSTRAINT");

		pInputType = mFileObject->FieldReadI("INPUTOUTPUTTYPE", pCharacter.GetInputType());

		if ((FbxCharacter::EInputType) pInputType == FbxCharacter::eInputActor)
		{
			pInputIndex = mFileObject->FieldReadI("ACTORINDEX", -1);
		}
		else if ((FbxCharacter::EInputType) pInputType == FbxCharacter::eInputCharacter)
		{
			pInputIndex = mFileObject->FieldReadI("CHARACTERINDEX", -1);
		}
		else
		{
			pInputIndex = -1;
		}
	}

    if(mFileObject->FieldReadBegin("REFERENCE"))
    {       
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLink(pCharacter, FbxCharacter::eReference);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("LEFT_FLOOR"))
    {       
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLink(pCharacter, FbxCharacter::eLeftFloor);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("RIGHT_FLOOR"))
    {       
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLink(pCharacter, FbxCharacter::eRightFloor);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("LEFT_HANDFLOOR"))
    {       
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLink(pCharacter, FbxCharacter::eLeftHandFloor);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("RIGHT_HANDFLOOR"))
    {       
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLink(pCharacter, FbxCharacter::eRightHandFloor);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("BASE"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupBase);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }
    
    if(mFileObject->FieldReadBegin("AUXILIARY"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupAuxiliary);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("SPINE"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupSpine);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("NECK"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupNeck);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("ROLL"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupRoll);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("SPECIAL"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupSpecial);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("LEFTHAND"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupLeftHand);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("RIGHTHAND"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupRightHand);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("LEFTFOOT"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupLeftFoot);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("RIGHTFOOT"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupRightFoot);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("PROPS"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupProps);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

   	if(mFileObject->IsBeforeVersion6())
	{
		if(mFileObject->FieldReadBegin("GAMEMODEPARENT"))
		{   
			if (mFileObject->FieldReadBlockBegin())
			{
				ReadCharacterLinkGroup(pCharacter, FbxCharacter::eGroupGameModeParent);
				mFileObject->FieldReadBlockEnd();
			}
			mFileObject->FieldReadEnd();
		}
    
		if(mFileObject->FieldReadBegin("FILTERSET"))
		{   
			if (mFileObject->FieldReadBlockBegin())
			{
				ReadFilterSet(pCharacter);
				mFileObject->FieldReadBlockEnd();
			}
			mFileObject->FieldReadEnd();
		}

		bool lFound = mFileObject->FieldReadBegin("CONTROLSET");
		if(!lFound) lFound = mFileObject->FieldReadBegin("MB_CONTROLSET");

		if(lFound)
		{   
			if (mFileObject->FieldReadBlockBegin())
			{
				ReadControlSet(pCharacter.GetControlSet());
				mFileObject->FieldReadBlockEnd();
			}
			mFileObject->FieldReadEnd();
		}
	}
}


void FbxReaderFbx5::ReadCharacterLinkGroup(FbxCharacter& pCharacter, int pCharacterGroupId)
{
	bool lBeforeDominus = true;

	if (mFileObject->FieldReadBegin("LINK"))
	{
		FbxString lName = FbxObject::StripPrefix(mFileObject->FieldReadC());
		mFileObject->FieldReadEnd();
		mFileObject->FieldReadResetPosition();

		if (!lName.IsEmpty())
		{
			lBeforeDominus = false;
		}
	}

	if (lBeforeDominus)
	{
		int i = 0, lCount = FbxCharacter::GetCharacterGroupCount((FbxCharacter::EGroupId) pCharacterGroupId);

		while(mFileObject->FieldReadBegin("LINK"))
		{
			if (mFileObject->FieldReadBlockBegin())
			{
				if(i < lCount) 
				{
					ReadCharacterLink(pCharacter, FbxCharacter::GetCharacterGroupElementByIndex((FbxCharacter::EGroupId) pCharacterGroupId, i));
				}

				mFileObject->FieldReadBlockEnd();
			}
			mFileObject->FieldReadEnd();

			i++;
		}
	}
	else
	{
		while(mFileObject->FieldReadBegin("LINK"))
		{
			FbxString lName = FbxObject::StripPrefix(mFileObject->FieldReadC());

			int lIndex;

			if (FbxCharacter::FindCharacterGroupIndexByName(lName.Buffer(), true, (FbxCharacter::EGroupId&) pCharacterGroupId, lIndex))
			{
				if (mFileObject->FieldReadBlockBegin())
				{
					ReadCharacterLink(pCharacter, FbxCharacter::GetCharacterGroupElementByIndex((FbxCharacter::EGroupId&) pCharacterGroupId, lIndex));

					mFileObject->FieldReadBlockEnd();
				}
			}

			mFileObject->FieldReadEnd();
		}
	}
}


void FbxReaderFbx5::ReadCharacterLink(FbxCharacter& pCharacter, int pCharacterNodeId)
{
    FbxString lName;
    FbxCharacterLink lCharacterLink;

	if(mFileObject->IsBeforeVersion6())
	{
		lName = mFileObject->FieldReadS("MODEL");

		if (!lName.IsEmpty())
		{
			FbxString lNodeName = FbxObject::StripPrefix(lName);			
			lCharacterLink.mNode = FindNode(lNodeName.Buffer());
		}
	}

	lName = mFileObject->FieldReadS("NAME");

    if (!lName.IsEmpty())
    {
        lCharacterLink.mTemplateName = lName;
    }

	//////////////////////////////////////////////////////////////////////////
	// character 6.0: Always read offset since nodes are not load at this time
	//
    // if(lCharacterLink.mNode || !lCharacterLink.mTemplateName.IsEmpty())
    {
        lCharacterLink.mOffsetT[0] = mFileObject->FieldReadD("TOFFSETX", 0.0);
        lCharacterLink.mOffsetT[1] = mFileObject->FieldReadD("TOFFSETY", 0.0);
        lCharacterLink.mOffsetT[2] = mFileObject->FieldReadD("TOFFSETZ", 0.0);
        lCharacterLink.mOffsetR[0] = mFileObject->FieldReadD("ROFFSETX", 0.0);
        lCharacterLink.mOffsetR[1] = mFileObject->FieldReadD("ROFFSETY", 0.0);
        lCharacterLink.mOffsetR[2] = mFileObject->FieldReadD("ROFFSETZ", 0.0);
        lCharacterLink.mOffsetS[0] = mFileObject->FieldReadD("SOFFSETX", 1.0);
        lCharacterLink.mOffsetS[1] = mFileObject->FieldReadD("SOFFSETY", 1.0);
        lCharacterLink.mOffsetS[2] = mFileObject->FieldReadD("SOFFSETZ", 1.0);

		lCharacterLink.mParentROffset[0] = mFileObject->FieldReadD("PARENTROFFSETX", 0.0);
 		lCharacterLink.mParentROffset[1] = mFileObject->FieldReadD("PARENTROFFSETY", 0.0);
 		lCharacterLink.mParentROffset[2] = mFileObject->FieldReadD("PARENTROFFSETZ", 0.0);
 
 		ReadCharacterLinkRotationSpace(lCharacterLink);
	}

    pCharacter.SetCharacterLink((FbxCharacter::ENodeId) pCharacterNodeId, lCharacterLink);
}
void FbxReaderFbx5::ReadCharacterLinkRotationSpace(FbxCharacterLink& pCharacterLink)
{
	if(mFileObject->FieldReadBegin("ROTATIONSPACE"))
	{
		pCharacterLink.mHasRotSpace = true;

		FbxVector4 lLimits;
		bool lActiveness[3];

		if (mFileObject->FieldReadBlockBegin())
		{
			mFileObject->FieldRead3D("PRE",pCharacterLink.mPreRotation.mData);
			mFileObject->FieldRead3D("POST",pCharacterLink.mPostRotation.mData);
			pCharacterLink.mAxisLen  = mFileObject->FieldReadD("AXISLEN");
			pCharacterLink.mRotOrder = mFileObject->FieldReadI("ORDER");

			lActiveness[0] = (mFileObject->FieldReadI("XMINENABLE") != 0);
			lActiveness[1] = (mFileObject->FieldReadI("YMINENABLE") != 0);
			lActiveness[2] = (mFileObject->FieldReadI("ZMINENABLE") != 0);
			pCharacterLink.mRLimits.SetMinActive(lActiveness[0],lActiveness[1],lActiveness[2]);

			lActiveness[0] = (mFileObject->FieldReadI("XMAXENABLE") != 0);
			lActiveness[1] = (mFileObject->FieldReadI("YMAXENABLE") != 0);
			lActiveness[2] = (mFileObject->FieldReadI("ZMAXENABLE") != 0);
			pCharacterLink.mRLimits.SetMaxActive(lActiveness[0],lActiveness[1],lActiveness[2]);

			mFileObject->FieldRead3D("MIN", lLimits.mData);
			pCharacterLink.mRLimits.SetMin(lLimits);

			mFileObject->FieldRead3D("MAX", lLimits.mData);
			pCharacterLink.mRLimits.SetMax(lLimits);

			mFileObject->FieldReadBlockEnd();
		}
		mFileObject->FieldReadEnd();
	}
	else
	{
		pCharacterLink.mHasRotSpace = false;
	}
}


void FbxReaderFbx5::ReadFilterSet(FbxCharacter& pCharacter)
{
    FbxString lName;
    int lMode;
    double lValue;
    double lMin;
    double lMax;
	
    while(mFileObject->FieldReadBegin("PARAM"))
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			lName = mFileObject->FieldReadS("NAME","PARAM");
			lMode = mFileObject->FieldReadI("MODE",0);
			lValue = mFileObject->FieldReadD("VALUE",0.0); // old store
			lValue = mFileObject->FieldReadD("VALUE0",lValue);
			lMin = mFileObject->FieldReadD("MIN",0.0);
			lMax = mFileObject->FieldReadD("MAX",0.0);

			
			char *lFbxCharacterPropertyName = NULL;
			char *lFbxCharacterPropertyModeName = NULL;
			int lPropertyIndex = 0 ;
			FbxCharacter::EPropertyUnit lPropertyUnit = FbxCharacter::ePropertyNoUnit;
			pCharacter.GetFbxCharacterPropertyFromHIKProperty(	lFbxCharacterPropertyName, lFbxCharacterPropertyModeName, 
																lPropertyUnit, lPropertyIndex,
																lName);
															
			if(lFbxCharacterPropertyName)
			{
				FbxProperty lFbxProperty = pCharacter.FindProperty(lFbxCharacterPropertyName);
				if(lFbxProperty.IsValid())
				{

					FbxDataType lType = lFbxProperty.GetPropertyDataType();
                    if(lType.GetType() == eFbxInt)
                    {
                        int lIntValue = static_cast<int>(lValue);
                        lFbxProperty.Set(lIntValue);
                    }
                    else if(lType.GetType() == eFbxBool)
                    {
                        bool lBoolValue = lValue > 0.0;
                        lFbxProperty.Set(lBoolValue);
                    }
					else if(lType.GetType() == eFbxDouble)
					{
						lFbxProperty.Set(lValue);
					}
					else if(lType.GetType() == eFbxEnum)
					{
						int lEnumValue = static_cast<int>(lValue);
						lFbxProperty.Set(lEnumValue);
					}
					else if(lType.GetType() == eFbxDouble3)
					{
						FbxDouble3 lOriginal;
						lOriginal = lFbxProperty.Get<FbxDouble3>();
						lOriginal[lPropertyIndex] = lValue;
						lFbxProperty.Set(lOriginal);
					}
				                    
					lFbxProperty.SetLimits(lMin,lMax);
				}
			}

			if(lFbxCharacterPropertyModeName)
			{
				FbxProperty lFbxPropertyMode = pCharacter.FindProperty(lFbxCharacterPropertyModeName);
				FbxDataType lType = lFbxPropertyMode.GetPropertyDataType();
				if(lFbxPropertyMode.IsValid())
				{
					if(lType.GetType() == eFbxBool)
					{
						bool lBoolMode = lMode == 1; 
						lFbxPropertyMode.Set(lBoolMode);
					}
					else if (lType.GetType() == eFbxEnum)
					{					
						lFbxPropertyMode.Set(lMode);
					}
					
				}
			}						
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }
}


void FbxReaderFbx5::ReadControlSet(FbxControlSet& pControlSet)
{
	pControlSet.SetType((FbxControlSet::EType) mFileObject->FieldReadI("TYPE", pControlSet.GetType()));
    pControlSet.SetLockTransform(mFileObject->FieldReadI("LOCK_XFORM", pControlSet.GetLockTransform()) ? true : false);
    pControlSet.SetLock3DPick(mFileObject->FieldReadI("LOCK_PICK", pControlSet.GetLock3DPick()) ? true : false);

    if(mFileObject->FieldReadBegin("REFERENCE"))
    {       
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadControlSetLink(pControlSet, FbxCharacter::eReference);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("BASE"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadControlSetLinkGroup(pControlSet, FbxCharacter::eGroupBase);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }
    
    if(mFileObject->FieldReadBegin("AUXILIARY"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadControlSetLinkGroup(pControlSet, FbxCharacter::eGroupAuxiliary);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("SPINE"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadControlSetLinkGroup(pControlSet, FbxCharacter::eGroupSpine);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("NECK"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadControlSetLinkGroup(pControlSet, FbxCharacter::eGroupNeck);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("ROLL"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadControlSetLinkGroup(pControlSet, FbxCharacter::eGroupRoll);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("SPECIAL"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadControlSetLinkGroup(pControlSet, FbxCharacter::eGroupSpecial);
			mFileObject->FieldReadBlockEnd();
		}

        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("LEFTHAND"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadControlSetLinkGroup(pControlSet, FbxCharacter::eGroupLeftHand);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("RIGHTHAND"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadControlSetLinkGroup(pControlSet, FbxCharacter::eGroupRightHand);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("LEFTFOOT"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadControlSetLinkGroup(pControlSet, FbxCharacter::eGroupLeftFoot);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("RIGHTFOOT"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			ReadControlSetLinkGroup(pControlSet, FbxCharacter::eGroupRightFoot);
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("EFFECTOR"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			while (mFileObject->FieldReadBegin("LINK"))
			{
				if (mFileObject->FieldReadBlockBegin())
				{
					ReadEffector(pControlSet);            
					mFileObject->FieldReadBlockEnd();
				}
				mFileObject->FieldReadEnd();
			}

			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if(mFileObject->FieldReadBegin("EFFECTOR_AUX1"))
    {   
        if (mFileObject->FieldReadBlockBegin())
		{
			while (mFileObject->FieldReadBegin("LINK"))
			{
				if (mFileObject->FieldReadBlockBegin())
				{
					ReadEffectorAux(pControlSet);            
					mFileObject->FieldReadBlockEnd();
				}
				mFileObject->FieldReadEnd();
			}

			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }
}


void FbxReaderFbx5::ReadControlSetLinkGroup(FbxControlSet& pControlSet, int pCharacterGroupId)
{
	bool lBeforeDominus = true;

	if (mFileObject->FieldReadBegin("LINK"))
	{
		FbxString lName = FbxObject::StripPrefix(mFileObject->FieldReadC());

		mFileObject->FieldReadEnd();
		mFileObject->FieldReadResetPosition();

		if (!lName.IsEmpty())
		{
			lBeforeDominus = false;
		}
	}

	if (lBeforeDominus)
	{
		int i = 0, lCount = FbxCharacter::GetCharacterGroupCount((FbxCharacter::EGroupId) pCharacterGroupId);

		while(mFileObject->FieldReadBegin("LINK"))
		{
			if (mFileObject->FieldReadBlockBegin())
			{
				if(i < lCount) 
				{
					ReadControlSetLink(pControlSet, FbxCharacter::GetCharacterGroupElementByIndex((FbxCharacter::EGroupId) pCharacterGroupId, i));
				}

				mFileObject->FieldReadBlockEnd();
			}
			mFileObject->FieldReadEnd();

			i++;
		}
	}
	else
	{
		while(mFileObject->FieldReadBegin("LINK"))
		{
			FbxString lName = FbxObject::StripPrefix(mFileObject->FieldReadC());

			int lIndex;

			if (FbxCharacter::FindCharacterGroupIndexByName(lName.Buffer(), true, (FbxCharacter::EGroupId&) pCharacterGroupId, lIndex))
			{
				if (mFileObject->FieldReadBlockBegin())
				{
					ReadControlSetLink(pControlSet, FbxCharacter::GetCharacterGroupElementByIndex((FbxCharacter::EGroupId&) pCharacterGroupId, lIndex));
					mFileObject->FieldReadBlockEnd();
				}
			}

			mFileObject->FieldReadEnd();
		}
	}
}


void FbxReaderFbx5::ReadControlSetLink(FbxControlSet& pControlSet, int pCharacterNodeId)
{
    FbxString lName;
    FbxControlSetLink lControlSetLink;
		
    lName = FbxObject::StripPrefix(mFileObject->FieldReadS("MODEL"));

    if (!lName.IsEmpty())
    {  
		lControlSetLink.mNode = FindNode(lName.Buffer());

    }

	lName = mFileObject->FieldReadS("NAME");

    if (!lName.IsEmpty())
    {
        lControlSetLink.mTemplateName = lName;
    }

    if (lControlSetLink.mNode || !lControlSetLink.mTemplateName.IsEmpty())
    {
        pControlSet.SetControlSetLink((FbxCharacter::ENodeId) pCharacterNodeId, lControlSetLink);
    }   
}


void FbxReaderFbx5::ReadEffector(FbxControlSet& pControlSet)
{
    FbxString lEffectorName = mFileObject->FieldReadS("NAME","Unknown");

    FbxEffector::ENodeId lEffectorNodeId = FbxControlSet::GetEffectorNodeId(lEffectorName.Buffer());

    if( lEffectorNodeId == FbxEffector::eNodeIdInvalid )
    {
        return;
    }

    FbxString lModelName =  FbxObject::StripPrefix(mFileObject->FieldReadS("MODEL"));
    
    if (lModelName.IsEmpty())
    {
        return; 
    }
	
    FbxNode* lNode = FindNode(lModelName.Buffer());

    if (!lNode)
    {
        return;
    }

    FbxEffector lEffector;

    pControlSet.GetEffector(lEffectorNodeId, &lEffector);

    lEffector.mNode = lNode;
    lEffector.mShow = mFileObject->FieldReadI("SHOW", lEffector.mShow) ? true : false;
    lEffector.mTActive = mFileObject->FieldReadI("TACTIVE", lEffector.mTActive) ? true : false;
    lEffector.mRActive = mFileObject->FieldReadI("RACTIVE", lEffector.mRActive) ? true : false;
    lEffector.mCandidateTActive = mFileObject->FieldReadI("CANDIDATE_TACTIVE", lEffector.mCandidateTActive) ? true : false;
    lEffector.mCandidateRActive = mFileObject->FieldReadI("CANDIDATE_RACTIVE", lEffector.mCandidateRActive) ? true : false;
    
    pControlSet.SetEffector(lEffectorNodeId, lEffector);
}


void FbxReaderFbx5::ReadEffectorAux(FbxControlSet& pControlSet)
{
	FbxString lEffectorName = mFileObject->FieldReadS("NAME","Unknown");

	FbxEffector::ENodeId lEffectorNodeId = FbxControlSet::GetEffectorNodeId(lEffectorName.Buffer());

	if( lEffectorNodeId == FbxEffector::eNodeIdInvalid )
	{
		return;
	}

	FbxString lModelName = mFileObject->FieldReadS("MODEL");
    
	if( lModelName.IsEmpty() )
	{
		return; 
	}

	FbxNode* lNode = FindNode(lModelName.Buffer());
   

	if( !lNode )
	{
		return;
	}
    
	pControlSet.SetEffectorAux(lEffectorNodeId, lNode);
}


int FbxReaderFbx5::ReadCharacterPose(FbxScene& pScene)
{
    FbxString lPoseName;
	FbxString lPoseType;

	int ret = 1;

	while (mFileObject->FieldReadBegin("Pose"))
	{
		lPoseName = FbxObject::StripPrefix(mFileObject->FieldReadS());

		if (mFileObject->FieldReadBlockBegin())
		{
			lPoseType = mFileObject->FieldReadS("Type");

			if (lPoseType.Compare("CharacterPose") == 0)
			{
				if (mFileObject->FieldReadBegin("PoseScene"))
				{
					if (mFileObject->FieldReadBlockBegin())
					{
						int lCharacterPoseIndex = pScene.CreateCharacterPose(lPoseName.Buffer());

						if (lCharacterPoseIndex != -1)
						{
							FbxCharacterPose* lCharacterPose = pScene.GetCharacterPose(lCharacterPoseIndex);
							
							if (!ReadCharacterPose(*lCharacterPose))
							{
								pScene.DestroyCharacterPose(lCharacterPoseIndex);
							}
						}

						mFileObject->FieldReadBlockEnd();
					}
					mFileObject->FieldReadEnd();
				}
			}
			else
			if (lPoseType.Compare("BindPose") == 0 ||
				lPoseType.Compare("RestPose") == 0)
			{
				bool isBindPose = (lPoseType.Compare("BindPose") == 0);
				FbxPose* lPose = FbxPose::Create(&mManager, lPoseName.Buffer());
				lPose->SetIsBindPose(isBindPose);
				
				if (lPose)
				{
					if (!ReadPose(pScene, lPose, isBindPose))
					{
						lPose->Destroy();
					}
					else
					{
						pScene.AddPose(lPose);
					}
				}
				ret = 0;
			}

			mFileObject->FieldReadBlockEnd();
		}
		mFileObject->FieldReadEnd();
	}

	return ret;
}


bool FbxReaderFbx5::ReadCharacterPose(FbxCharacterPose& pCharacterPose)
{
	bool lResult = false;

	FbxImporter* lImporter = FbxImporter::Create(&mManager, "");
    FbxIOSettings* lIOS = mManager.GetIOSettings();
    if (lIOS == NULL) lIOS = GetIOSettings();
    FBX_ASSERT(lIOS != NULL);
    lImporter->SetIOSettings( lIOS );

    // Keep original values
    bool bModel     = IOS_REF.GetBoolProp(IMP_FBX_MODEL,              false);
    bool bMaterial  = IOS_REF.GetBoolProp(IMP_FBX_MATERIAL,           false);
    bool bTexture   = IOS_REF.GetBoolProp(IMP_FBX_TEXTURE,            false);
    bool bShape     = IOS_REF.GetBoolProp(IMP_FBX_SHAPE,              false);
    bool bGobo      = IOS_REF.GetBoolProp(IMP_FBX_GOBO,               false);
    bool bPivot     = IOS_REF.GetBoolProp(IMP_FBX_PIVOT,              false);
    bool bAnimation = IOS_REF.GetBoolProp(IMP_FBX_ANIMATION,          false);
    bool bSettings  = IOS_REF.GetBoolProp(IMP_FBX_GLOBAL_SETTINGS,    false);


    IOS_REF.SetBoolProp(IMP_FBX_MODEL,			false);
    IOS_REF.SetBoolProp(IMP_FBX_MATERIAL,		false);
    IOS_REF.SetBoolProp(IMP_FBX_TEXTURE,			false);
    IOS_REF.SetBoolProp(IMP_FBX_SHAPE,			false);
    IOS_REF.SetBoolProp(IMP_FBX_GOBO,			false);
    IOS_REF.SetBoolProp(IMP_FBX_PIVOT,			false);
    IOS_REF.SetBoolProp(IMP_FBX_ANIMATION,       false);
    IOS_REF.SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, false);

	lResult = lImporter->Import(pCharacterPose.GetPoseScene(), mFileObject);

    // Restore original values
    IOS_REF.SetBoolProp(IMP_FBX_MODEL,              bModel     );
    IOS_REF.SetBoolProp(IMP_FBX_MATERIAL,           bMaterial  );
    IOS_REF.SetBoolProp(IMP_FBX_TEXTURE,            bTexture   );
    IOS_REF.SetBoolProp(IMP_FBX_SHAPE,              bShape     );
    IOS_REF.SetBoolProp(IMP_FBX_GOBO,               bGobo      );
    IOS_REF.SetBoolProp(IMP_FBX_PIVOT,              bPivot     );
    IOS_REF.SetBoolProp(IMP_FBX_ANIMATION,          bAnimation );
    IOS_REF.SetBoolProp(IMP_FBX_GLOBAL_SETTINGS,    bSettings  );

	lImporter->Destroy();

	return lResult;
}

void FbxReaderFbx5::ReadPose(FbxScene& pScene)
{
    FbxString lPoseName;
	FbxString lPoseType;

	while (mFileObject->FieldReadBegin("Pose"))
	{
		lPoseName = FbxObject::StripPrefix(mFileObject->FieldReadS());

		if (mFileObject->FieldReadBlockBegin())
		{
			lPoseType = mFileObject->FieldReadS("Type");

			bool isBindPose = (lPoseType.Compare("BindPose") == 0);
			if (isBindPose || lPoseType.Compare("RestPose") == 0)
			{
				FbxPose* lPose = FbxPose::Create(&mManager, lPoseName.Buffer());
				lPose->SetIsBindPose(isBindPose);

				if (!ReadPose(pScene, lPose, isBindPose))
				{
					lPose->Destroy();
				}					
				else
				{
					pScene.AddPose(lPose);
				}
			}

			mFileObject->FieldReadBlockEnd();
		}

		mFileObject->FieldReadEnd();
	}
}


bool FbxReaderFbx5::ReadPose(FbxScene& pScene, FbxPose* pPose, bool pAsBindPose)
{
	FbxMatrix identity;

	bool lResult = true;
	int nbNodes = mFileObject->FieldReadI("NbPoseNodes");
	for (int i = 0; i < nbNodes; i++)
	{
		if (mFileObject->FieldReadBegin("PoseNode"))
		{
			if (mFileObject->FieldReadBlockBegin())
			{
				bool local = false;
				FbxMatrix m;
				FbxString lNodeName = mFileObject->FieldReadS("Node");
				mFileObject->FieldReadDn("Matrix", &m.mData[0][0], &identity.mData[0][0], 16);
				if (!pAsBindPose) // for bindPoses the matrix is always global
				{				
					local = mFileObject->FieldReadI("Local") != 0;
				}

				// add an entry to the pose object.
				if (pPose)
				{
					FbxNode* lNode = FbxCast<FbxNode>(mObjectMap.Get(mObjectMap.Find(lNodeName)));
					if (lNode) {
						int index = pPose->Add(lNode, m, local);
					}
				}

				mFileObject->FieldReadBlockEnd();
			}
			mFileObject->FieldReadEnd();
		}
	}

	return lResult;
}

void FbxReaderFbx5::ReadGlobalTimeSettings(FbxScene& pScene)
{
	FbxGlobalSettings& lGlobalSettings = pScene.GetGlobalSettings();

    if (mFileObject->FieldReadBegin(FIELD_KFBXGLOBALCAMERASETTINGS_SETTINGS))
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			//
			// Check for new framerate variable
			//
			FbxString lFrameRate     = mFileObject->FieldReadC ( FIELD_KFBXGLOBALTIMESETTINGS_FRAMERATE, "0.0" );
			int    lTimeModeValue = 0;
			if ( lFrameRate == "0.0" )
			{
				lTimeModeValue = FbxGetTimeModeFromOldValue ((FbxTime::EOldMode) mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_TIME_MODE, lGlobalSettings.GetTimeMode()) );
			}
			else
			{
				lTimeModeValue = FbxGetTimeModeFromFrameRate ( lFrameRate.Buffer() );
			}

			lGlobalSettings.SetTimeMode((FbxTime::EMode)lTimeModeValue );
			lGlobalSettings.SetTimeProtocol((FbxTime::EProtocol) mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_TIME_PROTOCOL, lGlobalSettings.GetTimeProtocol()));         
			lGlobalSettings.SetSnapOnFrameMode((FbxGlobalSettings::ESnapOnFrameMode) mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_SNAP_ON_FRAMES, lGlobalSettings.GetSnapOnFrameMode()));              
        
			int lTimeMarkerCount = mFileObject->FieldGetInstanceCount(FIELD_KFBXGLOBALTIMESETTINGS_REFERENCE_TIME_MARKER);
            lGlobalSettings.RemoveAllTimeMarkers();
			for (int i = 0; i < lTimeMarkerCount; i++)
			{
				FbxGlobalSettings::TimeMarker lMarker;
        
				if (mFileObject->FieldReadBegin(FIELD_KFBXGLOBALTIMESETTINGS_REFERENCE_TIME_MARKER))
				{
					lMarker.mName = FbxObject::StripPrefix(mFileObject->FieldReadC());

					if (mFileObject->FieldReadBlockBegin())
					{
						lMarker.mTime = mFileObject->FieldReadT(FIELD_KFBXGLOBALTIMESETTINGS_REFERENCE_TIME);
						lMarker.mLoop = (mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_REFERENCE_LOOP, lMarker.mLoop) == 0 ? false : true);
                
						mFileObject->FieldReadBlockEnd();

						lGlobalSettings.AddTimeMarker(lMarker);
					}

					mFileObject->FieldReadEnd();
				}
            
				if (lGlobalSettings.GetTimeMarkerCount() != 0)
				{
					lGlobalSettings.SetCurrentTimeMarker(mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_REFERENCE_TIME_INDEX, lGlobalSettings.GetCurrentTimeMarker()));           
				}
			}
    
			mFileObject->FieldReadBlockEnd();
		}

        mFileObject->FieldReadEnd();
    }
}

void FbxReaderFbx5::ReadGlobalCameraSettings(FbxScene& pScene)
{
    FbxGlobalCameraSettings& lGlobalCameraSettings = pScene.GlobalCameraSettings();
	// we don't restore the global settings because we have copied the producer cameras
	// properties from the main object section to the globalCameraSetting cameras. 
	// Anyway, here we simply read the default camera and the viewing mode so there is
	// no danger in not resetting the object.
    if (mFileObject->FieldReadBegin(FIELD_KFBXGLOBALCAMERASETTINGS_RENDERER_SETTINGS))
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			FbxString lDefaultCamera = mFileObject->FieldReadC(FIELD_KFBXGLOBALCAMERASETTINGS_DEFAULT_CAMERA, pScene.GetGlobalSettings().GetDefaultCamera());

			lDefaultCamera = FbxObject::StripPrefix(lDefaultCamera);

			lDefaultCamera = ConvertCameraName(lDefaultCamera);
			
            pScene.GetGlobalSettings().SetDefaultCamera(lDefaultCamera.Buffer());
			lGlobalCameraSettings.SetDefaultCamera(lDefaultCamera.Buffer()); // this one is obsolete but we keep it to stay sync
			lGlobalCameraSettings.SetDefaultViewingMode((FbxGlobalCameraSettings::EViewingMode) mFileObject->FieldReadI(FIELD_KFBXGLOBALCAMERASETTINGS_DEFAULT_VIEWING_MODE, lGlobalCameraSettings.GetDefaultViewingMode()));

			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }
}



void FbxReaderFbx5::ReadGlobalCameraAndTimeSettings(FbxScene& pScene)
{
	// This method is called only in pre v6 files
    int i;

    FbxGlobalCameraSettings& lGlobalCameraSettings = pScene.GlobalCameraSettings();
    lGlobalCameraSettings.RestoreDefaultSettings();

    if (mFileObject->FieldReadBegin(FIELD_KFBXGLOBALCAMERASETTINGS_RENDERER_SETTINGS))
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			FbxString lDefaultCamera = mFileObject->FieldReadC(FIELD_KFBXGLOBALCAMERASETTINGS_DEFAULT_CAMERA, lGlobalCameraSettings.GetDefaultCamera());

			lDefaultCamera = FbxObject::StripPrefix(lDefaultCamera);

			lDefaultCamera = ConvertCameraName(lDefaultCamera);
			
			lGlobalCameraSettings.SetDefaultCamera(lDefaultCamera.Buffer());
			lGlobalCameraSettings.SetDefaultViewingMode((FbxGlobalCameraSettings::EViewingMode) mFileObject->FieldReadI(FIELD_KFBXGLOBALCAMERASETTINGS_DEFAULT_VIEWING_MODE, lGlobalCameraSettings.GetDefaultViewingMode()));

			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    if (mFileObject->FieldReadBegin(FIELD_KFBXGLOBALCAMERASETTINGS_SETTINGS))
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			FbxString lCameraName;
			int lCameraCount = mFileObject->FieldGetInstanceCount(FIELD_KFBXGLOBALCAMERASETTINGS_CAMERA);

			for (i = 0; i < lCameraCount; i++)
			{
				if (mFileObject->FieldReadBegin(FIELD_KFBXGLOBALCAMERASETTINGS_CAMERA))
				{
					lCameraName = FbxObject::StripPrefix(mFileObject->FieldReadC());
					lCameraName = ConvertCameraName(lCameraName);					
                    
					if (mFileObject->FieldReadBlockBegin())
					{
						if (lCameraName.Compare(FBXSDK_CAMERA_PERSPECTIVE) == 0)
						{
							if (pScene.GlobalCameraSettings().GetCameraProducerPerspective()) {
								ReadCamera(*lGlobalCameraSettings.GetCameraProducerPerspective());
							}
						}
						else if (lCameraName.Compare(FBXSDK_CAMERA_TOP) == 0)
						{
							if (pScene.GlobalCameraSettings().GetCameraProducerTop()) {
								ReadCamera(*lGlobalCameraSettings.GetCameraProducerTop());
							}
						}
						else if (lCameraName.Compare(FBXSDK_CAMERA_BOTTOM) == 0)
						{
							if (pScene.GlobalCameraSettings().GetCameraProducerBottom()) {
								ReadCamera(*lGlobalCameraSettings.GetCameraProducerBottom());
							}
						}
						else if (lCameraName.Compare(FBXSDK_CAMERA_FRONT) == 0)
						{
							if (pScene.GlobalCameraSettings().GetCameraProducerFront()) {
								ReadCamera(*lGlobalCameraSettings.GetCameraProducerFront());
							}
						}
						else if (lCameraName.Compare(FBXSDK_CAMERA_BACK) == 0)
						{
							if (pScene.GlobalCameraSettings().GetCameraProducerBack()) {
								ReadCamera(*lGlobalCameraSettings.GetCameraProducerBack());
							}
						}
						else if (lCameraName.Compare(FBXSDK_CAMERA_RIGHT) == 0)
						{
							if (pScene.GlobalCameraSettings().GetCameraProducerRight()) {
								ReadCamera(*lGlobalCameraSettings.GetCameraProducerRight());
							}
						}
						else if (lCameraName.Compare(FBXSDK_CAMERA_LEFT) == 0)
						{
							if (pScene.GlobalCameraSettings().GetCameraProducerLeft()) {
								ReadCamera(*lGlobalCameraSettings.GetCameraProducerLeft());
							}
						}

						mFileObject->FieldReadBlockEnd ();
					}

					mFileObject->FieldReadEnd ();
				}
			}
			//
			// Check for new framerate variable
			//
            FbxGlobalSettings& lGlobalSettings = pScene.GetGlobalSettings();
			FbxString lFrameRate     = mFileObject->FieldReadC ( FIELD_KFBXGLOBALTIMESETTINGS_FRAMERATE, "0.0" );
			int    lTimeModeValue = 0;
			if ( lFrameRate == "0.0" )
			{
				lTimeModeValue = FbxGetTimeModeFromOldValue ((FbxTime::EOldMode) mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_TIME_MODE, lGlobalSettings.GetTimeMode()) );
			}
			else
			{
				lTimeModeValue = FbxGetTimeModeFromFrameRate ( lFrameRate.Buffer() );
			}

			lGlobalSettings.SetTimeMode( (FbxTime::EMode)lTimeModeValue );         
			lGlobalSettings.SetTimeProtocol((FbxTime::EProtocol) mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_TIME_PROTOCOL, lGlobalSettings.GetTimeProtocol()));         
			lGlobalSettings.SetSnapOnFrameMode((FbxGlobalSettings::ESnapOnFrameMode) mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_SNAP_ON_FRAMES, lGlobalSettings.GetSnapOnFrameMode()));              
        
			int lTimeMarkerCount = mFileObject->FieldGetInstanceCount(FIELD_KFBXGLOBALTIMESETTINGS_REFERENCE_TIME_MARKER);
            lGlobalSettings.RemoveAllTimeMarkers();
			for (i = 0; i < lTimeMarkerCount; i++)
			{
				FbxGlobalSettings::TimeMarker lMarker;
        
				if (mFileObject->FieldReadBegin(FIELD_KFBXGLOBALTIMESETTINGS_REFERENCE_TIME_MARKER))
				{
					lMarker.mName = FbxObject::StripPrefix(mFileObject->FieldReadC());

					if (mFileObject->FieldReadBlockBegin())
					{
						lMarker.mTime = mFileObject->FieldReadT(FIELD_KFBXGLOBALTIMESETTINGS_REFERENCE_TIME);
						lMarker.mLoop = (mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_REFERENCE_LOOP, lMarker.mLoop) == 0 ? false : true);
                
						mFileObject->FieldReadBlockEnd();

						lGlobalSettings.AddTimeMarker(lMarker);
					}

					mFileObject->FieldReadEnd();
				}
            
				if (lGlobalSettings.GetTimeMarkerCount() != 0)
				{
					lGlobalSettings.SetCurrentTimeMarker(mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_REFERENCE_TIME_INDEX, lGlobalSettings.GetCurrentTimeMarker()));           
				}
			}
    
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }
}


void FbxReaderFbx5::RemoveDuplicateTextures(FbxScene& pScene)
{
    int i, j, k, lTextureLayerCount, lTextureCount, lGeometryCount;
    FbxArray<FbxTexture*> lTextureDuplicate;
    FbxArray<FbxTexture*> lTextureReplacement;

    // Scan texture array in entity for duplicate textures.

    lTextureCount = pScene.GetTextureCount();

    for (i = 0; i < lTextureCount; i++)
    {
        FbxFileTexture* lTextureA = FbxCast<FbxFileTexture>(pScene.GetTexture(i));

        for (j = lTextureCount - 1; j > i; j--)
        {
            FbxFileTexture* lTextureB = FbxCast<FbxFileTexture>(pScene.GetTexture(j));

            if (*lTextureB == *lTextureA)
            {
				// Since lTextureB is going to be deleted, make sure we don't
				// reference it anymore in the mObjectMap
				FbxString name = FbxString(lTextureB->GetClassId().GetObjectTypePrefix()) + lTextureB->GetName();
				mObjectMap.Remove(mObjectMap.Find(name));

                pScene.RemoveTexture(lTextureB);
                lTextureB->Destroy();
                lTextureDuplicate.Add(lTextureB);
                lTextureReplacement.Add(lTextureA);
            }
        }

        // Recompute texture count in case one or more textures were removed.
        lTextureCount = pScene.GetTextureCount();
    }

    // Scan geometries in scene to replace duplicate textures.
    lGeometryCount = pScene.GetGeometryCount();

    for (i = 0; i < lGeometryCount; i++)
    {
		FbxGeometry* lGeometry = pScene.GetGeometry(i);

		if (lGeometry)
		{
			lTextureLayerCount = lGeometry->GetLayerCount(FbxLayerElement::eTextureDiffuse);

			for (k=0; k<lTextureLayerCount; k++)
			{
				FbxLayerElementTexture* lTextureLayer = lGeometry->GetLayer(k, FbxLayerElement::eTextureDiffuse)->GetTextures(FbxLayerElement::eTextureDiffuse);
				lTextureCount = lTextureLayer->GetDirectArray().GetCount();

				for (j = 0; j < lTextureCount; j++)
				{
					int lReplacementIndex = lTextureDuplicate.Find(lTextureLayer->GetDirectArray().GetAt(j));

					if (lReplacementIndex != -1)
					{
						lTextureLayer->GetDirectArray().SetAt(j, lTextureReplacement[lReplacementIndex]);  
					}
				}
			}
        }
    }
}


void FbxReaderFbx5::RemoveDuplicateMaterials(FbxScene& pScene)
{
    int i, j, k, lMaterialLayerCount, lMaterialCount, lGeometryCount;
    FbxArray<FbxSurfaceMaterial*> lMaterialDuplicate;
    FbxArray<FbxSurfaceMaterial*> lMaterialReplacement;

    // Scan material array in entity for duplicate materials.

    lMaterialCount = pScene.GetMaterialCount();

    for (i = 0; i < lMaterialCount; i++)
    {
        FbxSurfaceMaterial* lMaterialA = pScene.GetMaterial(i);

        for (j = lMaterialCount - 1; j > i; j--)
        {
            FbxSurfaceMaterial* lMaterialB = pScene.GetMaterial(j);

            if (*lMaterialB == *lMaterialA)
            {
                // Since lMaterialB is going to be deleted, make sure we don't
                // reference it anymore in the mObjectMap
                FbxString name = FbxString(lMaterialB->GetClassId().GetObjectTypePrefix()) + lMaterialB->GetName();
                mObjectMap.Remove(mObjectMap.Find(name));

                pScene.RemoveMaterial(lMaterialB);
                lMaterialB->Destroy();
                lMaterialDuplicate.Add(lMaterialB);
                lMaterialReplacement.Add(lMaterialA);
            }
        }

        // Recompute material count in case one or more materials were removed.
        lMaterialCount = pScene.GetMaterialCount();
    }

    // Scan geometries in scene to replace duplicate materials.
    lGeometryCount = pScene.GetGeometryCount();

    for (i = 0; i < lGeometryCount; i++)
    {
        FbxGeometry* lGeometry = pScene.GetGeometry(i);

        if (lGeometry)
        {
            lMaterialLayerCount = lGeometry->GetLayerCount(FbxLayerElement::eMaterial);

            for (k = 0; k < lMaterialLayerCount; k++)
            {
                FbxLayerElementMaterial* lMaterialLayer = lGeometry->GetLayer(k, FbxLayerElement::eMaterial)->GetMaterials();
                lMaterialCount = lMaterialLayer->mDirectArray->GetCount();

                for (j = 0; j < lMaterialCount; j++)
                {
                    int lReplacementIndex = lMaterialDuplicate.Find(lMaterialLayer->mDirectArray->GetAt(j));

                    if (lReplacementIndex != -1)
                    {
                        lMaterialLayer->mDirectArray->SetAt(j, lMaterialReplacement[lReplacementIndex]);
                    }
                }
            }
        }
    }
}


bool FbxReaderFbx5::ReadMedia(FbxScene& pScene, const char* pEmbeddedMediaDirectory /* = "" */)
{
    FbxArray<FbxString*> lMediaNames;
    FbxArray<FbxString*> lFileNames;
    int i, lTextureCount;

    // Read list of media names and associated file names.
	if (mFileObject->IsBeforeVersion6())
	{
		if (mFileObject->FieldReadBegin(FIELD_MEDIA_MEDIA)) 
		{
			if (mFileObject->FieldReadBlockBegin())
			{
				if (mFileObject->FieldReadBegin(FIELD_MEDIA_VIDEO))
				{
					if (mFileObject->FieldReadBlockBegin())
					{
						while (mFileObject->FieldReadBegin(FIELD_MEDIA_VIDEO) == true)
						{
							FbxString lNewMediaName = FbxObject::StripPrefix(mFileObject->FieldReadS());

							// Duplicate media names are not allowed.
							if (!lNewMediaName.IsEmpty() && FindString(lNewMediaName, lMediaNames) == -1)
							{
								if (mFileObject->FieldReadBlockBegin ())
								{
									FbxString lNewMediaType = mFileObject->FieldReadC(FIELD_MEDIA_TYPE);

									// The only type of media currently supported.
									if (lNewMediaType.Compare(TOKEN_MEDIA_CLIP) == 0)
									{
										FbxString lNewFileName = ReadMediaClip(pEmbeddedMediaDirectory);

										if (!lNewFileName.IsEmpty())
										{
											lMediaNames.Add(FbxNew< FbxString >(lNewMediaName));
											lFileNames.Add(FbxNew< FbxString >(lNewFileName));
										}
									}

									mFileObject->FieldReadBlockEnd();
								}
							}

							mFileObject->FieldReadEnd();
						}

						mFileObject->FieldReadBlockEnd ();
					}
					mFileObject->FieldReadEnd ();
				}

				mFileObject->FieldReadBlockEnd ();
			}
			mFileObject->FieldReadEnd ();
		}
	}

    // Set file names in textures. 

    lTextureCount = pScene.GetTextureCount();

    for (i = 0; i < lTextureCount; i++)
    {
        FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(pScene.GetTexture(i));
		if (lTexture)
		{

			int lMediaIndex = FindString(lTexture->GetMediaName(), lMediaNames);

			if (lMediaIndex != -1)
			{
				FbxString lCorrectedFileName(FbxPathUtils::Clean(lFileNames[lMediaIndex]->Buffer()));
				lTexture->SetFileName(lCorrectedFileName.Buffer());
				
				if (FbxString(lTexture->GetRelativeFileName()) == "")
				{
					lTexture->SetRelativeFileName(mFileObject->GetRelativeFilePath(lCorrectedFileName.Buffer()));
				}

                if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true))
                {
                    // Check if the "absolute" path of the texture exist
                    // If the "absolute" path of the texture is not found BUT the "relative" path is found
                    // replace the "absolute" path of the texture, then if we later write this scene in a file, the "absolute" path exist.
                    // This can occur when a FBX file and "relative" texture are moved.
                    if( FbxFileUtils::Exist( lTexture->GetFileName() ) == false)
                    {
                        FbxString lNewAbsolutePath = mFileObject->GetFullFilePath( lTexture->GetRelativeFileName() );
                        lNewAbsolutePath = FbxPathUtils::Clean( lNewAbsolutePath );
                        if( FbxFileUtils::Exist( lNewAbsolutePath ) )
                        {
						    // Set with a valid "absolute" path...only if lNewAbsolutePath is not a folder
							const char* pFile = lNewAbsolutePath.Buffer();
							if (!FbxPathUtils::Exist( pFile ))
								lTexture->SetFileName( pFile );
                        }
                    }
               }
			}
		}
    }

    // Set file names in cameras.
	FbxCamera*					lCamera = NULL;
	FbxIteratorSrc<FbxCamera>	lCameraIter(&pScene);
	FbxForEach(lCameraIter, lCamera)
	{
        int lMediaIndex = FindString(lCamera->GetBackgroundMediaName(), lMediaNames);
        if( lMediaIndex != -1 )
        {
			FbxString lCorrectedFileName(FbxPathUtils::Clean(lFileNames[lMediaIndex]->Buffer()));
			lCamera->SetBackgroundFileName(lCorrectedFileName.Buffer());
        }
	}

	// and while at it, do the same on the lights that have gobos
	FbxLight*					lLight;
	FbxIteratorSrc<FbxLight>	lLightIter(&pScene);
	FbxForEach(lLightIter, lLight)
	{
        int lMediaIndex = FindString(lLight->FileName.Get(), lMediaNames);
        if( lMediaIndex != -1 )
        {
			FbxString lCorrectedFileName(FbxPathUtils::Clean(lFileNames[lMediaIndex]->Buffer()));
			lLight->FileName.Set(lCorrectedFileName.Buffer());
		}
	}

    // Delete local lists.
    FbxArrayDelete(lMediaNames);
    FbxArrayDelete(lFileNames);

    return true;
}

FbxString FbxReaderFbx5::ReadMediaClip(const char* pEmbeddedMediaDirectory)
{
	int lVersion = mFileObject->FieldReadI(FIELD_MEDIA_VERSION, 100);
	FbxString lOriginalFilename;
	bool    lOriginalFormat = true;

	if (lVersion > 100)
	{
		lOriginalFormat = mFileObject->FieldReadB(FIELD_MEDIA_ORIGINAL_FORMAT, true);
		lOriginalFilename = mFileObject->FieldReadS(FIELD_MEDIA_ORIGINAL_FILENAME);
	}

    FbxString lFileName = mFileObject->FieldReadS(FIELD_MEDIA_FILENAME);
    FbxString lRelativeFileName = mFileObject->FieldReadS(FIELD_MEDIA_RELATIVE_FILENAME);

    // If this field exist, the media is embedded.
    if (mFileObject->FieldReadBegin(FIELD_MEDIA_CONTENT))
    {
        bool lStatus = true;
        if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true))
            lStatus = mFileObject->FieldReadEmbeddedFile (lFileName, lRelativeFileName);

        mFileObject->FieldReadEnd();

        if (lStatus)
        {
            return lFileName;
        }
    }

    // The media is either not embedded or it couldn't be opened.
    if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true))
        return FindFile(lFileName, lRelativeFileName);
    else
        return true;
}

FbxNode* FbxReaderFbx5::FindNode (char* pName)
{
    return (FbxNode*)mNodeArrayName.Get (pName);   
}


bool FbxReaderFbx5::ReadNode (FbxNode& pNode)
{
	int lNodeVersion = mFileObject->FieldReadI(FIELD_KFBXNODE_VERSION, 100);

	if(lNodeVersion < 232)
	{
		pNode.mCorrectInheritType = true;				
	}

    ReadNodeShading (pNode);
    ReadNodeCullingType (pNode);

    ReadNodeTarget (pNode);
    ReadNodeChildrenName (pNode);
    ReadNodeAttribute (pNode);

	// New properties
	// Those properties replace the old pivots and the old limits.
	// So if we find a file containing old pivots and/or old limits,
	// and properties, the properties will overwrite previous values
	if (!mFileObject->IsBeforeVersion6()) 
	{
		ReadNodeProperties(pNode);		
	}

	else
	{
		// Old limits
		ReadNodeLimits (pNode);

		// Old pivots
        if(IOS_REF.GetBoolProp(IMP_FBX_PIVOT, true))
		{
			ReadNodePivots (pNode);
		}
	}

	ReadNodeDefaultAttributes(pNode);
    return true;
}

bool FbxReaderFbx5::ReadGenericNode(FbxGenericNode& pNode)
{
	mFileObject->FieldReadI(FIELD_KFBXGENERICNODE_VERSION, 100);
	return true;
}

bool FbxReaderFbx5::ReadNodeChildrenName (FbxNode& pNode)
{
    if (mFileObject->FieldReadBegin (FIELD_KFBXNODE_CHILDREN))
    {
        FbxString lName = FbxObject::StripPrefix(mFileObject->FieldReadS ());
		char*   pName = lName.Buffer();

        while (strlen (pName) > 0)
        {
            pNode.AddChildName (pName);
			lName = FbxObject::StripPrefix(mFileObject->FieldReadS ());
            pName = lName.Buffer();
        }

        mFileObject->FieldReadEnd ();
    }
    return true;
}


bool FbxReaderFbx5::ReadNodeShading (FbxNode& pNode)
{
    //
    // Retrieve The Hidden and Shading Informations...
    //
    pNode.SetVisibility(true);
    pNode.Show.Set(true);

    if (mFileObject->FieldReadBegin (FIELD_KFBXNODE_HIDDEN))
    {
        FbxString lHidden = mFileObject->FieldReadC ();

        if (FBXSDK_stricmp(lHidden.Buffer(), "True") == 0) 
        {
            pNode.SetVisibility (false);
            pNode.Show.Set(false);
        }

        mFileObject->FieldReadEnd();
    }

    // Set default value.
    pNode.SetShadingMode(FbxNode::eHardShading);

    if(mFileObject->FieldReadBegin (FIELD_KFBXNODE_SHADING)) 
    {
        char Shading = mFileObject->FieldReadCH ();

        switch (Shading) 
        {
            case 'W':   
                pNode.SetShadingMode(FbxNode::eWireFrame);
                break;

            case 'F':   
                pNode.SetShadingMode(FbxNode::eFlatShading);
                break;
                
            case 'Y':   
                pNode.SetShadingMode(FbxNode::eLightShading);
                break;

            case 'T':
                pNode.SetShadingMode(FbxNode::eTextureShading);
                break;

            case 'U':   
                pNode.SetShadingMode(FbxNode::eFullShading);
                break;
        }

        mFileObject->FieldReadEnd ();
    }

    return true;
}


bool FbxReaderFbx5::ReadNodeCullingType (FbxNode& pNode)
{
    FbxString lCullingType = mFileObject->FieldReadC(FIELD_KFBXNODE_CULLING_TYPE);

    if (lCullingType.Compare(TOKEN_KFBXNODE_CULLING_OFF) == 0)
    {
        pNode.mCullingType = FbxNode::eCullingOff;
    }
    else if(lCullingType.Compare(TOKEN_KFBXNODE_CULLING_ON_CCW) == 0)
    {
        pNode.mCullingType = FbxNode::eCullingOnCCW;
    }
    else if(lCullingType.Compare(TOKEN_KFBXNODE_CULLING_ON_CW) == 0)
    {
        pNode.mCullingType = FbxNode::eCullingOnCW;
    }
    else
    {
        pNode.mCullingType = FbxNode::eCullingOff;
    }

    return true;
}


bool FbxReaderFbx5::ReadNodeLimits ( FbxNode& pNode )
{
    if(mFileObject->FieldReadBegin(FIELD_KFBXNODE_LIMITS))
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			FbxLimitsUtilities lLimits(&pNode);
            FbxVector4 lVector, lVectorDefault;

			lLimits.SetAuto(FbxLimitsUtilities::eTranslation, mFileObject->FieldReadI(FIELD_KFBXNODE_LIMITS_T_AUTO,1) ? true : false);
			lLimits.SetAuto(FbxLimitsUtilities::eRotation, mFileObject->FieldReadI(FIELD_KFBXNODE_LIMITS_R_AUTO,1) ? true : false);
			lLimits.SetAuto(FbxLimitsUtilities::eScaling, mFileObject->FieldReadI(FIELD_KFBXNODE_LIMITS_S_AUTO,1) ? true : false);
    
			lLimits.SetEnable(FbxLimitsUtilities::eTranslation, mFileObject->FieldReadI(FIELD_KFBXNODE_LIMITS_T_ENABLE,0) ? true : false);
			lLimits.SetEnable(FbxLimitsUtilities::eRotation, mFileObject->FieldReadI(FIELD_KFBXNODE_LIMITS_R_ENABLE,0) ? true : false);
			lLimits.SetEnable(FbxLimitsUtilities::eScaling, mFileObject->FieldReadI(FIELD_KFBXNODE_LIMITS_S_ENABLE,0) ? true : false);
    
			lVector[0] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_T_X_DEFAULT,0.0);
			lVector[1] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_T_Y_DEFAULT,0.0);
			lVector[2] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_T_Z_DEFAULT,0.0);
			lLimits.SetDefault(FbxLimitsUtilities::eTranslation, lVector);
                                  
			lVector[0] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_R_X_DEFAULT,0.0);
			lVector[1] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_R_Y_DEFAULT,0.0);
			lVector[2] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_R_Z_DEFAULT,0.0);
			lLimits.SetDefault(FbxLimitsUtilities::eRotation, lVector);              
        
			lVector[0] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_S_X_DEFAULT,1.0);
			lVector[1] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_S_Y_DEFAULT,1.0);
			lVector[2] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_S_Z_DEFAULT,1.0);
			lLimits.SetDefault(FbxLimitsUtilities::eScaling, lVector);
    
			lLimits.SetMax(FbxLimitsUtilities::eScaling, lVector);
    
			lLimits.SetRotationType((FbxLimitsUtilities::ERotationType) mFileObject->FieldReadI(FIELD_KFBXNODE_LIMITS_R_TYPE,FbxLimitsUtilities::eEuler));
			//drop this field
            mFileObject->FieldReadI(FIELD_KFBXNODE_LIMITS_R_CLAMP_TYPE,FbxLimitsUtilities::eRectangular);
    
			lVector[0] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_R_X_AXIS,0.0);
			lVector[1] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_R_Y_AXIS,0.0);
			lVector[2] = mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_R_Z_AXIS,0.0);
			lLimits.SetRotationAxis(lVector);
    
			lLimits.SetAxisLength(mFileObject->FieldReadD(FIELD_KFBXNODE_LIMITS_AXIS_LENGTH,1.0));
    
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    return true;
}


bool FbxReaderFbx5::ReadNodeTarget ( FbxNode& pNode )
{
    if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_TARGET))
    {
		FbxString lString = FbxObject::StripPrefix(mFileObject->FieldReadC());
        mTargetArrayName.Add( lString.Buffer(), (FbxHandle) &pNode);
        mFileObject->FieldReadEnd();
    }

    FbxVector4 lPostTargetRotation;
    mFileObject->FieldRead3D(FIELD_KFBXNODE_POST_TARGET_ROTATION, lPostTargetRotation.mData, lPostTargetRotation.mData);
    pNode.SetPostTargetRotation(lPostTargetRotation);

    FbxVector4 lTargetUpVector;
    mFileObject->FieldRead3D(FIELD_KFBXNODE_TARGET_UP_VECTOR, lTargetUpVector.mData, lTargetUpVector.mData);
    pNode.SetTargetUpVector(lTargetUpVector);

    if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_UP_VECTOR_MODEL))
    {
		FbxString lString = FbxObject::StripPrefix(mFileObject->FieldReadC());
		mUpNodeArrayName.Add(lString.Buffer(), (FbxHandle) &pNode);
        mFileObject->FieldReadEnd();
    }

    return true;
}


bool FbxReaderFbx5::ReadNodeAttribute (FbxNode& pNode)
{
    FbxString  mObjectType;

    if (mFileObject-> FieldReadBegin (FIELD_KFBXNODE_TYPE)) 
    {
        mObjectType = mFileObject->FieldReadC ();
        mFileObject->FieldReadEnd();

        if (strcmp (mObjectType.Buffer(), "Null") == 0) 
        {
            FbxNull* lNull = FbxNull::Create(&mManager, "");
            ReadNull(*lNull);
            pNode.SetNodeAttribute(lNull);	
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        } 
        else if (strcmp (mObjectType.Buffer(), "Marker") == 0)
        {
            FbxMarker *lMarker = FbxMarker::Create(&mManager, "");
			lMarker->SetType(FbxMarker::eStandard);
            pNode.SetNodeAttribute(lMarker);
            ReadMarker(*lMarker);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        }     
        else if (strcmp (mObjectType.Buffer(), "OpticalMarker") == 0) 
        {
            FbxMarker *lMarker = FbxMarker::Create(&mManager, "");
			lMarker->SetType(FbxMarker::eOptical);
			pNode.SetNodeAttribute(lMarker);
            ReadMarker(*lMarker);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        } 		
        else if (strcmp (mObjectType.Buffer(), "IKEffector") == 0) 
        {
            FbxMarker *lMarker = FbxMarker::Create(&mManager, "");
			lMarker->SetType(FbxMarker::eEffectorIK);
			pNode.SetNodeAttribute(lMarker);
            ReadMarker(*lMarker);
            if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        } 		
        else if (strcmp (mObjectType.Buffer(), "FKEffector") == 0) 
        {
            FbxMarker *lMarker = FbxMarker::Create(&mManager, "");
			lMarker->SetType(FbxMarker::eEffectorFK);
			pNode.SetNodeAttribute(lMarker);
            ReadMarker(*lMarker);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        } 		
        else if (strcmp (mObjectType.Buffer(), "Root") == 0) 
        {
            FbxSkeleton *lSkeletonRoot = FbxSkeleton::Create(&mManager, "");
            lSkeletonRoot->SetSkeletonType (FbxSkeleton::eRoot);

            // Root information as written by Motion Builder 4.0.
            if (mFileObject->FieldReadBegin(FIELD_PROPERTIES))
            {
                if (mFileObject->FieldReadBlockBegin())
				{
					//int lVersion = mFileObject->FieldReadI(FIELD_PROPERTIES_VERSION, 0);

					if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYSKELETON_LIMB_NODE_SIZE))
					{
						lSkeletonRoot->Size.Set(mFileObject->FieldReadD());
						mFileObject->FieldReadEnd();
					}
                                
					mFileObject->FieldReadBlockEnd();
				}
                mFileObject->FieldReadEnd();
            }
            pNode.SetNodeAttribute(lSkeletonRoot);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        } 
        else if (strcmp (mObjectType.Buffer (), "Limb") == 0) 
        {
            FbxSkeleton *lSkeletonLimb = FbxSkeleton::Create(&mManager, "");
            lSkeletonLimb->SetSkeletonType (FbxSkeleton::eLimb);

            if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYSKELETON_LIMB_LENGTH))
            {
                lSkeletonLimb->LimbLength.Set(mFileObject->FieldReadD ());
                mFileObject->FieldReadEnd ();
            }

            // Limb information as written by Motion Builder 4.0.
            if (mFileObject->FieldReadBegin(FIELD_PROPERTIES))
            {
                if (mFileObject->FieldReadBlockBegin())
				{
					//int lVersion = mFileObject->FieldReadI(FIELD_PROPERTIES_VERSION, 0);

					if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYSKELETON_LIMB_NODE_SIZE))
					{
						lSkeletonLimb->Size.Set(mFileObject->FieldReadD());
						mFileObject->FieldReadEnd();
					}
                
					if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYSKELETON_LIMB_NODE_COLOR))
					{
						FbxColor lColor;
						lColor.mRed = mFileObject->FieldReadD();
						lColor.mGreen = mFileObject->FieldReadD();
						lColor.mBlue = mFileObject->FieldReadD();
						lSkeletonLimb->SetLimbNodeColor(lColor);
						mFileObject->FieldReadEnd();
					}
                
					mFileObject->FieldReadBlockEnd();
				}
                mFileObject->FieldReadEnd();
            }
            pNode.SetNodeAttribute (lSkeletonLimb);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        } 
        else if (strcmp (mObjectType.Buffer(), "LimbNode") == 0) 
        {
            FbxSkeleton *lSkeletonLimbNode = FbxSkeleton::Create(&mManager, "");
            lSkeletonLimbNode->SetSkeletonType(FbxSkeleton::eLimbNode);

            // Limb node information as written by Motion Builder 4.0.
            if (mFileObject->FieldReadBegin(FIELD_PROPERTIES))
            {
                if (mFileObject->FieldReadBlockBegin())
				{
					//int lVersion = mFileObject->FieldReadI(FIELD_PROPERTIES_VERSION, 0);

					if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYSKELETON_LIMB_NODE_SIZE))
					{
						lSkeletonLimbNode->Size.Set(mFileObject->FieldReadD());
						mFileObject->FieldReadEnd ();
					}
            
					if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYSKELETON_LIMB_NODE_COLOR))
					{
						FbxColor lColor;
						lColor.mRed = mFileObject->FieldReadD();
						lColor.mGreen = mFileObject->FieldReadD();
						lColor.mBlue = mFileObject->FieldReadD();
						lSkeletonLimbNode->SetLimbNodeColor(lColor);
						mFileObject->FieldReadEnd ();
					}
                
					mFileObject->FieldReadBlockEnd();
				}
                mFileObject->FieldReadEnd();
            }
            // Limb node information as written by FiLMBOX 3.5 and previous versions.
            else if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYSKELETON_LIMB_NODE_SIZE))
            {
                lSkeletonLimbNode->Size.Set(mFileObject->FieldReadD() * 100.0);
                mFileObject->FieldReadEnd ();
            }
            pNode.SetNodeAttribute (lSkeletonLimbNode);
            if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        } 
        else if (strcmp (mObjectType.Buffer(), "Effector") == 0) 
        {
            FbxSkeleton *lSkeletonEffector = FbxSkeleton::Create(&mManager, "");
            lSkeletonEffector->SetSkeletonType(FbxSkeleton::eEffector);
            pNode.SetNodeAttribute(lSkeletonEffector);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        } 
        else if (strcmp (mObjectType.Buffer(), "Nurb") == 0) 
        {
            FbxNurbs* lNurbs = FbxNurbs::Create(&mManager, "");
            ReadNurb(*lNurbs);
            pNode.SetNodeAttribute(lNurbs);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        } 
		else if (strcmp (mObjectType.Buffer(), "Patch") == 0) 
        {
            FbxPatch *lPatch = FbxPatch::Create(&mManager, "");
            ReadPatch(*lPatch);
            pNode.SetNodeAttribute (lPatch);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        }
        else if (strcmp (mObjectType.Buffer(), "Light") == 0) 
        {
            FbxLight* lLight = FbxLight::Create(&mManager, "");
			pNode.SetNodeAttribute (lLight);
            ReadLight(*lLight);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        }
        else if (strcmp (mObjectType.Buffer(), "Camera") == 0) 
        {
            FbxCamera* lCamera = FbxCamera::Create(&mManager, "");
            ReadCamera(*lCamera);
            pNode.SetNodeAttribute(lCamera);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        }
        else if (strcmp (mObjectType.Buffer(), "CameraSwitcher") == 0) 
        {
			// The camera switcher attribute read here only exist from fbx v6.0 files
            pNode.SetNodeAttribute(FbxCameraSwitcher::Create(&mManager, ""));
            ReadCameraSwitcher(*(FbxCameraSwitcher*)pNode.GetNodeAttribute());
        }
        else if (strcmp (mObjectType.Buffer(), "Optical") == 0) 
        {
            FbxOpticalReference *lOpticalReference = FbxOpticalReference::Create(&mManager, "");
            pNode.SetNodeAttribute(lOpticalReference);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
        } 
    } 
    else 
    { 
        FbxMesh *lMesh = FbxMesh::Create(&mManager, "");
		if (ReadMesh(*lMesh))
		{
			pNode.SetNodeAttribute(lMesh);
			if (mFileObject->IsBeforeVersion6()) ReadUserProperties(pNode);
		}
    }

	// Clear the temporary texture pointers we have store
	// during calls to ReadGeometryTexture
	mTemporaryTextures.Clear();
		
    return true;
}


bool FbxReaderFbx5::ReadNodePivots (FbxNode& pNode)
{
    if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_PIVOTS))
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			int lBlockVersion = 0;
            FbxVector4 lTmpData;

			// Version Field new in version 194
			if (mFileObject->FieldReadBegin (FIELD_KFBXNODE_VERSION)) 
			{
				lBlockVersion = mFileObject->FieldReadI();
				mFileObject->FieldReadEnd ();
			}
        
			if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_PACKAGE))
			{
				if (mFileObject->FieldReadBlockBegin())
				{
					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_PIVOT_ENABLED))
					{
						int lPivotEnabled = mFileObject->FieldReadI();
						pNode.SetPivotState(FbxNode::eSourcePivot, lPivotEnabled ? FbxNode::ePivotActive : FbxNode::ePivotReference);
						mFileObject->FieldReadEnd();
					}

					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_TRANSLATION_OFFSET))
					{
						mFileObject->FieldRead3D(lTmpData.mData);
                        pNode.SetRotationOffset(FbxNode::eSourcePivot, lTmpData);
						mFileObject->FieldReadEnd();
					}

					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_ROTATION_PIVOT))
					{
                        mFileObject->FieldRead3D(lTmpData.mData);
                        pNode.SetRotationPivot(FbxNode::eSourcePivot, lTmpData);
						mFileObject->FieldReadEnd();
					}
            
					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_PRE_ROTATION))
					{
						mFileObject->FieldRead3D(lTmpData.mData);
                        pNode.SetPreRotation(FbxNode::eSourcePivot, lTmpData);
						mFileObject->FieldReadEnd();
					}

					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_POST_ROTATION))
					{
						mFileObject->FieldRead3D(lTmpData.mData);
                        pNode.SetPostRotation(FbxNode::eSourcePivot, lTmpData);
						mFileObject->FieldReadEnd();
					}

					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_SCALING_PIVOT))
					{
						mFileObject->FieldRead3D(lTmpData.mData);
                        pNode.SetScalingPivot(FbxNode::eSourcePivot, lTmpData);
						mFileObject->FieldReadEnd();
					}

					mFileObject->FieldReadBlockEnd();    
				}
				mFileObject->FieldReadEnd();
			}

			if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_FILE))
			{
				if (mFileObject->FieldReadBlockBegin())
				{
					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_PIVOT_ENABLED))
					{
						int lPivotEnabled = mFileObject->FieldReadI();
						pNode.SetPivotState(FbxNode::eDestinationPivot, lPivotEnabled ? FbxNode::ePivotActive : FbxNode::ePivotReference);
						mFileObject->FieldReadEnd();
					}

					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_TRANSLATION_OFFSET))
					{
						mFileObject->FieldRead3D(lTmpData.mData);
                        pNode.SetRotationOffset(FbxNode::eDestinationPivot, lTmpData);
						mFileObject->FieldReadEnd();
					}

					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_ROTATION_PIVOT))
					{
						mFileObject->FieldRead3D(lTmpData.mData);
                        pNode.SetRotationPivot(FbxNode::eDestinationPivot, lTmpData);
						mFileObject->FieldReadEnd();
					}
            
					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_PRE_ROTATION))
					{
						mFileObject->FieldRead3D(lTmpData.mData);
                        pNode.SetPreRotation(FbxNode::eDestinationPivot, lTmpData);
						mFileObject->FieldReadEnd();
					}

					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_POST_ROTATION))
					{
						mFileObject->FieldRead3D(lTmpData);
                        pNode.SetPostRotation(FbxNode::eDestinationPivot, lTmpData);
						mFileObject->FieldReadEnd();
					}

					if (mFileObject->FieldReadBegin(FIELD_KFBXNODE_SCALING_PIVOT))
					{
						mFileObject->FieldRead3D(lTmpData);
                        pNode.SetScalingPivot(FbxNode::eDestinationPivot, lTmpData);
						mFileObject->FieldReadEnd();
					}

					mFileObject->FieldReadBlockEnd();    
				}
				mFileObject->FieldReadEnd();
			}

			mFileObject->FieldReadBlockEnd();    
		}
        mFileObject->FieldReadEnd();
    }

    return true;
}


bool FbxReaderFbx5::ReadNodeDefaultAttributes(FbxNode& pNode)
{
    FbxAnimUtilities::CurveNodeIntfce lParent = FbxAnimUtilities::CreateCurveNode("temp");
    FbxAnimUtilities::CurveNodeIntfce lChild = FbxAnimUtilities::CreateCurveNode(mFileObject, lParent, true);
    while (lChild.IsValid()) 
    {
        FbxAnimUtilities::CurveNodeIntfce lChild1 = FbxAnimUtilities::CreateCurveNode(mFileObject, lParent, true);
        lChild = lChild1;
    }

    // now we need to transfer the animation from the temp KFCurve (and children) to 
    // the corresponding properties.
    pNode.RootProperty.BeginCreateOrFindProperty();
    FbxProperty p = pNode.GetFirstProperty();  
    while (p.IsValid()) 
    {   
        TransferAnimation((void*)&lParent, p, true);
        p = pNode.GetNextProperty(p);    
    }
    pNode.RootProperty.EndCreateOrFindProperty();
    FbxAnimUtilities::DestroyCurveNode(lParent);
    return true;
}

bool FbxReaderFbx5::ReadNodeProperties(FbxNode& pNode)
{
	ReadProperties(&pNode);
	pNode.UpdatePivotsAndLimitsFromProperties();

	return true;
}

void FbxReaderFbx5::ReadAnimation(FbxIO& pFileObject, void* pCN)
{
    FbxAnimUtilities::CurveNodeIntfce* pCurveNode = (FbxAnimUtilities::CurveNodeIntfce*)pCN;
    FbxAnimUtilities::CurveNodeIntfce lChildNode = FbxAnimUtilities::CreateCurveNode(&pFileObject, *pCurveNode, false);

    while (lChildNode.IsValid()) 
    {
        FbxAnimUtilities::CurveNodeIntfce lChild = FbxAnimUtilities::CreateCurveNode(&pFileObject, *pCurveNode, false);
        lChildNode = lChild;
    }
}

void FbxReaderFbx5::TransferAnimation(void* pRCN, FbxProperty& pProperty, bool pValueOnly)
{
    FbxAnimUtilities::CurveNodeIntfce* pRootCurveNode = (FbxAnimUtilities::CurveNodeIntfce*)pRCN;
    FBX_ASSERT(pRootCurveNode != NULL && pRootCurveNode->IsValid());
	FbxString propName = pProperty.GetName();
    const char* lName = FbxAnimCurveNode::CurveNodeNameFrom(propName.Buffer());
    FbxAnimUtilities::CurveNodeIntfce fcn = pRootCurveNode->FindRecursive(const_cast<char*>(lName));
    if (fcn.IsValid())
    {
        FbxAnimCurve* ac = NULL;
        FbxAnimCurveNode* acn = NULL;
        
        int nbChildren = fcn.GetCount();
        unsigned int nbChannels = nbChildren;

        if (pValueOnly == false)
        {
            FBX_ASSERT(mAnimLayer != NULL);
            acn = pProperty.GetCurveNode(mAnimLayer, true); 
            FBX_ASSERT(acn != NULL);
            if(acn)
                nbChannels = (unsigned int)acn->GetChannelsCount();
        }
         
        if (nbChildren == 0)
        {
            // the animation (if exist) is directly on this curve node (fcn)
            // it must then have the KFCurve (either animated or not)
            FbxAnimUtilities::CurveIntfce fc(fcn.GetCurveHandle());
            if (fc.IsValid())
            {
                // we set the corresponding property with the default value
                double v = fc.GetValue();
                pProperty.Set(v);

                // and now, if we have keys, we create the AnimCurveNode and
                // transfer the keys to it.
                if (pValueOnly == false && fc.KeyGetCount() && acn)
                {
                    // copy the animation
                    ac = acn->GetCurve(0U);
                    if (ac == NULL) ac = acn->CreateCurve(acn->GetName(), 0U);
                    FBX_ASSERT(ac != NULL);
                    if (ac)
                        FbxAnimUtilities::CopyFrom(ac, fc);                        
                }
            }
        }
        else
        {
            // the nbChildren should match the nbChannels
            FBX_ASSERT(nbChildren == nbChannels);
            double* v = FbxNewArray< double >((int)nbChannels);
            for (unsigned int ch = 0; ch < nbChannels; ch++)
            {
                FbxAnimUtilities::CurveNodeIntfce ccn = fcn.GetHandle(ch); 
                FBX_ASSERT(ccn.IsValid());
                FbxAnimUtilities::CurveIntfce fc(ccn.GetCurveHandle());
                FBX_ASSERT(fc.IsValid());

                v[ch] = fc.GetValue();
                if (pValueOnly == false && fc.KeyGetCount() && acn)
                {
                    // copy the animation
                    ac = acn->GetCurve(ch);
                    if (ac == NULL) ac = acn->CreateCurve(acn->GetName(), ch);
                    FBX_ASSERT(ac != NULL);
                    if (ac)
                        FbxAnimUtilities::CopyFrom(ac, fc);
                }
            }

            switch (nbChannels)
            {
				case 1: pProperty.Set(*v); break;
				case 2: pProperty.Set(*((FbxDouble2*)v)); break;
				case 3: pProperty.Set(*((FbxDouble3*)v)); break;
				case 4: pProperty.Set(*((FbxDouble4*)v)); break;
				case 16: pProperty.Set(*((FbxDouble4x4*)v)); break;
				default:
                // this one will, most likely, cause an assert because the type is unknown.
                pProperty.Set(v);
            }
            FbxDeleteArray(v);
        }
    }        
}

void FbxReaderFbx5::ReadAnimation(FbxIO& pFileObject, FbxObject* pObj)
{
    FbxAnimUtilities::CurveNodeIntfce lParent = FbxAnimUtilities::CreateCurveNode("temp");
    ReadAnimation(pFileObject, (void*)&lParent);

    // now we need to transfer the animation from the temp KFCurve (and children) to 
    // the corresponding properties.
    pObj->RootProperty.BeginCreateOrFindProperty();
    FbxProperty p = pObj->GetFirstProperty();  
    while (p.IsValid()) 
    {   
        TransferAnimation((void*)&lParent, p);
        p = pObj->GetNextProperty(p);    
    }
    pObj->RootProperty.EndCreateOrFindProperty();
    FbxAnimUtilities::DestroyCurveNode(lParent);
}


bool FbxReaderFbx5::ReadGeometry(FbxGeometry& pGeometry)
{
    ReadGeometryMaterial(pGeometry);
    ReadGeometryTexture(pGeometry);
    ReadGeometryLinks(pGeometry);
    ReadGeometryShapes(pGeometry);
    ReadGeometryLayer(pGeometry);	

    return true;
}

bool FbxReaderFbx5::ReadLayerElements(FbxGeometry& pGeometry)
{
	FbxArray<FbxLayerElement*> lElementsMaterial;
	ReadLayerElementsMaterial(&pGeometry, lElementsMaterial);

	FbxArray<FbxLayerElement*> lElementsNormal;
	ReadLayerElementsNormal(&pGeometry, lElementsNormal);

	FbxArray<FbxLayerElement*> lElementsVertexColor;
	ReadLayerElementsVertexColor(&pGeometry, lElementsVertexColor);

	FbxArray<FbxLayerElement*> lElementsPolygonGroup; 
	ReadLayerElementsPolygonGroup(&pGeometry, lElementsPolygonGroup);

	FbxArray<FbxLayerElement*> lElementsTexture;
	ReadLayerElementsTexture(&pGeometry, lElementsTexture);

	FbxArray<FbxLayerElement*> lElementsUV; 
	ReadLayerElementsUV(&pGeometry, lElementsUV);

	while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER))
	{
		int lLayerIndex = mFileObject->FieldReadI();

		if (mFileObject->FieldReadBlockBegin())
		{
			/*int lLayerVersion = */ mFileObject->FieldReadI();

			while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT))
			{
				if (mFileObject->FieldReadBlockBegin())
				{
					if(!pGeometry.GetLayer(lLayerIndex))
					{
						int lCreatedLayer = pGeometry.CreateLayer();
						FBX_ASSERT(lCreatedLayer == lLayerIndex);
					}
					FbxLayer *lLayer = pGeometry.GetLayer(lLayerIndex);
				
					const char*	lLayerElementType = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_TYPE);
					int		lLayerElementIndex	= mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_TYPED_INDEX);

					if ((lLayerElementIndex >= 0) && (lLayer != NULL))
					{
						if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_MATERIAL))
						{
							if (lElementsMaterial.GetCount() > 0 && lLayerElementIndex < lElementsMaterial.GetCount())
							{
								lLayer->SetMaterials(static_cast<FbxLayerElementMaterial*>(lElementsMaterial[lLayerElementIndex]));
							}
							else
							{
								// Should be here only when ALL_SAME and access is DIRECT.
								// This is a patch to correctly retrieve the material. If we don't
								// do this, there will be no material on the model).
								if (lLayerElementIndex == 0)
								{
									FbxLayerElementMaterial* lem = FbxLayerElementMaterial::Create(&pGeometry, "");
									lem->SetMappingMode(FbxLayerElement::eAllSame);
									lem->SetReferenceMode(FbxLayerElement::eDirect);
									lLayer->SetMaterials(lem);
								}
							}
						}
						else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_NORMAL))
						{
							if (lElementsNormal.GetCount() > 0 && lLayerElementIndex < lElementsNormal.GetCount())
								lLayer->SetNormals(static_cast<FbxLayerElementNormal*>(lElementsNormal[lLayerElementIndex]));
						}
						else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_COLOR))
						{
							if (lElementsVertexColor.GetCount() > 0 && lLayerElementIndex < lElementsVertexColor.GetCount())
								lLayer->SetVertexColors(static_cast<FbxLayerElementVertexColor*>(lElementsVertexColor[lLayerElementIndex]));
						}
						else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_TEXTURE))
						{
							if (lElementsTexture.GetCount() > 0 && lLayerElementIndex < lElementsTexture.GetCount())
							{
								lLayer->SetTextures(FbxLayerElement::eTextureDiffuse, static_cast<FbxLayerElementTexture*>(lElementsTexture[lLayerElementIndex]));
							}
							else
							{
								// Should be here only when ALL_SAME and access is DIRECT.
								// This is a patch to correctly retrieve the texture. If we don't
								// do this, there will be no texture on the model).
								if (lLayerElementIndex == 0)
								{
									FbxLayerElementTexture* let = FbxLayerElementTexture::Create(&pGeometry, "");
									let->SetMappingMode(FbxLayerElement::eAllSame);
									let->SetReferenceMode(FbxLayerElement::eDirect);
									lLayer->SetTextures(FbxLayerElement::eTextureDiffuse, let);
								}
							}

						}
						else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_UV))
						{
							if (lElementsUV.GetCount() > 0 && lLayerElementIndex < lElementsUV.GetCount())
								lLayer->SetUVs(static_cast<FbxLayerElementUV*>(lElementsUV[lLayerElementIndex]));
						}
						else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_POLYGON_GROUP))
						{
							if (lElementsPolygonGroup.GetCount() > 0 && lLayerElementIndex < lElementsPolygonGroup.GetCount())
								lLayer->SetPolygonGroups(static_cast<FbxLayerElementPolygonGroup*>(lElementsPolygonGroup[lLayerElementIndex]));
						}
					}

					mFileObject->FieldReadBlockEnd();
				}
				mFileObject->FieldReadEnd();
			}		

			mFileObject->FieldReadBlockEnd();
		}
		mFileObject->FieldReadEnd();
	}

	return true; 
}

bool FbxReaderFbx5::ReadLayerElementsMaterial(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsMaterial)
{
	while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_MATERIAL))
	{
		FbxLayerElementMaterial* lLayerElementMaterial = FbxLayerElementMaterial::Create(pGeometry, "");
		int lLayerElementIndex = mFileObject->FieldReadI();

		if (mFileObject->FieldReadBlockBegin())
		{
			int lLayerElementVersion	= mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
			if (lLayerElementVersion >= 101)
			{
				FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
				lLayerElementMaterial->SetName(lLayerName.Buffer());
			}

			const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
			const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

			lLayerElementMaterial->SetMappingMode(ConvertMappingModeToken(lMappingMode));
			lLayerElementMaterial->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));

			FBX_ASSERT(ConvertReferenceModeToken(lReferenceMode) != FbxLayerElement::eDirect) ; 
							
			if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_MATERIALS_ID))
			{
				int lMaterialCounter ;
				int lMaterialCount  = mFileObject->FieldReadGetCount() ; 
				FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementMaterial->GetIndexArray();					
				
				for(lMaterialCounter = 0 ; lMaterialCounter < lMaterialCount  ; lMaterialCounter ++)
				{
					int lMaterialIndex = mFileObject->FieldReadI();					
					lIndexArray.Add(lMaterialIndex);
				}
				mFileObject->FieldReadEnd();
			}			
			mFileObject->FieldReadBlockEnd();
		}		
		
		mFileObject->FieldReadEnd();

		int lAddedIndex  = pElementsMaterial.Add(lLayerElementMaterial);

		FBX_ASSERT( lAddedIndex == lLayerElementIndex);
	}
	return true; 
}

bool FbxReaderFbx5::ReadLayerElementsNormal(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsNormal)
{
	while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_NORMAL))
	{
		FbxLayerElementNormal* lLayerElementNormal = FbxLayerElementNormal::Create(pGeometry, "");
		int lLayerElementIndex = mFileObject->FieldReadI();

		if (mFileObject->FieldReadBlockBegin())
		{
			int lLayerElementVersion	= mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
			if (lLayerElementVersion >= 101)
			{
				FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
				lLayerElementNormal->SetName(lLayerName.Buffer());
			}
			
			const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
			const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

			lLayerElementNormal->SetMappingMode(ConvertMappingModeToken(lMappingMode));
			lLayerElementNormal->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));

			FBX_ASSERT(ConvertReferenceModeToken(lReferenceMode) == FbxLayerElement::eDirect) ; 
							
			if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_NORMALS))
			{
				int lNormalCounter ;
				int lNormalCount  = mFileObject->FieldReadGetCount() / 3 ; 
				FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementNormal->GetDirectArray();

				for(lNormalCounter = 0 ; lNormalCounter < lNormalCount  ; lNormalCounter ++)
				{
					FbxVector4 lNormal ;
					mFileObject->FieldRead3D(lNormal);															
					lDirectArray.Add(lNormal);
				}
				mFileObject->FieldReadEnd();
			}			
			mFileObject->FieldReadBlockEnd();
		}		
		
		mFileObject->FieldReadEnd();

		int lAddedIndex  = pElementsNormal.Add(lLayerElementNormal);

		FBX_ASSERT( lAddedIndex == lLayerElementIndex);
	}
	return true; 
}

bool FbxReaderFbx5::ReadLayerElementsVertexColor(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsVertexColor)
{
	while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_COLOR))
	{
		FbxLayerElementVertexColor* lLayerElementVertexColor = FbxLayerElementVertexColor::Create(pGeometry, "");
		int lLayerElementIndex = mFileObject->FieldReadI();

		if (mFileObject->FieldReadBlockBegin())
		{
			int lLayerElementVersion	= mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
			if (lLayerElementVersion >= 101)
			{
				FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
				lLayerElementVertexColor->SetName(lLayerName.Buffer());
			}

			const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
			const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

			lLayerElementVertexColor->SetMappingMode(ConvertMappingModeToken(lMappingMode));
			lLayerElementVertexColor->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));
			FBX_ASSERT(ConvertReferenceModeToken(lReferenceMode) != FbxLayerElement::eIndex) ; 

			if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_VALUES))
			{
				int lVertexColorCounter ;
				int lVertexColorCount  = mFileObject->FieldReadGetCount() / 4 ; 
				FbxLayerElementArrayTemplate<FbxColor>& lDirectArray = lLayerElementVertexColor->GetDirectArray();

				for(lVertexColorCounter = 0 ; lVertexColorCounter < lVertexColorCount  ; lVertexColorCounter ++)
				{
					FbxColor lColor;					
					lColor.mRed		= mFileObject->FieldReadD();
					lColor.mGreen	= mFileObject->FieldReadD();
					lColor.mBlue	= mFileObject->FieldReadD();
					lColor.mAlpha	= mFileObject->FieldReadD();
					lDirectArray.Add(lColor);					
				}
				mFileObject->FieldReadEnd();
			}			

			if (lLayerElementVertexColor->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
			{
				if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_INDEX))
				{
					int lIndexCounter ;
					int lIndexCount = mFileObject->FieldReadGetCount();
					FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementVertexColor->GetIndexArray();
					for (lIndexCounter = 0; lIndexCounter < lIndexCount ; lIndexCounter++) 
					{
						int lCurrentIndex = mFileObject->FieldReadI();
						lIndexArray.Add(lCurrentIndex);
					}	
					mFileObject->FieldReadEnd();
				}				
			}
			mFileObject->FieldReadBlockEnd();
		}		
		
		mFileObject->FieldReadEnd();

		int lAddedIndex  = pElementsVertexColor.Add(lLayerElementVertexColor);

		FBX_ASSERT( lAddedIndex == lLayerElementIndex);
	}
	return true; 
}


bool FbxReaderFbx5::ReadLayerElementsTexture(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsTexture)
{
	while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_TEXTURE))
	{
		FbxLayerElementTexture* lLayerElementTexture = FbxLayerElementTexture::Create(pGeometry, "");
		int lLayerElementIndex = mFileObject->FieldReadI();
		bool lValid             = false;

		if (mFileObject->FieldReadBlockBegin())
		{
			int lLayerElementVersion	= mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
			if (lLayerElementVersion >= 101)
			{
				FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
				lLayerElementTexture->SetName(lLayerName.Buffer());
			}
			
			const char* lMappingMode	= mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
			const char* lReferenceMode	= mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);
			const char* lBlendMode		= mFileObject->FieldReadC(FIELD_KFBXTEXTURE_BLEND_MODE);
			double	 lAlpha				= mFileObject->FieldReadD(FIELD_KFBXTEXTURE_ALPHA);
			
			// Direct mapping should not be fond in a file
			lLayerElementTexture->SetMappingMode(ConvertMappingModeToken(lMappingMode));
			lLayerElementTexture->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));
			lLayerElementTexture->SetBlendMode(ConvertBlendModeToken(lBlendMode));
			lLayerElementTexture->SetAlpha(lAlpha);

			if (ConvertReferenceModeToken(lReferenceMode) != FbxLayerElement::eDirect)
			{
				if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_TEXTURE_ID))
				{
					int lTextureCounter ;
					int lTextureCount  = mFileObject->FieldReadGetCount() ; 
					FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementTexture->GetIndexArray();					
					
					lValid = lTextureCount > 0;

					for(lTextureCounter = 0 ; lTextureCounter < lTextureCount  ; lTextureCounter ++)
					{
						int lTextureIndex = mFileObject->FieldReadI();					
						lIndexArray.Add(lTextureIndex);
					}
					mFileObject->FieldReadEnd();
				}			
			}
			mFileObject->FieldReadBlockEnd();
		}		
		
		mFileObject->FieldReadEnd();

		if (lValid)
		{
			int lAddedIndex  = pElementsTexture.Add(lLayerElementTexture);

			FBX_ASSERT( lAddedIndex == lLayerElementIndex);
		}
	}
	return true; 
}
bool FbxReaderFbx5::ReadLayerElementsUV(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsUV)
{
	while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_UV))
	{
		FbxLayerElementUV* lLayerElementUV = FbxLayerElementUV::Create(pGeometry, "");
		int lLayerElementIndex = mFileObject->FieldReadI();

		if (mFileObject->FieldReadBlockBegin())
		{
			int lLayerElementVersion	= mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
			if (lLayerElementVersion >= 101)
			{
				FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
				lLayerElementUV->SetName(lLayerName.Buffer());
			}
			
			const char* lMappingMode	= mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
			const char* lReferenceMode	= mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

			lLayerElementUV->SetMappingMode(ConvertMappingModeToken(lMappingMode));
			lLayerElementUV->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));
							
			if (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_UV))
			{
				int lUVCounter ;
				int lUVCount  = mFileObject->FieldReadGetCount() / 2 ; 
				FbxLayerElementArrayTemplate<FbxVector2>& lDirectArray = lLayerElementUV->GetDirectArray();

				for(lUVCounter = 0 ; lUVCounter < lUVCount  ; lUVCounter ++)
				{
					FbxVector2 lUV;					
					lUV.mData[0] = mFileObject->FieldReadD();
					lUV.mData[1] = mFileObject->FieldReadD();

					lDirectArray.Add(lUV);
				}
				mFileObject->FieldReadEnd();
			}
			
			if (lLayerElementUV->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
			{
				if (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_UV_INDEX))
				{
					int lIndexCounter ;
					int lIndexCount = mFileObject->FieldReadGetCount();
					FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementUV->GetIndexArray();
					for (lIndexCounter = 0; lIndexCounter < lIndexCount ; lIndexCounter++) 
					{
						int lCurrentIndex = mFileObject->FieldReadI();
						lIndexArray.Add(lCurrentIndex);
					}	
					mFileObject->FieldReadEnd();
				}				
			}
			mFileObject->FieldReadBlockEnd();
		}
		
		mFileObject->FieldReadEnd();

		int lAddedIndex  = pElementsUV.Add(lLayerElementUV);

		FBX_ASSERT( lAddedIndex == lLayerElementIndex);
	}
	return true; 
}
bool FbxReaderFbx5::ReadLayerElementsPolygonGroup(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsPolygonGroup)
{
	while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_POLYGON_GROUP))
	{
		FbxLayerElementPolygonGroup* lLayerElementPolygonGroup = FbxLayerElementPolygonGroup::Create(pGeometry, "");
		int lLayerElementIndex = mFileObject->FieldReadI();

		if (mFileObject->FieldReadBlockBegin())
		{
			int lLayerElementVersion	= mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
			if (lLayerElementVersion >= 101)
			{
				FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
				lLayerElementPolygonGroup->SetName(lLayerName.Buffer());
			}
			
			const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
			const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

			lLayerElementPolygonGroup->SetMappingMode(ConvertMappingModeToken(lMappingMode));
			lLayerElementPolygonGroup->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));
							
			if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_POLYGON_GROUP))
			{
				int lPolygonGroupCounter ;
				int lPolygonGroupCount  = mFileObject->FieldReadGetCount() ; 
				FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementPolygonGroup->GetIndexArray();					
				
				for(lPolygonGroupCounter = 0 ; lPolygonGroupCounter < lPolygonGroupCount  ; lPolygonGroupCounter ++)
				{
					int lPolygonGroupIndex = mFileObject->FieldReadI();					
					lIndexArray.Add(lPolygonGroupIndex);
				}
				mFileObject->FieldReadEnd();
			}			
			mFileObject->FieldReadBlockEnd();
		}		
		
		mFileObject->FieldReadEnd();

		int lAddedIndex  = pElementsPolygonGroup.Add(lLayerElementPolygonGroup);

		FBX_ASSERT( lAddedIndex == lLayerElementIndex);
	}
	return true; 
}


bool FbxReaderFbx5::ReadGeometryMaterial (FbxGeometry& pGeometry)
{
    if(IOS_REF.GetBoolProp(IMP_FBX_MATERIAL, true))
    {

        //
        //Retrieve all the material
        //
        while (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRY_MATERIAL))
        {
            FbxSurfacePhong* lRetrieveMaterial = FbxSurfacePhong::Create(&mManager, "");
            
            if (ReadSurfaceMaterial(*lRetrieveMaterial))
            {
                pGeometry.AM(lRetrieveMaterial,0, NULL, true);
            }
            else
            {
                lRetrieveMaterial->Destroy();
            }

            mFileObject->FieldReadEnd();
        }
    }

    return true;
}


bool FbxReaderFbx5::ReadGeometryTexture (FbxGeometry& pGeometry)
{
    if (IOS_REF.GetBoolProp(IMP_FBX_TEXTURE, true))
    {
        //
        // Retrieve all the Texture...
        //
        while (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRY_TEXTURE))
        {
            FbxFileTexture* lRetrieveTexture = FbxFileTexture::Create(&mManager, "");

            
            if (ReadTexture(*lRetrieveTexture))
            {
				mTemporaryTextures.Add(lRetrieveTexture);
            }
            else
            {
                lRetrieveTexture->Destroy();
            }

            mFileObject->FieldReadEnd();
        }
    }

    return true;
}


bool FbxReaderFbx5::ReadGeometryLinks (FbxGeometry& pGeometry)
{                         
    if(IOS_REF.GetBoolProp(IMP_FBX_LINK, true))
    {
		FbxSkin* lSkinDeformer = NULL;

        while(mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRY_LINK)) 
        {
            FbxCluster* lRetrieveCluster= NULL;
          
            lRetrieveCluster = FbxCluster::Create(&mManager, "");

            if (ReadLink(*lRetrieveCluster))
            {
				if (pGeometry.GetDeformerCount(FbxDeformer::eSkin) < 1)
				{
					lSkinDeformer = FbxSkin::Create(&mManager, "");
					pGeometry.AddDeformer(lSkinDeformer);
				}

				if (lSkinDeformer)
				{
					lSkinDeformer->AddCluster(lRetrieveCluster);
				}
            }
            else
            {
                lRetrieveCluster->Destroy();
            }

            mFileObject->FieldReadEnd();
        }
    }

    return true;
}                                         


bool FbxReaderFbx5::ReadGeometryShapes (FbxGeometry& pGeometry)
{
    if (IOS_REF.GetBoolProp(IMP_FBX_SHAPE, true))
    {
		FbxBlendShape* lBlendShape = FbxBlendShape::Create(&mManager,"");
		if(!lBlendShape)
		{
			return false;
		}

		pGeometry.AddDeformer(lBlendShape);

        while(mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRY_SHAPE)) 
        {
			FbxString lShapeName = FbxObject::StripPrefix(mFileObject->FieldReadC());
            FbxShape* lShape = FbxShape::Create(&mManager, "");
			if(!lShape)
			{
				return false;
			}
			// Before monument, we only support one layer of shapes in parallel.
			// So put each shape on one separate blend shape channel to imitate this behavior.
            if (ReadShape(*lShape, pGeometry))
            {
				FbxBlendShapeChannel* lBlendShapeChannel = FbxBlendShapeChannel::Create(&mManager,"");
				if(!lBlendShapeChannel)
				{
					return false;
				}

				lBlendShape->AddBlendShapeChannel(lBlendShapeChannel);

				if(!lBlendShapeChannel->AddTargetShape(lShape))
				{
					lShape->Destroy();
					lBlendShapeChannel->Destroy();
				}
            }
            else
            {
                lShape->Destroy();
            }

            mFileObject->FieldReadEnd();
        }
    }

    return true;
}


bool FbxReaderFbx5::ReadGeometryLayer(FbxGeometry& pGeometry)
{
    bool lStatus     = true;
	int  lLayerCount = 0;

    while (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_GEOMETRY_LAYER))
    {		
		lLayerCount++;
        if (mFileObject->FieldReadBlockBegin())
		{
			if (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_LAYER_TEXTURE_INFO))
			{
				if (mFileObject->FieldReadBlockBegin())
				{
					int lLayerIndex = pGeometry.CreateLayer();

					// This field contains the layer index, not used for now...
					// mFileObject->FieldReadI("LayerIndex", lLayerIndex);

					lStatus = ReadGeometryTextureLayer(pGeometry, lLayerIndex) && lStatus;

					mFileObject->FieldReadBlockEnd();
				}
				mFileObject->FieldReadEnd();
			}

			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

	if (lLayerCount == 0 && mTemporaryTextures.GetCount() && pGeometry.GetAttributeType() == FbxNodeAttribute::eMesh)
	{
		// This is an old FBX file where the Layer 0 for a mesh was not saved as a layer
		int i, lCount = mTemporaryTextures.GetCount();
		for (i=0; i<lCount; i++)
		{
			pGeometry.AT(mTemporaryTextures[i],0, FbxLayerElement::eTextureDiffuse);
		}
	}

    return lStatus;
}


bool FbxReaderFbx5::ReadGeometryTextureLayer(FbxGeometry& pGeometry, int pLayerIndex)
{
	FbxLayer* lLayer = pGeometry.GetLayer(pLayerIndex);

    FbxLayerElementTexture* lLayerElementTexture = lLayer->GetTextures(FbxLayerElement::eTextureDiffuse);

	if (!lLayerElementTexture)
	{
		lLayerElementTexture = FbxLayerElementTexture::Create(&pGeometry, "");
		lLayer->SetTextures(FbxLayerElement::eTextureDiffuse, lLayerElementTexture);
	}

    bool lStatus = true;

    int lTextureMappingMode = mFileObject->FieldReadI(FIELD_KFBXLAYER_TEXTURE_MODE, 0);

    lLayerElementTexture->SetMappingMode(FbxLayerElement::eNone);
    lLayerElementTexture->SetReferenceMode(FbxLayerElement::eIndexToDirect);

    switch (lTextureMappingMode)
    {
    case 0: 
		// In MotionBuilder: KGPS_AttributeNone = 0
        lLayerElementTexture->SetMappingMode(FbxLayerElement::eNone);
        break;
    case 2: 
		// In MotionBuilder: KGPS_IdByPrimitive = 2
        lLayerElementTexture->SetMappingMode(FbxLayerElement::eAllSame);
        break;
    case 6: 
		// In MotionBuilder: KGPS_IdByPolygon = 6
        if (pGeometry.GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            lLayerElementTexture->SetMappingMode(FbxLayerElement::eByPolygon);
        }
        else
        {
            FBX_ASSERT_NOW("Unsupported texture mapping mode in texture layer.");
            lStatus = false;
        }
        break;
    default:
        FBX_ASSERT_NOW("Unsupported texture mapping mode in texture layer.");
        lStatus = false;
        break;
    }

    lLayerElementTexture->SetBlendMode((FbxLayerElementTexture::EBlendMode) mFileObject->FieldReadI(FIELD_KFBXLAYER_TEXTURE_BLEND_MODE, FbxLayerElementTexture::eModulate));

    FBX_ASSERT(lLayerElementTexture->GetBlendMode() <= FbxLayerElementTexture::eModulate2);

    if (lLayerElementTexture->GetMappingMode() == FbxLayerElement::eByPolygon)
    {
        FbxMesh* lMesh = (FbxMesh*) &pGeometry;

        int lPolygonCount = lMesh->GetPolygonCount();
        int lTextureIDCount = lPolygonCount;

        FBX_ASSERT(lTextureIDCount);

        if (lTextureIDCount)
        {
            if (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_TEXTURE_ID_COUNT))
            {
                int lTextureIDCountInFile = mFileObject->FieldReadI();
                mFileObject->FieldReadEnd();

                FBX_ASSERT(lTextureIDCountInFile == lTextureIDCount);
            }

            if (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_TEXTURE_ID))
            {
				FbxMultiMap lGlobaToLocalIndex;
				int  lSearchIndex;
                do
                {
					int lLocalTextureIndex;
					int lGlobalTextureIndex = mFileObject->FieldReadI();
					lGlobaToLocalIndex.Get(lGlobalTextureIndex, &lSearchIndex);

					if (lSearchIndex == -1)
					{
						// Not there yet
						lLocalTextureIndex = lLayerElementTexture->GetDirectArray().Add(mTemporaryTextures[lGlobalTextureIndex]);

						lGlobaToLocalIndex.Add(lGlobalTextureIndex, lLocalTextureIndex);
					}
					else
					{
						lLocalTextureIndex = int(lGlobaToLocalIndex.GetFromIndex(lSearchIndex));
					}

                    lLayerElementTexture->GetIndexArray().Add(lLocalTextureIndex);

                    if (lLayerElementTexture->GetIndexArray().GetLast() >= lPolygonCount)
                    {
                        FBX_ASSERT_NOW("Invalid texture index in texture layer.");
                        lLayerElementTexture->GetIndexArray().SetLast(0);
                        lStatus = false;
                    }

                    lTextureIDCount--;
                } while (lTextureIDCount);
                
                mFileObject->FieldReadEnd();
            }
        }
    }
    else
    {
		int lTextureIndex = mFileObject->FieldReadI(FIELD_KFBXLAYER_TEXTURE_ID, 0);

		// lTextureID is the index of the texture in the geometry's array of texture
		// (currently store in mTemporaryTextures. Transpose that to an index in
		// the layer's array of texture.
		FBX_ASSERT(lTextureIndex != -1);

		if (lTextureIndex >= 0 && lTextureIndex < mTemporaryTextures.GetCount())
		{
			int lLclIndex = lLayerElementTexture->GetDirectArray().Add(mTemporaryTextures[lTextureIndex]);
			lLayerElementTexture->GetIndexArray().Add(lLclIndex);
		}
    }

    int lTextureUVMappingMode = mFileObject->FieldReadI(FIELD_KFBXLAYER_UV_MODE, 0);

	if (lTextureUVMappingMode)
	{
		FbxLayerElementUV* lLayerUV = NULL;

		lLayerUV = FbxLayerElementUV::Create(&pGeometry, "");
		lLayer->SetUVs(lLayerUV);

		lLayerUV->SetMappingMode(FbxLayerElement::eNone);
		lLayerUV->SetReferenceMode(FbxLayerElement::eDirect);

		switch (lTextureUVMappingMode)
		{
		//case 0: // In FiLMBOX: KGPS_UVNone = 0
		case 3: // In FiLMBOX: KGPS_UVByVertex = 3
			lLayerUV->SetMappingMode(FbxLayerElement::eByControlPoint);
			lLayerUV->SetReferenceMode(FbxLayerElement::eDirect);
			break;
		case 4: // In FiLMBOX: KGPS_IdUVByVertex = 4
			lLayerUV->SetMappingMode(FbxLayerElement::eByControlPoint);
			lLayerUV->SetReferenceMode(FbxLayerElement::eIndexToDirect);
			break;
		case 7: // In FiLMBOX: KGPS_UVByPolygonVertex = 7
			if (pGeometry.GetAttributeType() == FbxNodeAttribute::eMesh)
			{
				lLayerUV->SetMappingMode(FbxLayerElement::eByPolygonVertex);
				lLayerUV->SetReferenceMode(FbxLayerElement::eDirect);
			}
			else
			{
				FBX_ASSERT_NOW("Unsupported texture UV mapping mode in texture layer.");
				lStatus = false;
			}
			break;
		case 8: // In FiLMBOX: KGPS_UVIdByPolygonVertex = 8
			if (pGeometry.GetAttributeType() == FbxNodeAttribute::eMesh)
			{
				lLayerUV->SetMappingMode(FbxLayerElement::eByPolygonVertex);
				lLayerUV->SetReferenceMode(FbxLayerElement::eIndexToDirect);
			}
			else
			{
				FBX_ASSERT_NOW("Unsupported texture UV mapping mode in texture layer.");
				lStatus = false;
			}
			break;
		default:
			FBX_ASSERT_NOW("Unsupported texture UV mapping mode in texture layer.");
			lStatus = false;
			break;
		}

		if(lLayerUV->GetMappingMode() != FbxLayerElement::eNone)
		{
			FbxUInt lUVCount = mFileObject->FieldReadI(FIELD_KFBXLAYER_UV_COUNT, 0);

			if (lUVCount)
			{
				if (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_UV))
				{
					FbxLayerElementArrayTemplate<FbxVector2>& lTextureUVArray = lLayerUV->GetDirectArray();

					do
					{
						FbxVector2 lUV;
						lUV[0] = mFileObject->FieldReadD();
						lUV[1] = mFileObject->FieldReadD();

						lTextureUVArray.Add(lUV);
                    
						lUVCount--;
					}
					while (lUVCount);

					mFileObject->FieldReadEnd();
				}
			}
        
			if(lLayerUV->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
			{
				int lTextureUVIDCount; 
            
				if (lLayerUV->GetMappingMode() == FbxLayerElement::eByControlPoint)
				{
					lTextureUVIDCount = pGeometry.mControlPoints.GetCount();
				}
				else // lLayerUV->GetMappingMode() == FbxLayerElement::eByPolygonVertex
				{
					FbxMesh* lMesh = (FbxMesh*) &pGeometry;

					lTextureUVIDCount = lMesh->mPolygonVertices.GetCount();                 
				}

				if (lTextureUVIDCount)
				{
					if (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_UV_ID_COUNT))
					{
						int lTextureUVIDCountInFile = mFileObject->FieldReadI();
						mFileObject->FieldReadEnd();

						FBX_ASSERT(lTextureUVIDCountInFile == lTextureUVIDCount);
					}

					if (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_UV_ID))
					{
						FbxLayerElementArrayTemplate<int>& lTextureUVID = lLayerUV->GetIndexArray();
						int lTextureUVCount = lLayerUV->GetDirectArray().GetCount();
                    
						do
						{
							lTextureUVID.Add(mFileObject->FieldReadI());

							if (lTextureUVID.GetLast() >= lTextureUVCount)
							{
								FBX_ASSERT_NOW("Invalid texture UV index in texture layer.");
								lTextureUVID.SetLast(0);
								lStatus = false;
							}
                        
							lTextureUVIDCount--;
						}
						while (lTextureUVIDCount);

						mFileObject->FieldReadEnd();    
					}
				}
			}
		}
	}

    return lStatus;
}


bool FbxReaderFbx5::ReadNull(FbxNull& pNull)
{
    // Root information as written by Motion Builder 4.0.
    if (mFileObject->FieldReadBegin(FIELD_PROPERTIES))
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			//int lVersion = mFileObject->FieldReadI(FIELD_PROPERTIES_VERSION, 0);

			if (mFileObject->FieldReadBegin("Size"))
			{
				pNull.Size.Set(mFileObject->FieldReadD());
				mFileObject->FieldReadEnd();
			}
                        
			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

	return true;
}


bool FbxReaderFbx5::ReadMarker(FbxMarker& pMarker)
{
	FbxDouble3 c = pMarker.Color.Get();
    FbxColor lColor(c[0], c[1], c[2]);

    // Marker information as written by Motion Builder 4.0.
    if (mFileObject->FieldReadBegin(FIELD_PROPERTIES))
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			//int lVersion = mFileObject->FieldReadI(FIELD_PROPERTIES_VERSION, 0);

			pMarker.Look.Set((FbxMarker::ELook) mFileObject->FieldReadI(FIELD_KFBXMARKER_LOOK, pMarker.Look.Get()));
			pMarker.Size.Set(mFileObject->FieldReadD(FIELD_KFBXMARKER_SIZE, pMarker.Size.Get()));

			if (mFileObject->FieldReadBegin(FIELD_KFBXMARKER_COLOR))
			{
				lColor.mRed = mFileObject->FieldReadD();
				lColor.mGreen = mFileObject->FieldReadD();
				lColor.mBlue = mFileObject->FieldReadD();

				mFileObject->FieldReadEnd();
			}
        
			if (mFileObject->FieldReadBegin(FIELD_KFBXMARKER_IK_PIVOT))
			{
				FbxVector4 lIKPivot;

				lIKPivot[0] = mFileObject->FieldReadD();
				lIKPivot[1] = mFileObject->FieldReadD();
				lIKPivot[2] = mFileObject->FieldReadD();

				pMarker.IKPivot.Set(lIKPivot);

				mFileObject->FieldReadEnd();
			}

			pMarker.ShowLabel.Set(mFileObject->FieldReadI(FIELD_KFBXMARKER_SHOW_LABEL, pMarker.ShowLabel.Get()) ? true : false);

			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }
    // Marker information as written by FiLMBOX 3.5 and previous versions.
    // Color was an animated parameter at the time.
    // This block extracts the color's default value.
    // The animated data associated with a marker's color is lost.
    else 
    {
    bool lColorChannelFound = false;

    while (!lColorChannelFound && mFileObject->FieldReadBegin("Channel"))
    {
        FbxString lChannelName = FbxObject::StripPrefix(mFileObject->FieldReadS());

        if (lChannelName.Compare("Color") == 0)
        {
            lColorChannelFound = true;
        }
        else
        {
            mFileObject->FieldReadEnd();
        }
    }

    if (lColorChannelFound)
    {
        if (mFileObject->FieldReadBlockBegin())
		{
			while (mFileObject->FieldReadBegin("Channel"))
			{
				FbxString lChannelName = FbxObject::StripPrefix(mFileObject->FieldReadS());
				double* lColorValue;

				if (lChannelName.Compare("X") == 0)
				{
					lColorValue = &(lColor.mRed);
				}
				else if(lChannelName.Compare("Y") == 0)
				{
					lColorValue = &(lColor.mGreen);
				}
				else if(lChannelName.Compare("Z") == 0)
				{
					lColorValue = &(lColor.mBlue);
				}
				else
				{
					mFileObject->FieldReadEnd();
					continue;
				}

				if (mFileObject->FieldReadBlockBegin())
				{
					*lColorValue = mFileObject->FieldReadD("Default", *lColorValue);
					mFileObject->FieldReadBlockEnd();
				}
				mFileObject->FieldReadEnd();
			}

			mFileObject->FieldReadBlockEnd();
		}
        mFileObject->FieldReadEnd();
    }

    // Reset position to allow later retrieval of other animation channels.
    mFileObject->FieldReadResetPosition();
    }

    pMarker.Color.Set(c);

	if (pMarker.GetType() == FbxMarker::eEffectorIK)
	{
		pMarker.SetDefaultIKReachTranslation(mFileObject->FieldReadD(FIELD_KFBXMARKER_IK_REACH_TRANSLATION, 0.0));
		pMarker.SetDefaultIKReachRotation(mFileObject->FieldReadD(FIELD_KFBXMARKER_IK_REACH_ROTATION, 0.0));
             
        const char *lIKReachT = "IK Reach Translation";
		FbxProperty lFbxProperty = pMarker.FindProperty(lIKReachT);
		if (!lFbxProperty.IsValid()) {
			lFbxProperty = FbxProperty::Create(&pMarker, FbxIKReachTranslationDT, lIKReachT);
			lFbxProperty.ModifyFlag( FbxPropertyFlags::eAnimatable,true );
		}

        const char *lIKReachR = "IK Reach Rotation";
		lFbxProperty = pMarker.FindProperty(lIKReachR);
		if (!lFbxProperty.IsValid()) {
			lFbxProperty = FbxProperty::Create(&pMarker, FbxIKReachRotationDT, lIKReachR);
			lFbxProperty.ModifyFlag( FbxPropertyFlags::eAnimatable,true );
		}
	}

    return true;
}


bool FbxReaderFbx5::ReadCamera(FbxCamera& pCamera)
{
    double X, Y, Z;

    // This field isn't worth being read since it must be equal to the node name in FiLMBOX.
    // FbxString lName = mFileObject->FieldReadC(FIELD_KFBXGEOMETRYCAMERA_NAME);
    int lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_VERSION);

    // Camera Position and Orientation

    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_POSITION))
	{
        X = mFileObject->FieldReadD();
        Y = mFileObject->FieldReadD();
        Z = mFileObject->FieldReadD();
        pCamera.Position.Set(FbxVector4(X, Y, Z));

		mFileObject->FieldReadEnd(); 
	}

    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_UP_VECTOR))
	{
        X = mFileObject->FieldReadD();
        Y = mFileObject->FieldReadD();
        Z = mFileObject->FieldReadD();
        pCamera.UpVector.Set(FbxVector4(X, Y, Z));

		mFileObject->FieldReadEnd();
	}

    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_DEFAULT_CAMERA_INTEREST_POSITION))
	{
        X = mFileObject->FieldReadD();
        Y = mFileObject->FieldReadD();
        Z = mFileObject->FieldReadD();
        pCamera.InterestPosition.Set(FbxVector4(X, Y, Z));

		mFileObject->FieldReadEnd();
	}

	if (mFileObject->IsBeforeVersion6())
	{
		pCamera.Roll.Set(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_CAMERA_ROLL, 0.0));	
		pCamera.ProjectionType.Set((FbxCamera::EProjectionType) mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_CAMERA_PROJECTION_TYPE, FbxCamera::ePerspective));
	}

    // Viewing Area Controls

	if (mFileObject->IsBeforeVersion6())
	{
		if (lVersion < 117)
		{
			pCamera.SetAspect((FbxCamera::EAspectRatioMode) mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_CAMERA_ASPECT_TYPE, FbxCamera::eWindowSize),
							mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_CAMERA_ASPECT_WIDTH),
							mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_CAMERA_ASPECT_HEIGHT));
		}
		else
		{
			pCamera.SetAspect((FbxCamera::EAspectRatioMode) mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_CAMERA_ASPECT_TYPE, FbxCamera::eWindowSize),
							mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_CAMERA_ASPECT_WIDTH),
							mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_CAMERA_ASPECT_HEIGHT));		
		}
	}

	if (mFileObject->IsBeforeVersion6())
	{
		pCamera.SetPixelRatio(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_CAMERA_PIXEL_RATIO, 1.0));
	
		FbxString lFormatName = mFileObject->FieldReadC(FIELD_KFBXGEOMETRYCAMERA_FORMAT_NAME);

		if (lFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_NTSC) == 0) pCamera.SetFormat(FbxCamera::eNTSC);
		else if (lFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_D1_NTSC) == 0) pCamera.SetFormat(FbxCamera::eD1NTSC);
		else if (lFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_PAL) == 0) pCamera.SetFormat(FbxCamera::ePAL);
		else if (lFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_D1_PAL) == 0) pCamera.SetFormat(FbxCamera::eD1PAL);
		else if (lFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_HD) == 0) pCamera.SetFormat(FbxCamera::eHD);
		else if (lFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_640x480) == 0) pCamera.SetFormat(FbxCamera::e640x480);
		else if (lFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_320x200) == 0) pCamera.SetFormat(FbxCamera::e320x200);
		else if (lFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_320x240) == 0) pCamera.SetFormat(FbxCamera::e320x240);
		else if (lFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_128x128) == 0) pCamera.SetFormat(FbxCamera::e128x128);
		else if (lFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_FULL_SCREEN) == 0) pCamera.SetFormat(FbxCamera::eFullscreen);
		else pCamera.SetFormat(FbxCamera::eCustomFormat);
	
		pCamera.LockMode.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_CAMERA_LOCK, false));
	}

    // Aperture and Film Controls
	if (mFileObject->IsBeforeVersion6())
	{
		pCamera.SetApertureMode((FbxCamera::EApertureMode) mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_APERTURE_MODE));	

		if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_APERTURE_DIMENSION))
		{
			pCamera.SetApertureWidth(mFileObject->FieldReadD());
			pCamera.SetApertureHeight(mFileObject->FieldReadD());

			mFileObject->FieldReadEnd();
		}

		pCamera.SetSqueezeRatio(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_CAMERA_SQUEEZERATIO));
	
		FbxString lApertureFormatName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXGEOMETRYCAMERA_APERTURE_FORMAT_NAME));

		if (!lApertureFormatName.IsEmpty())
		{
			if (lApertureFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_16MM_THEATRICAL) == 0) pCamera.SetApertureFormat(FbxCamera::e16mmTheatrical);
			else if (lApertureFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_SUPER_16MM) == 0) pCamera.SetApertureFormat(FbxCamera::eSuper16mm);
			else if (lApertureFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_35MM_ACADEMY) == 0) pCamera.SetApertureFormat(FbxCamera::e35mmAcademy);
			else if (lApertureFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_35MM_TV_PROJECTION) == 0) pCamera.SetApertureFormat(FbxCamera::e35mmTVProjection);
			else if (lApertureFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_35MM_FULL_APERTURE) == 0) pCamera.SetApertureFormat(FbxCamera::e35mmFullAperture);
			else if (lApertureFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_35MM_185_PROJECTION) == 0) pCamera.SetApertureFormat(FbxCamera::e35mm185Projection);
			else if (lApertureFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_35MM_ANAMORPHIC) == 0) pCamera.SetApertureFormat(FbxCamera::e35mmAnamorphic);
			else if (lApertureFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_70MM_PROJECTION) == 0) pCamera.SetApertureFormat(FbxCamera::e70mmProjection);
			else if (lApertureFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_VISTA_VISION) == 0) pCamera.SetApertureFormat(FbxCamera::eVistaVision);
			else if (lApertureFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_DYNAVISION) == 0) pCamera.SetApertureFormat(FbxCamera::eDynaVision);
			else if (lApertureFormatName.Compare(TOKEN_KFBXGEOMETRYCAMERA_IMAX) == 0) pCamera.SetApertureFormat(FbxCamera::eIMAX);
			else pCamera.SetApertureFormat(FbxCamera::eCustomAperture);
		}
		else
		{
			pCamera.SetApertureFormat(FbxCamera::e35mmTVProjection);
		}
	
		pCamera.SetNearPlane(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_CAMERA_NEAR_PLANE, 10.0));
		pCamera.SetFarPlane(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_CAMERA_FAR_PLANE, 4000.0));

		if (lVersion >= 210)
		{
			pCamera.FocalLength.Set(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_CAMERA_FOCAL_LENGTH, 35.0));
		}
		else
		{
			pCamera.FieldOfView.Set(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_CAMERA_APERTURE));
		}

		if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_APERTURE_X))
		{
			pCamera.FieldOfViewX.Set(mFileObject->FieldReadD());
			mFileObject->FieldReadEnd();
		}
		if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_APERTURE_Y))
		{
			pCamera.FieldOfViewY.Set(mFileObject->FieldReadD());
			mFileObject->FieldReadEnd();
		}

		if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_OPTICAL_CENTER_X))
		{
			pCamera.OpticalCenterX.Set(mFileObject->FieldReadD());
			mFileObject->FieldReadEnd();
		}
		if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_OPTICAL_CENTER_Y))
		{
			pCamera.OpticalCenterY.Set(mFileObject->FieldReadD());
			mFileObject->FieldReadEnd();
		}

		if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_TURNTABLE))
		{
			pCamera.TurnTable.Set(mFileObject->FieldReadD());
			mFileObject->FieldReadEnd();
		}
	}


    // Background Properties
	if (mFileObject->IsBeforeVersion6())
	{
		if (lVersion >= 200)
		{
			pCamera.SetBackgroundMediaName(mFileObject->FieldReadC(FIELD_KFBXGEOMETRYCAMERA_BACKGROUND_MEDIA_NAME)); 
		}
		else
		{
			if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_BACKGROUND_TEXTURE)) 
			{
				pCamera.SetBackgroundFileName(mFileObject->FieldReadC());

				mFileObject->FieldReadEnd();
			}

			if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_BACKGROUND_VIDEO_CLIP_TEXTURE)) 
			{
				pCamera.SetBackgroundFileName(mFileObject->FieldReadC());
	                
				// The FBX SDK does not support video clip medias yet.
				//if (mFileObject->FieldReadBlockBegin()) 
				//{ 
				//  int lVideoClipStartFrame = mFileObject->FieldReadI("StartFrame");
				//  int lVideoClipEndFrame = mFileObject->FieldReadI("StopFrame");
				//  int lVideoClipPlayStepFrame = mFileObject->FieldReadI("PlayStep");
				//  int lVideoClipCycleMode = mFileObject->FieldReadI("Cycle");
				//  int lVideoClipFreeRunningMode = mFileObject->FieldReadI("FreeRunning");
				//  FbxTime lVideoClipStartTime = mFileObject->FieldReadT("StartTime");
				//  int lVideoClipProxyMode = mFileObject->FieldReadI("Proxy");
				//  mFileObject->FieldReadBlockEnd(); 
				//}
	            
				mFileObject->FieldReadEnd();
			}

			if (pCamera.GetBackgroundFileName())
			{
				mCameraBackgroundArrayName.Add(const_cast<char*>(pCamera.GetBackgroundFileName()), (FbxHandle) &pCamera);
			}
		}
	}

	if (mFileObject->IsBeforeVersion6())
	{
		pCamera.ViewFrustumBackPlaneMode.Set((FbxCamera::EFrontBackPlaneDisplayMode) mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_BACKGROUND_DISPLAY_MODE, FbxCamera::ePlanesWhenMedia));
		pCamera.ShowFrontplate.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_FOREGROUND_MATTE_THRESHOLD_ENABLE, true));
		pCamera.BackgroundAlphaTreshold.Set(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_FOREGROUND_MATTE_TRESHOLD, 0.5));
		
        //SetBackgroundPlacementOptions
		// WARNING: the function GetBackgroundPlacementOptions() in kfbxwriterfbx.cxx file 
		// is directly writing the flags using the order: (lFit<<0)|(lCenter<<1)|(lKeepRatio<<2)|(lCrop<<3) so 
		// it is important to keep the defines sync. But, honestly, these defines should never change anyway.
		#define eFIT		(1<<0)
		#define eCENTER		(1<<1)
		#define eKEEP_RATIO (1<<2)
		#define eCROP		(1<<3)

        FbxUInt lOptions = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_BACKGROUND_PLACEMENT_OPTIONS, eCENTER | eKEEP_RATIO);
        bool lFit = (lOptions & eFIT) != 0;
        bool lCenter = (lOptions & eCENTER) != 0;
        bool lKeepRatio = (lOptions & eKEEP_RATIO) != 0;
        bool lCrop = (lOptions & eCROP) != 0;

		#undef eFIT
		#undef eCENTER
		#undef eKEEP_RATIO
		#undef eCROP

        pCamera.BackPlateFitImage.Set(lFit);
        pCamera.BackPlateCenter.Set(lCenter);
        pCamera.BackPlateKeepRatio.Set(lKeepRatio);
        pCamera.BackPlateCrop.Set(lCrop);
		
        pCamera.BackPlaneDistance.Set(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_BACKGROUND_DISTANCE, 100.0));
		pCamera.BackPlaneDistanceMode.Set((FbxCamera::EFrontBackPlaneDistanceMode) mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_BACKGROUND_DISTANCE_MODE, FbxCamera::eRelativeToInterest));
	}

    // Camera View Options
	if (mFileObject->IsBeforeVersion6())
	{
		pCamera.ViewCameraToLookAt.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_VIEW_CAMERA_INTEREST, true));
		pCamera.ViewFrustumNearFarPlane.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_VIEW_NEAR_FAR_PLANES, false));
		pCamera.ShowGrid.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_SHOW_GRID, true));
		pCamera.ShowAzimut.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_SHOW_AXIS, true));
		pCamera.ShowName.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_SHOW_NAME, true));
		pCamera.ShowTimeCode.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_SHOW_TIME_CODE, false));
		pCamera.DisplaySafeArea.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_DISPLAY_SAFE_AREA, false));
		pCamera.SafeAreaDisplayStyle.Set((FbxCamera::ESafeAreaStyle) mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_SAFE_AREA_STYLE, FbxCamera::eSafeAreaSquare)); 
		pCamera.DisplaySafeAreaOnRender.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_DISPLAY_SAFE_AREA_ON_RENDER, false));
	}
	pCamera.ShowInfoOnMoving.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_SHOW_INFO_ON_MOVING, true));
	pCamera.ShowAudio.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_SHOW_AUDIO, false));

	if (mFileObject->IsBeforeVersion6())
	{
		if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_BACKGROUND_COLOR))
		{
			FbxVector4 lColor;
			lColor[0] = mFileObject->FieldReadD();
			lColor[1] = mFileObject->FieldReadD();
			lColor[2] = mFileObject->FieldReadD();

			pCamera.BackgroundColor.Set(lColor);

			mFileObject->FieldReadEnd();
		}
		else
		{
			FbxVector4 lColor(0.0, 0.0, 0.0);
			pCamera.BackgroundColor.Set(lColor);
		}
	}

    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_AUDIO_COLOR))
    {
		FbxVector4 lColor;
        lColor[0] = mFileObject->FieldReadD();
        lColor[1] = mFileObject->FieldReadD();
        lColor[2] = mFileObject->FieldReadD();

		pCamera.AudioColor.Set(lColor);

        mFileObject->FieldReadEnd();
    }
    else
    {
        FbxVector4 lColor(0.0, 1.0, 0.0);
		pCamera.AudioColor.Set(lColor);
    }

    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_ORTHO_ZOOM))
    {
		double lOrthoZoom = mFileObject->FieldReadD();

		pCamera.OrthoZoom.Set(lOrthoZoom);

        mFileObject->FieldReadEnd();
    }
    else
    {
		pCamera.OrthoZoom.Set(1.0);
    }

	if (mFileObject->IsBeforeVersion6())
	{
		pCamera.UseFrameColor.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_USE_FRAME_COLOR, false));

		if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_FRAME_COLOR))
		{
			FbxVector4 lColor;
			lColor[0] = mFileObject->FieldReadD();
			lColor[1] = mFileObject->FieldReadD();
			lColor[2] = mFileObject->FieldReadD();

			pCamera.FrameColor.Set(lColor);

			mFileObject->FieldReadEnd();
		}
		else
		{
			FbxVector4 lColor(0.3, 0.3, 0.3);
			pCamera.FrameColor.Set(lColor);
		}
	}

    // Render Options

	if (mFileObject->IsBeforeVersion6())
	{
		if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_LENS))
		{
			if (mFileObject->FieldReadBlockBegin())
			{
				if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_LENS_DEPTH_OF_FIELD))
				{
					pCamera.UseDepthOfField.Set(mFileObject->FieldReadB());
					pCamera.FocusSource.Set((FbxCamera::EFocusDistanceSource) mFileObject->FieldReadI());
					pCamera.FocusAngle.Set(mFileObject->FieldReadD());
					pCamera.FocusDistance.Set(mFileObject->FieldReadD());

					mFileObject->FieldReadEnd(); 
				}

				if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_LENS_ANTIALIASING))
				{
					pCamera.UseRealTimeDOFAndAA.Set(mFileObject->FieldReadB());
					pCamera.AntialiasingMethod.Set((FbxCamera::EAntialiasingMethod) mFileObject->FieldReadI());
					pCamera.AntialiasingIntensity.Set(mFileObject->FieldReadD());
					mFileObject->FieldReadEnd();
				}

				if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_LENS_OVERSAMPLING))
				{
					pCamera.FrameSamplingCount.Set(mFileObject->FieldReadI());
					pCamera.FrameSamplingType.Set((FbxCamera::ESamplingType) mFileObject->FieldReadI());
					pCamera.UseAccumulationBuffer.Set(mFileObject->FieldReadB());

					mFileObject->FieldReadEnd();
				}

				pCamera.UseRealTimeDOFAndAA.Set(FbxCamera::eInteractive == (FbxCamera::ERenderOptionsUsageTime) mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_RENDER_OPTIONS_USAGE_TIME, FbxCamera::eOnDemand));

				mFileObject->FieldReadBlockEnd();
			}

			mFileObject->FieldReadEnd(); 
		}
	}

    return true;
}


bool FbxReaderFbx5::ReadLight(FbxLight& pLight)
{
	int lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYLIGHT_LIGHT_TYPE_VERSION, 0);
	if( lVersion > 201 )
	{
		if( mFileObject->FieldReadBegin("Color") )
		{
			FbxVector4 lColor;
			lColor[0] = mFileObject->FieldReadD();
			lColor[1] = mFileObject->FieldReadD();
			lColor[2] = mFileObject->FieldReadD();
			mFileObject->FieldReadEnd();
			pLight.Color.Set(lColor);
		}
		pLight.Intensity.Set(mFileObject->FieldReadD("Intensity"));
		pLight.OuterAngle.Set(mFileObject->FieldReadD("ConeAngle"));
		pLight.Fog.Set(mFileObject->FieldReadD("Fog"));
	}
	pLight.LightType.Set((FbxLight::EType)mFileObject->FieldReadI(FIELD_KFBXGEOMETRYLIGHT_LIGHT_TYPE, FbxLight::ePoint));
	pLight.CastLight.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYLIGHT_CAST_LIGHT, true));

    return true;
}


bool FbxReaderFbx5::ReadMeshVertices(FbxMesh& pMesh)
{
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_VERTICES))
    {
        int lCount, lTotalCount = (mFileObject->FieldReadGetCount () / 3);

        pMesh.mControlPoints.Resize(lTotalCount);

        for (lCount = 0; lCount < lTotalCount; lCount ++)
        {
            FbxVector4& lVector = pMesh.GetControlPoints()[lCount];
            mFileObject->FieldRead3D (lVector.mData);
        }

        mFileObject->FieldReadEnd ();
    }

    return true;
}


bool FbxReaderFbx5::ReadMeshNormals (FbxMesh& pMesh)
{
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_NORMALS))
    {

		int lCount;
		// Now we don't read the normals for NumberofControlPoints times. We read in the 
		// file the number of normal and we initialize the normal on that value
		int lTotalCount = (mFileObject->FieldReadGetCount () / 3);

		pMesh.InitNormals (lTotalCount);
		
		FbxLayerElementArrayTemplate<FbxVector4>* direct;
		pMesh.GetNormals(&direct);
		FBX_ASSERT(direct != NULL);
		FbxVector4* lNormals = NULL;
		lNormals = direct->GetLocked(lNormals);

        for (lCount = 0; lCount < lTotalCount; lCount ++)
        {
            mFileObject->FieldRead3D (lNormals[lCount].mData);
        }

		direct->Release(&lNormals, lNormals);
        mFileObject->FieldReadEnd ();
    }

    return true;
}


bool FbxReaderFbx5::ReadMeshAssignation (FbxMesh& pMesh)
{
	// In v6.0, Material/Texture assignation info is in the layer, not in the model
	if (!mFileObject->IsBeforeVersion6()) return true;

    const char* lAssignType;
     
    lAssignType = mFileObject->FieldReadC (FIELD_KFBXGEOMETRYMESH_MATERIAL_ASSIGNATION);
        
    if (lAssignType)
    {
        if (!strcmp (lAssignType, TOKEN_KFBXGEOMETRYMESH_BY_VERTICE)) 
        {
            pMesh.InitMaterialIndices(FbxLayerElement::eByControlPoint);
        } 
        else if (!strcmp (lAssignType, TOKEN_KFBXGEOMETRYMESH_BY_POLYGON)) 
        {
            pMesh.InitMaterialIndices(FbxLayerElement::eByPolygon);
        } 
        else if (!strcmp (lAssignType, TOKEN_KFBXGEOMETRYMESH_ALL_SAME)) 
        {
            pMesh.InitMaterialIndices(FbxLayerElement::eAllSame);
        } 
        else 
        {
            //pMesh.InitMaterialIndices(FbxMesh::eAllSame);
        }
    }
    else
    {
        //pMesh.InitMaterialIndices(FbxMesh::eAllSame);
    }

    lAssignType = mFileObject->FieldReadC (FIELD_KFBXGEOMETRYMESH_TEXTURE_ASSIGNATION);
        
    if (lAssignType && mTemporaryTextures.GetCount())
    {
        if (!strcmp (lAssignType, TOKEN_KFBXGEOMETRYMESH_BY_POLYGON)) 
        {
			pMesh.InitTextureIndices(FbxLayerElement::eByPolygon, FbxLayerElement::eTextureDiffuse);
        }

        // Eric:: if the mapping mode is None, set it to eAllSame as default value
        else// if (!strcmp (lAssignType, TOKEN_KFBXGEOMETRYMESH_BY_MODEL)) 
        {
            pMesh.InitTextureIndices(FbxLayerElement::eAllSame, FbxLayerElement::eTextureDiffuse);
        } 
    }

    return true;
}


bool FbxReaderFbx5::ReadMeshPolygonIndex (FbxMesh& pMesh)
{
    int        lIndexCount;
    int        lCount;

    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_POLYGON_INDEX))
    {
        lIndexCount = mFileObject->FieldReadGetCount ();

        if (lIndexCount > 0)
        {
			int lIndex;

            pMesh.BeginPolygon ();

            for (lCount = 0; lCount < lIndexCount; lCount ++)
            {
				lIndex = mFileObject->FieldReadI ();

				if (lIndex < 0)
                {
					pMesh.AddPolygon ((FbxAbs (lIndex)) - 1);
                    pMesh.EndPolygon ();
                    if (lCount < lIndexCount - 1)
                    {
                        pMesh.BeginPolygon ();
                    }
                }
                else
                {
					pMesh.AddPolygon (lIndex);
                }
            }
        }

        mFileObject->FieldReadEnd();
    }

    return true;
}


bool FbxReaderFbx5::ReadMeshPolyGroupIndex (FbxMesh& pMesh)
{
    int i, lPolygonCount = pMesh.GetPolygonCount();
    bool lFoundPolygonGroup = false;

    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_POLYGON_GROUP))
    {
        if (mFileObject->FieldReadGetCount() == lPolygonCount)
        {
			FbxLayerElementPolygonGroup* lLayerElementPolygonGroup = NULL;
			FbxLayer*                    lLayer;

			lLayer = pMesh.GetLayer(0);
			if (!lLayer) 
			{
				int lLayerAbsoluteIndex = pMesh.CreateLayer();
				lLayer = pMesh.GetLayer(lLayerAbsoluteIndex);
			}

			lLayerElementPolygonGroup = lLayer->GetPolygonGroups();
			if (!lLayerElementPolygonGroup)
			{
				lLayerElementPolygonGroup = FbxLayerElementPolygonGroup::Create(&pMesh, "");
				lLayer->SetPolygonGroups(lLayerElementPolygonGroup);
			}

			lLayerElementPolygonGroup->SetReferenceMode(FbxLayerElement::eIndex);
			lLayerElementPolygonGroup->SetMappingMode(FbxLayerElement::eByPolygon);

			FbxLayerElementArrayTemplate<int>& lPolygonGroupIndices = lLayerElementPolygonGroup->GetIndexArray();
			lPolygonGroupIndices.SetCount(lPolygonCount);

            lFoundPolygonGroup = true;

            for (i = 0; i < lPolygonCount; i++)
            {
                pMesh.SetPolygonGroup(i, mFileObject->FieldReadI());
				lPolygonGroupIndices.SetAt(i, pMesh.GetPolygonGroup(i));
            }
        }

        mFileObject->FieldReadEnd();
    }
    
    if (!lFoundPolygonGroup)
    {
        for (i = 0; i < lPolygonCount; i++)
        {
            pMesh.SetPolygonGroup(i, 0);
        }
    }

    return true;
}


bool FbxReaderFbx5::ReadMeshMaterialsID (FbxMesh& pMesh)
{
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_MATERIALS_ID))
    {
        int lCount, lMaterialCount = mFileObject->FieldReadGetCount ();
		FbxLayerElementMaterial* lLayerElementMaterial = NULL;
		FbxLayer*                lLayer;
		
		lLayer = pMesh.GetLayer(0);
		if (!lLayer) 
		{
			int lLayerAbsoluteIndex = pMesh.CreateLayer();
			lLayer = pMesh.GetLayer(lLayerAbsoluteIndex);
		}

		lLayerElementMaterial = lLayer->GetMaterials();
		if (!lLayerElementMaterial)
		{
			lLayerElementMaterial = FbxLayerElementMaterial::Create(&pMesh, "");
			lLayer->SetMaterials(lLayerElementMaterial);

		}

		if (lLayerElementMaterial->GetReferenceMode() == FbxLayerElement::eIndexToDirect ||
			lLayerElementMaterial->GetReferenceMode() == FbxLayerElement::eIndex)
		{
			// This should always be the case for FBX file v6 and Up. For old version (<6),
			// of the file format, the mapping FbxLayerElement::eAllSame was stored
			// with the ID "0". This info is irrelevent with the new structure.
			FbxLayerElementArrayTemplate<int>& lMaterialIndices = lLayerElementMaterial->GetIndexArray();
			
			lMaterialIndices.SetCount(lMaterialCount);

			for (lCount = 0; lCount < lMaterialCount; lCount ++)
			{
				lMaterialIndices.SetAt(lCount, mFileObject->FieldReadI());
			}
		}
    
		if (lLayerElementMaterial->GetReferenceMode() == FbxLayerElement::eIndexToDirect &&
			lLayerElementMaterial->mDirectArray->GetCount() == 0)
		{
			// Catch an inconsistency in old FBX Files, where ALL_SAME was set for material at
			// index 0, but this material did not exist. It used to fallback to the default material.
			lLayer->SetMaterials(NULL);
			lLayerElementMaterial->Destroy();
		}

        mFileObject->FieldReadEnd();
    }
    else
    {
		/* Obsolete code. Remove for MB 6.0. Luc
        int lLastMaterial = pMesh.GetMaterialCount() - 1;

        lMaterialIndices.SetCount(1);

        if (lLastMaterial >= 0)
        {
            lMaterialIndices[0] = lLastMaterial;
        }
        else
        {
            lMaterialIndices[0] = 0;
        }*/
    }

    return true;
}


bool FbxReaderFbx5::ReadMeshTexturesID (FbxMesh& pMesh)
{
	FbxLayerElementTexture* lLayerElementTexture = NULL;
	FbxLayer*               lLayer;

	lLayer = pMesh.GetLayer(0);
	if (!lLayer) 
	{
		int lLayerAbsoluteIndex = pMesh.CreateLayer();
		lLayer = pMesh.GetLayer(lLayerAbsoluteIndex);
	}

    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_TEXTURE_ID))
    {
        lLayerElementTexture = lLayer->GetTextures(FbxLayerElement::eTextureDiffuse);
        if (!lLayerElementTexture)
        {
            lLayerElementTexture = FbxLayerElementTexture::Create(&pMesh, "");
            lLayer->SetTextures(FbxLayerElement::eTextureDiffuse, lLayerElementTexture);
        }

        int lCount, lTextureCount = mFileObject->FieldReadGetCount ();

        FbxLayerElementArrayTemplate<int>& lTextureIndices = lLayerElementTexture->GetIndexArray();

        // Another solution would be to simply call SetAt(lCount, index) but in the "impossible" case
        // the lTextureCount does not match the size of lTextureIndices we may try to set outside
        // the allocated size and we would crash!
        int x = lTextureIndices.GetCount();
        FBX_ASSERT(x == lTextureCount);
        lTextureIndices.Clear();
        for (lCount = 0; lCount < lTextureCount; lCount ++)
        {
            int index = mFileObject->FieldReadI();
            lTextureIndices.Add(index);
        }

        // Note: reference mode and mapping mode of the layer element
        // is done thru the obsolete function FbxMesh::InitMTextureIndices
        // in ReadMeshAssignation

        mFileObject->FieldReadEnd();
    }
    else
    {
		// This situation should arise only when texture mapping
		// is eAllSame
		lLayerElementTexture = lLayer->GetTextures(FbxLayerElement::eTextureDiffuse);
		
		if (lLayerElementTexture)
		{
			FbxLayerElementArrayTemplate<int>& lTextureIndices = lLayerElementTexture->GetIndexArray();

			int lLastTexture = pMesh.GTC(0, FbxLayerElement::eTextureDiffuse) - 1;

			if (lLastTexture >= 0)
			{
				lTextureIndices.Add(lLastTexture);
			}
/*			else
			{
				lTextureIndices.Add(0);
			}
*/		}
    }

    return true;
}


bool FbxReaderFbx5::ReadMeshTextureType (FbxMesh& pMesh)
{
    const char* lTextureType = NULL;
    int lUVType = 0;
    FbxLayerElement::EMappingMode lMappingType;

    // GeometryVersion >= 123, Version in GeometryUVInfo block == 240
    {
        lTextureType = mFileObject-> FieldReadC (FIELD_KFBXGEOMETRYMESH_MAPPING_INFO_TYPE);

        lMappingType = FbxLayerElement::eAllSame;

        if (lTextureType)
        {
            if (!strcmp (lTextureType, TOKEN_KFBXGEOMETRYMESH_BY_VERTICE)) 
            {
                lMappingType = FbxLayerElement::eByControlPoint;
            } 
            else if (!strcmp (lTextureType, TOKEN_KFBXGEOMETRYMESH_BY_POLYGON)) 
            {
                lMappingType = FbxLayerElement::eByPolygonVertex;
            } 
        }   
    }

    // GeometryVersion >= 123, Version in GeometryUVInfo block == 222
    if (!lTextureType)
    {
        lUVType = mFileObject-> FieldReadI (FIELD_KFBXGEOMETRYMESH_UV_TYPE, 0);

        if (lUVType == 1) // KGPS_TEXTURECOORDINATE_BYVERTEX
        {
            lMappingType = FbxLayerElement::eByPolygonVertex;
        }
        else if (lUVType == 2) // KGPS_TEXTURECOORDINATE_BYPOLYGON
        {
            lMappingType = FbxLayerElement::eByPolygon;
        }
        else // Make sure we didn't caught garbage...
        {
            lUVType = 0; 
        }
    }

    // GeometryVersion < 123
    if (!lTextureType && !lUVType)
    {
        lTextureType = mFileObject->FieldReadC (FIELD_KFBXGEOMETRYMESH_TEXTURE_TYPE);
    
        lMappingType = FbxLayerElement::eByPolygonVertex;

        if (lTextureType)
        {
            if (!strcmp (lTextureType, TOKEN_KFBXGEOMETRYMESH_BY_VERTICE)) 
            {
                lMappingType = FbxLayerElement::eByPolygonVertex;
            } 
            else if (!strcmp (lTextureType, TOKEN_KFBXGEOMETRYMESH_BY_FACE)) 
            {
                lMappingType = FbxLayerElement::eByPolygon;
            } 
        }
    }

    pMesh.InitTextureUVIndices(lMappingType);

    return true;
}


bool FbxReaderFbx5::ReadMeshTextureUV (FbxMesh& pMesh)
{
    int        lCount;

    //
    // Texture UV...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_TEXTURE_UV) ||
        // Special case:
        // GeometryVersion >= 123 &&
        // Version in GeometryUVInfo block == 222 &&
        // pMesh.mTextureUVMappingType == FbxGeometry::eByPolygon
        mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_TEXTURE_POLYGON_UV)) 
    {
        int lVertexTextureCount  = pMesh.mControlPoints.GetCount();

        if (pMesh.GetLayer(0)->GetUVs()->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
        {
            lVertexTextureCount = (mFileObject->FieldReadGetCount () / 2);
        }

        pMesh.InitTextureUV (lVertexTextureCount);

        FbxVector2* TextureUV = NULL;
		FbxLayerElementArrayTemplate<FbxVector2>* lUVsDA;
		if (pMesh.GetTextureUV(&lUVsDA))
			TextureUV = lUVsDA->GetLocked(TextureUV);
		FBX_ASSERT(TextureUV != NULL);

        for (lCount = 0; lCount < lVertexTextureCount; lCount ++)
        {
            TextureUV [lCount][0] = mFileObject->FieldReadD ();
            TextureUV [lCount][1] = mFileObject->FieldReadD ();
        }

		if (lUVsDA)
			lUVsDA->Release(&TextureUV, TextureUV);

        mFileObject->FieldReadEnd ();
    }

    return true;
}


bool FbxReaderFbx5::ReadMeshTextureIndex (FbxMesh& pMesh)
{
    int        lIndexCount;
    int        lCount;    

    //
    // Retrieve the texture index vertex direct in mTextureUVIndex because we can't pass
    // by the AddPolygon method.
    //
    if (pMesh.GetLayer(0)->GetUVs()->GetMappingMode() == FbxLayerElement::eByPolygonVertex)
    {
		FbxLayerElementArrayTemplate<int>& lTextureUVIndex = pMesh.GetLayer(0)->GetUVs()->GetIndexArray();

        if (// GeometryVersion < 123 
            mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_TEXTURE_VERTEX_INDEX) || 
            // GeometryVersion >= 123 && Version in GeometryUVInfo block == 240
            mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_TEXTURE_UV_INDEX))
        {
            lIndexCount = mFileObject->FieldReadGetCount ();

            if (lIndexCount > 0)
            {
                lTextureUVIndex.Clear();
                lTextureUVIndex.SetCount(lIndexCount);
    
                for (lCount = 0; lCount < lIndexCount; lCount ++)
                {
                    lTextureUVIndex.SetAt(lCount, mFileObject->FieldReadI ());
                }
            }

            mFileObject->FieldReadEnd ();
        }
        else
        // GeometryVersion >= 123 && Version in GeometryUVInfo block == 222
        {
            lIndexCount = pMesh.GetTextureUVCount();

            if (lIndexCount > 0)
            {
                lTextureUVIndex.Clear();    
                lTextureUVIndex.SetCount(lIndexCount);
    
                for (lCount = 0; lCount < lIndexCount; lCount ++)
                {
                    lTextureUVIndex.SetAt(lCount, lCount);
                }
            }
        }
    }

    return true;
}

bool FbxReaderFbx5::ReadMeshVertexColors(FbxMesh& pMesh)
{
	bool lValid = true;

	if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_INFO))
	{
		FbxLayer*                   lLayer;
		FbxLayerElementVertexColor* lLayerElementVertexColor;

		lLayer = pMesh.GetLayer(0);
		if (!lLayer) 
		{
			int lLayerAbsoluteIndex = pMesh.CreateLayer();
			lLayer = pMesh.GetLayer(lLayerAbsoluteIndex);
		}

		lLayerElementVertexColor = lLayer->GetVertexColors();
		if (!lLayerElementVertexColor)
		{
			lLayerElementVertexColor = FbxLayerElementVertexColor::Create(&pMesh, "");
			lLayer->SetVertexColors(lLayerElementVertexColor);
		}

		int             lVersion    = 100;
		FbxString         lMappingMode;

		if (mFileObject->FieldReadBlockBegin())
		{
			lVersion     = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_VERSION);
			lMappingMode = mFileObject->FieldReadC(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_ASSIGNATION);

			lLayerElementVertexColor->SetReferenceMode(FbxLayerElement::eIndexToDirect);

			if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_VALUES))
			{
				int lElementIndex;
				int lElementCount = mFileObject->FieldReadGetCount() / 4; // RGBA
				FbxLayerElementArrayTemplate<FbxColor>& lColorArray = lLayerElementVertexColor->GetDirectArray();
				FbxColor lColor;

				lColorArray.SetCount(lElementCount);

				for (lElementIndex = 0; lElementIndex < lElementCount; lElementIndex++)
				{
					lColor.mRed   = mFileObject->FieldReadD();
					lColor.mGreen = mFileObject->FieldReadD();
					lColor.mBlue  = mFileObject->FieldReadD();
					lColor.mAlpha = mFileObject->FieldReadD();

					lColorArray.SetAt(lElementIndex, lColor);
				}

				mFileObject->FieldReadEnd();
			}

			if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_INDEX))
			{
				int lElementIndex;
				int lElementCount = mFileObject->FieldReadGetCount();
				FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementVertexColor->GetIndexArray();
				
				lIndexArray.SetCount(lElementCount);

				for (lElementIndex = 0; lElementIndex < lElementCount; lElementIndex++)
				{
					lIndexArray.SetAt(lElementIndex,  mFileObject->FieldReadI());
				}

				mFileObject->FieldReadEnd();
			}

			mFileObject->FieldReadBlockEnd();
		}

		mFileObject->FieldReadEnd();

		//
		// Check for consistency
		//
		lValid = false;

		if (lLayerElementVertexColor->GetDirectArray().GetCount())
		{
			if (lLayerElementVertexColor->GetIndexArray().GetCount())
			{
				// Index to Direct
				lLayerElementVertexColor->SetReferenceMode(FbxLayerElement::eIndexToDirect);

				if (lMappingMode == TOKEN_KFBXGEOMETRYMESH_BY_VERTICE)
				{
					lLayerElementVertexColor->SetMappingMode(FbxLayerElement::eByControlPoint);

					if (lLayerElementVertexColor->GetIndexArray().GetCount() == pMesh.mControlPoints.GetCount())
					{
						lValid = true;
					}
				}
				else if (lMappingMode == TOKEN_KFBXGEOMETRYMESH_BY_POLYGON_VERTEX)
				{
					lLayerElementVertexColor->SetMappingMode(FbxLayerElement::eByPolygonVertex);

					if (lLayerElementVertexColor->GetIndexArray().GetCount() == pMesh.mPolygonVertices.GetCount())
					{
						lValid = true;
					}				
				}
			}
			else
			{
				// Direct
				lLayerElementVertexColor->SetReferenceMode(FbxLayerElement::eDirect);

				if (lMappingMode == TOKEN_KFBXGEOMETRYMESH_BY_VERTICE)
				{
					lLayerElementVertexColor->SetMappingMode(FbxLayerElement::eByControlPoint);

					if (lLayerElementVertexColor->GetDirectArray().GetCount() == pMesh.mControlPoints.GetCount())
					{
						lValid = true;
					}
				}
				else if (lMappingMode == TOKEN_KFBXGEOMETRYMESH_BY_POLYGON_VERTEX)
				{
					lLayerElementVertexColor->SetMappingMode(FbxLayerElement::eByPolygonVertex);

					if (lLayerElementVertexColor->GetDirectArray().GetCount() == pMesh.mPolygonVertices.GetCount())
					{
						lValid = true;
					}				
				}
			}
		}

		if (!lValid)
		{
			// Make sure we clear everything
			lLayerElementVertexColor->GetDirectArray().Clear();
			lLayerElementVertexColor->GetIndexArray().Clear();
		}
	}

	return lValid;
}

bool FbxReaderFbx5::ReadMesh (FbxMesh& pMesh)
{
    int    lVersion;
    
    lVersion = mFileObject->FieldReadI (FIELD_KFBXGEOMETRYMESH_GEOMETRY_VERSION, 0);

	/* * Obsolete code. Remove for MB 6.0. Luc
    if (!pMesh.GetLayer(0))
    {
        pMesh.CreateLayer();
    }
	*/

    ReadMeshVertices(pMesh);
	ReadMeshPolygonIndex(pMesh);

	if(mFileObject->IsBeforeVersion6())
	{

		ReadMeshNormals(pMesh);
		FBX_ASSERT_RETURN_VALUE(GetStatus().GetCode() != FbxStatus::eInvalidFile, false)
		ReadMeshPolyGroupIndex(pMesh);
		FBX_ASSERT_RETURN_VALUE(GetStatus().GetCode() != FbxStatus::eInvalidFile, false)
		ReadMeshVertexColors(pMesh);
		FBX_ASSERT_RETURN_VALUE(GetStatus().GetCode() != FbxStatus::eInvalidFile, false)

		// Shapes must be read after vertices and normals.
		// Materials must be read before material indices.
		// Textures must be read before texture indices.
		ReadGeometry(pMesh);
		FBX_ASSERT_RETURN_VALUE(GetStatus().GetCode() != FbxStatus::eInvalidFile, false);
		ReadMeshAssignation(pMesh);
		FBX_ASSERT_RETURN_VALUE(GetStatus().GetCode() != FbxStatus::eInvalidFile, false);
		ReadMeshMaterialsID(pMesh);
		FBX_ASSERT_RETURN_VALUE(GetStatus().GetCode() != FbxStatus::eInvalidFile, false);
		ReadMeshTexturesID(pMesh);
		FBX_ASSERT_RETURN_VALUE(GetStatus().GetCode() != FbxStatus::eInvalidFile, false);

		//
		// Texture management 
		//
    
		// Texture UV mapping went through 3 versions.
		// Here's a quick description of each version:

		/////////////////////////////////////////////////////////////////////////////
		//
		// Version < 123
		//
		/////////////////////////////////////////////////////////////////////////////
		//
		// field: "TextureType", type: string, allowed values: {"ByVertice", "ByFace"}, default value: "ByVertice"
		//
		// if "TextureType: "ByVertice""
		//      field "TextureUV", type: array of double, values: one pair per polygon vertex
		// 
		// else if "TextureType: "ByFace""
		//      field "TextureUV", type: array of double, values: an undetermined amount of pair of doubles
		//      field "TextureVertexIndex", type: array of integer, values: an integer per polygon
		//
		/////////////////////////////////////////////////////////////////////////////
    
		/////////////////////////////////////////////////////////////////////////////
		//
		// Version >= 123, field Version in GeometryUVInfo block == 222
		//
		/////////////////////////////////////////////////////////////////////////////
		//
		// open block "GeometryUVInfo"
		//
		// field "Version", type: integer, value: 222
		// field "MappingStyle", type: integer, allowed values: {0 (KGPS_NOMAPPING), 1 (KGPS_MAPPING_UV)}, unused field
		// field "UVType", type: integer, allowed values: {0 (KGPS_TEXTURECOORDINATE_NONE), 1 (KGPS_TEXTURECOORDINATE_BYVERTEX), 2 (KGPS_TEXTURECOORDINATE_BYPOLYGON)}
		//
		// if "UVType: 1"
		//      field "TextureUV", type: array of double, values: one pair per polygon vertex
		//
		// else if "UVType: 2"
		//      field "TexturePUV", type: array of double, values: one pair per polygon
		//
		// close block "GeometryUVInfo"
		//
		/////////////////////////////////////////////////////////////////////////////

		/////////////////////////////////////////////////////////////////////////////
		//
		// Version >= 123, field Version in GeometryUVInfo block == 240
		//
		/////////////////////////////////////////////////////////////////////////////
		//
		// open block "GeometryUVInfo"
		//
		// field "Version", type: integer, value: 240
		// field "MappingInformationType", type: string, allowed values: {"ByVertice", "ByPolygon", "NoMappingInformation"}, default value: "NoMappingInformation"
		//
		// if "MappingInformationType: "ByVertice""
		//      field "TextureUV", type: array of double, values: one pair per polygon vertex
		//
		// else if "MappingInformationType: "ByPolygon""
		//      field "TextureUV", type: array of double, values: an undetermined amount of pair of doubles
		//      field "TextureUVVerticeIndex", type: array of integer, values: an integer per polygon
		//
		// close block "GeometryUVInfo"
		//
		/////////////////////////////////////////////////////////////////////////////
    
		bool FBGeometryTexture = false;
		bool FBGeometryTextureBlockOk = false;

		if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_GEOMETRY_UV_INFO)) 
		{
			FBGeometryTexture = true;

			if (mFileObject->FieldReadBlockBegin())
			{
				FBGeometryTextureBlockOk = true;
			}
		}
            
		if (mTemporaryTextures.GetCount() > 0)
		{
		        ReadMeshTextureType (pMesh);
		        ReadMeshTextureUV (pMesh);
		        ReadMeshTextureIndex (pMesh);
		}
            
		if (FBGeometryTextureBlockOk)
		{
			mFileObject->FieldReadBlockEnd();
		}

		if (FBGeometryTexture) 
		{
			mFileObject->FieldReadEnd();
		}
    
		if (lVersion <= 123)
		{
			CorrectTextureLayers(pMesh);
		}
    
	}

	else // 6.0 or more!
	{
		ReadLayerElements(pMesh);
		FBX_ASSERT_RETURN_VALUE(GetStatus().GetCode() != FbxStatus::eInvalidFile, false)
		ReadGeometryLinks(pMesh);
		FBX_ASSERT_RETURN_VALUE(GetStatus().GetCode() != FbxStatus::eInvalidFile, false)
		ReadGeometryShapes(pMesh);
		FBX_ASSERT_RETURN_VALUE(GetStatus().GetCode() != FbxStatus::eInvalidFile, false)
	}

    return true;
}

bool FbxReaderFbx5::ReadNurb (FbxNurbs& pNurbs)
{
    int            lVersion,U,V;
    bool            Return = true;
    int            Count;
    int            TotalCount;
    double*        Points;

    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYNURB_NURB_VERSION);

    //
    // Type of the surface 
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURB_NURB_ORDER)) 
    {
        U = mFileObject->FieldReadI ();
        V = mFileObject->FieldReadI ();
        mFileObject->FieldReadEnd();
        pNurbs.SetOrder (U, V);
    }

    {

    const char* Line;

    FbxNurbs::EType lTypeU = FbxNurbs::ePeriodic, lTypeV = FbxNurbs::ePeriodic;

    //
    // FORM...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURB_FORM)) 
    {
        Line = mFileObject->FieldReadC ();

        if(FBXSDK_stricmp(Line, "Periodic") == 0)  
        {
            lTypeU = FbxNurbs::ePeriodic;
        } 
        else if(FBXSDK_stricmp(Line, "Closed") == 0) 
        {
			if( lVersion > 100 )
			{
				lTypeU = FbxNurbs::eClosed;
			}
			else 
			{	
				// we interpret closed as periodic in the old versions of the SDK
				lTypeU = FbxNurbs::ePeriodic;
			}
        } 
        else if(FBXSDK_stricmp(Line, "Open") == 0)     
        {
            lTypeU = FbxNurbs::eOpen;
        } 
        else 
        {
            GetStatus().SetCode(FbxStatus::eInvalidParameter, "Type of nurb unknown");
            FBX_ASSERT_NOW ("Type of nurb unknown (invalid data).");
            Return = false;                 
        }

        Line = mFileObject->FieldReadC ();

        if(FBXSDK_stricmp(Line, "Periodic") == 0)  
        {
            lTypeV = FbxNurbs::ePeriodic;
        } 
        else if(FBXSDK_stricmp(Line, "Closed") == 0)   
        {
			if( lVersion > 100 )
			{
				lTypeV = FbxNurbs::eClosed;
			}
			else 
			{	
				// we interpret closed as periodic in the old versions of the SDK
				lTypeV = FbxNurbs::ePeriodic;
			}
		} 
        else if(FBXSDK_stricmp(Line, "Open") == 0) 
        {
            lTypeV = FbxNurbs::eOpen;
        } 
        else 
        {
            GetStatus().SetCode(FbxStatus::eInvalidParameter, "Type of nurb unknown");
            FBX_ASSERT_NOW("Type of nurb unknown (invalid data).");
            Return = false;                 
        }
        
        mFileObject->FieldReadEnd();
    }

    //
    // SURFACE DISPLAY...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURB_SURFACE_DISPLAY))
    {
        pNurbs.SetSurfaceMode((FbxGeometry::ESurfaceMode)mFileObject->FieldReadI());
        U = mFileObject->FieldReadI ();
        V = mFileObject->FieldReadI ();
        pNurbs.SetStep (U,V);

        mFileObject->FieldReadEnd ();
    }

    //
    // STEP...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURB_STEP)) 
    {
        U = mFileObject->FieldReadI ();
        V = mFileObject->FieldReadI ();
        mFileObject->FieldReadEnd ();
        pNurbs.SetStep (U,V);
    }

    //
    // Surface information 
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURB_DIMENSION)) 
    {
        U = mFileObject->FieldReadI ();
        V = mFileObject->FieldReadI ();
        mFileObject->FieldReadEnd ();
        pNurbs.InitControlPoints (U, lTypeU, V, lTypeV);
    }

    //
    // Control points 
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURB_POINTS)) 
    {
        TotalCount = pNurbs.GetUCount () * pNurbs.GetVCount ();

        if(mFileObject->FieldReadGetCount () != (TotalCount*4)) 
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Control point definition error (wrong number of data)");
            FBX_ASSERT_NOW("Control point definition error (wrong number of data).");
            Return = false;
        } 
        else 
        {
            for (Count=0; Count<TotalCount; Count++) 
            {
                FbxVector4& lVector = pNurbs.GetControlPoints()[Count];

                lVector[0] = mFileObject->FieldReadD();
                lVector[1] = mFileObject->FieldReadD();
                lVector[2] = mFileObject->FieldReadD();
                lVector[3] = mFileObject->FieldReadD();;

                if (lVector[3] <= 0.00001) 
                {
                    GetStatus().SetCode(FbxStatus::eFailure, "Weight must be greater than 0 (invalid data)");
                    FBX_ASSERT_NOW("Weight must be greater than 0 (invalid data).");
                    Return = false;
                }
            }
        }

        mFileObject->FieldReadEnd ();
    }

    //
    // MULTIPLICITY_U...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURB_MULTIPLICITY_U)) 
    {
        TotalCount  = pNurbs.GetUCount();
        int *Pts  = pNurbs.GetUMultiplicityVector();
        
        if (mFileObject->FieldReadGetCount () != TotalCount) 
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Multiplicity U definition error (wrong number of data)");
            FBX_ASSERT_NOW("Multiplicity U definition error (wrong number of data).");
            Return = false;
        } 
        else 
        {
            for (Count = 0; Count < TotalCount; Count ++) 
            {
                *(Pts +  Count) = mFileObject->FieldReadI ();
            }
        }
    }
    mFileObject->FieldReadEnd ();

    //
    // MULTIPLICITY_V...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURB_MULTIPLICITY_V)) 
    {
        TotalCount  = pNurbs.GetVCount ();
        int *Pts= pNurbs.GetVMultiplicityVector ();

        if(mFileObject->FieldReadGetCount  () != TotalCount) 
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Multiplicity V definition error (wrong number of data)");
            FBX_ASSERT_NOW("Multiplicity V definition error (wrong number of data).");
            Return = false;
        } 
        else 
        {
            for (Count=0; Count<TotalCount; Count++) 
            {
                *(Pts +  Count) = mFileObject->FieldReadI();
            }
        }

        mFileObject->FieldReadEnd ();
    }

    //
    // U Knot Vector
    //
    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYNURB_KNOTVECTOR_U)) 
    {
        TotalCount  =   pNurbs.GetUKnotCount  ();
        Points      =   pNurbs.GetUKnotVector ();

        if (mFileObject->FieldReadGetCount () != TotalCount) 
        {
            GetStatus().SetCode(FbxStatus::eFailure, "U knot vector definition error (wrong number of data)");
            FBX_ASSERT_NOW("U knot vector definition error (wrong number of data).");
            Return = false;
        } 
        else 
        {
            for (Count=0; Count<TotalCount; Count++) 
            {
                *(Points+Count) = mFileObject->FieldReadD();
            }
        }

        mFileObject->FieldReadEnd ();
    }

    //
    // V Knot Vector
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURB_KNOTVECTOR_V))
    {
        TotalCount  =   pNurbs.GetVKnotCount  ();
        Points      =   pNurbs.GetVKnotVector ();

        if (mFileObject->FieldReadGetCount () != TotalCount) 
        {
            GetStatus().SetCode(FbxStatus::eFailure, "V knot vector definition error (wrong number of data)");
            FBX_ASSERT_NOW("V knot vector definition error (wrong number of data).");
            Return = false;
        } 
        else 
        {
            for (Count=0; Count<TotalCount; Count++) 
            {
                *(Points+Count) = mFileObject->FieldReadD();
            }
        }

        mFileObject->FieldReadEnd();
    }

    }       

	if(mFileObject->IsBeforeVersion6())
	{
		// RR: Not sure this is really obsolete!!!!

		// Obsolete code. Remove for MB 6.0. Luc
		ReadGeometry(pNurbs);
		GenerateParametricGeometryLayer(pNurbs);
		
	}
	else
	{
		ReadLayerElements(pNurbs);
		ReadGeometryLinks(pNurbs);
		ReadGeometryShapes(pNurbs);
	}

    if (Return) 
    { 
        return true; 
    } 
    else 
    {
        return false;
    }
    
}


bool FbxReaderFbx5::ReadPatch (FbxPatch& pPatch)
{
    int    Count;
    int    Version;
    int    IU, IV;
    bool    BU, BV;
    FbxPatch::EType lUType = FbxPatch::eLinear, lVType = FbxPatch::eLinear;

    Version = mFileObject->FieldReadI (FIELD_KFBXGEOMETRYPATCH_PATCH_VERSION, 100);

    //
    // Read the PATCHTYPE...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYPATCH_PATCH_TYPE)) 
    {
        lUType = (FbxPatch::EType) ReadPatchType(pPatch);
        lVType = (FbxPatch::EType) ReadPatchType(pPatch);
        mFileObject->FieldReadEnd();
    }

    //
    // Read the DIMENSIONS...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYPATCH_DIMENSIONS)) 
    {
        IU = mFileObject->FieldReadI();
        IV = mFileObject->FieldReadI();
        mFileObject->FieldReadEnd();
        pPatch.InitControlPoints(IU, lUType, IV, lVType);
    }

    //
    // Read the SURFACEDISPLAY...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYPATCH_SURFACE_DISPLAY))
    {
        pPatch.SetSurfaceMode((FbxGeometry::ESurfaceMode)mFileObject->FieldReadI());
        IU = mFileObject->FieldReadI();
        IV = mFileObject->FieldReadI();
        mFileObject->FieldReadEnd();
        pPatch.SetStep(IU,IV);
    }
        
    //
    // Read the STEP...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYPATCH_STEP)) 
    {
        IU = mFileObject->FieldReadI();
        IV = mFileObject->FieldReadI();
        mFileObject->FieldReadEnd();
        pPatch.SetStep(IU,IV);
    }

    //
    // Read the CLOSED...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYPATCH_CLOSED)) 
    {
        BU = mFileObject->FieldReadB();
        BV = mFileObject->FieldReadB();
        mFileObject->FieldReadEnd();
        pPatch.SetClosed(BU, BV);
    }

    //
    // Read the UCAPPED...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYPATCH_UCAPPED)) 
    {
        BU = mFileObject->FieldReadB();
        BV = mFileObject->FieldReadB();
        mFileObject->FieldReadEnd();
        pPatch.SetUCapped(BU, BV);
    }

    //
    // Read the VCAPPED...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYPATCH_VCAPPED)) 
    {
        BU = mFileObject->FieldReadB();
        BV = mFileObject->FieldReadB();
        mFileObject->FieldReadEnd();
        pPatch.SetVCapped(BU, BV);
    }

    //
    // Read the Control points ...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYPATCH_POINTS)) 
    {
        for (Count = 0; Count < pPatch.GetControlPointsCount (); Count ++) 
        {        
            FbxVector4& lVector = pPatch.GetControlPoints()[Count];
            lVector[0]  = mFileObject->FieldReadD();            
            lVector[1]  = mFileObject->FieldReadD();
            lVector[2]  = mFileObject->FieldReadD();
            lVector[3]  = 1.0;
        }

        mFileObject->FieldReadEnd();
    }   
    
	if(mFileObject->IsBeforeVersion6())
	{
		// RR: Not sure this is really obsolete!!!!

		// Obsolete code. Remove for MB 6.0. Luc
		ReadGeometry(pPatch);
		GenerateParametricGeometryLayer(pPatch);
		
    }
	else
	{
		ReadLayerElements(pPatch);
		ReadGeometryLinks(pPatch);
		ReadGeometryShapes(pPatch);
	}

    return true;
}


int FbxReaderFbx5::ReadPatchType(FbxPatch& pPatch)
{
    const char* SurfaceType = mFileObject->FieldReadC ();

    if (FBXSDK_stricmp (SurfaceType, "Bezier") == 0) 
    {
        return FbxPatch::eBezier;
    } 
    else if (FBXSDK_stricmp(SurfaceType,"BezierQuadric") == 0) 
    {
        return FbxPatch::eBezierQuadric;
    } 
    else if (FBXSDK_stricmp(SurfaceType,"Cardinal") == 0) 
    {
        return FbxPatch::eCardinal;
    } 
    else if (FBXSDK_stricmp(SurfaceType,"BSpline") == 0) 
    {
        return FbxPatch::eBSpline;
    }
    else 
    {
        return FbxPatch::eLinear;
    } 
}


bool FbxReaderFbx5::ReadTexture(FbxFileTexture& pTexture)
{

	if (mFileObject->IsBeforeVersion6())
	{
		//
		// Get the name
		//
		FbxString lName(FbxObject::StripPrefix(mFileObject->FieldReadC()));
		pTexture.SetName(lName.Buffer());

		if (mFileObject->FieldReadBlockBegin())
		{
			//
			// Read the Media Name...
			//
			FbxString lString = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXTEXTURE_MEDIA));
			pTexture.SetMediaName(lString.Buffer());

			//
			// Read TRANSLATION...
			//
			if (mFileObject->FieldReadBegin(FIELD_KFBXTEXTURE_TRANSLATION))
			{
				FbxVector4 lVector;
				lVector[0] = mFileObject->FieldReadD();
				lVector[1] = mFileObject->FieldReadD();
				lVector[2] = 0.0;
				mFileObject->FieldReadEnd();

				pTexture.SetDefaultT(lVector);
			}

			//
			// Read ROTATION...
			//
			if (mFileObject->FieldReadBegin(FIELD_KFBXTEXTURE_ROTATION))
			{
				FbxVector4 lVector;
				lVector[0] = mFileObject->FieldReadD();
				lVector[1] = mFileObject->FieldReadD();
				lVector[2] = mFileObject->FieldReadD();
				mFileObject->FieldReadEnd();

				pTexture.SetDefaultR(lVector);
			}

			//
			// Read SCALING...
			//
			if (mFileObject->FieldReadBegin(FIELD_KFBXTEXTURE_SCALING))
			{
				FbxVector4 lVector;
				lVector[0] = mFileObject->FieldReadD();
				lVector[1] = mFileObject->FieldReadD();
				lVector[2] = 1.0;
				mFileObject->FieldReadEnd();

				pTexture.SetDefaultS(lVector);
			}

			//
			// Read VISIBILITY...
			//
			if (mFileObject->FieldReadBegin(FIELD_KFBXTEXTURE_ALPHA))
			{
				double lAlpha = mFileObject->FieldReadD();
				mFileObject->FieldReadEnd();

				pTexture.SetDefaultAlpha(FbxClamp(lAlpha, 0.0, 1.0));
			}

			//
			// Read UV TRANSLATION...
			//
			if (mFileObject->FieldReadBegin(FIELD_KFBXTEXTURE_UV_TRANSLATION))
			{
				FbxVector2 lV(mFileObject->FieldReadD(), mFileObject->FieldReadD());
				pTexture.SetUVTranslation(lV);
				mFileObject->FieldReadEnd();
			}

			//
			// Read UV SCALING...
			//
			if (mFileObject->FieldReadBegin(FIELD_KFBXTEXTURE_UV_SCALING))
			{
				FbxVector2 lV(mFileObject->FieldReadD(), mFileObject->FieldReadD());
				pTexture.SetUVScaling(lV);
				mFileObject->FieldReadEnd();
			}

			//
			// Read TEXTURE_ALPHA_SOURCE...
			//
			const char* lTexAlphaSource = mFileObject->FieldReadC(FIELD_KFBXTEXTURE_ALPHA_SRC);

			//
			// Default value for Texture Alpha Source...
			//
			FbxTexture::EAlphaSource lAlphaSource = FbxTexture::eNone;

			if (lTexAlphaSource)
			{
				if (!strcmp(lTexAlphaSource, "None"))
				{
					lAlphaSource = FbxTexture::eNone;
				}
				else if (!strcmp(lTexAlphaSource, "RGB_Intensity"))
				{
					lAlphaSource = FbxTexture::eRGBIntensity;
				}
				else if (!strcmp(lTexAlphaSource, "Alpha_Black"))
				{
					lAlphaSource = FbxTexture::eBlack;
				}
				else
				{
					lAlphaSource = FbxTexture::eNone;
				}
			}

			pTexture.SetAlphaSource(lAlphaSource);

			//
			// Read CROPPING...
			//
			if (mFileObject->FieldReadBegin(FIELD_KFBXTEXTURE_CROPPING))
			{
				pTexture.SetCropping(mFileObject->FieldReadI(), mFileObject->FieldReadI(), mFileObject->FieldReadI(), mFileObject->FieldReadI());
				mFileObject->FieldReadEnd();
			}


			//
			//Nurbs & Patch Mapping Helpers
			//

			//
			// Read TEXTURE_MAPPING_TYPE...
			//
			const char* lTexMapType = mFileObject->FieldReadC(FIELD_KFBXTEXTURE_MAPPING_TYPE);

			//
			// Default Value for Texture Mapping Type...
			//
			FbxTexture::EMappingType lMappingType = FbxTexture::eNull;

			if (lTexMapType)
			{
				if (!strcmp(lTexMapType, "None"))
				{
					lMappingType = FbxTexture::eNull;
				}
				else if (!strcmp(lTexMapType, "Planar"))
				{
					lMappingType = FbxTexture::ePlanar;
				}
				else if (!strcmp(lTexMapType, "Spherical"))
				{
					lMappingType = FbxTexture::eSpherical;
				}
				else if (!strcmp(lTexMapType, "Cylindrical"))
				{
					lMappingType = FbxTexture::eCylindrical;
				}
				else if (!strcmp(lTexMapType, "Box"))
				{
					lMappingType = FbxTexture::eBox;
				}
				else if (!strcmp(lTexMapType, "Face"))
				{
					lMappingType = FbxTexture::eFace;
				}
				else if (!strcmp(lTexMapType, "UV"))
				{
					lMappingType = FbxTexture::eUV;
				}
				else if (!strcmp(lTexMapType, "Environment"))
				{
					lMappingType = FbxTexture::eEnvironment;
				}
				else
				{
					lMappingType = FbxTexture::eNull;
				}
			}

			pTexture.SetMappingType(lMappingType);

			//
			// Read TEXTURE_PLANAR_MAPPING_NORMAL...
			//
			const char* lPlanarMappingNormal = mFileObject->FieldReadC(FIELD_KFBXTEXTURE_PLANAR_NORMAL);

			//
			// Default Value for mPlanarMapNormal.
			//
			FbxTexture::EPlanarMappingNormal lPMN = FbxTexture::ePlanarNormalX;

			if (lPlanarMappingNormal)
			{
				if (!strcmp(lPlanarMappingNormal, "X"))
				{
					lPMN = FbxTexture::ePlanarNormalX;
				}
				else if (!strcmp(lPlanarMappingNormal, "Y"))
				{
					lPMN = FbxTexture::ePlanarNormalY;
				}
				else if (!strcmp(lPlanarMappingNormal, "Z"))
				{
					lPMN = FbxTexture::ePlanarNormalZ;
				}
				else
				{
					lPMN = FbxTexture::ePlanarNormalX;
				}
			}

			pTexture.SetPlanarMappingNormal(lPMN);

			//
			// Read SWAPUV...
			//

			bool lSwapUV = false;

			if (mFileObject->FieldReadBegin(FIELD_KFBXTEXTURE_SWAPUV))
			{
				lSwapUV = mFileObject->FieldReadB();

				mFileObject->FieldReadEnd();
			}

			pTexture.SetSwapUV(lSwapUV);

			//
			// Read MATERIAL_USE...
			//
			pTexture.SetMaterialUse((FbxFileTexture::EMaterialUse)mFileObject->FieldReadI(FIELD_KFBXTEXTURE_MATERIAL_USE));

			//
			// Read TEXTURE_PLANAR_MAPPING_NORMAL...
			//
			const char* lTextureUse = mFileObject->FieldReadC(FIELD_KFBXTEXTURE_TEXTURE_USE);

			//
			// Default Value for mTextureUse.
			//
			FbxTexture::ETextureUse lTextureUseValue = FbxTexture::eStandard;

			if (lTextureUse)
			{
				if (!strcmp(lTextureUse, TOKEN_KFBXTEXTURE_TEXTURE_USE_STANDARD))
				{
					lTextureUseValue = FbxTexture::eStandard;
				}
				else if (!strcmp(lTextureUse, TOKEN_KFBXTEXTURE_TEXTURE_USE_SHADOW_MAP))
				{
					lTextureUseValue = FbxTexture::eShadowMap;
				}
				else if (!strcmp(lTextureUse, TOKEN_KFBXTEXTURE_TEXTURE_USE_LIGHT_MAP))
				{
					lTextureUseValue = FbxTexture::eLightMap;
				}
				else if (!strcmp(lTextureUse, TOKEN_KFBXTEXTURE_TEXTURE_USE_SPHERICAL_REFLEXION_MAP))
				{
					lTextureUseValue = FbxTexture::eSphericalReflectionMap;
				}
				else if (!strcmp(lTextureUse, TOKEN_KFBXTEXTURE_TEXTURE_USE_SPHERE_REFLEXION_MAP))
				{
					lTextureUseValue = FbxTexture::eSphereReflectionMap;
				}
				else if (!strcmp(lTextureUse, TOKEN_KFBXTEXTURE_TEXTURE_USE_BUMP_NORMAL_MAP))
				{
					lTextureUseValue = FbxTexture::eBumpNormalMap;
				}
				else
				{
					lTextureUseValue = FbxTexture::eStandard;
				}
			}

			pTexture.SetTextureUse(lTextureUseValue);

			pTexture.SetWrapMode((FbxTexture::EWrapMode) mFileObject->FieldReadI(FIELD_KFBXTEXTURE_WRAP_U, FbxTexture::eRepeat),
				(FbxTexture::EWrapMode) mFileObject->FieldReadI(FIELD_KFBXTEXTURE_WRAP_V, FbxTexture::eRepeat));
			pTexture.SetBlendMode((FbxTexture::EBlendMode) mFileObject->FieldReadI(FIELD_KFBXTEXTURE_BLEND_MODE, FbxTexture::eTranslucent));

			mFileObject->FieldReadBlockEnd();
		}	
	}

	else //6.0 +
	{
		//
		// Read the 
		//
		FbxString lString = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXTEXTURE_TEXTURE_NAME));
		pTexture.SetName(lString.Buffer());
		pTexture.SetFileName(mFileObject->FieldReadC(FIELD_KFBXTEXTURE_FILENAME));
        pTexture.SetRelativeFileName(mFileObject->FieldReadC(FIELD_KFBXTEXTURE_RELATIVE_FILENAME));


		//
		// Read the Media Name...
		//
		lString = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXTEXTURE_MEDIA));
		pTexture.SetMediaName(lString.Buffer());
		
		// Read the properties
		ReadProperties(&pTexture);
		//
		// Read UV TRANSLATION...
		//
		if ( mFileObject->FieldReadBegin ( FIELD_KFBXTEXTURE_UV_TRANSLATION ) ) 
		{
			FbxVector2 lV(mFileObject->FieldReadD (), mFileObject->FieldReadD ());
			pTexture.SetUVTranslation(lV);
			mFileObject->FieldReadEnd ();
		}

		//
		// Read UV SCALING...
		//
		if ( mFileObject->FieldReadBegin ( FIELD_KFBXTEXTURE_UV_SCALING ) ) 
		{
			FbxVector2 lV(mFileObject->FieldReadD (), mFileObject->FieldReadD ());
			pTexture.SetUVScaling(lV);
			mFileObject->FieldReadEnd ();
		}

		//
		// Read TEXTURE_ALPHA_SOURCE...
		//
		const char* lTexAlphaSource = mFileObject->FieldReadC ( FIELD_KFBXTEXTURE_ALPHA_SRC );

		//
		// Default value for Texture Alpha Source...
		//
		FbxTexture::EAlphaSource lAlphaSource = FbxTexture::eNone;

		if (lTexAlphaSource)
		{
			if (!strcmp ( lTexAlphaSource, "None" ) ) 
			{
				lAlphaSource = FbxTexture::eNone;
			} 
			else if (!strcmp ( lTexAlphaSource, "RGB_Intensity" ) ) 
			{
				lAlphaSource = FbxTexture::eRGBIntensity;
			} 
			else if (!strcmp ( lTexAlphaSource, "Alpha_Black" ) ) 
			{
				lAlphaSource = FbxTexture::eBlack;
			} 
			else 
			{
				lAlphaSource = FbxTexture::eNone;
			}
		}

		pTexture.SetAlphaSource(lAlphaSource);
    
		//
		// Read CROPPING...
		//
		if ( mFileObject->FieldReadBegin ( FIELD_KFBXTEXTURE_CROPPING ) ) 
		{
			pTexture.SetCropping(mFileObject->FieldReadI (), mFileObject->FieldReadI (), mFileObject->FieldReadI (), mFileObject->FieldReadI ());
			mFileObject->FieldReadEnd ();
		}
        				
	}

    return true;
}


bool FbxReaderFbx5::ReadSurfaceMaterial(FbxSurfacePhong& pMaterial)
{

	if (mFileObject->IsBeforeVersion6()) 
	{
		FbxString lString = FbxObject::StripPrefix(mFileObject->FieldReadC ());
		pMaterial.SetName(lString.Buffer() );

		if(mFileObject->FieldReadBlockBegin())
		{
			FbxDouble3 lColor;
			double lValue;

			FbxString lStrSurfaceMat = FbxString(mFileObject->FieldReadC(FIELD_KFBXMATERIAL_SHADING_MODEL, "phong")).Lower();

			pMaterial.ShadingModel.Set(lStrSurfaceMat);
        
			if(mFileObject->FieldReadBegin(FIELD_KFBXMATERIAL_AMBIENT))
			{
				mFileObject->FieldRead3D(lColor.Buffer());
				mFileObject->FieldReadEnd();

				pMaterial.Ambient.Set(lColor);
				pMaterial.AmbientFactor.Set(1.);
			}

			if(mFileObject->FieldReadBegin(FIELD_KFBXMATERIAL_DIFFUSE))
			{
				mFileObject->FieldRead3D(lColor.Buffer());
				mFileObject->FieldReadEnd();

				pMaterial.Diffuse.Set(lColor);
				pMaterial.DiffuseFactor.Set(1.);
			}

			if(mFileObject->FieldReadBegin(FIELD_KFBXMATERIAL_SPECULAR))
			{
				mFileObject->FieldRead3D(lColor.Buffer());
				mFileObject->FieldReadEnd();

				pMaterial.Specular.Set(lColor);
				pMaterial.SpecularFactor.Set(1.);
			}

			if(mFileObject->FieldReadBegin(FIELD_KFBXMATERIAL_EMISSIVE))
			{
				mFileObject->FieldRead3D(lColor.Buffer());
				mFileObject->FieldReadEnd();

				pMaterial.Emissive.Set(lColor);
				pMaterial.EmissiveFactor.Set(1.);
			}

			if(mFileObject->FieldReadBegin(FIELD_KFBXMATERIAL_SHININESS))
			{
				lValue = mFileObject->FieldReadD();

				if (mFileObject->ProjectGetCurrentSectionVersion() < 3000)
				{
					// Material shininess range was from 0.0 to 128.0 before FiLMBOX 3.0.
					// Since then it is from 0.0 to 100.0
					lValue /= 1.28;
				}

				pMaterial.Shininess.Set(lValue);
				mFileObject->FieldReadEnd();
			}

			if(mFileObject->FieldReadBegin(FIELD_KFBXMATERIAL_REFLECTIVITY))
			{
				lValue = mFileObject->FieldReadD();
				mFileObject->FieldReadEnd();

				pMaterial.ReflectionFactor.Set(lValue);
				pMaterial.Reflection.Set(FbxDouble3(1., 1., 1.));
			}

			if(mFileObject->FieldReadBegin(FIELD_KFBXMATERIAL_ALPHA))
			{
				lValue = mFileObject->FieldReadD();
				mFileObject->FieldReadEnd();

				pMaterial.TransparentColor.Set(FbxDouble3(1., 1., 1.));
				pMaterial.TransparencyFactor.Set(1. - lValue);
			}

			mFileObject->FieldReadBlockEnd();
		}
	}
	else // we should never get here, so i did not update the code for the new materials
	{
		/*int lVersion*/ mFileObject->FieldReadI(FIELD_KFBXMATERIAL_VERSION, 100);
		FbxString lString = FbxString(mFileObject->FieldReadC(FIELD_KFBXMATERIAL_SHADING_MODEL, "phong")).Lower();
		pMaterial.ShadingModel.Set(lString.Buffer());
		pMaterial.MultiLayer.Set(mFileObject->FieldReadI(FIELD_KFBXMATERIAL_MULTI_LAYER) != 0);

		// Read the properties
		ReadProperties(&pMaterial);
	}

    return true;
}


bool FbxReaderFbx5::ReadVideo(FbxVideo& pVideo)
{
	if (mFileObject->IsBeforeVersion6())
		return true; // no use

	// Read the properties
	ReadProperties(&pVideo);
	//
	// Read the 
	//
	pVideo.ImageTextureSetMipMap(mFileObject->FieldReadB (FIELD_KFBXVIDEO_USEMIPMAP) );


	if (mFileObject->FieldReadI(FIELD_MEDIA_VERSION) > 100)
	{
		pVideo.SetOriginalFormat(mFileObject->FieldReadB (FIELD_MEDIA_ORIGINAL_FORMAT) );
		pVideo.SetOriginalFilename(mFileObject->FieldReadC (FIELD_MEDIA_ORIGINAL_FILENAME) );
	}

	FbxString lFileName, lRelativeFileName;
	lFileName = mFileObject->FieldReadC (FIELD_MEDIA_FILENAME);
	lFileName = pVideo.GetFileName();
    lRelativeFileName = mFileObject->FieldReadC (FIELD_MEDIA_RELATIVE_FILENAME);

    // If this field exist, the media is embedded.
    bool lSkipValidation = true;
    if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true))
    {
        lSkipValidation = false;
        if (mFileObject->FieldReadBegin(FIELD_MEDIA_CONTENT))
        {
            bool lStatus = mFileObject->FieldReadEmbeddedFile (lFileName, lRelativeFileName);
            mFileObject->FieldReadEnd();
        }

    }

	pVideo.SetFileName(lFileName.Buffer()); 
	pVideo.SetRelativeFileName(lRelativeFileName.Buffer()); 

    if (lSkipValidation == false)
    {
	    // Check if the "absolute" path of the video exist
	    // If the "absolute" path of the video is not found BUT the "relative" path is found
	    // replace the "absolute" path of the video, then if we later write this scene in a file, the "absolute" path exist.
	    // This can occur when a FBX file and "relative" video are moved.
	    if( FbxFileUtils::Exist(  pVideo.GetFileName() ) == false)
	    {
		    FbxString lNewAbsolutePath = mFileObject->GetFullFilePath( pVideo.GetRelativeFileName() );
		    lNewAbsolutePath = FbxPathUtils::Clean( lNewAbsolutePath );
		    if( FbxFileUtils::Exist( lNewAbsolutePath ) )
		    {
			    // Set with a valid "absolute" path
			    pVideo.SetFileName( lNewAbsolutePath.Buffer() );
		    }
	    }
    }

    return !lFileName.IsEmpty();
}


bool FbxReaderFbx5::ReadLink(FbxCluster& pLink)
{
    const char*    ModeStr;
    const char*    lAssModel;
    int        PointCount;
    int        Count;
    FbxVector4 lRow;


	// The name of the link node is stored, and will be resolved in
	// function FbxReader::ResolveLinks()
	pLink.mBeforeVersion6LinkName = FbxObject::StripPrefix(mFileObject->FieldReadC());

    //
    // Read The Link Block...
    //
    if ( mFileObject->FieldReadBlockBegin () ) 
    {
        //
        // Read The Link MODE...
        //
        pLink.SetLinkMode(FbxCluster::eNormalize);
        bool lTest = false;

        if (mFileObject->FieldReadBegin( FIELD_KFBXLINK_MODE ) ) 
        {
            ModeStr = mFileObject->FieldReadC ();
            
            if (FBXSDK_stricmp ( ModeStr,TOKEN_KFBXLINK_ADDITIVE ) == 0 ) 
            {
                pLink.SetLinkMode(FbxCluster::eAdditive);
            }
            else if (FBXSDK_stricmp ( ModeStr,TOKEN_KFBXLINK_TOTAL1 ) == 0 )
            {
                pLink.SetLinkMode(FbxCluster::eTotalOne);
            }
            
            mFileObject->FieldReadEnd ();

            lTest = true;
        }

        //
        // Read the USER DATA...
        //
        if ( mFileObject->FieldReadBegin ( FIELD_KFBXLINK_USERDATA ) != false) 
        {
			FbxString UserDataID = mFileObject->FieldReadC();
			FbxString UserData = mFileObject->FieldReadC();
			pLink.SetUserData(UserDataID, UserData);
            mFileObject->FieldReadEnd ();
        }
            
        //
        // Read the Link INDICES...
        //
        PointCount = 0;

        if ( mFileObject->FieldReadBegin ( FIELD_KFBXLINK_INDEXES ) ) 
        {
            PointCount = mFileObject->FieldReadGetCount ();

			pLink.SetControlPointIWCount(PointCount);
           
            for (Count = 0; Count < PointCount; Count ++) 
            {
				pLink.GetControlPointIndices()[Count] = mFileObject->FieldReadI();
            }

            mFileObject->FieldReadEnd ();
        }

        //
        // Read the Link WEIGHTS...
        // 
        if ( mFileObject->FieldReadBegin ( FIELD_KFBXLINK_WEIGHTS ) ) 
        {
            for (Count = 0; Count < PointCount; Count ++) 
            {
                pLink.GetControlPointWeights()[Count] = mFileObject->FieldReadD();
            }

            mFileObject->FieldReadEnd ();
        }

        //
        // Read the TRANSFORM matrix...
        //
        FbxMatrix Transform;
        Transform.SetIdentity();
        mFileObject->FieldReadDn(FIELD_KFBXLINK_TRANSFORM, (double*) Transform.mData, NULL, 16);

        //
        // Read the TRANSFORM LINK matrix...
        //
        FbxMatrix TransformLink;
        TransformLink.SetIdentity();
        if ( mFileObject->FieldReadBegin ( FIELD_KFBXLINK_TRANSFORM_LINK ) ) 
        {
            mFileObject->FieldReadDn((double*) TransformLink.mData, 16);
            mFileObject->FieldReadEnd ();
        }

        Transform = TransformLink * Transform;
        pLink.SetTransformMatrix(*(FbxAMatrix*)&Transform);
        pLink.SetTransformLinkMatrix(*(FbxAMatrix*)&TransformLink);

        //
        // Read the ASSOCIATE MODEL...
        //

        if ( mFileObject->FieldReadBegin ( FIELD_KFBXLINK_ASSOCIATE_MODEL ) ) 
        {
            lAssModel = FbxObject::StripPrefix(mFileObject->FieldReadC());

			// The name of the associate model node is stored, and will be resolved in
			// function FbxReader::ResolveLinks()
			pLink.mBeforeVersion6AssociateModelName = lAssModel;

            if ( mFileObject->FieldReadBlockBegin () ) 
            {
                if( mFileObject->FieldReadBegin ( FIELD_KFBXLINK_TRANSFORM ) ) 
                {
                    FbxMatrix lMatrix;
            
                    mFileObject->FieldReadDn((double*) lMatrix.mData, 16);

                    pLink.SetTransformAssociateModelMatrix(*(FbxAMatrix*)&lMatrix);

                    FbxAMatrix TransformAssociate, TransformLink1;

                    pLink.GetTransformAssociateModelMatrix(TransformAssociate);
                    pLink.GetTransformLinkMatrix(TransformLink1);

					TransformAssociate = TransformLink1 * TransformAssociate;

                    pLink.SetTransformAssociateModelMatrix(TransformAssociate);

                    mFileObject->FieldReadEnd ();
                }

                mFileObject->FieldReadBlockEnd ();
            }
        }  

        if ( mFileObject->FieldReadBegin ( FIELD_KFBXLINK_TRANSFORM_PARENT ) ) 
        {
            FbxMatrix lMatrix;
            
            mFileObject->FieldReadDn((double*) lMatrix.mData, 16);
                        
            pLink.SetTransformParentMatrix(*(FbxAMatrix*)&lMatrix);
            
            mFileObject->FieldReadEnd ();
        }

        mFileObject->FieldReadBlockEnd (); 
    }

    return true;
}


bool FbxReaderFbx5::ReadSkin(FbxSkin& pSkin)
{
	/*int lVersion*/ mFileObject->FieldReadI(FIELD_KFBXDEFORMER_VERSION, 100);
	pSkin.SetMultiLayer(mFileObject->FieldReadI(FIELD_KFBXDEFORMER_MULTI_LAYER) != 0);

	// Read the properties
	ReadProperties(&pSkin);

	if (mFileObject->FieldReadBegin(FIELD_KFBXSKIN_DEFORM_ACCURACY))
	{
		pSkin.SetDeformAccuracy(mFileObject->FieldReadD());
		mFileObject->FieldReadEnd();
	}

	return true;
}


bool FbxReaderFbx5::ReadCluster(FbxCluster& pCluster)
{
	/*int lVersion*/ mFileObject->FieldReadI(FIELD_KFBXDEFORMER_VERSION, 100);
	pCluster.SetMultiLayer(mFileObject->FieldReadI(FIELD_KFBXDEFORMER_MULTI_LAYER) != 0);

	// Read the properties
	ReadProperties(&pCluster);

    //
    // Read The Link MODE...
    //
    pCluster.SetLinkMode(FbxCluster::eNormalize);

    if (mFileObject->FieldReadBegin(FIELD_KFBXDEFORMER_MODE)) 
    {
        FbxString lModeStr = mFileObject->FieldReadC();
        
        if (lModeStr.CompareNoCase(TOKEN_KFBXLINK_ADDITIVE) == 0) 
        {
            pCluster.SetLinkMode(FbxCluster::eAdditive);
        }
        else if (lModeStr.CompareNoCase(TOKEN_KFBXLINK_TOTAL1) == 0)
        {
            pCluster.SetLinkMode(FbxCluster::eTotalOne);
        }
        
        mFileObject->FieldReadEnd ();
    }


	//
    // Read the USER DATA...
    //
	if (mFileObject->FieldReadBegin(FIELD_KFBXDEFORMER_USERDATA)) 
    {
		FbxString UserDataID = mFileObject->FieldReadC();
		FbxString UserData = mFileObject->FieldReadC();
		pCluster.SetUserData(UserDataID, UserData);
        mFileObject->FieldReadEnd ();
    }

    //
    // Read the Link INDICES...
    //        
	int lPointCount = 0;
    if (mFileObject->FieldReadBegin(FIELD_KFBXDEFORMER_INDEXES)) 
    {
        lPointCount = mFileObject->FieldReadGetCount();

		pCluster.SetControlPointIWCount(lPointCount);
        
		int lCount;
        for (lCount=0; lCount<lPointCount; lCount++) 
        {
            pCluster.GetControlPointIndices()[lCount] = mFileObject->FieldReadI();
        }

        mFileObject->FieldReadEnd ();
    }

    //
    // Read the Link WEIGHTS...
    // 
    if (mFileObject->FieldReadBegin(FIELD_KFBXDEFORMER_WEIGHTS)) 
    {
		int lCount;
        for (lCount=0; lCount<lPointCount; lCount++) 
        {
            pCluster.GetControlPointWeights()[lCount] = mFileObject->FieldReadD ();
        }

        mFileObject->FieldReadEnd ();
    }

    //
    // Read the TRANSFORM matrix...
    //
	FbxMatrix Transform;
	Transform.SetIdentity();
    mFileObject->FieldReadDn(FIELD_KFBXDEFORMER_TRANSFORM, (double*)Transform.mData, NULL, 16);

    //
    // Read the TRANSFORM LINK matrix...
    //
	FbxMatrix TransformLink;
    TransformLink.SetIdentity();
    if (mFileObject->FieldReadBegin(FIELD_KFBXLINK_TRANSFORM_LINK)) 
    {
        mFileObject->FieldReadDn((double*)TransformLink.mData, 16);
        
        mFileObject->FieldReadEnd ();
    }

	Transform = TransformLink * Transform;
	pCluster.SetTransformMatrix(*(FbxAMatrix*)&Transform);
	pCluster.SetTransformLinkMatrix(*(FbxAMatrix*)&TransformLink);

    //
    // Read the ASSOCIATE MODEL...
    //
    if (mFileObject->FieldReadBegin(FIELD_KFBXDEFORMER_ASSOCIATE_MODEL)) 
    {
        if (mFileObject->FieldReadBlockBegin()) 
        {
            if (mFileObject->FieldReadBegin(FIELD_KFBXDEFORMER_TRANSFORM)) 
            {
				FbxMatrix TransformAssociate;
        
                mFileObject->FieldReadDn((double*)TransformAssociate.mData, 16);

				TransformAssociate = TransformLink * TransformAssociate;
				pCluster.SetTransformAssociateModelMatrix(*(FbxAMatrix*)&TransformAssociate);

                mFileObject->FieldReadEnd ();
            }

            mFileObject->FieldReadBlockEnd ();
        }
		mFileObject->FieldReadEnd ();
    }  

	if (mFileObject->FieldReadBegin(FIELD_KFBXDEFORMER_TRANSFORM_PARENT)) 
    {
		FbxMatrix TransformParent;
        mFileObject->FieldReadDn((double*)TransformParent.mData, 16);
		pCluster.SetTransformParentMatrix(*(FbxAMatrix*)&TransformParent);
        mFileObject->FieldReadEnd ();
    }

	return true;
}

bool FbxReaderFbx5::ReadConstraint(FbxConstraint& pConstraint)
{
    if(IOS_REF.GetBoolProp(IMP_FBX_CONSTRAINT, true))
	{
		// Read the properties
		ReadProperties(&pConstraint);

		// patch patch patch !!!
		// Version 100: original version
		// Version 101: 
		//		- Added the translation, rotation and scaling offset
		//		  Note: Only the first source weight is supported
		if (pConstraint.GetConstraintType() == FbxConstraint::eParent)
		{
			if (mFileObject->FieldReadBegin(FIELD_CONSTRAINT_VERSION))
			{
				int lVersion = mFileObject->FieldReadI(FIELD_CONSTRAINT_VERSION);
				if (lVersion == 101)
				{
					FbxIterator<FbxProperty>	lFbxPropertyIter(&pConstraint);
					FbxProperty				lFbxProperty;

					FbxForEach(lFbxPropertyIter,lFbxProperty) {
						FbxString lName = lFbxProperty.GetName();

						int lIndexR = lName.Find(".Offset R");
				
						if (lIndexR != -1)
						{
							mFileObject->FieldReadBegin(FIELD_CONSTRAINT_OFFSET);
								
							//Set the rotation value
							FbxVector4 lRotationVector;
							lRotationVector[0] = mFileObject->FieldReadD();
							lRotationVector[1] = mFileObject->FieldReadD();
							lRotationVector[2] = mFileObject->FieldReadD();
							//new way of setting values
							lFbxProperty.Set(lRotationVector);

							//Get the translation property
							FbxString lTranslationOffsetName(lName.Left(lIndexR));
							lTranslationOffsetName += ".Offset T";

							FbxProperty lPropertyTranslation = pConstraint.FindProperty(lTranslationOffsetName.Buffer());
							FbxVector4 lTranslationVector;
							lTranslationVector[0] = mFileObject->FieldReadD();
							lTranslationVector[1] = mFileObject->FieldReadD();
							lTranslationVector[2] = mFileObject->FieldReadD();
							//new way of setting values
							lPropertyTranslation.Set(lTranslationVector);

							mFileObject->FieldReadEnd ();

							break;
						}
					}

				}

				mFileObject->FieldReadEnd ();
			}
		}

		return true;
	}
	return false;
}

bool FbxReaderFbx5::ReadShape(FbxShape& pShape, FbxGeometry& pGeometry)
{
    int    i;
    FbxArray<int> lIndices;

    if ( mFileObject->FieldReadBlockBegin () ) 
    { 
        //
        // Read the indices.
        //
        if ( mFileObject->FieldReadBegin ( FIELD_KFBXSHAPE_INDEXES ) ) 
        {
            int lTotalCount = mFileObject->FieldReadGetCount ();
                             
            for (i = 0; i < lTotalCount; i ++) 
            {
				int index = mFileObject->FieldReadI();
				if (index < 0 || index >= pGeometry.GetControlPointsCount())
				{
					mStatus.SetCode(FbxStatus::eInvalidParameter, "Invalid parameter while reading shape indices");
					return false;
				}
                lIndices.Add(index);
            }       
            
            mFileObject->FieldReadEnd ();
        }

        //
        // Read the control points.
        //
        if (mFileObject->FieldReadBegin(FIELD_KFBXSHAPE_VERTICES))
        {
            //pShape.GetControlPoints() = pGeometry.GetControlPoints();
            pShape.mControlPoints = pGeometry.mControlPoints;

            int lTotalCount = mFileObject->FieldReadGetCount() / 3;

            FbxVector4* const controlPoints = pShape.GetControlPoints();

            if (controlPoints)
            {
                const int controlPointsCount = pShape.GetControlPointsCount();

                for (i = 0; i < lTotalCount; i++)
                {
                    int index = lIndices[i];
                    if (index >= 0 && index < controlPointsCount)
                    {
                        FbxVector4& lVector = controlPoints[index];

                        lVector[0] += mFileObject->FieldReadD();
                        lVector[1] += mFileObject->FieldReadD();
                        lVector[2] += mFileObject->FieldReadD();
                    }
                }
            }

            mFileObject->FieldReadEnd();
        }

        if (pGeometry.GetAttributeType() == FbxNodeAttribute::eMesh)
        {
			// Read the normals on Layer 0
			FbxLayer*              lLayer;
			FbxLayerElementNormal* lLayerElementNormal;

			lLayer = pShape.GetLayer(0);
			if (!lLayer) 
			{
				int lLayerAbsoluteIndex = pShape.CreateLayer();
				lLayer = pShape.GetLayer(lLayerAbsoluteIndex);
			}

			lLayerElementNormal = lLayer->GetNormals();
			if(lLayerElementNormal)
			{


				lLayerElementNormal->SetMappingMode(FbxLayerElement::eByControlPoint);
				lLayerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);

				//
				// Read the NORMALS...
				//
				if ( mFileObject->FieldReadBegin ( FIELD_KFBXSHAPE_NORMALS ) ) 
				{
					int lTotalCount = mFileObject->FieldReadGetCount () / 3;
					lLayerElementNormal->GetDirectArray().Resize(lTotalCount);
					FbxLayerElementArrayTemplate<FbxVector4>& lNormals = lLayerElementNormal->GetDirectArray();

					for (i = 0; i < lTotalCount; i ++) 
					{
						FbxVector4 lVector = lNormals.GetAt(lIndices[i]);

						lVector[0] += mFileObject->FieldReadD ();
						lVector[1] += mFileObject->FieldReadD ();
						lVector[2] += mFileObject->FieldReadD ();

						lNormals.SetAt(lIndices[i], lVector);
					}

					mFileObject->FieldReadEnd ();
				}
			}
        }       
        
        mFileObject->FieldReadBlockEnd();  
    }

    return true;
}

bool FbxReaderFbx5::ReadUserProperties(FbxNode& pNode)
{
	if (mFileObject->IsBeforeVersion6())
	{
		while (mFileObject->FieldReadBegin(FIELD_USERPROPERTIES))
		{
			if (mFileObject->FieldReadBlockBegin())
			{
				FbxProperty lProperty;

				FbxString lName = mFileObject->FieldReadS(FIELD_USERPROPERTIES_NAME, "UserProp");
				FbxString lTypeName = mFileObject->FieldReadS(FIELD_USERPROPERTIES_TYPE, "");
				FbxString lLabel= mFileObject->FieldReadS(FIELD_USERPROPERTIES_LABEL,"");

				if (lTypeName.CompareNoCase("Vector") != 0)
				{
					// The type vector is the only one not to have his type
					// in the cannel name (!)
					lName += FbxString(" (") + lTypeName + FbxString(")");
				}

				// interpretation of the data types
				FbxDataType			lDataType= mManager.GetDataTypeFromName(lTypeName.Buffer());
				if (!lDataType.Valid()) lDataType=mManager.GetDataTypeFromName(lTypeName.Buffer());

				FBX_ASSERT_MSG(lDataType.Valid(),"Unsupported type!"); 

				lProperty = FbxProperty::Create(&pNode, lDataType, lName.Buffer(), lLabel.Buffer());
				if (lProperty.IsValid())
				{
					// Properties in v5 was all user properties and animatable
					lProperty.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
					lProperty.ModifyFlag(FbxPropertyFlags::eAnimatable, true);
					lProperty.ModifyFlag(FbxPropertyFlags::eImported, true);

					// read the default values
					// This field apears only if the property is not animated
					EFbxType lPropType = lProperty.GetPropertyDataType().GetType();
					if (lProperty.GetPropertyDataType() == FbxColor3DT ||
						lProperty.GetPropertyDataType() == FbxColor4DT)
					{
						double lValue[3];
						if (mFileObject->FieldReadBegin(FIELD_USERPROPERTIES_VALUE))
						{
							mFileObject->FieldRead3D(lValue);
							mFileObject->FieldReadEnd();

							FbxColor lColorValue(lValue[0], lValue[1], lValue[2]);
							lProperty.Set(lColorValue);
						}
					}
					else
					switch (lPropType)
					{
					    default:
				        break;
						case eFbxBool:
						{
							bool lValue;
							lValue = mFileObject->FieldReadB(FIELD_USERPROPERTIES_VALUE, false);
							lProperty.Set(lValue);
						}
						break;
						case eFbxFloat:
						case eFbxDouble:
						{
							double lValue;
							lValue = mFileObject->FieldReadD(FIELD_USERPROPERTIES_VALUE, 0.0);
							lProperty.Set(lValue);
						}
						break;
						case eFbxInt:
						{
							int lValue;
							lValue = mFileObject->FieldReadI(FIELD_USERPROPERTIES_VALUE, 0);
							lProperty.Set(lValue);
						}
						break;
						case eFbxDouble3:
						case eFbxDouble4:
						{
							FbxDouble3 lValue;
							if (mFileObject->FieldReadBegin(FIELD_USERPROPERTIES_VALUE))
							{
								mFileObject->FieldRead3D(&(lValue[0]));
								mFileObject->FieldReadEnd();
								lProperty.Set(lValue);
							}
						}
						break;
					}

					// read the min and max values (if present)
					double lMin = mFileObject->FieldReadD(FIELD_USERPROPERTIES_MIN, -HUGE_VAL);
					double lMax = mFileObject->FieldReadD(FIELD_USERPROPERTIES_MAX, HUGE_VAL);

					lProperty.SetMinLimit(lMin);
					lProperty.SetMaxLimit(lMax);
				}

				mFileObject->FieldReadBlockEnd();
			}

			mFileObject->FieldReadEnd();
		}
	}

	return true;
}

bool FbxReaderFbx5::ReadConnectionSection()
{
	if (mFileObject->FieldReadBegin("Connections")) 
	{
		if (mFileObject->FieldReadBlockBegin())
		{
			while (mFileObject->FieldReadBegin("Connect")) 
			{
			    char Type[32];	
				FbxProperty SrcP,DstP;
			    FbxObject*	Src = NULL;
			    FbxObject*	Dst = NULL;
			    FbxObject* Object = NULL;

				FBXSDK_strncpy(Type, 32, mFileObject->FieldReadC(), 31);
				if (strcmp(Type,"OO")==0) {
					Src = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
					Dst = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
					// Patch for connections to FbxNode vs Node Attributes
//					ConvertConnectionSrcDst(Src,Dst);
				} else if (strcmp(Type,"PO")==0) {
					Object = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
					if (Object) {
						FbxString PropertyName = mFileObject->FieldReadC();
						SrcP = Object->FindProperty(PropertyName.Buffer());
						if (SrcP.IsValid()) {
							Src	 = Object;
						}
					}
					Dst = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
				} else if (strcmp(Type,"OP")==0) {						
					Src	   = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
					Object = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
					if (Object) {
						FbxString PropertyName = mFileObject->FieldReadC();
						DstP = Object->FindProperty(PropertyName.Buffer());
						if (DstP.IsValid()) {
							Dst	 = Object;
						}
					}
				} else if (strcmp(Type,"PP")==0) {	
				}
				if (Src && Dst) { 
					if (SrcP.IsValid()) {
						if (DstP.IsValid()) {
							if (!DstP.IsConnectedSrcProperty(SrcP)) { 
								DstP.ConnectSrcProperty(SrcP);
							}
						} else {
							if (!Dst->IsConnectedSrcProperty(SrcP)) { 
								Dst->ConnectSrcProperty(SrcP);
							}
						}
					} else if (DstP.IsValid()) {
						if (!DstP.IsConnectedSrcObject(Src)) { 
							DstP.ConnectSrcObject(Src);
						}
					} else {
						if (!Dst->IsConnectedSrcObject(Src)) { 
							Dst->ConnectSrcObject(Src);
						}
					}
				}
				mFileObject->FieldReadEnd();
			}
			mFileObject->FieldReadBlockEnd();
		}
		mFileObject->FieldReadEnd();
	}
	return true;
}

FbxString FbxReaderFbx5::ConvertCameraName(FbxString pCameraName)
{
	FbxString lKModel = "Model::";
	FbxString lKModelProducerPerspective	= lKModel + FBXSDK_CAMERA_PERSPECTIVE;
	FbxString lKModelProducerTop			= lKModel + FBXSDK_CAMERA_TOP;
	FbxString lKModelProducerFront		= lKModel + FBXSDK_CAMERA_FRONT;
	FbxString lKModelProducerBack			= lKModel + FBXSDK_CAMERA_BACK;
	FbxString lKModelProducerRight		= lKModel + FBXSDK_CAMERA_RIGHT;
	FbxString lKModelProducerLeft			= lKModel + FBXSDK_CAMERA_LEFT;
	FbxString lKModelCameraSwitcher		= lKModel + FBXSDK_CAMERA_SWITCHER;

	if (pCameraName == lKModelProducerPerspective)
	{
		return FBXSDK_CAMERA_PERSPECTIVE;
	}
	else if (pCameraName == lKModelProducerTop)
	{
		return FBXSDK_CAMERA_TOP;
	}
	else if (pCameraName == lKModelProducerFront)
	{
		return FBXSDK_CAMERA_FRONT;
	}
	else if (pCameraName == lKModelProducerBack)
	{
		return FBXSDK_CAMERA_BACK;
	}
	else if (pCameraName == lKModelProducerRight)
	{
		return FBXSDK_CAMERA_RIGHT;
	}
	else if (pCameraName == lKModelProducerLeft)
	{
		return FBXSDK_CAMERA_LEFT;
	}
	else if (pCameraName == lKModelCameraSwitcher)
	{
		return FBXSDK_CAMERA_SWITCHER;
	}
	else
	{
		return pCameraName;
	}
}


void FbxReaderFbx5::ReadTimeWarps(FbxIO& pFileObject, FbxMultiMap& pTimeWarpSet)
{
    int lVersion = 0;

    if (pFileObject.FieldReadBegin("TimeWarps")) 
    {
        if (pFileObject.FieldReadBlockBegin())
		{
			lVersion = pFileObject.FieldReadI("Version");

			while (pFileObject.FieldReadBegin("TW"))
			{
				// TimeWarp ID
				int lNickNumber = pFileObject.FieldReadI();

				if (pFileObject.FieldReadBlockBegin())
				{
                    FbxAnimUtilities::CurveNodeIntfce lTimeWarp = FbxAnimUtilities::CreateCurveNode(&pFileObject);

					if (lTimeWarp.IsValid()) 
					{
						pTimeWarpSet.Add(lNickNumber, (FbxHandle) lTimeWarp.GetHandle());
					}

					pFileObject.FieldReadBlockEnd();
				}
				pFileObject.FieldReadEnd();
			}

			pFileObject.FieldReadBlockEnd();
		}
        pFileObject.FieldReadEnd();
    }
}

// Obsolete code. Remove for MB 6.0. Luc
bool FbxReaderFbx5::GenerateParametricGeometryLayer(FbxGeometry& pGeometry)
{
	pGeometry.CreateLayer();

	FbxLayer* lLayer = pGeometry.GetLayer(0);

	FbxLayerElementMaterial* lMaterial = lLayer->GetMaterials();
	if(lMaterial)
	{
		lMaterial->SetMappingMode(FbxLayerElement::eAllSame);
		lMaterial->SetReferenceMode(FbxLayerElement::eDirect);
	}
	

	FbxLayerElementTexture* lTexture = lLayer->GetTextures(FbxLayerElement::eTextureDiffuse);
	if(lTexture)
	{
		lTexture->SetMappingMode(FbxLayerElement::eAllSame);
		lTexture->SetReferenceMode(FbxLayerElement::eDirect);
	}

	return true;
}

void FbxReaderFbx5::CorrectTextureLayers(FbxMesh& pMesh)
{
	// This function correct the very few v5.0 files containing
	// texture layering. In those files, texture assignation 
	// was not present for all textures. This result in FbxLayerElement
	// not filled properly
	if (pMesh.GetLayerCount(FbxLayerElement::eTextureDiffuse) > 1)
	{
		int lLayerIndex, lLayerCount = pMesh.GetLayerCount(FbxLayerElement::eTextureDiffuse);
		for (lLayerIndex=0; lLayerIndex<lLayerCount; lLayerIndex++)
		{
			FbxLayerElementTexture* lLET = pMesh.GetLayer(lLayerIndex, FbxLayerElement::eTextureDiffuse)->GetTextures(FbxLayerElement::eTextureDiffuse);

			if (lLET->GetReferenceMode() == FbxLayerElement::eIndexToDirect && mTemporaryTextures.GetCount())
			{
				int i, lIndexCount = lLET->GetIndexArray().GetCount();
				for (i=0; i<lIndexCount; i++)
				{
					int lIndexToDirect = lLET->GetIndexArray().GetAt(i);

					// Element not in the array
					while (lLET->GetDirectArray().GetCount() <= lIndexToDirect)
					{
						int lNextIndex = lLET->GetDirectArray().GetCount();

						// There should not be more textures in the direct array
						// then they are in the mTemporaryTextures array
						if (lNextIndex < mTemporaryTextures.GetCount())
						{
							lLET->GetDirectArray().Add(mTemporaryTextures[lNextIndex]);
						}
						else
						{
							// This should never be the case
							lLET->GetDirectArray().Add(mTemporaryTextures[0]);
						}
					}
				}
			}
		}
	}
}


int FbxReaderFbx5::FindString(FbxString pString, FbxArray<FbxString*>& pStringArray)
{
    int i, lCount = pStringArray.GetCount();

    for (i = 0; i < lCount; i++)
    {
        if (pStringArray[i]->Compare(pString) == 0)
        {
            return i;
        }
    }

    return -1;
}


FbxString FbxReaderFbx5::FindFile(FbxString pFullFilePath, FbxString pRelativeFilePath /* = "" */)
{
    FbxString lFullFilePath;
    FbxString lRelativeFilePath;

    if (!pFullFilePath.IsEmpty())
    {
        lFullFilePath = mFileObject->GetFullFilePath(pFullFilePath.Buffer());
    }

    if (!pRelativeFilePath.IsEmpty())
    {
        lRelativeFilePath = mFileObject->GetFullFilePath(pRelativeFilePath.Buffer());
    }

    if (!lFullFilePath.IsEmpty() && FbxFileUtils::Exist(lFullFilePath.Buffer()))
    {
        return lFullFilePath;
    }
    else if (!lRelativeFilePath.IsEmpty() && FbxFileUtils::Exist(lRelativeFilePath.Buffer()))
    {
        return lRelativeFilePath;
    }
    else if (!pFullFilePath.IsEmpty())
    {
        return lFullFilePath;
    }
    else if (!pRelativeFilePath.IsEmpty())
    {
        return lRelativeFilePath;
    }
    else
    {
        return "";
    }
}


bool FbxReaderFbx5::ReadPassword(FbxString pPassword)
{
    if (mFileObject->IsPasswordProtected())
    {
        if (!mFileObject->CheckPassword(pPassword.Buffer()))
		{
			return false;			
		}
    }

    return true;
}

void FbxReaderFbx5::ReadSceneGenericPersistenceSection(FbxScene& pScene)
{
	if (mFileObject->FieldReadBegin("SceneGenericPersistence"))
	{
		if (mFileObject->FieldReadBlockBegin())
		{
			int lSkipReadPose = false;

            if(IOS_REF.GetBoolProp(IMP_FBX_CHARACTER, true))
			{
				//mImporter.ProgressUpdate(NULL, "Retrieving character poses", "", 0);
				lSkipReadPose = ReadCharacterPose(pScene) == 1;
			}

			if (!lSkipReadPose)
			{
				ReadPose(pScene);
			}

			pScene.SetSceneInfo(ReadSceneInfo());

			mFileObject->FieldReadBlockEnd();
		}
		mFileObject->FieldReadEnd();
	}
}

void FbxReaderFbx5::ReadPoses(FbxScene& pScene)
{
	// Version 6.0 poses, in the "Objects" section
    
    if(IOS_REF.GetBoolProp(IMP_FBX_CHARACTER, true))
	{
		ReadCharacterPose(pScene);
	}
}

//
// Local utility functions
//
int FindTypeIndex(FbxString& pTypeName, FbxArray<Fbx5ObjectTypeInfo*>& pTypeList)
{
	int i, lCount = pTypeList.GetCount();

	for (i=0; i<lCount; i++)
	{
		if (pTypeList[i]->mType == pTypeName) return i;		
	}

	return -1;
}



FbxLayerElement::EMappingMode ConvertMappingModeToken(const char* pToken)
{
	FbxLayerElement::EMappingMode lMappingMode = FbxLayerElement::eNone;

	if(!strcmp(pToken, TOKEN_KFBXGEOMETRYMESH_BY_VERTICE))
	{
		lMappingMode = FbxLayerElement::eByControlPoint;
	}
	else if(!strcmp(pToken, TOKEN_KFBXGEOMETRYMESH_BY_POLYGON_VERTEX))
	{
		lMappingMode = FbxLayerElement::eByPolygonVertex;
	}
	else if(!strcmp(pToken, TOKEN_KFBXGEOMETRYMESH_BY_POLYGON))
	{
		lMappingMode = FbxLayerElement::eByPolygon;
	}
	else if(!strcmp(pToken, TOKEN_KFBXGEOMETRYMESH_ALL_SAME))
	{
		lMappingMode = FbxLayerElement::eAllSame;
	}

	return lMappingMode; 
}


FbxLayerElement::EReferenceMode ConvertReferenceModeToken(const char* pToken)
{
	FbxLayerElement::EReferenceMode lReferenceMode = FbxLayerElement::eDirect;

	if(!strcmp(pToken , TOKEN_REFERENCE_INDEX))
	{
		lReferenceMode = FbxLayerElement::eIndex;
	}
	else if(!strcmp(pToken , TOKEN_REFERENCE_INDEX_TO_DIRECT))
	{
		lReferenceMode = FbxLayerElement::eIndexToDirect;
	}

	return lReferenceMode;
}


FbxLayerElementTexture::EBlendMode ConvertBlendModeToken(const char* pToken)
{
	FbxLayerElementTexture::EBlendMode lBlendMode = FbxLayerElementTexture::eTranslucent;

	if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_ADD))
	{
		lBlendMode = FbxLayerElementTexture::eAdd;
	}
	else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_MODULATE))
	{
		lBlendMode = FbxLayerElementTexture::eModulate;
	}
	else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_MODULATE2))
	{
		lBlendMode = FbxLayerElementTexture::eModulate2;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_OVER))
    {
        lBlendMode = FbxLayerElementTexture::eOver;
    }
	else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_MAXBLEND))
	{
		lBlendMode = FbxLayerElementTexture::eBlendModeCount;
	}

	return lBlendMode; 
}


FbxConstraint::EType ConvertConstraintToken(const char* pToken)
{
	FbxConstraint::EType lConstraintMode = FbxConstraint::eUnknown;

	if(!strcmp(pToken, TOKEN_KFBXCONSTRAINT_POSITION))
	{
		lConstraintMode = FbxConstraint::ePosition;
	}
	else if(!strcmp(pToken, TOKEN_KFBXCONSTRAINT_ROTATION))
	{
		lConstraintMode = FbxConstraint::eRotation;
	}
	else if(!strcmp(pToken, TOKEN_KFBXCONSTRAINT_SCALE))
	{
		lConstraintMode = FbxConstraint::eScale;
	}
	else if(!strcmp(pToken, TOKEN_KFBXCONSTRAINT_PARENT))
	{
		lConstraintMode = FbxConstraint::eParent;
	}
	else if(!strcmp(pToken, TOKEN_KFBXCONSTRAINT_SINGLECHAINIK))
	{
		lConstraintMode = FbxConstraint::eSingleChainIK;
	}
	else if(!strcmp(pToken, TOKEN_KFBXCONSTRAINT_AIM))
	{
		lConstraintMode = FbxConstraint::eAim;
	}
	else if(!strcmp(pToken, TOKEN_KFBXCONSTRAINT_CHARACTER))
	{
		lConstraintMode = FbxConstraint::eCharacter;
	}

	return lConstraintMode; 
}

bool FbxReaderFbx5::ReadProperties(FbxObject *pObject)
{
	if ( mFileObject->IsBeforeVersion6() )	// Before MB 6.0
	{
		if(mFileObject->FieldReadBegin("Properties"))
		{
			if (mFileObject->FieldReadBlockBegin())
			{
				// -- Version 100 is the first version.
				int lVersion = mFileObject->FieldReadI("Version", 0);

				if(lVersion >= 100)
				{
					FbxObject*			lPropertyObject	= pObject;
					FbxProperty		lFbxProperty    = lPropertyObject->GetFirstProperty();
					FbxProperty		lFbxNextProperty;
					FbxNodeAttribute*	lNodeAttribute	= pObject->GetSrcObject<FbxNodeAttribute>();

					while (lFbxProperty!=0) {
						lFbxNextProperty = lPropertyObject->GetNextProperty(lFbxProperty);

						// Patch for node attributes
						// @@@ !!
						if (!lFbxNextProperty.IsValid() && lNodeAttribute && lNodeAttribute!=lPropertyObject) {
							lPropertyObject = lNodeAttribute;
							lFbxNextProperty= lPropertyObject->GetFirstProperty();
						}
						
						// The property can be loaded and saved -> save it!
						if (!lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable))
						{
							// The property can be loaded and saved -> load it!
							FbxString lString = lFbxProperty.GetName();
							if(mFileObject->FieldReadBegin( lString.Buffer() ) ) {
								switch (lFbxProperty.GetPropertyDataType().GetType()) {
									case eFbxBool:	lFbxProperty.Set( FbxBool(mFileObject->FieldReadB()) );	 break;
									case eFbxInt: lFbxProperty.Set( FbxInt(mFileObject->FieldReadI()) ); break;
									case eFbxFloat:	lFbxProperty.Set( FbxFloat(mFileObject->FieldReadF()) );	 break;
									case eFbxDouble:  lFbxProperty.Set( FbxDouble(mFileObject->FieldReadD()) );	 break;
									case eFbxDouble3: {
										FbxDouble3 lValue;
										mFileObject->FieldRead3D((double *)&lValue);		
										lFbxProperty.Set( lValue );
									} break;
									case eFbxDouble4: {
										FbxDouble4 lValue;
										mFileObject->FieldRead4D((double *)&lValue);		
										lFbxProperty.Set( lValue );
									} break;
									case eFbxDouble4x4: {
										FbxDouble4x4 lValue;
										mFileObject->FieldRead4D( (double *)&(lValue[0]) );		
										mFileObject->FieldRead4D( (double *)&(lValue[1]) );		
										mFileObject->FieldRead4D( (double *)&(lValue[2]) );		
										mFileObject->FieldRead4D( (double *)&(lValue[3]) );		
										lFbxProperty.Set( lValue );
									} break;
									case eFbxEnum:		  lFbxProperty.Set( FbxEnum(mFileObject->FieldReadI()) ); break;
									case eFbxString:	  lFbxProperty.Set( FbxString(mFileObject->FieldReadS()) ); break;
									case eFbxTime:		  lFbxProperty.Set( FbxTime(mFileObject->FieldReadT()) ); break;
									case eFbxReference:  break; // used as a port entry to reference object or properties
									case eFbxDistance:	 {
										float value = mFileObject->FieldReadF();
										FbxString unit = mFileObject->FieldReadS();
										lFbxProperty.Set( FbxDistance(value, unit));
									} break;
									default:
										FBX_ASSERT_NOW("Unsupported type!"); 
									break;
								}
								mFileObject->FieldReadEnd();
							}
						}
						lFbxProperty    = lFbxNextProperty;
					}
				}
				mFileObject->FieldReadBlockEnd();
			}
			mFileObject->FieldReadEnd();
		}
	}
	return true;
}

#include <fbxsdk/fbxsdk_nsend.h>

