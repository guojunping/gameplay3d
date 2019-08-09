/****************************************************************************************
 
   Copyright (C) 2017 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/scene/shading/fbxsemanticentryview.h>
#include <fbxsdk/fileio/collada/fbxreadercollada14.h>
#include <fbxsdk/fileio/collada/fbxcolladaanimationelement.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

// Save the offset and the index array for this layer element;
struct LayerElementInfo
{
    LayerElementInfo(int pOffset, FbxLayerElementArray * pIndexArray) : mOffset(pOffset), mIndexArray(pIndexArray) {}
	static int Compare(const void* pA, const void* pB)
    {
		LayerElementInfo* lA = (LayerElementInfo*)pA;
		LayerElementInfo* lB = (LayerElementInfo*)pB;
		if( !lA || !lB ) return 0;
		return lA->mOffset < lB->mOffset ? -1 : lA->mOffset > lB->mOffset ? 1 : 0;
    }
    int mOffset;
    FbxLayerElementArray * mIndexArray;
};

//
//Constructors
//
FbxReaderCollada::FbxReaderCollada(FbxManager& pManager, int pID, FbxStatus& pStatus) :
    FbxReader(pManager, pID, pStatus),
	mFileName(""),
	mXmlDoc(NULL),
    mAnimLayer(NULL)
{
    mFileObject         = FbxNew<FbxFile>();

    mColladaElement = NULL;
    mScene = NULL;
    mGlobalSettings = NULL;
    mDocumentInfo = NULL;

    // Initialize the type traits
    mEffectTypeTraits.library_tag = COLLADA_LIBRARY_EFFECT_ELEMENT;
    mEffectTypeTraits.element_tag = COLLADA_EFFECT_ELEMENT;

    mMaterialTypeTraits.library_tag = COLLADA_LIBRARY_MATERIAL_ELEMENT;
    mMaterialTypeTraits.element_tag = COLLADA_MATERIAL_STRUCTURE;

    mImageTypeTraits.library_tag = COLLADA_LIBRARY_IMAGE_ELEMENT;
    mImageTypeTraits.element_tag = COLLADA_IMAGE_STRUCTURE;

    mGeometryTypeTraits.library_tag = COLLADA_LIBRARY_GEOMETRY_ELEMENT;
    mGeometryTypeTraits.element_tag = COLLADA_GEOMETRY_STRUCTURE;

    mControllerTypeTraits.library_tag = COLLADA_LIBRARY_CONTROLLER_ELEMENT;
    mControllerTypeTraits.element_tag = COLLADA_CONTROLLER_STRUCTURE;

    mLightTypeTraits.library_tag = COLLADA_LIBRARY_LIGHT_ELEMENT;
    mLightTypeTraits.element_tag = COLLADA_LIGHT_STRUCTURE;

    mCameraTypeTraits.library_tag = COLLADA_LIBRARY_CAMERA_ELEMENT;
    mCameraTypeTraits.element_tag = COLLADA_CAMERA_STRUCTURE;

    mNodeTypeTraits.library_tag = COLLADA_LIBRARY_NODE_ELEMENT;
    mNodeTypeTraits.element_tag = COLLADA_NODE_STRUCTURE;

    mAnimationTypeTraits.library_tag = COLLADA_LIBRARY_ANIMATION_ELEMENT;
    mAnimationTypeTraits.element_tag = COLLADA_ANIMATION_STRUCTURE;
}

//
//Destructor
//
FbxReaderCollada::~FbxReaderCollada()
{
    if (mFileObject->IsOpen())
    {
        FileClose();
    }

    FbxArrayDelete(mTakeInfo);
    FBX_SAFE_DELETE(mFileObject);
}

//
//Open file with the given name.
//Return true on success, false otherwise.
//
bool FbxReaderCollada::FileOpen(char* pFileName)
{
	if (mFileObject->IsOpen())
	{
		FileClose();
	}

	if (!mFileObject->Open(pFileName, FbxFile::eReadOnly, false))
	{
        GetStatus().SetCode(FbxStatus::eFailure, "File not opened");
		return false;
	}

	mFileName = pFileName;

    // Parse the file into an XML object tree
    //
    if (mXmlDoc)
    {
        xmlFreeDoc(mXmlDoc);
    }

    // Replace xmlParseFile with xmlReadFile so we have the option to allow parsing huge
    // data (relaxing the xml parser)	
	mXmlDoc = xmlReadFile(mFileName, NULL, XML_PARSE_HUGE);
    if (!mXmlDoc)
    {
        GetStatus().SetCode(FbxStatus::eFailure, "Unable to parse xml/dae file");
        AddNotificationError( "Unable to parse xml/dae file\n" );

        return false;
    }

    mColladaElement = xmlDocGetRootElement(mXmlDoc);
    mGlobalSettings = FbxGlobalSettings::Create(&mManager, "");

    // For preview section in UI of Maya/Max plug-ins, read asset and animation clips (animation takes)
    xmlNode * lAssetElement = DAE_FindChildElementByTag(mColladaElement, COLLADA_ASSET_STRUCTURE);
    if (lAssetElement)
    {
        mDocumentInfo = FbxDocumentInfo::Create(&mManager, "");
        ImportAsset(lAssetElement, *mGlobalSettings, *mDocumentInfo);
    }

    xmlNode * lLibraryAnimationsClipsElement = DAE_FindChildElementByTag(mColladaElement, COLLADA_LIBRARY_ANIMATION_CLIP_ELEMENT);
    if (lLibraryAnimationsClipsElement)
    {
        xmlNode * lAnimationClipElement = DAE_FindChildElementByTag(lLibraryAnimationsClipsElement, COLLADA_ANIMCLIP_ELEMENT);
		FbxString lExternalRef;
		while (lAnimationClipElement)
        {
            const FbxString lID = DAE_GetElementAttributeValue(lAnimationClipElement, COLLADA_ID_PROPERTY);
            AnimationClipData lAnimClipData(lID);
            FbxTakeInfo * lTakeInfo = FbxNew<FbxTakeInfo>();
            lTakeInfo->mName = lID;

            xmlNode * lInstanceAnimationElement = DAE_FindChildElementByTag(lAnimationClipElement, COLLADA_INSTANCE_ANIMATION_ELEMENT);
            while (lInstanceAnimationElement)
            {
                const FbxString lURL = DAE_GetIDFromUrlAttribute(lInstanceAnimationElement, lExternalRef);
                lAnimClipData.mAnimationElementIDs.Insert(lURL);

                lInstanceAnimationElement = DAE_FindChildElementByTag(lAnimationClipElement, COLLADA_INSTANCE_ANIMATION_ELEMENT, lInstanceAnimationElement);
            }

			mAnimationClipData.PushBack(lAnimClipData);
            mTakeInfo.Add(lTakeInfo);

            lAnimationClipElement = DAE_FindChildElementByTag(lLibraryAnimationsClipsElement, COLLADA_ANIMCLIP_ELEMENT, lAnimationClipElement);
        }
    }

	return GetStatus();
}

//
//Close file.
//Return true on success, false otherwise.
//
bool FbxReaderCollada::FileClose()
{
	// Not an error case
	if (!mFileObject->IsOpen())
	{
		GetStatus().SetCode(FbxStatus::eFailure, "File not opened");
		return false;
	}

	if( mFileObject->IsOpen() )
	{
        mFileObject->Close();
	}
	mFileName = "";

	if (mGlobalSettings)            
	{
		// this object is not given to the scene (like the mDocumentInfo) so
		// we need to destroy it to avoid memory leaks.
		mGlobalSettings->Destroy(); 
		mGlobalSettings = NULL;
	}
	return true;
}

//
//Check if current file is open.
//Return true if file is open, false otherwise.
//   
bool FbxReaderCollada::IsFileOpen()
{
    return mFileObject->IsOpen();
}


//
//Read from Collada file and import it to the FBX document, according to the given options settings.
//1. Get FBX scene from FBX document (pDocument);
//2. Set current take with the default take name;
//3. Parse Collada file into a XML nodes tree;
//4. Read Collada document, import Collada XML nodes tree to FBX scene; 
//   all nodes, all animations, scene info, global ambient are both imported;
//5. Set global time settings;
//6. Set take info;
//7. Apply renaming strategy, rename all the nodes to avoid name clash;
//8. Free XML document;
//9. Importing Collada to FBX is done.
//
bool FbxReaderCollada::Read(FbxDocument* pDocument) 
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

    char lPrevious_Locale_LCNUMERIC[100];
    memset(lPrevious_Locale_LCNUMERIC, 0, 100);
    FBXSDK_strcpy(lPrevious_Locale_LCNUMERIC, 100, setlocale(LC_NUMERIC, 0)); // query current setting for LC_NUMERIC
    setlocale(LC_NUMERIC, "C");  // set locale using period as decimal separator

    // Read file
    bool lResult = ReadCollada(*lScene, mColladaElement);
    if (!lResult)
    {
		// don't overwrite any previously set error code
		if (GetStatus().GetCode() == FbxStatus::eSuccess)
			GetStatus().SetCode(FbxStatus::eFailure, "Unable to parse xml/dae file");
        AddNotificationError( "Unable to parse xml/dae file\n" );
    }

    // set numeric locale back
    setlocale(LC_NUMERIC, lPrevious_Locale_LCNUMERIC);

    if (mXmlDoc)
    {
        xmlFreeDoc(mXmlDoc);
        mXmlDoc = NULL;
    }

    return GetStatus();
}

//
//Import Collada XML nodes tree to FBX scene.
//1. Get and verify Collada Version;
//2. Reset global ambient;
//3. Import asset;
//4. Read library_animation element firstly, record the mapping between animation elements and node/attribute IDs
//5. Read the scene which includes nodes and node attributes, add animation if exists
//6. Import deformers, creates skins and poses
//6. Set scene info;
//9. ReadCollada is done.
//
bool FbxReaderCollada::ReadCollada(FbxScene &pScene, xmlNode* pXmlNode) 
{
    mScene = &pScene;

    // Create a default animation stack
    if (mAnimLayer == NULL)
    {
        const char * TAKE_NAME = "Take 001";
        FbxAnimStack* lAnimStack = FbxAnimStack::Create(mScene, TAKE_NAME);
        mAnimLayer = FbxAnimLayer::Create(lAnimStack, "Layer0");
    }

	// Parse only the document structure
	if (strcmp((const char*) pXmlNode->name, COLLADA_DOCUMENT_STRUCTURE))
	{
		return false;
	}

	// Get and verify Collada Version
	FbxString sgColladaVersion = DAE_GetElementAttributeValue(pXmlNode, COLLADA_VERSION_PROPERTY);
    if (CheckColladaVersion(sgColladaVersion) == false)
        return false;

    // Reset scene ambient
    pScene.GetGlobalSettings().SetAmbientColor( FbxColor(0,0,0) );

    // Set asset
    if (mGlobalSettings)
    {
        pScene.GetGlobalSettings().SetSystemUnit(mGlobalSettings->GetSystemUnit());
        pScene.GetGlobalSettings().SetAxisSystem(mGlobalSettings->GetAxisSystem());
    }
    
    if (mDocumentInfo)
    {
        pScene.SetSceneInfo(mDocumentInfo);
    }

    Preprocess(mColladaElement);

    BuildUpLibraryMap();

    ImportScene(pXmlNode);

	// In the case the Collada object names are coming from an FBX renaming strategy,
	// set the original name back
	FbxRenamingStrategyCollada lRenamer;
	lRenamer.DecodeScene(mScene);

    return GetStatus();
}

bool FbxReaderCollada::GetAxisInfo(FbxAxisSystem* pAxisSystem, FbxSystemUnit* pSystemUnits)
{
    if (mGlobalSettings)
    {
        *pAxisSystem  = mGlobalSettings->GetAxisSystem();
        *pSystemUnits = mGlobalSettings->GetSystemUnit();
        return true;
    }

    return false;
}

FbxArray<FbxTakeInfo*>* FbxReaderCollada::GetTakeInfo()
{
    return &mTakeInfo;
}

bool FbxReaderCollada::ImportVisualScene(xmlNode* pXmlNode, FbxScene * pScene)
{
    const FbxString lId = DAE_GetElementAttributeValue(pXmlNode, COLLADA_ID_PROPERTY);
    const FbxString lName = DAE_GetElementAttributeValue(pXmlNode, COLLADA_NAME_PROPERTY);
    DAE_SetName(pScene, lName, lId);

    FbxSystemUnit lLocalUnit;

    if (mGlobalSettings != NULL)
    {
        lLocalUnit = mGlobalSettings->GetSystemUnit();
    }

    for (xmlNode* lChild = pXmlNode->children; lChild != NULL; lChild = lChild->next)
    {
        if (lChild->type != XML_ELEMENT_NODE) continue;

        FbxString structure = (const char*)lChild->name;
        if (structure == COLLADA_NODE_STRUCTURE)
        {
            FbxNode * lNode = ImportNode(lChild);

            if (lNode)
            {
                pScene->GetRootNode()->ConnectSrcObject(lNode);
            }
        }
        else if (structure == COLLADA_EXTRA_STRUCTURE) 
        {
            xmlNode * lTechniqueElement = DAE_FindChildElementByTag(lChild, COLLADA_TECHNIQUE_STRUCTURE);
            while (lTechniqueElement)
            {
                const FbxString lProfile = DAE_GetElementAttributeValue(lTechniqueElement, COLLADA_PROFILE_PROPERTY);
                if (lProfile == COLLADA_MAX3D_PROFILE)
                {
                    ImportVisualSceneMax3DExtension(lTechniqueElement, pScene);
                }
                else if (lProfile == COLLADA_FCOLLADA_PROFILE)
                {
                    ImportVisualSceneFCOLLADAExtension(lTechniqueElement, pScene);
                }
                else if (lProfile == COLLADA_MAYA_PROFILE)
                {
                    ImportVisualSceneMayaExtension(lTechniqueElement, pScene);
                }
                else
                {
                    const FbxString lMessage = FbxString("The unsupported technique element with profile \"") + lProfile 
                        + "\" in visual_scene element \"" + lId + "\"";
                    AddNotificationWarning(lMessage);
                }

                lTechniqueElement = DAE_FindChildElementByTag(lChild, COLLADA_TECHNIQUE_STRUCTURE, lTechniqueElement);
            }
        }
        else if (structure == COLLADA_ASSET_STRUCTURE)
        {
            xmlNode * lUnitElement = DAE_FindChildElementByTag(lChild, COLLADA_UNIT_STRUCTURE);
            if (lUnitElement)
            {
                lLocalUnit = DAE_ImportUnit(lUnitElement);
            }
        }
        else
        {
            FbxString msg = FbxString("Structure ") + structure +  " unknown";
            AddNotificationError( msg );
        }
    }

	SkinMapType::ConstIterator lSkinElementIter = mSkinElements.Begin();
    for (; lSkinElementIter != mSkinElements.End(); ++lSkinElementIter)
    {
		xmlNode * lSkinElement = lSkinElementIter->GetValue();
        ImportSkin(lSkinElement);
    }

	TargetIDMapType::ConstIterator lTargetIDIter = mTargetIDs.Begin();
    for (; lTargetIDIter != mTargetIDs.End(); ++lTargetIDIter)
    {
		FbxNode * lNode = lTargetIDIter->GetKey();
		FbxNode * lTargetNode = mIDNamespaceNodes[lTargetIDIter->GetValue()];
        if (lNode && lTargetNode)
            lNode->SetTarget(lTargetNode);
    }

    if ((mGlobalSettings != NULL) && (lLocalUnit != mGlobalSettings->GetSystemUnit()))
        mGlobalSettings->GetSystemUnit().ConvertChildren(mScene->GetRootNode(), lLocalUnit);

    return true;
}

bool FbxReaderCollada::ImportVisualSceneMax3DExtension(xmlNode * pTechniqueElement, FbxScene * pScene)
{
    for (xmlNode* lChildElement = pTechniqueElement->children; lChildElement != NULL; lChildElement = lChildElement->next)
    {
        if (lChildElement->type != XML_ELEMENT_NODE) continue;

        const FbxString lElementTag = (const char*)lChildElement->name;
        if (lElementTag == COLLADA_MAX3D_FRAMERATE_ELEMENT)
        {
            double lFrameRate = 0;
            DAE_GetElementContent(lChildElement, lFrameRate);
            FbxTime::EMode lTimeMode = FbxTime::ConvertFrameRateToTimeMode(lFrameRate);
            if (lTimeMode == FbxTime::eDefaultMode || lTimeMode == FbxTime::eCustom)
            {
                lTimeMode = FbxTime::eCustom;
                pScene->GetGlobalSettings().SetCustomFrameRate(lFrameRate);
            }

            pScene->GetGlobalSettings().SetTimeMode(lTimeMode);
        }
        else
        {
            const FbxString lMessage = FbxString("The unsupported element in visual_scene MAX3D extension: \"") + lElementTag + "\"";
            AddNotificationWarning(lMessage);
        }
    }

    return true;
}

bool FbxReaderCollada::ImportVisualSceneFCOLLADAExtension(xmlNode * pTechniqueElement, FbxScene * pScene)
{
    for (xmlNode* lChildElement = pTechniqueElement->children; lChildElement != NULL; lChildElement = lChildElement->next)
    {
        if (lChildElement->type != XML_ELEMENT_NODE) continue;

        const FbxString lElementTag = (const char*)lChildElement->name;
        if (lElementTag == COLLADA_FCOLLADA_STARTTIME_ELEMENT)
        {
            double lStartTimeDouble = 0;
            DAE_GetElementContent(lChildElement, lStartTimeDouble);
            FbxTime lStartTime;
            lStartTime.SetSecondDouble(lStartTimeDouble);
            
            FbxTimeSpan lTimeSpan;
            pScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeSpan);
            lTimeSpan.SetStart(lStartTime);
            pScene->GetGlobalSettings().SetTimelineDefaultTimeSpan(lTimeSpan);
        }
        else if (lElementTag == COLLADA_FCOLLADA_ENDTIME_ELEMENT)
        {
            double lEndTimeDouble = 0;
            DAE_GetElementContent(lChildElement, lEndTimeDouble);
            FbxTime lEndTime;
            lEndTime.SetSecondDouble(lEndTimeDouble);

            FbxTimeSpan lTimeSpan;
            pScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeSpan);
            lTimeSpan.SetStop(lEndTime);
            pScene->GetGlobalSettings().SetTimelineDefaultTimeSpan(lTimeSpan);
        }
        else
        {
            const FbxString lMessage = FbxString("The unsupported element in visual_scene FCOLLADA extension: \"") + lElementTag + "\"";
            AddNotificationWarning(lMessage);
        }
    }

    return true;
}

bool FbxReaderCollada::ImportVisualSceneMayaExtension(xmlNode * pTechniqueElement, FbxScene * pScene)
{
    for (xmlNode* lChildElement = pTechniqueElement->children; lChildElement != NULL; lChildElement = lChildElement->next)
    {
        if (lChildElement->type != XML_ELEMENT_NODE) continue;

        const FbxString lElementTag = (const char*)lChildElement->name;
        if (lElementTag == COLLADA_MAYA_LAYER_ELEMENT)
        {
            const FbxString lLayerName = DAE_GetElementAttributeValue(lChildElement, COLLADA_NAME_PROPERTY);

            FbxDisplayLayer * lLayer = NULL;
            if (lLayerName.IsEmpty() == false)
            {
                int lDisplayLayerCount = mScene->GetMemberCount<FbxDisplayLayer>();
                for (int lIndex = 0; lIndex < lDisplayLayerCount; ++lIndex)
                {
                    FbxDisplayLayer * lDisplayLayer = mScene->GetMember<FbxDisplayLayer>(lIndex);
                    if (lDisplayLayer->GetName() == lLayerName)
                    {
                        lLayer = lDisplayLayer;
                        break;
                    }
                }
                lLayer = FbxDisplayLayer::Create(mScene, lLayerName);
            }
            FbxString lLayerElementName;
            ElementContentAccessor lLayerAccessor(lChildElement);
            while (lLayerAccessor.GetNext(&lLayerElementName))
            {
				NodeMapType::RecordType* citer = mIDNamespaceNodes.Find(lLayerElementName);
                if (citer)
					lLayer->AddMember(citer->GetValue());
            }
        }
        else
        {
            const FbxString lMessage = FbxString("The unsupported element in visual_scene MAYA extension: \"") + lElementTag + "\"";
            AddNotificationWarning(lMessage);
        }
    }

    return true;
}

bool FbxReaderCollada::ImportAsset(xmlNode* pXmlNode, FbxGlobalSettings & pGlobalSettings, FbxDocumentInfo& pSceneInfo) 
{
	for (xmlNode* lChild = pXmlNode->children; lChild != NULL; lChild = lChild->next) {
		if (lChild->type != XML_ELEMENT_NODE) continue;

		FbxString structure = (const char*)lChild->name;

		if (structure == COLLADA_UP_AXIS_STRUCTURE) {
			// Up_axis
			xmlNode* upAxisNode = lChild;
			FbxString content;
            DAE_GetElementContent(upAxisNode, content);

			// can be COLLADA_X_UP, COLLADA_Y_UP or COLLADA_Z_UP. Else, show warning.
			FbxAxisSystem::EUpVector lUpVector = FbxAxisSystem::eYAxis;
			FbxAxisSystem::EFrontVector lFrontVector = FbxAxisSystem::eParityOdd;
			if (strcmp(content, COLLADA_X_UP) == 0) {
				lUpVector = FbxAxisSystem::eXAxis;
			} else if (strcmp(content, COLLADA_Y_UP) == 0) {
				lUpVector = FbxAxisSystem::eYAxis;
			} else if (strcmp(content, COLLADA_Z_UP) == 0) {
				lUpVector = FbxAxisSystem::eZAxis;
				// Max and Maya Z-up are like that:
				lFrontVector = (FbxAxisSystem::EFrontVector)-FbxAxisSystem::eParityOdd;
			} else {
				FbxString msg = FbxString("Unrecognized up_axis value: ") + content;
				AddNotificationWarning( msg );
			}
			FbxAxisSystem lAxisSystem( lUpVector, lFrontVector, FbxAxisSystem::eRightHanded );
            pGlobalSettings.SetAxisSystem( lAxisSystem );
        }
        else if (structure == COLLADA_UNIT_STRUCTURE)
        {
            pGlobalSettings.SetSystemUnit(DAE_ImportUnit(lChild));
        }
        else if (structure == COLLADA_CONTRIBUTOR_ASSET_ELEMENT) {
			for (xmlNode* lContribChild = lChild->children; lContribChild != NULL; lContribChild = lContribChild->next) {
				if (lContribChild->type != XML_ELEMENT_NODE) continue;

				FbxString structure1 = (const char*)lContribChild->name;
				// We use author and comments. We don't use authoring_tool, copyright and source_data.
				// The right order for these nodes is the alphabetical order; it is not checked.

                if (structure1 == COLLADA_AUTHOR_STRUCTURE)
                    DAE_GetElementContent(lContribChild, pSceneInfo.mAuthor);
                else if (structure == COLLADA_COMMENTS_STRUCTURE)
                    DAE_GetElementContent(lContribChild, pSceneInfo.mComment);
            }
        }
        else if (structure == COLLADA_TITLE_STRUCTURE)
            DAE_GetElementContent(lChild, pSceneInfo.mTitle);
        else if (structure == COLLADA_SUBJECT_STRUCTURE)
            DAE_GetElementContent(lChild, pSceneInfo.mSubject);
        else if (structure == COLLADA_KEYWORDS_STRUCTURE)
            DAE_GetElementContent(lChild, pSceneInfo.mKeywords);
        else if (structure == COLLADA_REVISION_STRUCTURE)
            DAE_GetElementContent(lChild, pSceneInfo.mRevision);
    }

    return true;
}

bool FbxReaderCollada::ConnectMaterialsToNode(FbxNode * pNode, xmlNode * pElement,
                                               FbxDynamicArray<FbxString> & pMaterialSequence)
{
    FBX_ASSERT(pNode && pElement);
    if (!pNode || !pElement)
        return false;

    xmlNode* lBindMaterialElement = DAE_FindChildElementByTag(pElement, COLLADA_BINDMATERIAL_ELEMENT);

    // Find <common_technique> or <technique>
    xmlNode * lCommonTechniqueElement = DAE_FindChildElementByTag(lBindMaterialElement, COLLADA_TECHNIQUE_COMMON_ELEMENT);
    if (!lCommonTechniqueElement) 
    {
        lCommonTechniqueElement = DAE_FindChildElementByTag(lBindMaterialElement, COLLADA_TECHNIQUE_STRUCTURE);
    }

    if (lCommonTechniqueElement)
    {
        CNodeList materialNodes;
        findChildrenByType(lCommonTechniqueElement, COLLADA_INSTANCE_MATERIAL_ELEMENT, materialNodes);
        const int materialNodeCount = materialNodes.GetCount();

        pNode->RemoveAllMaterials();
        for (int i = 0; i < materialNodeCount; ++i)
        {
            xmlNode *lMaterialNode = materialNodes[i];
            const FbxString lSymbol = DAE_GetElementAttributeValue(lMaterialNode, COLLADA_SYMBOL_PROPERTY);
            const FbxString lTargetID = DAE_GetIDFromTargetAttribute(lMaterialNode);

            FbxSurfaceMaterial * lMaterial = FbxCast<FbxSurfaceMaterial>(GetLibrary(mMaterialTypeTraits, lTargetID));
            if (lMaterial)
            {
                pNode->AddMaterial(lMaterial);
                pMaterialSequence.PushBack(lSymbol);
            }
        }
    }

    return true;
}

FbxNode * FbxReaderCollada::ImportNode(xmlNode* pXmlNode)
{
    const FbxString lName = DAE_GetElementAttributeValue(pXmlNode, COLLADA_NAME_PROPERTY);
    const FbxString lNodeId = DAE_GetElementAttributeValue(pXmlNode, COLLADA_ID_PROPERTY);
    const FbxString lNodeSId = DAE_GetElementAttributeValue(pXmlNode, COLLADA_SUBID_PROPERTY);
    const FbxString lLayerName = DAE_GetElementAttributeValue(pXmlNode, COLLADA_LAYER_PROPERTY);
    if (!IsNodeExportable(lNodeId))
        return NULL;

    // Add the node into a display layer if its layer attribute is not empty
    FbxDisplayLayer * lLayer  = NULL;
    if (lLayerName.IsEmpty() == false)
    {
        int lDisplayLayerCount = mScene->GetMemberCount<FbxDisplayLayer>();
        for (int lIndex = 0; lIndex < lDisplayLayerCount; ++lIndex)
        {
            FbxDisplayLayer * lDisplayLayer = mScene->GetMember<FbxDisplayLayer>(lIndex);
            if (lDisplayLayer->GetName() == lLayerName)
            {
                lLayer = lDisplayLayer;
                break;
            }
        }
        lLayer = FbxDisplayLayer::Create(mScene, lLayerName);
    }

    FbxNode *lNode = FbxNode::Create(mScene, "");
    if (lLayer)
        lLayer->AddMember(lNode);

    ImportTransforms(pXmlNode, lNode);

    // The node to accept the next node attribute
    FbxNode * lTempNode = lNode;
	FbxString lExternalRef;
	for (xmlNode* lChild = pXmlNode->children; lChild != NULL; lChild = lChild->next)
    {
        if (lChild->type != XML_ELEMENT_NODE)
            continue;

        const FbxString structure = (const char*)lChild->name;

        if (structure == COLLADA_INSTANCE_GEOMETRY_ELEMENT)
        {
            FbxDynamicArray<FbxString> lMaterialSequence;
            ConnectMaterialsToNode(lTempNode, lChild, lMaterialSequence);

            const FbxString lGeometryID = DAE_GetIDFromUrlAttribute(lChild, lExternalRef);
            FbxGeometry * lGeometry = ImportGeometry(lGeometryID, lMaterialSequence);

            if (lGeometry)
            {
				ColladaElementMapType::RecordType* lIter = mColladaElements.Find(lGeometryID);
                lTempNode->SetName(lGeometry->GetName());
                if (!lIter)
					lTempNode->SetNodeAttribute(lGeometry);
				else
				{
					// check if we have multiple attributes
					for (int i = 0, c = lIter->GetValue().mFBXObjects.GetCount(); i < c; i++)
					{
						FbxGeometry* lGeo = FbxCast<FbxGeometry>(lIter->GetValue().mFBXObjects[i]);
						lTempNode->AddNodeAttribute(lGeo);
					}
				}
            }
        }
        else if (structure == COLLADA_INSTANCE_CONTROLLER_ELEMENT)
        {
            FbxDynamicArray<FbxString> lMaterialSequence;
            ConnectMaterialsToNode(lTempNode, lChild, lMaterialSequence);

            // Process the controller if it is a morph.
            // The clusters and bind pose will be treated after the whole scene is set up.
            const FbxString lControllerID = DAE_GetIDFromUrlAttribute(lChild, lExternalRef);
            FbxGeometry * lGeometry = ImportController(lControllerID, lMaterialSequence);
            
            if (lGeometry)
            {
                lTempNode->SetName(lGeometry->GetName());
                lTempNode->SetNodeAttribute(lGeometry);
            }
        }
        else if (structure == COLLADA_INSTANCE_LIGHT_ELEMENT)
        {
            const FbxString lAttributeID = DAE_GetIDFromUrlAttribute(lChild, lExternalRef);
            FbxLight * lLight = FbxCast<FbxLight>(GetLibrary(mLightTypeTraits, lAttributeID));
            if (lLight)
            {
                lTempNode->SetName(lLight->GetName());
                // If spotlight or directional light, 
                // rotate node so spotlight is directed at the Z axis (default is X axis).
                if (lLight->LightType.Get() == FbxLight::eSpot || lLight->LightType.Get() == FbxLight::eDirectional)
                {
                    lTempNode->SetPostRotation(FbxNode::eSourcePivot, FbxVector4(-90,0,0,1));
                    lTempNode->SetRotationActive(true);
                }
                lTempNode->SetNodeAttribute(lLight);
            }
        }
        else if (structure == COLLADA_INSTANCE_CAMERA_ELEMENT)
        {
            const FbxString lAttributeID = DAE_GetIDFromUrlAttribute(lChild, lExternalRef);
            FbxCamera * lCamera = FbxCast<FbxCamera>(GetLibrary(mCameraTypeTraits, lAttributeID));
            if (lCamera)
            {
                lTempNode->SetName(lCamera->GetName());
                // Camera position, interest, up vector computed from the lookat
                FbxVector4 lCameraPosition;
                FbxVector4 lInterestPosition = FbxVector4(0,-1,0,0);
                FbxVector4 lUpVector = FbxVector4(0,1,0,0);
                FbxAMatrix lCameraTransformMatrix;
                bool lLookAtFound = ImportLookAt(pXmlNode,
                    lCameraPosition, lInterestPosition, lUpVector, 
                    lCameraTransformMatrix);

                if (lLookAtFound) {
                    lCamera->Position.Set(lCameraPosition);
                    lCamera->InterestPosition.Set(lInterestPosition);
                    lCamera->UpVector.Set(lUpVector);

                    // Camera rotation derived from the lookat
                    lTempNode->LclRotation.Set(lCameraTransformMatrix.GetR());

                    // Focus Distance
                    FbxVector4 v = lInterestPosition-lCameraPosition;
                    double lFocusDistance = v.Length();
                    lCamera->FocusSource.Set(FbxCamera::eFocusSrcCameraInterest);
                    lCamera->FocusDistance.Set(lFocusDistance);
                }

                // rotate node so camera looks at the Z axis (default is X axis).
                lTempNode->SetPostRotation(FbxNode::eSourcePivot, FbxVector4(0,-90,0,0));
                lTempNode->SetRotationActive(true);

                lTempNode->SetNodeAttribute(lCamera);
            }
        }
        else if (structure == COLLADA_NODE_STRUCTURE)
        {
            FbxNode * lChildNode = ImportNode(lChild);
            if (lChildNode)
                lNode->ConnectSrcObject(lChildNode);
        }
        else if (structure == COLLADA_INSTANCE_NODE_ELEMENT)
        {
            const FbxString lAttributeID = DAE_GetIDFromUrlAttribute(lChild, lExternalRef);
			if (!lExternalRef.IsEmpty())
			{
				// the Url references an external file. We don't support this so,
				// just stop processing the Url (and avoid possible endless loop)
				const FbxString lMessage = FbxString("External file reference is not supported: \"") + lExternalRef + "#" + lAttributeID + "\"";
				AddNotificationWarning(lMessage);
			}
			else
			{
				FbxNode * lChildNode = FbxCast<FbxNode>(GetLibrary(mNodeTypeTraits, lAttributeID));
				if (lChildNode)
					lNode->ConnectSrcObject(lChildNode);
			}
        }
        else if (structure == COLLADA_EXTRA_STRUCTURE) 
        {
            xmlNode * lTechniqueElement = DAE_FindChildElementByTag(lChild, COLLADA_TECHNIQUE_STRUCTURE);
            while (lTechniqueElement)
            {
                const FbxString lProfile = DAE_GetElementAttributeValue(lTechniqueElement, COLLADA_PROFILE_PROPERTY);
                if (lProfile == COLLADA_FCOLLADA_PROFILE)
                {
                    ImportNodeFCOLLADAExtension(lTechniqueElement, lNode);
                }
                else if (lProfile == COLLADA_XSI_PROFILE)
                {
                    ImportNodeXSIExtension(lTechniqueElement, lNode);
                }
                else if (lProfile == COLLADA_FBX_PROFILE)
                {
                    ImportNodeFBXExtension(lTechniqueElement, lNode);
                }
                else
                {
                    const FbxString lMessage = FbxString("The unsupported technique element with profile \"") + lProfile 
                        + "\" in node element \"" + lNodeId + "\"";
                    AddNotificationWarning(lMessage);
                }

                lTechniqueElement = DAE_FindChildElementByTag(lChild, COLLADA_TECHNIQUE_STRUCTURE, lTechniqueElement);
            }
        }
        else if ((structure == COLLADA_LOOKAT_STRUCTURE)
            || (structure == COLLADA_MATRIX_STRUCTURE)
            || (structure == COLLADA_ROTATE_STRUCTURE)
            || (structure == COLLADA_SCALE_STRUCTURE)
            || (structure == COLLADA_SKEW_STRUCTURE)
            || (structure == COLLADA_TRANSLATE_STRUCTURE))
        {
            continue; // already treated
        }
        else 
        {
            FbxString msg = FbxString("Structure ") + structure + " unknown";
            AddNotificationError( msg );
        }

        if (lTempNode->GetNodeAttribute())
        {
            if (lTempNode != lNode)
                lNode->ConnectSrcObject(lTempNode);
            lTempNode = FbxNode::Create(mScene, "");
            if (lLayer)
                lLayer->AddMember(lTempNode);
        }
    }

    if (!lTempNode->GetNodeAttribute() && lTempNode != lNode)
        lTempNode->Destroy();
    DAE_SetName(lNode, lName, lNodeId);

    // If no node attribute was set,
    //   If node type is JOINT, set a Skeleton.
    //   Else, set a Null.
    if (lNode->GetNodeAttribute() == NULL)
    {
        const FbxString lNodeType = DAE_GetElementAttributeValue(pXmlNode, COLLADA_TYPE_PROPERTY);
        if (lNodeType == COLLADA_JOINT_NODE_TYPE)
        {
            FbxSkeleton *lSkeleton = FbxSkeleton::Create(mScene, "");
            DAE_SetName(lSkeleton, lName, lNodeId);
            lSkeleton->SetSkeletonType(FbxSkeleton::eLimbNode);
            lNode->SetNodeAttribute((FbxNodeAttribute*)lSkeleton);
        }
        else if (lNode->GetChildCount() == 0)
        {
            FbxNull *lNull = FbxNull::Create(mScene, "");
            lNode->SetNodeAttribute((FbxNodeAttribute*)lNull);
        }
    }

    if (lNodeId.IsEmpty() == false)
        mIDNamespaceNodes[lNodeId] = lNode;
    if (lNodeSId.IsEmpty() == false)
        mSIDNamespaceNodes[lNodeSId] = lNode;

    return lNode;
}

bool FbxReaderCollada::ImportNodeFCOLLADAExtension(xmlNode* pTechniqueElement, FbxNode * pNode)
{
    for (xmlNode* lChildElement = pTechniqueElement->children; lChildElement != NULL; lChildElement = lChildElement->next)
    {
        if (lChildElement->type != XML_ELEMENT_NODE) continue;

        const FbxString lElementTag = (const char*)lChildElement->name;
        if (lElementTag == COLLADA_FCOLLADA_VISIBILITY_ELEMENT)
        {
            double lVisibility = 0;
            DAE_GetElementContent(lChildElement, lVisibility);
            pNode->Visibility.Set(lVisibility);
        }
        else
        {
            const FbxString lMessage = FbxString("The unsupported element in node FCOLLADA extension: \"") + lElementTag + "\"";
            AddNotificationWarning(lMessage);
        }
    }

    return true;
}

bool FbxReaderCollada::ImportNodeXSIExtension(xmlNode* pTechniqueElement, FbxNode * pNode)
{
    for (xmlNode* lChildElement = pTechniqueElement->children; lChildElement != NULL; lChildElement = lChildElement->next)
    {
        if (lChildElement->type != XML_ELEMENT_NODE) continue;

        const FbxString lElementTag = (const char*)lChildElement->name;
        if (lElementTag == COLLADA_XSI_VISIBILITY_ELEMENT)
        {
            xmlNode * lVisibilityElement = DAE_FindChildElementByAttribute(lChildElement, COLLADA_SUBID_PROPERTY, "visibility");
            if (lVisibilityElement)
            {
                FbxString lVisibilityValue;
                DAE_GetElementContent(lVisibilityElement, lVisibilityValue);
                if (lVisibilityValue == COLLADA_TRUE_KEYWORD)
                    pNode->Visibility.Set(1);
                else if (lVisibilityValue == COLLADA_FALSE_KEYWORD)
                    pNode->Visibility.Set(0);
                else
                    FBX_ASSERT_NOW("Invalid value");
            }
        }
        else
        {
            const FbxString lMessage = FbxString("The unsupported element in node XSI extension: \"") + lElementTag + "\"";
            AddNotificationWarning(lMessage);
        }
    }

    return true;
}

bool FbxReaderCollada::ImportNodeFBXExtension(xmlNode* pTechniqueElement, FbxNode * pNode)
{
    for (xmlNode* lChildElement = pTechniqueElement->children; lChildElement != NULL; lChildElement = lChildElement->next)
    {
        if (lChildElement->type != XML_ELEMENT_NODE) continue;

        const FbxString lElementTag = (const char*)lChildElement->name;
        if (lElementTag == COLLADA_FBX_TARGET_ELEMENT)
        {
            // If this node has its target, save the ID of the target node.
            // Establish the connection after the whole scene is built up.
            FbxString lTargetURL;
            DAE_GetElementContent(lChildElement, lTargetURL);
            if (lTargetURL.IsEmpty() == false)
                mTargetIDs[pNode] = lTargetURL.Mid(1);
        }
        else
        {
            const FbxString lMessage = FbxString("The unsupported element in node XSI extension: \"") + lElementTag + "\"";
            AddNotificationWarning(lMessage);
        }
    }

    return true;
}

FbxGeometry * FbxReaderCollada::ImportGeometry(const FbxString & pGeometryID, const FbxDynamicArray<FbxString> & pMaterialSequence)
{
	ColladaElementMapType::RecordType* lIter = mColladaElements.Find(pGeometryID);
    if (!lIter)
    {
        return NULL;
    }

    FbxString lElementTag;
	DAE_GetElementTag(lIter->GetValue().mColladaElement, lElementTag);
    if (lElementTag != COLLADA_GEOMETRY_STRUCTURE)
    {
        return NULL;
    }

    // If already imported, return the cache.
    if (lIter->GetValue().mFBXObject)
    {
        return FbxCast<FbxGeometry>(lIter->GetValue().mFBXObject);
    }

    double lLocalUnitConversion = 1;
    FbxGeometry * lGeometry = NULL;
    for (xmlNode* lChild = lIter->GetValue().mColladaElement->children; lChild != NULL; lChild = lChild->next)
    {
        if (lChild->type != XML_ELEMENT_NODE) continue;

        const char * lElementTag1 = reinterpret_cast<const char*>(lChild->name);
        if (strcmp(COLLADA_MESH_STRUCTURE, lElementTag1) == 0)
        {
            lGeometry = ImportMesh(lChild, pMaterialSequence, lIter->GetValue().mFBXObjects);
        }
        else if (strcmp(COLLADA_ASSET_STRUCTURE, lElementTag1) == 0)
        {
            xmlNode * lUnitElement = DAE_FindChildElementByTag(lChild, COLLADA_UNIT_STRUCTURE);
            if (lUnitElement)
            {
                FbxSystemUnit lLocalUnit = DAE_ImportUnit(lUnitElement);
                lLocalUnitConversion = lLocalUnit.GetConversionFactorTo(mGlobalSettings->GetSystemUnit());
            }
        }
        else
        {
            const FbxString msg = FbxString("Unrecognized <geometry> element: ") + lElementTag1;
            AddNotificationWarning( msg );
        }
    }

    if (lLocalUnitConversion != 1 && lGeometry)
    {
        FbxVector4 * lControlPoints = lGeometry->GetControlPoints();
        int lControlPointCount = lGeometry->GetControlPointsCount();
        for (int lControlPointIndex = 0; lControlPointIndex < lControlPointCount; ++lControlPointIndex)
        {
            lControlPoints[lControlPointIndex][0] *= lLocalUnitConversion;
            lControlPoints[lControlPointIndex][1] *= lLocalUnitConversion;
            lControlPoints[lControlPointIndex][2] *= lLocalUnitConversion;
        }
    }

    lIter->GetValue().mFBXObject = lGeometry;
    return lGeometry;
}

bool FbxReaderCollada::ImportSkin(xmlNode* pSkinElement) 
{
    const FbxString lControllerName = DAE_GetElementAttributeValue(pSkinElement->parent, COLLADA_NAME_PROPERTY);
    const FbxString lControllerID = DAE_GetElementAttributeValue(pSkinElement->parent, COLLADA_ID_PROPERTY);

    FbxSkin * lSkin = FbxSkin::Create(mScene, "");
    DAE_SetName(lSkin, lControllerName, lControllerID);

    const FbxString lGeometryID = DAE_GetIDFromSourceAttribute(pSkinElement);
	ColladaElementMapType::RecordType* lIter = mColladaElements.Find(lGeometryID);
	if (!lIter || !lIter->GetValue().mFBXObject)
        return false;
    FbxGeometry * lGeometry = FbxCast<FbxGeometry>(lIter->GetValue().mFBXObject);

    // Get joints element, which contains per-joint information
    xmlNode * lJointsElement = DAE_FindChildElementByTag(pSkinElement, COLLADA_JOINTS_STRUCTURE);
    FBX_ASSERT(lJointsElement);
    
    // Get the SID or ID of the joints which is necessary in COLLADA Specification
    xmlNode * lJointsSourceElement = DAE_GetSourceWithSemantic(lJointsElement, COLLADA_JOINT_PARAMETER, mSourceElements);
    FBX_ASSERT(lJointsSourceElement);
    xmlNode * lIDRefArrayElement = DAE_FindChildElementByTag(lJointsSourceElement, COLLADA_IDREF_ARRAY_STRUCTURE);
    xmlNode * lSIDRefArrayElement = DAE_FindChildElementByTag(lJointsSourceElement, COLLADA_NAME_ARRAY_STRUCTURE);
    FbxArray<FbxCluster*> lClusters;

    {
        SourceElementContentAccessor<FbxString> lJointsAccessor(lJointsSourceElement);
        FbxString lJointToken;
        while (lJointsAccessor.GetNext(&lJointToken))
        {
			bool lMakeDummy = false;
			NodeMapType::RecordType* iter = NULL;
            if (lIDRefArrayElement) // Search in ID namespace if IDREF_array exists
            {
                iter = mIDNamespaceNodes.Find(lJointToken);
                if (!iter)
                {
                    const FbxString lMessage = FbxString("There is no joint element with ID \"") + lJointToken + "\"";
                    AddNotificationError(lMessage);
                    lMakeDummy = true;
                }
            }
            if (lSIDRefArrayElement) // Search in SID namespace if Name_array exists
            {
                iter = mSIDNamespaceNodes.Find(lJointToken);
                if (!iter)
                {
                    const FbxString lMessage = FbxString("There is no joint element with SID \"") + lJointToken + "\"";
                    AddNotificationError(lMessage);
					lMakeDummy = true;
                }
            }

			if (lMakeDummy)
			{
				// although this is really wrong! It seems that Collada files can live wery well with invalid
				// data. FBX can't! But since the rest of the skin computation is all been setup including the
				// invalid joints, we need to create a dummy cluster so we do not fight to correctly retrieve 
				// weights, matrices and al... :-(
				FbxCluster * lCluster = FbxCluster::Create(mScene, lJointToken);
				lCluster->SetLinkMode(FbxCluster::eNormalize);
	            lClusters.Add(lCluster);
		        lSkin->AddCluster(lCluster);
			}
			else if( iter )
			{
				FbxNode * lJointNode = iter->GetValue();
				FbxCluster * lCluster = FbxCluster::Create(mScene, lJointToken);
				lCluster->SetLinkMode(FbxCluster::eNormalize);
				lCluster->SetLink(lJointNode);
				lClusters.Add(lCluster);
				lSkin->AddCluster(lCluster);
			}
        }
    }

    // Get joints element containing per vertex combination of joints and weights.
    // It is necessary in COLLADA Specification.
    xmlNode * lVertexWeightsElement = DAE_FindChildElementByTag(pSkinElement, COLLADA_WEIGHTS_ELEMENT);
    FBX_ASSERT(lVertexWeightsElement);
    int lVertexCount = 0;
    DAE_GetElementAttributeValue(lVertexWeightsElement, COLLADA_COUNT_PROPERTY, lVertexCount);

    // Assume the offset of JOINT input is 0 and WEIGHT input is 1.
    // Get the weigh of the vertices
    double * lWeights = NULL;
    int lWeightsCount;
    {
        xmlNode * lWeightsElement = DAE_GetSourceWithSemantic(lVertexWeightsElement, COLLADA_WEIGHT_PARAMETER, mSourceElements);
        FBX_ASSERT(lWeightsElement);
        SourceElementContentAccessor<double> lWeightsAccessor(lWeightsElement);
        lWeightsCount = lWeightsAccessor.mCount;
        lWeights = FbxNewArray<double>(lWeightsCount);
        lWeightsAccessor.GetArray(lWeights, lWeightsCount);
    }

    {
        xmlNode * lVCountElement = DAE_FindChildElementByTag(lVertexWeightsElement, COLLADA_VERTEXCOUNT_ELEMENT);
        xmlNode * lVElement = DAE_FindChildElementByTag(lVertexWeightsElement, COLLADA_VALUE_STRUCTURE);
        ElementContentAccessor lVCountAccessor(lVCountElement);
        ElementContentAccessor lVAccessor(lVElement);
        int lVCount, lJointIndex, lWeightIndex;
        for (int lVertexIndex = 0; lVertexIndex < lVertexCount; ++lVertexIndex)
        {
            lVCountAccessor.GetNext(&lVCount);
            for (int lVIndex = 0; lVIndex < lVCount; ++lVIndex)
            {
                lVAccessor.GetNext(&lJointIndex);
                lVAccessor.GetNext(&lWeightIndex);

				if (lWeightIndex < 0 || lWeightIndex >= lWeightsCount)
					GetStatus().SetCode(FbxStatus::eFailure, "Bad weight index detected");

				double w = (lWeightIndex >= 0 && lWeightIndex < lWeightsCount) ? lWeights[lWeightIndex] : 0.0;
                if (lJointIndex >= 0 && lJointIndex < (int)lClusters.Size())
                    lClusters[lJointIndex]->AddControlPointIndex(lVertexIndex, w);
            }
        }
    }
    FbxDeleteArray(lWeights);
    lGeometry->AddDeformer(lSkin);

    // Create the bind pose for the skin
    // If shape bind matrix and joints bind matrix is not both available, bind pose will not be created.
    // Both shape bind matrix and joints bind matrix are optional in COLLADA specification.
    xmlNode *lBindShapeMatrixElement = DAE_FindChildElementByTag(pSkinElement, COLLADA_BINDSHAPEMX_SKIN_PARAMETER);
    xmlNode * lInvBindMatricesElement = DAE_GetSourceWithSemantic(lJointsElement, COLLADA_BIND_MATRIX_SEMANTIC, mSourceElements);
    if (lClusters.Size() && lInvBindMatricesElement)
    {
        int lNodeCount = lGeometry->GetNodeCount();
        for (int lNodeIndex = 0; lNodeIndex < lNodeCount; ++lNodeIndex)
        {
            FbxNode * lGeometryNode = lGeometry->GetNode(lNodeIndex);
            FbxPose * lBindPose = FbxPose::Create(mScene, "");
            lBindPose->SetIsBindPose(true);
            lBindPose->SetName(lGeometryNode->GetName());

            // The default value is identity matrix
            FbxAMatrix lBindShapeMatrix;
            if (lBindShapeMatrixElement)
                DAE_GetElementContent(lBindShapeMatrixElement, lBindShapeMatrix);
			FbxAMatrix lParentTransform = lGeometryNode->EvaluateGlobalTransform();
            lBindPose->Add(lGeometryNode, FbxMatrix(lBindShapeMatrix * lParentTransform));

            SourceElementContentAccessor<FbxAMatrix> lInvBindMatricesAccessor(lInvBindMatricesElement);
            FBX_ASSERT(lInvBindMatricesAccessor.mStride == MATRIX_STRIDE);

            FbxAMatrix lInvBindMatrix;
            int lJointIndex = 0;
            while (lInvBindMatricesAccessor.GetNext(&lInvBindMatrix))
            {
				FBX_ASSERT(lJointIndex < (int)lClusters.Size());
				if (lJointIndex < (int)lClusters.Size())
				{
					FbxCluster * lCluster = lClusters[lJointIndex];
					lCluster->SetTransformMatrix(lBindShapeMatrix);

					const FbxAMatrix lBindMatrix = lInvBindMatrix.Inverse();
					lBindPose->Add(lCluster->GetLink(), lBindMatrix);
					lCluster->SetTransformLinkMatrix(lBindMatrix);
				}
                lJointIndex++;				
            }

            mScene->AddPose(lBindPose);
        }
    }

    return true;
}

FbxGeometry * FbxReaderCollada::ImportController(const FbxString & pControllerID, const FbxDynamicArray<FbxString> & pMaterialSequence)
{
	ColladaElementMapType::RecordType* lIter = mColladaElements.Find(pControllerID);
    if (!lIter)
    {
        return NULL;
    }

    FbxString lElementTag;
	DAE_GetElementTag(lIter->GetValue().mColladaElement, lElementTag);
    if (lElementTag != COLLADA_CONTROLLER_STRUCTURE)
    {
        return NULL;
    }

    // If already imported, return the cache.
    if (lIter->GetValue().mFBXObject)
    {
        return FbxCast<FbxGeometry>(lIter->GetValue().mFBXObject);
    }
    xmlNode * lControllerElement = lIter->GetValue().mColladaElement;

    // Precondition: Every controller element contains only one morph or skin element.
    FbxGeometry * lGeometry = NULL;
    xmlNode * lMorphElement = DAE_FindChildElementByTag(lControllerElement, COLLADA_CONTROLLER_MORPH_ELEMENT);
    if (lMorphElement)
    {
        lGeometry = ImportMorph(lMorphElement, pMaterialSequence);
    }
    else
    {
        xmlNode * lSkinElement = DAE_FindChildElementByTag(lControllerElement, COLLADA_CONTROLLER_SKIN_ELEMENT);
        if (lSkinElement)
        {
            const FbxString lInfluencedElementID = DAE_GetIDFromSourceAttribute(lSkinElement);
            
            // Try to import the influenced element as a controller firstly
            lGeometry = ImportController(lInfluencedElementID, pMaterialSequence);
            if (!lGeometry)
            {
                // Try to import the influenced element as a geometry
                lGeometry = ImportGeometry(lInfluencedElementID, pMaterialSequence);
            }

            if (lGeometry)
            {
                const FbxString lControllerID = DAE_GetElementAttributeValue(lControllerElement, COLLADA_ID_PROPERTY);
                mSkinElements[lControllerID] = lSkinElement;
            }
        }
    }

    lIter->GetValue().mFBXObject = lGeometry;
    return lGeometry;
}

FbxGeometry * FbxReaderCollada::ImportMorph(xmlNode * pMorphElement, const FbxDynamicArray<FbxString> & pMaterialSequence)
{
    FBX_ASSERT(pMorphElement);
    if (!pMorphElement)
        return NULL;

    FbxString lElementTag;
    DAE_GetElementTag(pMorphElement, lElementTag);
    if (lElementTag != COLLADA_CONTROLLER_MORPH_ELEMENT)
    {
        return NULL;
    }

    // The influenced element may be a geometry, embedded morph or embedded skin;
    // At present, if the source geometry of a morph is skin or another morph, this morph is skipped.
    const FbxString lInfluencedID = DAE_GetIDFromSourceAttribute(pMorphElement);
    FbxGeometry * lGeometry = ImportController(lInfluencedID, pMaterialSequence);
    if (!lGeometry)
    {
        lGeometry = ImportGeometry(lInfluencedID, pMaterialSequence);
        if (lGeometry)
        {
            FbxBlendShape* lBlendShape = FbxBlendShape::Create(mScene,"");
            lGeometry->AddDeformer(lBlendShape);

            xmlNode * lTargetElement = DAE_FindChildElementByTag(pMorphElement, COLLADA_TARGETS_ELEMENT);
            FBX_ASSERT(lTargetElement);
            xmlNode * lMorphTargetElement = DAE_GetSourceWithSemantic(lTargetElement, COLLADA_MORPH_TARGET_SEMANTIC, mSourceElements);
            xmlNode * lMorphWeightElement = DAE_GetSourceWithSemantic(lTargetElement, COLLADA_MORPH_WEIGHT_SEMANTIC, mSourceElements);
            FBX_ASSERT(lMorphTargetElement && lMorphWeightElement);
            SourceElementContentAccessor<FbxString> lMorphTargetAccessor(lMorphTargetElement);
            SourceElementContentAccessor<double> lMorphWeightAccessor(lMorphWeightElement);
            FBX_ASSERT(lMorphTargetAccessor.mCount == lMorphWeightAccessor.mCount);

            FbxString lMorphTarget;
            double lMorphWeight;
            while (lMorphTargetAccessor.GetNext(&lMorphTarget) && lMorphWeightAccessor.GetNext(&lMorphWeight))
            {
                FbxGeometry * lTargetGeometry = ImportGeometry(lMorphTarget, pMaterialSequence);
                FBX_ASSERT(lTargetGeometry);
                FbxShape * lShape = FbxShape::Create(mScene, lTargetGeometry->GetName());
                int lControlPointCount = lTargetGeometry->GetControlPointsCount();
                lShape->InitControlPoints(lControlPointCount);
                for (int lControlPointIndex = 0; lControlPointIndex < lControlPointCount; ++lControlPointIndex)
                {
                    lShape->SetControlPointAt(lTargetGeometry->GetControlPointAt(lControlPointIndex), lControlPointIndex);
                }

                FbxBlendShapeChannel* lBlendShapeChannel = FbxBlendShapeChannel::Create(mScene,"");
                lBlendShape->AddBlendShapeChannel(lBlendShapeChannel);
                lBlendShapeChannel->AddTargetShape(lShape, 100.0);
                lBlendShapeChannel->DeformPercent.Set(lMorphWeight * COLLADA_MORPH_WEIGHT_TO_FBX_RATIO);
            }
        }
    }

    return lGeometry;
}

FbxCamera * FbxReaderCollada::ImportCamera(xmlNode* pXmlNode) 
{
    const FbxString lId = DAE_GetElementAttributeValue(pXmlNode, COLLADA_ID_PROPERTY);
    const FbxString lName = DAE_GetElementAttributeValue(pXmlNode, COLLADA_NAME_PROPERTY);
    FbxCamera *lCamera = FbxCamera::Create(mScene, "");
    DAE_SetName(lCamera, lName, lId);

    xmlNode *lOpticsElement = DAE_FindChildElementByTag(pXmlNode, COLLADA_OPTICS_STRUCTURE);
    FBX_ASSERT(lOpticsElement);
    if (!lOpticsElement)
    {
        FbxString msg = FbxString("No <optics> element found for camera element with ID \"") + lId + "\".";
        AddNotificationError( msg );
        return NULL;
    }

    xmlNode* lCommonTechniqueElement = DAE_FindChildElementByTag(lOpticsElement, COLLADA_TECHNIQUE_COMMON_ELEMENT);
    FBX_ASSERT(lOpticsElement);
    if (!lCommonTechniqueElement)
    {
        const FbxString lMessage = FbxString("No <technique_common> element found for camera element with ID \"") + lId + "\".";
        AddNotificationError( lMessage );
        return NULL;
    }
    
    for (xmlNode* lCameraTypeNode = lCommonTechniqueElement->children; lCameraTypeNode != NULL; lCameraTypeNode = lCameraTypeNode->next)
    {
        if (lCameraTypeNode->type != XML_ELEMENT_NODE) continue;
        const FbxString lCameraType = (const char*)lCameraTypeNode->name;

        if (lCameraType == COLLADA_CAMERA_PERSP_ELEMENT) {
            lCamera->ProjectionType.Set(FbxCamera::ePerspective);
        } else if (lCameraType == COLLADA_CAMERA_ORTHO_ELEMENT) {
            lCamera->ProjectionType.Set(FbxCamera::eOrthogonal);
        } else {
            FbxString msg = FbxString("Unknown camera type: ") + lCameraType + ".";
            msg += " Camera type will be set to Perspective.";
            AddNotificationWarning( msg );
            lCamera->ProjectionType.Set(FbxCamera::ePerspective);
        }

        // perspective children:
        // xfov yfov aspect_ratio: [xfov] or [yfov] or [xfov + yfov] or [xfov + aspect_ratio] or [yfov + aspect_ratio]
        // znear: 1
        // zfar: 1
        // orthographic children:
        // xmag ymag aspect_ratio: same as perspective
        // znear: 1
        // zfar: 1
        double orthoWidth = 0.0;
        double orthoHeight = 0.0;
        double xfov = -1;
        double yfov = -1;
        double lAspectRatio = -1;
        for (xmlNode* lParam = lCameraTypeNode->children; lParam != NULL; lParam = lParam->next)
        {
            if (lParam->type != XML_ELEMENT_NODE) continue;
            const FbxString lParamName = (const char*)lParam->name;
            const FbxString lSid = DAE_GetElementAttributeValue(lParam, COLLADA_SUBID_PROPERTY);

            double lValue = 0;
            DAE_GetElementContent(lParam, lValue);

            if (lParamName == COLLADA_XFOV_CAMERA_PARAMETER)
            {
                xfov = lValue;
                ImportPropertyAnimation(lCamera->FieldOfView, lId + "/" + lSid);
            }
            else if (lParamName == COLLADA_YFOV_CAMERA_PARAMETER)
            {
                yfov = lValue;
                ImportPropertyAnimation(lCamera->FieldOfView, lId + "/" + lSid);
            }
            else if (lParamName == COLLADA_ZNEAR_CAMERA_PARAMETER)
            {
                lCamera->SetNearPlane(lValue);
            }
            else if (lParamName == COLLADA_ZFAR_CAMERA_PARAMETER)
            {
                lCamera->SetFarPlane(lValue);
            }
            else if (lParamName == COLLADA_XMAG_CAMERA_PARAMETER)
            {
                lCamera->OrthoZoom.Set(lValue);
            }
            else if (lParamName == COLLADA_YMAG_CAMERA_PARAMETER)
            {
                lCamera->OrthoZoom.Set(lValue);
            }
            else if (lParamName == COLLADA_ASPECT_CAMERA_PARAMETER)
            {
                double lAspectWidth = lCamera->AspectWidth.Get();
                double lAspectHeight = lAspectWidth / lValue;
                lCamera->SetAspect(FbxCamera::eWindowSize, lAspectWidth, lAspectHeight);

                double lApertureHeight = lCamera->GetApertureHeight();
                double lApertureWidth = lApertureHeight * lValue;
                lCamera->SetApertureWidth(lApertureWidth);
            }
            // Parameters bottom, top, left, right are obsolete in Collada 1.4, but still use them if they are present.
            else if (lParamName == COLLADA_CAMERA_ORTHO_BOTTOM_PARAMETER)
            {
                orthoHeight += lValue;
            }
            else if (lParamName == COLLADA_CAMERA_ORTHO_TOP_PARAMETER)
            {
                orthoHeight -= lValue;
            }
            else if (lParamName == COLLADA_CAMERA_ORTHO_LEFT_PARAMETER)
            {
                orthoWidth -= lValue;
            }
            else if (lParamName == COLLADA_CAMERA_ORTHO_RIGHT_PARAMETER)
            {
                orthoWidth += lValue;
            }
            else
            {
                FbxString msg = FbxString("Unrecognized camera parameter: ") + lParamName;
                AddNotificationWarning( msg );
            }
        }
        if (orthoWidth != 0 && orthoHeight != 0)
        {
            lCamera->SetAspect(FbxCamera::eWindowSize, (double) orthoWidth, (double)orthoHeight);
        }

        // Update field of view depending on read values for xfov, yfov and aspect ratio.
        if (xfov >= 0 && yfov >= 0)
        {
            lCamera->SetApertureMode(FbxCamera::eHorizAndVert);
        }
        else if (xfov >= 0)
        {
            lCamera->FieldOfView.Set(xfov);
            lCamera->SetApertureMode(FbxCamera::eHorizontal);
        }
        else if (yfov >= 0)
        {
            lCamera->FieldOfView.Set(yfov);
            lCamera->SetApertureMode(FbxCamera::eVertical);
        }
    }

    // default from Maya: as long as there are Maya parameters, set these.
    lCamera->InterestPosition.Set(FbxVector4(0.0, 0.0, -1.0f));
    lCamera->FocusDistance.Set(5.0);
    xmlNode* lTechniqueElement = DAE_FindChildElementByTag(lOpticsElement, COLLADA_TECHNIQUE_STRUCTURE);
    while (lTechniqueElement)
    {
        for (xmlNode* lParam = lTechniqueElement->children; lParam != NULL; lParam = lParam->next)
        {
            if (lParam->type != XML_ELEMENT_NODE) continue;
            const FbxString lParamName = (const char*)lParam->name;
            double lValue = 0;
            DAE_GetElementContent(lParam, lValue);

            if (lParamName == COLLADA_CAMERA_VERTICAL_APERTURE_PARAMETER)
                lCamera->SetApertureHeight(lValue);
            else if (lParamName == COLLADA_CAMERA_HORIZONTAL_APERTURE_PARAMETER)
                lCamera->SetApertureWidth(lValue);
            else if (lParamName == COLLADA_CAMERA_LENS_SQUEEZE_PARAMETER)
                lCamera->SetSqueezeRatio(lValue);
            else
            {
                FbxString msg = FbxString("Unrecognized camera technique parameter: ") + lParamName;
                AddNotificationWarning( msg );
            }
        }

        lTechniqueElement = DAE_FindChildElementByTag(lOpticsElement, COLLADA_TECHNIQUE_STRUCTURE, lTechniqueElement);
    }

    return lCamera;
}

FbxLight * FbxReaderCollada::ImportLight( xmlNode* pXmlNode )
{
    const FbxString lLightID = DAE_GetElementAttributeValue(pXmlNode, COLLADA_ID_PROPERTY);
    FbxString lName = DAE_GetElementAttributeValue(pXmlNode, COLLADA_NAME_PROPERTY);

    FbxLight *lLight = NULL;

    xmlNode* lCommonTechniqueNode = DAE_FindChildElementByTag(pXmlNode, COLLADA_TECHNIQUE_COMMON_ELEMENT);
    if (lCommonTechniqueNode == NULL)
    {
        const FbxString lMessage = FbxString("No <technique_common> element found for light element \"")
            + lLightID + "\".";
        AddNotificationError( lMessage );
        return NULL;
    }

	for (xmlNode* lTypeNode = lCommonTechniqueNode->children; lTypeNode != NULL; lTypeNode = lTypeNode->next) {
		if (lTypeNode->type != XML_ELEMENT_NODE) continue;
		FbxString lLightType = (const char*)lTypeNode->name;

		// Light property: AMBIENT, DIRECTIONAL, POINT, SPOT
		if (lLightType == COLLADA_LIGHT_AMBIENT_ELEMENT) {
			lLight = NULL;

		} else if (lLightType == COLLADA_LIGHT_DIRECTIONAL_ELEMENT) {
			lLight = FbxLight::Create(mScene, "");
			lLight->LightType.Set(FbxLight::eDirectional);

		} else if (lLightType == COLLADA_LIGHT_POINT_ELEMENT) {
			lLight = FbxLight::Create(mScene, "");
			lLight->LightType.Set(FbxLight::ePoint);

		} else if (lLightType == COLLADA_LIGHT_SPOT_ELEMENT) {
			lLight = FbxLight::Create(mScene, "");
			lLight->LightType.Set(FbxLight::eSpot);

		} else {
			FbxString msg = FbxString("Unknown light type: ") + lLightType;
			AddNotificationWarning( msg );
            return NULL;
		}

        if (lLight)
            DAE_SetName(lLight, lName, lLightID);

        // Light parameters
        xmlNode *paramNode = lTypeNode->children;
        for (xmlNode* param = paramNode; param != NULL; param = param->next)
        {
            if (param->type != XML_ELEMENT_NODE) continue;

            const FbxString paramName = (const char*)param->name;
            const FbxString lAttributeSID = DAE_GetElementAttributeValue(param, COLLADA_SUBID_PROPERTY);
            const FbxString lAnimationToken = lLightID + "/" + lAttributeSID;

            if (paramName == COLLADA_COLOR_LIGHT_PARAMETER)
            {
                FbxDouble3 color;
                DAE_GetElementContent(param, color);
                if ( lLight )
                {
                    lLight->Color.Set(color);

                    ImportPropertyAnimation(lLight->Color, lAnimationToken);
                    ImportPropertyAnimation(lLight->Color, lAnimationToken + ".R", FBXSDK_CURVENODE_COLOR_RED);
                    ImportPropertyAnimation(lLight->Color, lAnimationToken + ".G", FBXSDK_CURVENODE_COLOR_GREEN);
                    ImportPropertyAnimation(lLight->Color, lAnimationToken + ".B", FBXSDK_CURVENODE_COLOR_BLUE);
                }
                else
                {
                    // this is an ambient light, add it to the global ambient
                    FbxColor lGlobalAmbient = mScene->GetGlobalSettings().GetAmbientColor();
                    lGlobalAmbient.mRed += color[0];
                    lGlobalAmbient.mGreen += color[1];
                    lGlobalAmbient.mBlue += color[2];
                    mScene->GetGlobalSettings().SetAmbientColor(lGlobalAmbient);
                }
            }
            else if (paramName == COLLADA_FALLOFFANGLE_LIGHT_PARAMETER)
            {
                // note that in Collada as in FBX, angles are in degrees.
                if ( lLight )
                {
                    double lConeAngle = 0;
                    DAE_GetElementContent(param, lConeAngle);
                    lLight->OuterAngle.Set(lConeAngle);
                    ImportPropertyAnimation(lLight->OuterAngle, lLightID + "/" + lAttributeSID);
                }
            }
            else
            {
                const FbxString msg = FbxString("Unsupported light attribute <") + paramName + "> on light \"" + lLightID + "\"";
                AddNotificationWarning( msg );
            }
        }
    }

    // Maya Light parameters
    xmlNode* lMayaTechniqueNode = getTechniqueNode(pXmlNode, COLLADA_MAYA_PROFILE);

    if (lMayaTechniqueNode)
    {
        for (xmlNode* param = lMayaTechniqueNode->children; param != NULL; param = param->next)
        {
            if (param->type != XML_ELEMENT_NODE) continue;

            const FbxString paramName = (const char*)param->name;
            const FbxString lAttributeSID = DAE_GetElementAttributeValue(param, COLLADA_SUBID_PROPERTY);

            if (paramName == COLLADA_LIGHT_INTENSITY_PARAMETER_14)
            {
                // FBX units are 100x those of Maya
                double lIntensity = 0;
                DAE_GetElementContent(param, lIntensity);
                lLight->Intensity.Set(lIntensity * 100);
                ImportPropertyAnimation(lLight->Intensity, lLightID + "/" + lAttributeSID);
            }
            else
            {
                const FbxString msg = FbxString("Unsupported light parameter (MAYA technique): <") + paramName + "> on light element \"" + lLightID + "\"";
                AddNotificationWarning( msg );
            }
        }
    }
    return lLight;
}

//----------------------------------------------------------------------------//

FbxSurfaceMaterial * FbxReaderCollada::ImportMaterial(xmlNode* pXmlNode) 
{
    const FbxString lName = DAE_GetElementAttributeValue(pXmlNode, COLLADA_NAME_PROPERTY);
    const FbxString lId = DAE_GetElementAttributeValue(pXmlNode, COLLADA_ID_PROPERTY);

    // Find the effect instance
    xmlNode* lInstanceEffectElement = DAE_FindChildElementByTag(pXmlNode, COLLADA_INSTANCE_EFFECT_ELEMENT);
    if (!lInstanceEffectElement)
    {
        const FbxString msg = FbxString("No <instance_effect> element found for the <material> element with ID \"")
            + lId + "\"";
        AddNotificationError( msg );
        return NULL;
    }
    
    // find effect of the right url
    mNamespace.Push(lInstanceEffectElement);
	FbxString lExternalRef;
    const FbxString lEffectID = DAE_GetIDFromUrlAttribute(lInstanceEffectElement, lExternalRef);
    FbxSurfaceMaterial * lMaterial = FbxCast<FbxSurfaceMaterial>(GetLibrary(mEffectTypeTraits, lEffectID));
    if (lMaterial)
    {
        DAE_SetName(lMaterial, lName, lId);
        if (lMaterial->GetDefaultImplementation())
        {
            lMaterial->GetDefaultImplementation()->SetName(
                FbxString(lMaterial->GetName()) +"_Implementation");
        }
    }
    mNamespace.Pop();

    return lMaterial;
}

//----------------------------------------------------------------------------//

bool FbxReaderCollada::ImportTransparent(xmlNode * pElement, FbxSurfaceLambert * pSurfaceMaterial)
{
    FbxString lOpaqueMode = COLLADA_OPAQUE_MODE_A_ONE; // A_ONE is default mode
    FbxDouble4 lTransparent(1.0, 1.0, 1.0, 1.0);
    double lTransparency = 1.0;
	bool lHonorTransparent = false;
	bool lHonorTransparency = false;
	bool lHasTransparentTexture = false;

    xmlNode * lTransparencyElement = DAE_FindChildElementByTag(pElement, COLLADA_TRANSPARENCY_MATERIAL_PARAMETER);
    if (lTransparencyElement)
    {
        DAE_GetElementContent(lTransparencyElement, lTransparency);
		lHonorTransparency = true;
    }

    xmlNode * lTransparentElement = DAE_FindChildElementByTag(pElement, COLLADA_TRANSPARENT_MATERIAL_PARAMETER);
    if (lTransparentElement)
    {
        FbxString lOpaqueValue = DAE_GetElementAttributeValue(lTransparentElement, COLLADA_OPAQUE_MODE_ATTRIBUTE);
        if (lOpaqueValue.IsEmpty() == false)
            lOpaqueMode = lOpaqueValue;

        xmlNode * lColorElement = DAE_FindChildElementByTag(lTransparentElement, COLLADA_FXSTD_COLOR_ELEMENT);
        if (lColorElement)
		{
            DAE_GetElementContent(lColorElement, lTransparent);
			lHonorTransparent = true;
		}

        // For transparent texture, only support RGB_ZERO
        xmlNode * lTextureElement = DAE_FindChildElementByTag(lTransparentElement, COLLADA_FXSTD_SAMPLER_ELEMENT);
		lHasTransparentTexture = lTextureElement != NULL;
        if (lTextureElement && lOpaqueMode != COLLADA_OPAQUE_MODE_RGB_ZERO)
        {
            const FbxString msg = FbxString("Unsupported opaque mode for transparent texture: \"") + lOpaqueMode + "\"";
            AddNotificationWarning( msg );
        }
    }

    // COLLADA and FBX and different spec about transparent;
    // Reference to "Determining Transparency (Opacity)" in COLLADA spec

	// However, first case we can treat is when lHonorTransparent AND lHonorTransparency are bot set to
	// false, this mean that their definition is not read in the file so we keep FBX default settings.
	if (lHonorTransparent == false && lHonorTransparency == false)
	{
		return false;
	}

    if (lOpaqueMode == COLLADA_OPAQUE_MODE_RGB_ZERO || lOpaqueMode == COLLADA_OPAQUE_MODE_A_ZERO)
    {
		// RGB_ZERO is the same as FBX so nothing else to do

		if (lOpaqueMode == COLLADA_OPAQUE_MODE_A_ZERO)
		{
			lTransparent[0] = lTransparent[3];
			lTransparent[1] = lTransparent[3];
			lTransparent[2] = lTransparent[3];
		}

		FbxDouble3 lValue(lTransparent[0] * lTransparency,
						  lTransparent[1] * lTransparency,
						  lTransparent[2] * lTransparency);

		pSurfaceMaterial->TransparentColor.Set(lValue);
		pSurfaceMaterial->TransparencyFactor.Set(1.0);
	}
	else
	if (lOpaqueMode == COLLADA_OPAQUE_MODE_RGB_ONE || lOpaqueMode == COLLADA_OPAQUE_MODE_A_ONE)
	{
		lTransparency = 1 - lTransparency; // this one is to compensate for Sketchup/Blender files
        // where they define the Transparency = 0 for totally opaque objects :-(
        // We decided to support them intead of the specification because, unfortunately, these
        // files may be the majority out there!
        
		if (lHonorTransparent)
		{
			if (lOpaqueMode == COLLADA_OPAQUE_MODE_A_ONE)
			{
				lTransparent[0] = 1 - lTransparent[3];
				lTransparent[1] = 1 - lTransparent[3];
				lTransparent[2] = 1 - lTransparent[3];
			}
			else
			{
				lTransparent[0] = 1 - lTransparent[0];
				lTransparent[1] = 1 - lTransparent[1];
				lTransparent[2] = 1 - lTransparent[2];
			}
		}
		
		FbxDouble3 lValue(lTransparent[0] * lTransparency,
						  lTransparent[1] * lTransparency,
						  lTransparent[2] * lTransparency);

		pSurfaceMaterial->TransparentColor.Set(lValue);
		pSurfaceMaterial->TransparencyFactor.Set(1.0);
    }
    else
    {
        const FbxString msg = FbxString("Invalid opaque mode \"") + lOpaqueMode + "\"";
        AddNotificationWarning( msg );
    }

    return true;
}

//----------------------------------------------------------------------------//

void FbxReaderCollada::SetProperty(xmlNode* pPropertyElement, FbxProperty & pProperty)
{
    xmlNode * lParamElement = DAE_FindChildElementByTag(pPropertyElement, COLLADA_PARAMETER_STRUCTURE);
    if (lParamElement)
    {
        // If the property is a parameter, so the parameter must be defined with <newparam> in this local scope.
        FbxString lRef;
        DAE_GetElementAttributeValue(lParamElement, COLLADA_REF_PROPERTY, lRef);
        xmlNode * lParamDefinitionElement = mNamespace.FindParamDefinition(lRef);
        // If the parameter is modified by a <setparam>, use the modified value;
        // Otherwise, use the default value in <newparam>.
        if (lParamDefinitionElement)
        {
            xmlNode * lParamValueElement = mNamespace.FindParamModification(lRef);
            if (!lParamValueElement)
                lParamValueElement = lParamDefinitionElement;

            if (pProperty.GetPropertyDataType() == FbxDoubleDT)
            {
                xmlNode * lValueElement = DAE_FindChildElementByTag(
                    lParamValueElement, COLLADA_FLOAT_TYPE);
                FBX_ASSERT(lValueElement);
                double lValue;
                DAE_GetElementContent(lValueElement, lValue);
                pProperty.Set(lValue);
            }
            else if (pProperty.GetPropertyDataType() == FbxColor3DT)
            {
                xmlNode * lValueElement = DAE_FindChildElementByTag(lParamValueElement, COLLADA_FLOAT4_TYPE);
                FBX_ASSERT(lValueElement);
                FbxDouble3 lValue;
                DAE_GetElementContent(lValueElement, lValue);
                pProperty.Set(lValue);
            }
            else 
            {
                FBX_ASSERT_NOW("Unknown data type");
            }
        }
    }
    else
    {
        xmlNode * lTextureElement = DAE_FindChildElementByTag(pPropertyElement, COLLADA_FXSTD_SAMPLER_ELEMENT);
        if (lTextureElement)
        {
            FbxTexture * lTexture = ImportTexture(lTextureElement);
            FBX_ASSERT(lTexture);
            pProperty.ConnectSrcObject(lTexture);
        }
        else
        {
            if (pProperty.GetPropertyDataType() == FbxDoubleDT)
            {
                xmlNode * lValueElement = DAE_FindChildElementByTag(pPropertyElement, COLLADA_FXSTD_FLOAT_ELEMENT);
                FBX_ASSERT(lValueElement);
                double lValue;
                DAE_GetElementContent(lValueElement, lValue);
                pProperty.Set(lValue);
            }
            else if (pProperty.GetPropertyDataType() == FbxColor3DT)
            {
                xmlNode * lValueElement = DAE_FindChildElementByTag(pPropertyElement, COLLADA_FXSTD_COLOR_ELEMENT);
                FBX_ASSERT(lValueElement);
                FbxDouble3 lValue;
                DAE_GetElementContent(lValueElement, lValue);
                pProperty.Set(lValue);
            }
            else
            {
                FBX_ASSERT_NOW("Unknown data type");
            }
        }
    }
}

//----------------------------------------------------------------------------//

FbxSurfaceMaterial * FbxReaderCollada::ImportEffect(xmlNode* pEffectElement)
{
    const FbxString lEffectID = DAE_GetElementAttributeValue(pEffectElement,
        COLLADA_ID_PROPERTY);
    
    // If a NVidia FXComposer extension exists, use it instead of PROFILE_COMMON
    xmlNode * lExtraElement = DAE_FindChildElementByTag(pEffectElement,
        COLLADA_EXTRA_STRUCTURE);
    if (DAE_CompareAttributeValue(lExtraElement,
        COLLADA_TYPE_PROPERTY,"import"))
    {
        xmlNode * lTechniqueElement = DAE_FindChildElementByTag(lExtraElement,
            COLLADA_TECHNIQUE_STRUCTURE);
        while (lTechniqueElement)
        {
            if (DAE_CompareAttributeValue(lTechniqueElement,
                COLLADA_PROFILE_PROPERTY, COLLADA_NVIDIA_FXCOMPOSER_PROFILE))
            {
                return ImportEffectNVidiaExtension(lTechniqueElement);
            }

            lTechniqueElement = DAE_FindChildElementByTag(lExtraElement,
                COLLADA_TECHNIQUE_STRUCTURE, lTechniqueElement);
        }
    }

    xmlNode * lProfileCommonElement = DAE_FindChildElementByTag(pEffectElement,
        COLLADA_FX_PROFILE_COMMON_ELEMENT);
    FBX_ASSERT(lProfileCommonElement);
    mNamespace.Push(lProfileCommonElement);

    xmlNode * lTechniqueElement = DAE_FindChildElementByTag(
        lProfileCommonElement, COLLADA_TECHNIQUE_STRUCTURE);
    if (!lTechniqueElement)
        return NULL;

    const FbxString lEffectName = DAE_GetElementAttributeValue(pEffectElement,
        COLLADA_NAME_PROPERTY);
    for (xmlNode* lShaderNode = lTechniqueElement->children;
        lShaderNode != NULL; lShaderNode = lShaderNode->next)
    {
        if (lShaderNode->type != XML_ELEMENT_NODE) continue;
        const FbxString lType = (const char*)lShaderNode->name;

        if (lType.CompareNoCase("lambert") == 0)
        {
            FbxSurfaceLambert* lLambert = FbxSurfaceLambert::Create(mScene,
                lEffectName);
            ImportTransparent(lShaderNode, lLambert);

            for (xmlNode* lParamNode = lShaderNode->children; lParamNode != NULL; lParamNode = lParamNode->next)
            {
                if (lParamNode->type != XML_ELEMENT_NODE) continue;
                const FbxString lName = (const char*)lParamNode->name;

                    if (lName == COLLADA_EMISSION_MATERIAL_PARAMETER)
                    {
                        SetProperty(lParamNode, lLambert->Emissive);
                        lLambert->EmissiveFactor.Set(1);
                    }
                    else if (lName == COLLADA_AMBIENT_MATERIAL_PARAMETER)
                    {
                        SetProperty(lParamNode, lLambert->Ambient);
                        lLambert->AmbientFactor.Set(1);
                    }
                    else if (lName == COLLADA_DIFFUSE_MATERIAL_PARAMETER)
                    {
                        SetProperty(lParamNode, lLambert->Diffuse);
                        lLambert->DiffuseFactor.Set(1);
                    }
                    else if (lName == COLLADA_TRANSPARENT_MATERIAL_PARAMETER)
                    {
						// We cannot call SetProperty because in the case there is no texture, we will overwrite the
						// property value but this one has already been set taking into account the ALPHA mode while
						// SetProperty does not take it into account :-(
						xmlNode * lTextureElement = DAE_FindChildElementByTag(lParamNode, COLLADA_FXSTD_SAMPLER_ELEMENT);
						if (lTextureElement)
						{
							FbxTexture * lTexture = ImportTexture(lTextureElement);
							if (lTexture)
								lLambert->TransparentColor.ConnectSrcObject(lTexture);
						}
					}
                    else if (lName == COLLADA_REFLECTIVE_MATERIAL_PARAMETER ||
                        lName == COLLADA_REFLECTIVITY_MATERIAL_PARAMETER ||
                        lName == COLLADA_INDEXOFREFRACTION_MATERIAL_PARAMETER ||
                        lName == COLLADA_TRANSPARENCY_MATERIAL_PARAMETER)
                    {
                        // Ignore these parameters in Lambert model
                        // Transparency has been processed.
                    }
                    else
                    {
                        const FbxString msg = FbxString("Material parameter not supported: ") + lName;
                        AddNotificationWarning( msg );
                    }
                }

                return lLambert;
            }
            else
            {
                // Now we just recognize Phong and Lambert material modes.
                // Other modes will be converted into a Phong.
                if (lType.CompareNoCase("phong") != 0)
                {
                    const FbxString msg = FbxString("A <material> element with type \"") + lType + "\" is converted to Phong material.";
                    AddNotificationWarning( msg );
                }

                FbxSurfacePhong* lPhong = FbxSurfacePhong::Create(mScene,
                    lEffectName);
                ImportTransparent(lShaderNode, lPhong);

                for (xmlNode* lParamNode = lShaderNode->children; lParamNode != NULL; lParamNode = lParamNode->next) {
                    if (lParamNode->type != XML_ELEMENT_NODE) continue;
                    FbxString lName = (const char*)lParamNode->name;

                    if (lName == COLLADA_EMISSION_MATERIAL_PARAMETER)
                    {
                        SetProperty(lParamNode, lPhong->Emissive);
                        lPhong->EmissiveFactor.Set(1);
                    }
                    else if (lName == COLLADA_AMBIENT_MATERIAL_PARAMETER)
                    {
                        SetProperty(lParamNode, lPhong->Ambient);
                        lPhong->AmbientFactor.Set(1);
                    }
                    else if (lName == COLLADA_DIFFUSE_MATERIAL_PARAMETER)
                    {
                        SetProperty(lParamNode, lPhong->Diffuse);
                        lPhong->DiffuseFactor.Set(1);
                    }
                    else if (lName == COLLADA_SPECULAR_MATERIAL_PARAMETER)
                    {
                        SetProperty(lParamNode, lPhong->Specular);
                        lPhong->SpecularFactor.Set(1);
                    }
                    else if (lName == COLLADA_SHININESS_MATERIAL_PARAMETER)
                    {
                        SetProperty(lParamNode, lPhong->Shininess);
                    }
                    else if (lName == COLLADA_REFLECTIVE_MATERIAL_PARAMETER)
                    {
                        SetProperty(lParamNode, lPhong->Reflection);
                        lPhong->ReflectionFactor.Set(1);
                    }
                    else if (lName == COLLADA_REFLECTIVITY_MATERIAL_PARAMETER)
                    {
                        SetProperty(lParamNode, lPhong->ReflectionFactor);
                    }
                    else if (lName == COLLADA_TRANSPARENT_MATERIAL_PARAMETER)
                    {
						// We cannot call SetProperty because in the case there is no texture, we will overwrite the
						// property value but this one has already been set taking into account the ALPHA mode while
						// SetProperty does not take it into account :-(
						xmlNode * lTextureElement = DAE_FindChildElementByTag(lParamNode, COLLADA_FXSTD_SAMPLER_ELEMENT);
						if (lTextureElement)
						{
							FbxTexture * lTexture = ImportTexture(lTextureElement);
							if (lTexture)
								lPhong->TransparentColor.ConnectSrcObject(lTexture);
						}
                    }
                    else if (lName == COLLADA_INDEXOFREFRACTION_MATERIAL_PARAMETER ||
                        lName == COLLADA_TRANSPARENCY_MATERIAL_PARAMETER)
                    {
                        // Ignore these parameters in Phong model
                        // Transparency has been processed.
                    }
                    else
                    {
                        const FbxString msg = FbxString("Material parameter not supported: ") + lName;
                        AddNotificationWarning( msg );
                    }
                }
                return lPhong;
            }
        }

    mNamespace.Pop();

    return NULL;
    }

//----------------------------------------------------------------------------//

FbxSurfaceMaterial * FbxReaderCollada::ImportEffectNVidiaExtension(
    xmlNode *pEffectElement)
{
    xmlNode * lImportElement = DAE_FindChildElementByTag(pEffectElement,
        COLLADA_NVIDIA_FXCOMPOSER_IMPORT_ELEMENT);
    if (!lImportElement)
    return NULL;

    FbxSurfaceMaterial * lMaterial = FbxSurfaceMaterial::Create(mScene, NULL);
    FbxImplementation* lImpl = FbxImplementation::Create(mScene, NULL);

    lMaterial->AddImplementation(lImpl);
    lMaterial->SetDefaultImplementation(lImpl);
    lImpl->RenderAPI = FBXSDK_RENDERING_API_OPENGL;
    lImpl->RenderAPIVersion = "1.5";
    lImpl->Language = FBXSDK_SHADING_LANGUAGE_CGFX;
    lImpl->LanguageVersion = "1.5";

    FbxBindingTable* lTable = lImpl->AddNewTable("root", "shader");
    lImpl->RootBindingName = "root";

    FbxString lURL;
    DAE_GetElementAttributeValue(lImportElement,
        COLLADA_NVIDIA_FXCOMPOSER_URL_ATTRIBUTE, lURL);
    if (FbxPathUtils::IsRelative(lURL.Buffer()))
    {
        FbxString lDirectory = FbxPathUtils::GetFolderName(mFileName.Buffer());
        lURL = FbxPathUtils::Bind(lDirectory, lURL.Buffer());
        lTable->DescRelativeURL = lURL;
}
    else
    {
        lTable->DescAbsoluteURL = lURL;
    }

    int lSetParamCount = mNamespace.GetParamModificationCount();
    for (int lIndex = 0; lIndex < lSetParamCount; ++lIndex)
    {
        xmlNode * lSetParam = mNamespace.GetParamModification(lIndex);
        FbxBindingTableEntry& lEntry = lTable->AddNewEntry();

        const FbxString lRef = DAE_GetElementAttributeValue(lSetParam,
            COLLADA_REF_PROPERTY);
        for (xmlNode * lDataElement = lSetParam->children; lDataElement != NULL;
            lDataElement = lDataElement->next)
        {
            if (lDataElement->type != XML_ELEMENT_NODE) continue;
            ImportPropertyValue(lMaterial, lRef, lDataElement);

            FbxPropertyEntryView lSrc(&lEntry, true, true);
            lSrc.SetProperty(lRef);
            FbxSemanticEntryView lDest(&lEntry, false, true);
            lDest.SetSemantic(lRef);
        }
}

    return lMaterial;
}

//----------------------------------------------------------------------------//

FbxFileTexture * FbxReaderCollada::ImportImage(xmlNode* pXmlNode) 
{
    // Read image informations
    const FbxString lId = DAE_GetElementAttributeValue(pXmlNode, COLLADA_ID_PROPERTY);
    FbxString lName = DAE_GetElementAttributeValue(pXmlNode, COLLADA_NAME_PROPERTY);

    // image file path is relative to the DAE file path if it's a relative path
    const FbxString lDAEFileDir = FbxPathUtils::GetFolderName(mFileName);

    xmlNode *lFileNode = DAE_FindChildElementByTag(pXmlNode, COLLADA_INITFROM_ELEMENT);
    if (!lFileNode)
    {
        FbxString msg = FbxString("Image ") + lId + " has no input file.";
        AddNotificationError( msg );
        return NULL;
    }

    FbxString lSource;
    DAE_GetElementContent(lFileNode, lSource);
    // Decode percent encoding for URL
    lSource = DecodePercentEncoding(lSource);

    // Create and set texture
    // File path could begin with either "file:///" or "file://"
    bool lFile = lSource.FindAndReplace("file:///", "", 0);
    if (!lFile)
        lFile = lSource.FindAndReplace("file://", "", 0);

    FbxFileTexture *lImage = FbxFileTexture::Create(mScene, "");
    DAE_SetName(lImage, lName, lId);

    // if file path is "C|/Documents/...." change it to "C:/Documents/...."
    lSource.FindAndReplace("|", ":", 0);
    
    const FbxString lAbsPath = FbxPathUtils::Bind(lDAEFileDir, lSource);
    lImage->SetFileName(lAbsPath);
    lImage->SetRelativeFileName(FbxPathUtils::Clean(lSource));

    return lImage;
}

FbxFileTexture * FbxReaderCollada::ImportTexture(xmlNode* pXmlNode)
{
    FbxString lTextureID;
    DAE_GetElementAttributeValue(pXmlNode, COLLADA_FXSTD_TEXTURE_ATTRIBUTE, lTextureID);

    // Find the <newparam> with texture SID
    xmlNode * lSamplerParam = mNamespace.FindParamDefinition(lTextureID);
    if (lSamplerParam)
    {
        xmlNode * lSamplerElement = DAE_FindChildElementByTag(lSamplerParam, COLLADA_FXCMN_SAMPLER2D_ELEMENT);
        FBX_ASSERT(lSamplerElement);
        xmlNode * lSourceElement = DAE_FindChildElementByTag(lSamplerElement, COLLADA_SOURCE_STRUCTURE);
        FBX_ASSERT(lSourceElement);
        FbxString lSurfaceID;
        DAE_GetElementContent(lSourceElement, lSurfaceID);
			
        // Find the <newparam> with surface SID
        xmlNode * lSurfaceParam = mNamespace.FindParamDefinition(lSurfaceID);
        FBX_ASSERT(lSurfaceParam);
        xmlNode *lSurfaceElement = DAE_FindChildElementByTag(lSurfaceParam, COLLADA_FXCMN_SURFACE_ELEMENT);
        FBX_ASSERT(lSurfaceElement);
        xmlNode * lInitFromElement = DAE_FindChildElementByTag(lSurfaceElement, COLLADA_INITFROM_ELEMENT);
        FBX_ASSERT(lInitFromElement);
        DAE_GetElementContent(lInitFromElement, lTextureID);
	}

    // Find image with specific ID
    FbxFileTexture * lImage = FbxCast<FbxFileTexture>(GetLibrary(mImageTypeTraits, lTextureID));
    if (lImage == NULL)
        return NULL;

	// Find extra node for extra information.
	xmlNode* lExtraNode = DAE_FindChildElementByTag(pXmlNode, COLLADA_EXTRA_STRUCTURE);
	if (lExtraNode != NULL) 
    {
		// Maya technique node adds extra parameters.
		// Parameters both supported by COLLADA and FBX:
		// - blend mode
		// - wrap in U and in V
		// the rest is not supported.
		xmlNode* lMayaTechniqueNode = getTechniqueNode(lExtraNode, COLLADA_MAYA_PROFILE);
		if (lMayaTechniqueNode != NULL) {

			FbxTexture::EWrapMode lWrapU = FbxTexture::eRepeat, lWrapV = FbxTexture::eRepeat;
			FbxTexture::EBlendMode lBlendMode = FbxTexture::eTranslucent;

			for (xmlNode* lParamNode = lMayaTechniqueNode->children; lParamNode != NULL; lParamNode = lParamNode->next) {
				if (lParamNode->type != XML_ELEMENT_NODE) continue;
				FbxString lName = (const char*)lParamNode->name;
				FbxString lContent;
                DAE_GetElementContent(lParamNode, lContent);

				if (lName == COLLADA_TEXTURE_WRAPU_PARAMETER) {
					if (lContent.Upper() == "FALSE") lWrapU = FbxTexture::eClamp; else lWrapU = FbxTexture::eRepeat;

				} else if (lName == COLLADA_TEXTURE_WRAPV_PARAMETER) {
					if (lContent.Upper() == "FALSE") lWrapV = FbxTexture::eClamp; else lWrapV = FbxTexture::eRepeat;

				} else if (lName == COLLADA_TEXTURE_BLEND_MODE_PARAMETER_14) {
					if (lContent.Upper() == "NONE") lBlendMode = FbxTexture::eTranslucent;
					else if (lContent.Upper() == "ADD") lBlendMode = FbxTexture::eAdditive;
					else 
                    {
						FbxString msg = FbxString("Texture blend mode not recognized: ") + lName;
						AddNotificationWarning( msg );
						lBlendMode = FbxTexture::eAdditive;
					}

				}
                else
                {
					FbxString msg = FbxString("This texture parameter not supported: ") + lName;
					AddNotificationWarning( msg );
				}
			}
			lImage->SetWrapMode(lWrapU, lWrapV);
			lImage->SetBlendMode(lBlendMode);
			lImage->SetMaterialUse(FbxFileTexture::eModelMaterial);
		}
	}
	
	return lImage;
}

// When the mapping mode is by polygon vertex
// check if the number of index UV match the number of polygon vertex count
// *** this is a patch to avoid some modeler to crash when less items in UV index array than polygon vertex count
// Note: Technically this reader can also set the mapping by control point but for now we ignore it.
void ValidateMesh(FbxMesh *pFbxMesh, FbxStatus* pStatus)
{
	if( !pFbxMesh )
	{
		return;
	}

	// get the number of layers
    const int lLayerCount = pFbxMesh->GetLayerCount();

    // Compute total number of UVSets
    int lUVElmCount = 0;
    for(int lLayerIndex = 0; lLayerIndex < lLayerCount; lLayerIndex++)
    {
        lUVElmCount += pFbxMesh->GetLayer(lLayerIndex)->GetUVSetCount();
    }

    if(lUVElmCount == 0)
    {
        //No UV's let's return
        return;
    }

    for(int lLayerIndex = 0; lLayerIndex < lLayerCount; ++lLayerIndex)
    {
        FbxLayer* lLayer = pFbxMesh->GetLayer(lLayerIndex);
        FBX_ASSERT( lLayer );
        if( !lLayer ) continue;

        FbxArray<FbxLayerElement::EType> lUVTypes = lLayer->GetUVSetChannels();
        for(int lTypeIndex = 0; lTypeIndex < lUVTypes.GetCount(); lTypeIndex++)
        {
            FbxLayerElementUV* pUVElm = lLayer->GetUVs(lUVTypes[lTypeIndex]);
            if( !pUVElm ) continue;

            // only support mapping by polygon vertex.
			if( pUVElm->GetMappingMode() != FbxLayerElement::eByPolygonVertex)
			{
				continue;
			}

			// get the total number of polygon vertex
			int lTotPolyVtxCount = 0;
			const int lPolyCount = pFbxMesh->GetPolygonCount();
			bool badData = false;
			for(int lPolyIndex = 0; !badData && lPolyIndex < lPolyCount; ++lPolyIndex)
			{
				const int lPolySize = pFbxMesh->GetPolygonSize(lPolyIndex);
				badData = (lPolySize < 0);
				lTotPolyVtxCount += lPolySize;
			}

			if (badData)
			{
				pStatus->SetCode(FbxStatus::eFailure, "Bad polygon size detected.");
				lTotPolyVtxCount = 0;
			}

			// for now, patch only for by polygon vertex
			if( pUVElm->GetMappingMode() == FbxLayerElement::eByPolygonVertex )
			{
				// add to the UV index array the 0 value to match the lTotPolyVtxCount
				// *** this is a patch to avoid some modeler to crash when less items in UV index array than lTotPolyVtxCount
				for (int c = pUVElm->GetIndexArray().GetCount(); c < lTotPolyVtxCount; c++)
				{
					pUVElm->GetIndexArray().Add(0);
				}
			}
		}
	}
}

FbxGeometry * FbxReaderCollada::ImportMesh(xmlNode* pMeshElement, const FbxDynamicArray<FbxString> & pMaterialSequence, FbxArray<FbxObject*>& pObjects)
{
    // 1) Read vertices (including source)
    // 2) Read polygons and triangles (including sources)
    // Not treated: The trifans and tristrips could be added in ImportPolygons.
    // linestrips is not supported now.
    
    const FbxString lGeometryID = DAE_GetElementAttributeValue(pMeshElement->parent, COLLADA_ID_PROPERTY);
    const FbxString lGeometryName = DAE_GetElementAttributeValue(pMeshElement->parent, COLLADA_NAME_PROPERTY);
    xmlNode* lVerticesElement = DAE_FindChildElementByTag(pMeshElement, COLLADA_VERTICES_STRUCTURE);
    FBX_ASSERT(lVerticesElement);
	FbxLine * lLine = NULL;

    CNodeList lLinesElements;
    findChildrenByType(pMeshElement, COLLADA_LINES_STRUCTURE, lLinesElements);
    if (lLinesElements.GetCount())
    {
        lLine = FbxLine::Create(mScene, "");
        DAE_SetName(lLine, lGeometryName, lGeometryID);

        if (ImportVertices(lVerticesElement, lLine))
		{
			FbxArray<int> * lIndexArray = lLine->GetIndexArray();
			for (int lLineElementIndex = 0; lLineElementIndex < lLinesElements.GetCount(); ++lLineElementIndex)
			{
				xmlNode * lLineElement = lLinesElements.GetAt(lLineElementIndex);

				int lVertexOffset = 0;
				int lStride = 0;
				// The stride for each control point is the maximum offset added by 1.
				xmlNode * lInputElement = DAE_FindChildElementByTag(lLineElement, COLLADA_INPUT_STRUCTURE);
				while (lInputElement)
				{
					int lOffset = 0;
					DAE_GetElementAttributeValue(lInputElement, COLLADA_OFFSET_PROPERTY, lOffset);
					const FbxString lSemantic = DAE_GetElementAttributeValue(lInputElement, COLLADA_SEMANTIC_PROPERTY);
					if (lSemantic == FbxString(COLLADA_VERTEX_INPUT))
						lVertexOffset = lOffset;
					lStride = lStride >= lOffset ? lStride : lOffset;

					lInputElement = DAE_FindChildElementByTag(lLineElement, COLLADA_INPUT_STRUCTURE, lInputElement);
				}
				lStride += 1;
            
				xmlNode * lPElement = DAE_FindChildElementByTag(lLineElement, COLLADA_P_STRUCTURE);
				while (lPElement)
				{
					ElementContentAccessor lPAccessor(lPElement);
					int lData = -1;
					int lLastSegmentEnd = -1; // The index of end point of last line segment
					int lReadCounter = 0;
					int lVertexCounter = 0; // How many vertex has been read
					while (lPAccessor.GetNext(&lData))
					{
						if (lReadCounter == lVertexOffset)
						{
							if (lVertexCounter % 2 == 0) // Begin of a line segment
							{
								if (lLastSegmentEnd != lData) // Begin of a new line strip
								{
									lLine->AddEndPoint(lIndexArray->GetCount() - 1);
									lIndexArray->Add(lData);
								}
							}
							else // End of a line segment
							{
								lLastSegmentEnd = lData;
								lIndexArray->Add(lData);
							}

							++lVertexCounter;
						}
						++lReadCounter;
						if (lReadCounter == lStride)
							lReadCounter = 0;
					}
					lPElement = DAE_FindChildElementByTag(lLineElement, COLLADA_P_STRUCTURE, lPElement);
				}
	        }

			lLine->AddEndPoint(lIndexArray->GetCount() - 1);
			pObjects.Add(lLine);
		} // if (ImportVertices(lVerticesElement, lLine))
    }

    CNodeList lLinestripsElements;
    findChildrenByType(pMeshElement, COLLADA_LINESTRIP_STRUCTURE, lLinestripsElements);
    if (lLinestripsElements.GetCount())
    {
        lLine = FbxLine::Create(mScene, "");
        DAE_SetName(lLine, lGeometryName, lGeometryID);

        if (ImportVertices(lVerticesElement, lLine))
		{            
			for (int lLinestripElementIndex = 0; lLinestripElementIndex < lLinestripsElements.GetCount(); ++lLinestripElementIndex)
			{
				xmlNode * lLinestripElement = lLinestripsElements.GetAt(lLinestripElementIndex);

				int lVertexOffset = 0;
				int lStride = 0;
				// The stride for each control point is the maximum offset added by 1.
				xmlNode * lInputElement = DAE_FindChildElementByTag(lLinestripElement, COLLADA_INPUT_STRUCTURE);
				while (lInputElement)
				{
					int lOffset = 0;
					DAE_GetElementAttributeValue(lInputElement, COLLADA_OFFSET_PROPERTY, lOffset);
					const FbxString lSemantic = DAE_GetElementAttributeValue(lInputElement, COLLADA_SEMANTIC_PROPERTY);
					if (lSemantic == FbxString(COLLADA_VERTEX_INPUT))
						lVertexOffset = lOffset;
					lStride = lStride >= lOffset ? lStride : lOffset;

					lInputElement = DAE_FindChildElementByTag(lLinestripElement, COLLADA_INPUT_STRUCTURE, lInputElement);
				}
				lStride += 1;

				FbxArray<int> * lIndexArray = lLine->GetIndexArray();
				xmlNode * lPElement = DAE_FindChildElementByTag(lLinestripElement, COLLADA_P_STRUCTURE);
				while (lPElement)
				{
					ElementContentAccessor lPAccessor(lPElement);
					int lData = -1;
					int lReadCounter = 0;
					while (lPAccessor.GetNext(&lData))
					{
						if (lReadCounter == lVertexOffset)
							lIndexArray->Add(lData);
						++lReadCounter;
						if (lReadCounter == lStride)
							lReadCounter = 0;
					}
					lLine->AddEndPoint(lIndexArray->GetCount() - 1);
					lPElement = DAE_FindChildElementByTag(lLinestripElement, COLLADA_P_STRUCTURE, lPElement);
				}
			}

			pObjects.Add(lLine);
		} // if (ImportVertices(lVerticesElement, lLine))
    }

    FbxMesh * lMesh = FbxMesh::Create(mScene, "");
    DAE_SetName(lMesh, lGeometryName, lGeometryID);

    if (!lMesh->GetLayer(0))
        lMesh->CreateLayer();

    if (!ImportVertices(lVerticesElement, lMesh))
        return (lLine) ? lLine : NULL;

    if (ImportPolygons(pMeshElement, *lMesh, pMaterialSequence) == false)
        return (lLine) ? lLine : NULL;

	// if the Mesh does not have any polygon BUT we have a line, we return the line
	if (lLine && lMesh->GetPolygonCount() == 0)
		return lLine;

	// check if the number of UV match the number of polygon vertex count
	// when the mapping mode is by polygon vertex only for now
	ValidateMesh(lMesh, &GetStatus());

	pObjects.InsertAt(0, lMesh);
    return lMesh;
}

bool FbxReaderCollada::ImportVertices(xmlNode* pVerticesElement, FbxGeometry * pGeometry)
{
    // COLLADA vertices element describe the vertices and per-vertex data of a mesh.
    // Currently per-vertex normal and color are supported.

    int lVerticeCount = 0;
    xmlNode * lInputElement = DAE_FindChildElementByTag(pVerticesElement, COLLADA_INPUT_STRUCTURE);
    while (lInputElement)
    {
        const FbxString lSemantic = DAE_GetElementAttributeValue(lInputElement, COLLADA_SEMANTIC_PROPERTY);
        xmlNode * lSourceElement = DAE_GetSourceWithSemantic(pVerticesElement, lSemantic, mSourceElements);
        if (!lSourceElement)
		{
			GetStatus().SetCode(FbxStatus::eInvalidParameter);
			return false;
		}

        int lStride = 1;
		int lFbxStride = 1;
		int lSize = 0;
		int lNbRead = 0;

        if (lSemantic == COLLADA_POSITION_INPUT)
        {
            SourceElementContentAccessor<double> lSourceElementAccessor(lSourceElement);
            lVerticeCount = lSourceElementAccessor.mCount;
            lStride = lSourceElementAccessor.mStride;
            FBX_ASSERT(lStride >= VECTOR_STRIDE);
            pGeometry->InitControlPoints(lVerticeCount);

			lFbxStride = sizeof(FbxVector4)/sizeof(double);
			lSize = lVerticeCount * FbxMin(lFbxStride, lStride);
            lNbRead = lSourceElementAccessor.GetArray((double *)pGeometry->GetControlPoints(), lSize,
								0, lStride, lStride, 0, VECTOR_STRIDE, lFbxStride, 1.0);

			if (lNbRead == 0 && lVerticeCount > 0)
			{
				GetStatus().SetCode(FbxStatus::eFailure, "Corrupted data structure");
				return false;
			}
        }
        else if (lSemantic == COLLADA_NORMAL_INPUT)
        {
            SourceElementContentAccessor<double> lSourceElementAccessor(lSourceElement);
            int lNormalCount = lSourceElementAccessor.mCount;
            lStride = lSourceElementAccessor.mStride;
            FBX_ASSERT(lVerticeCount == lNormalCount && lStride >= VECTOR_STRIDE);
            pGeometry->InitNormals(lNormalCount);
            FbxLayerElementNormal *lLayerElementNormal = pGeometry->GetLayer(0)->GetNormals();
            FbxVector4 * lNormalArray = NULL;
            lNormalArray = lLayerElementNormal->GetDirectArray().GetLocked(lNormalArray);

			lFbxStride = sizeof(FbxVector4)/sizeof(double);
			lSize = lNormalCount * FbxMin(lFbxStride, lStride);
            lNbRead = lSourceElementAccessor.GetArray((double *)lNormalArray, lSize,
								0, lStride, lStride, 0, VECTOR_STRIDE, lFbxStride, 1.0);
            lLayerElementNormal->GetDirectArray().Release(&lNormalArray, lNormalArray);

			if (lNbRead == 0 && lNormalCount > 0)
			{
				GetStatus().SetCode(FbxStatus::eFailure, "Corrupted data structure");
				return false;
			}
        }
        else if (lSemantic == COLLADA_COLOR_INPUT)
        {
            SourceElementContentAccessor<double> lSourceElementAccessor(lSourceElement);
            int lVertexColorCount = lSourceElementAccessor.mCount;
            lStride = lSourceElementAccessor.mStride;
            FBX_ASSERT(lVerticeCount == lVertexColorCount && (lStride >= 3 && lStride <= 4));
			if (lStride < 3 || lStride > 4)
			{
				// Would be surprising if a COLOR field has more than 4 components
				GetStatus().SetCode(FbxStatus::eFailure, "Corrupted data structure");
				return false;
			}

            FbxLayerElementVertexColor *lLayerElementVertexColor = pGeometry->GetLayer(0)->GetVertexColors();
            if (!lLayerElementVertexColor)
            {
                lLayerElementVertexColor = FbxLayerElementVertexColor::Create(pGeometry, "");
                pGeometry->GetLayer(0)->SetVertexColors(lLayerElementVertexColor);
            }
            lLayerElementVertexColor->SetMappingMode(FbxLayerElement::eByControlPoint);
            lLayerElementVertexColor->SetReferenceMode(FbxLayerElement::eDirect);
            lLayerElementVertexColor->GetDirectArray().SetCount(lVertexColorCount);
            FbxColor * lVertexColorArray = NULL;
            lVertexColorArray = lLayerElementVertexColor->GetDirectArray().GetLocked(lVertexColorArray);

			lFbxStride = sizeof(FbxColor)/sizeof(double);
			lSize = lVertexColorCount * FbxMin(lFbxStride, lStride);
            lNbRead = lSourceElementAccessor.GetArray((double *)lVertexColorArray, lSize,
								0, lStride, lStride, 
								0, lStride, lFbxStride,
								1.0);
            lLayerElementVertexColor->GetDirectArray().Release(&lVertexColorArray, lVertexColorArray);

			if (lNbRead == 0 && lVertexColorCount > 0)
			{
				GetStatus().SetCode(FbxStatus::eFailure, "Corrupted data structure");
				return false;
			}

        }
        else if (lSemantic == COLLADA_TEXCOORD_INPUT)
        {
            SourceElementContentAccessor<double> lSourceElementAccessor(lSourceElement);
            int lUVCount = lSourceElementAccessor.mCount;
            lStride = lSourceElementAccessor.mStride;
            FBX_ASSERT(lVerticeCount == lUVCount && lStride <= 3);
            FbxLayerElementUV *lLayerElementUV = pGeometry->GetLayer(0)->GetUVs();
            if (!lLayerElementUV)
            {
                lLayerElementUV = FbxLayerElementUV::Create(pGeometry, "");
                pGeometry->GetLayer(0)->SetUVs(lLayerElementUV);
            }
            lLayerElementUV->SetMappingMode(FbxLayerElement::eByControlPoint);
            lLayerElementUV->SetReferenceMode(FbxLayerElement::eDirect);
            lLayerElementUV->GetDirectArray().SetCount(lUVCount);
            FbxVector2 * lUVArray = NULL;
            lUVArray = lLayerElementUV->GetDirectArray().GetLocked(lUVArray);

			lFbxStride = sizeof(FbxVector2)/sizeof(double);
			lSize = lUVCount * FbxMin(lFbxStride, lStride);
            lNbRead = lSourceElementAccessor.GetArray((double *)lUVArray, lSize,
								0, lStride, lStride, 0, 2, lFbxStride, 1.0);
            lLayerElementUV->GetDirectArray().Release(&lUVArray, lUVArray);

			if (lNbRead == 0 && lUVCount)
			{
				GetStatus().SetCode(FbxStatus::eFailure, "Corrupted data structure");
				return false;
			}
        }
        else
        {
            const FbxString lMessage = lSemantic + " not supported in vertices element";
            AddNotificationError(lMessage);
        }

        lInputElement = DAE_FindChildElementByTag(pVerticesElement, COLLADA_INPUT_STRUCTURE, lInputElement);
    }
    
    return true;
}

bool FbxReaderCollada::ImportPolygons(xmlNode* pXmlNode, FbxMesh& pMesh, const FbxDynamicArray<FbxString> & pMaterialSequence)
{
    // Import the pXmlNode's polygons. Also read the sources.
    // Read polygon, polylist and triangle nodes
    CNodeList polygonNodes;
    FbxSet<FbxString> lTypes;
    lTypes.Insert(COLLADA_POLYGONS_STRUCTURE);
    lTypes.Insert(COLLADA_POLYLIST_STRUCTURE);
    lTypes.Insert(COLLADA_TRIANGLES_STRUCTURE);
    findChildrenByType(pXmlNode, lTypes, polygonNodes);
    int lPolygonNodeCount = polygonNodes.GetCount();
    if (lPolygonNodeCount == 0)
        return true;

    //Calculate total number of polygon faces.
    //Useful to set right indexArray for materials/textures.
    int lTotalPolyFace = 0;
    for (int lPolygonNodeIndex = 0; lPolygonNodeIndex < lPolygonNodeCount; ++lPolygonNodeIndex)
    {
        int lCount = 0;
        DAE_GetElementAttributeValue(polygonNodes[lPolygonNodeIndex], COLLADA_COUNT_PROPERTY, lCount);
        lTotalPolyFace += lCount;
    }
    if (lTotalPolyFace == 0)
        return false;

    // Only import normal element if all sub-meshes has normal input, the same for other elements;
    // If only one sub-mesh exists, import all.
    FbxSet<FbxString> lInputSetUnion;
    if (lPolygonNodeCount > 1)
    {
        for (int lPolygonNodeIndex = 0; lPolygonNodeIndex < lPolygonNodeCount; ++lPolygonNodeIndex)
        {
            FbxSet<FbxString> lInputSet;
            xmlNode * lPolygonElement = polygonNodes[lPolygonNodeIndex];
            xmlNode * lInputElement = DAE_FindChildElementByTag(lPolygonElement, COLLADA_INPUT_STRUCTURE);
            while (lInputElement)
            {
                const FbxString lSemanticValue = DAE_GetElementAttributeValue(lInputElement, COLLADA_SEMANTIC_PROPERTY);
                lInputSet.Insert(lSemanticValue);
                lInputElement = DAE_FindChildElementByTag(lPolygonElement, COLLADA_INPUT_STRUCTURE, lInputElement);
            }
            if (lPolygonNodeIndex == 0) // The first polygon element
                lInputSetUnion = lInputSet;
            else
                lInputSetUnion = lInputSetUnion.Union(lInputSet);
        }
    }

    const FbxString lGeometryID = DAE_GetElementAttributeValue(pXmlNode->parent, COLLADA_ID_PROPERTY);

    int lCurrentPolyFaceIndex = 0;
    for (int lPolygonNodeIndex = 0; lPolygonNodeIndex < lPolygonNodeCount; lPolygonNodeIndex++)
    {
        xmlNode* polygonNode = polygonNodes[lPolygonNodeIndex];
        FbxString lPolygonNodeType((char*)polygonNode->name);

        //Get polygon face number of this polygonNode.
        int lPolyFaceCount = 0;
        DAE_GetElementAttributeValue(polygonNode, COLLADA_COUNT_PROPERTY, lPolyFaceCount);

        // Create LayerElementMaterial if any material connected to this mesh
        int lMaterialIndex = -1;
        if( pMaterialSequence.Size() )
        {
            FbxLayer* lLayer0 = pMesh.GetLayer(0);
            FbxLayerElementMaterial *lLayerElementMaterial = (FbxLayerElementMaterial*)lLayer0->GetLayerElementOfType(FbxLayerElement::eMaterial);
            if (!lLayerElementMaterial)
            {
                lLayerElementMaterial = (FbxLayerElementMaterial*)lLayer0->CreateLayerElementOfType(FbxLayerElement::eMaterial);
                lLayer0->SetMaterials(lLayerElementMaterial);
                lLayerElementMaterial->SetReferenceMode(FbxLayerElement::eIndexToDirect);

                // Set mapping mode.
                // Materials are defined on the polygon nodes, so if there is only one polygon node,
                // that means all polygons have the same material.
                // In COLLADA materials could only be on layer 0.
                if (lPolygonNodeCount <= 1 || pMaterialSequence.Size() == 1)
                {
                    lLayerElementMaterial->SetMappingMode(FbxLayerElement::eAllSame);
                    lLayerElementMaterial->GetIndexArray().Add(0);
                }
                else
                {
                    lLayerElementMaterial->SetMappingMode(FbxLayerElement::eByPolygon);
                    lLayerElementMaterial->GetIndexArray().AddMultiple(lTotalPolyFace);
                }
            }

            // If the material count is more than 1, the material index is 0 by default.
            if (pMaterialSequence.Size() > 1)
            {
                const FbxString lMaterialSymbol = DAE_GetElementAttributeValue(polygonNode, COLLADA_MATERIAL_PROPERTY);
                const int lMaterialCount = (int)pMaterialSequence.Size();
                for (int lIndex = 0; lIndex < lMaterialCount; ++lIndex)
                {
                    if (lMaterialSymbol == pMaterialSequence[lIndex])
                    {
                        lMaterialIndex = lIndex;
                        break;
                    }
                }
            }

            if (lMaterialIndex != -1)
            {
                //Set correct IndexArray for materials.
                int lCounter = 0;
                while (lCounter < lPolyFaceCount)
                {
                    lLayerElementMaterial->GetIndexArray().SetAt(lCurrentPolyFaceIndex++, lMaterialIndex);
                    ++lCounter;
                }
            }
        }

        // Build a list of the inputs for this set of polygons
        CNodeList lInputElements;
        findChildrenByType(polygonNode, COLLADA_INPUT_STRUCTURE, lInputElements);
        int lInputElementCount = lInputElements.GetCount();

        // Find index offset for vertices.
        // Import direct array for layer elements, and save the offset and the index array pointer
        FbxArray<LayerElementInfo> lLayerElementInfos;
        int lTotalOffset = 0;
        int lVertexOffset = -1;
        for (int lInputElementIndex = 0; lInputElementIndex < lInputElementCount; ++lInputElementIndex)
        {
            const FbxString lSemantic = DAE_GetElementAttributeValue(lInputElements[lInputElementIndex], COLLADA_SEMANTIC_PROPERTY);
            int lOffset;
            DAE_GetElementAttributeValue(lInputElements[lInputElementIndex], COLLADA_OFFSET_PROPERTY, lOffset);
            if (lOffset > lTotalOffset)
                lTotalOffset = lOffset;
            if (lSemantic == COLLADA_VERTEX_INPUT)
            {
                lVertexOffset = lOffset;
            }
            else
            {
                FbxLayerElementArray * lIndexArray = NULL;
                // If this input's semantic is not in all sub-meshes, ignore it.
                if ((lPolygonNodeCount > 1 && lInputSetUnion.Find(lSemantic) == NULL) == false)
                {
                    xmlNode * lInputElement = lInputElements[lInputElementIndex];
                    const FbxString lSourceElementID = DAE_GetIDFromSourceAttribute(lInputElement);
                    int lLayerIndex = 0;
                    DAE_GetElementAttributeValue(lInputElement, COLLADA_SET_PROPERTY, lLayerIndex);

                    // Get or create layer with right index
                    FbxLayer *lLayer = pMesh.GetLayer(lLayerIndex);
                    while (!lLayer)
                    {
                        pMesh.CreateLayer();
                        lLayer = pMesh.GetLayer(lLayerIndex);
                    }

                    // Get source element
                    xmlNode *lSourceElement = DAE_FindChildElementByAttribute(pXmlNode, COLLADA_ID_PROPERTY, lSourceElementID);
                    if (lSourceElement == NULL)
                    {
                        const FbxString msg = FbxString(COLLADA_SOURCE_PROPERTY) + " \"" + lSourceElementID + "\" not found as <" + COLLADA_SOURCE_PROPERTY + "> for <input>";
                        AddNotificationError( msg );
                        return false;
                    }

                    const ColladaLayerTraits lLayerTraits = ColladaLayerTraits::GetLayerTraits(lSemantic);
                    FbxLayerElement * lLayerElement = lLayer->GetLayerElementOfType(lLayerTraits.mLayerType);
                    if (!lLayerElement)
                        lLayerElement = lLayer->CreateLayerElementOfType(lLayerTraits.mLayerType);

					// make sure that we were able to create a layer element of the correct type (the
					// above call can fail if: 
					//    1) no memory
					//	  2) the lSemantic does not resolve to any of the followings:
					//			eNormal, eVertexColor, eUV, eTangent or eBiNormal
					switch (lLayerTraits.mLayerType)
					{	
						case FbxLayerElement::eUV:
							lIndexArray = PopulateLayerElementDirectArray<FbxDouble2>(lLayerElement, lSourceElement, lLayerTraits.mLayerElementLength);
							break;
						case FbxLayerElement::eVertexColor:
							lIndexArray = PopulateLayerElementDirectArray<FbxColor>(lLayerElement, lSourceElement, lLayerTraits.mLayerElementLength);
							break;
						case FbxLayerElement::eNormal:
						case FbxLayerElement::eTangent:
						case FbxLayerElement::eBiNormal:
							lIndexArray = PopulateLayerElementDirectArray<FbxVector4>(lLayerElement, lSourceElement, lLayerTraits.mLayerElementLength);
							break;
						default:
							// invalid trait!
							break;
					};
                }

                lLayerElementInfos.Add(LayerElementInfo(lOffset, lIndexArray));
            }
        }
		lLayerElementInfos.Sort(LayerElementInfo::Compare);

        if (lVertexOffset == -1)
        {
            FbxString msg = ("No vertices defined in mesh ");
            AddNotificationError( msg );
            return false;
        }
        ++lTotalOffset;
        FBX_ASSERT(lTotalOffset > 0);

        // Create polygons. Store indices for later use.
        CNodeList lPElements;
        findChildrenByType(polygonNode, COLLADA_P_STRUCTURE, lPElements);
        int lPElementCount = lPElements.GetCount();
        for (int lPElementIndex = 0; lPElementIndex < lPElementCount; ++lPElementIndex)
        {
            int lLayerElementInfoCount = (int)lLayerElementInfos.Size();
            xmlNode* lPElement = lPElements[lPElementIndex];
            ElementContentAccessor lPAccessor(lPElement);

            if (lPolygonNodeType == COLLADA_TRIANGLES_STRUCTURE)
            {
                const int lPolygonVertexCount = 3;
                int lReadCount = 0;
                int lReadVertexCount = 0;
                int lLayerElementInfoIndex = 0;
                int lIndex = 0;
                while (lPAccessor.GetNext(&lIndex))
                {
                    if (lReadCount == lVertexOffset)
                    {
                        if (lReadVertexCount == 0)
                            pMesh.BeginPolygon(lMaterialIndex, -1, -1, false);
                        pMesh.AddPolygon(lIndex);
                        if (lReadVertexCount == 2)
                            pMesh.EndPolygon();
                    }

                    while (lLayerElementInfoIndex < lLayerElementInfoCount &&
                        lLayerElementInfos[lLayerElementInfoIndex].mOffset == lReadCount)
                    {
                        if (lLayerElementInfos[lLayerElementInfoIndex].mIndexArray)
                            lLayerElementInfos[lLayerElementInfoIndex].mIndexArray->Add(lIndex);
                        ++lLayerElementInfoIndex;
                    }

                    if (lLayerElementInfoIndex == lLayerElementInfoCount)
                    {
                        lLayerElementInfoIndex = 0;
                        ++lReadVertexCount;
                        if (lReadVertexCount == lPolygonVertexCount)
                            lReadVertexCount = 0;
                    }

                    ++lReadCount;
                    if (lReadCount == lTotalOffset)
                        lReadCount = 0;
                }
            }
            else if(lPolygonNodeType == COLLADA_POLYLIST_STRUCTURE) //To handle xml statement "polylist"
            {
                //Create polygons according to <vcount>(specify number of vertices for each polygon) and <p> (specify vertex attributes indices)
                xmlNode *lVCountElement = DAE_FindChildElementByTag(polygonNode, COLLADA_VERTEXCOUNT_ELEMENT);
                ElementContentAccessor lVCountAccessor(lVCountElement);
                int lPolygonVertexCount = 0;

                while (lVCountAccessor.GetNext(&lPolygonVertexCount))
                {
                    int lReadVertexCount = 0;
                    pMesh.BeginPolygon(lMaterialIndex, -1, -1, false);
                    int lReadCount = 0;
                    int lLayerElementInfoIndex = 0;
                    int lIndex = 0;
                    while (lReadVertexCount < lPolygonVertexCount && lPAccessor.GetNext(&lIndex))
                    {
                        if (lReadCount == lVertexOffset)
                        {
                            pMesh.AddPolygon(lIndex);
                        }

                        while (lLayerElementInfoIndex < lLayerElementInfoCount &&
                            lLayerElementInfos[lLayerElementInfoIndex].mOffset == lReadCount)
                        {
                            if (lLayerElementInfos[lLayerElementInfoIndex].mIndexArray)
                                lLayerElementInfos[lLayerElementInfoIndex].mIndexArray->Add(lIndex);
                            ++lLayerElementInfoIndex;
                        }

                        if (lLayerElementInfoIndex == lLayerElementInfoCount)
                        {
                            lLayerElementInfoIndex = 0;
                            ++lReadVertexCount;
                        }

                        ++lReadCount;
                        if (lReadCount == lTotalOffset)
                            lReadCount = 0;
                    }
                    pMesh.EndPolygon();
                }
            }
            else
            {
                pMesh.BeginPolygon(lMaterialIndex, -1, -1, false);
                int lReadCount = 0;
                int lLayerElementInfoIndex = 0;
                int lIndex = 0;
                while (lPAccessor.GetNext(&lIndex))
                {
                    if (lReadCount == lVertexOffset)
                    {
                        pMesh.AddPolygon(lIndex);
                    }
            
                    while (lLayerElementInfoIndex < lLayerElementInfoCount &&
                        lLayerElementInfos[lLayerElementInfoIndex].mOffset == lReadCount)
                    {
                        if (lLayerElementInfos[lLayerElementInfoIndex].mIndexArray)
                            lLayerElementInfos[lLayerElementInfoIndex].mIndexArray->Add(lIndex);
                        ++lLayerElementInfoIndex;
                    }
                    if (lLayerElementInfoIndex == lLayerElementInfoCount)
                        lLayerElementInfoIndex = 0;
    

                    ++lReadCount;
                    if (lReadCount == lTotalOffset)
                        lReadCount = 0;
                }
                pMesh.EndPolygon();
            }
        }
    }

    return true;
}

bool FbxReaderCollada::ImportTransforms(xmlNode* pXmlNode, FbxNode *pNode)
{
    FbxArray<int> lRotationOrder;
	int iAxis = 0;
    pNode->SetTransformationInheritType(FbxTransform::eInheritRSrs); // only one inheritance type in Collada
    const FbxString lNodeID = DAE_GetElementAttributeValue(pXmlNode, COLLADA_ID_PROPERTY);

    bool lFBXCompatible = DAE_CheckCompatibility(pXmlNode);
    FbxAMatrix lTransformStack;

    // The transformation elements in COLLADA is postmultiplied in the order in which they are specified in <node> element;
    // So process the last child element firstly, the second to the last and so on.
    for (xmlNode* lChild = pXmlNode->last; lChild != NULL; lChild = lChild->prev)
    {
        if (lChild->type != XML_ELEMENT_NODE) continue;

        const FbxString structure = (const char*)lChild->name;
        const FbxString lSid = DAE_GetElementAttributeValue(lChild, COLLADA_SUBID_PROPERTY);

        if (structure == COLLADA_PERSPECTIVE_STRUCTURE || structure == COLLADA_SKEW_STRUCTURE)
        {
            const FbxString lMessage = FbxString("<") + structure + "> is not supported.";
            AddNotificationWarning(lMessage);
        }
        else if (structure == COLLADA_MATRIX_STRUCTURE)
        {
            FbxAMatrix lMatrix;
            DAE_GetElementContent(lChild, lMatrix);
            lTransformStack = lMatrix * lTransformStack;

            if (ImportMatrixAnimation(pNode, lNodeID + "/" + COLLADA_MATRIX_STRUCTURE) == false)
				GetStatus().SetCode(FbxStatus::eFailure, "Corrupted data structure");
            if (ImportMatrixAnimation(pNode, lNodeID + "/" + COLLADA_TRANSFORM_STRUCTURE) == false)
				GetStatus().SetCode(FbxStatus::eFailure, "Corrupted data structure");
        }
        else if (structure == COLLADA_ROTATE_STRUCTURE)
        {
            if (lFBXCompatible)
            {
                /* WARNING this code identifies a pre- or post-rotation based solely on its <sid>. This will not work
                   if the <sid> of a pre- or post-rotation had a value different than expected. 
                   Pre-rotations and post-rotations are not defined as such in COLLADA 1.4. */

                // If lSid indicates a pre-rotation, update the pre-rotation. 
                // If lSid indicates a post-rotation, update the post-rotation.
                // NB: the read order is not taken into account for pre- and post-rotations.
                // Normal case: update the transform matrix.
                if (lSid == COLLADA_PRE_ROTATION_X || lSid == COLLADA_PRE_ROTATION_Y || lSid == COLLADA_PRE_ROTATION_Z)
                {
                    FbxVector4 lPreRV = pNode->GetPreRotation(FbxNode::eSourcePivot);
                    ImportRotationElement(lChild, lPreRV);
                    pNode->SetPreRotation(FbxNode::eSourcePivot,lPreRV);
                    pNode->SetRotationActive(true);
                }
                else if (lSid == COLLADA_POST_ROTATION_X || lSid == COLLADA_POST_ROTATION_Y || lSid == COLLADA_POST_ROTATION_Z)
                {
                    FbxVector4 lPostRV = pNode->GetPostRotation(FbxNode::eSourcePivot);
                    ImportRotationElement(lChild, lPostRV);
                    pNode->SetPostRotation(FbxNode::eSourcePivot,lPostRV);
                    pNode->SetRotationActive(true);
                }
                // The following three sid are used by ColladaMaya and their value equal to Maya's "rotate axis"
                // which is inversion of FBX's "post rotation"
                else if (lSid == COLLADA_ROTATE_AXIS_X || lSid == COLLADA_ROTATE_AXIS_Y || lSid == COLLADA_ROTATE_AXIS_Z)
                {
                    FbxVector4 lPostRV = pNode->GetPostRotation(FbxNode::eSourcePivot);
                    int lIndex = ImportRotationElement(lChild, lPostRV);
                    lPostRV[lIndex] = -lPostRV[lIndex];
                    pNode->SetPostRotation(FbxNode::eSourcePivot,lPostRV);
                    pNode->SetRotationActive(true);
                }
                else if (lSid == COLLADA_ROTATE_X || lSid == COLLADA_ROTATE_Y || lSid == COLLADA_ROTATE_Z || 
                    lSid == COLLADA_ROT_X || lSid == COLLADA_ROT_Y || lSid == COLLADA_ROT_Z ||
                    lSid == COLLADA_ROTATION_X || lSid == COLLADA_ROTATION_Y || lSid == COLLADA_ROTATION_Z || 
					lSid == COLLADA_ROTATIONX || lSid == COLLADA_ROTATIONY || lSid == COLLADA_ROTATIONZ ||lSid.IsEmpty())
                {
                    FbxVector4 lRV = pNode->LclRotation.Get();
                    const int lAxisIndex = ImportRotationElement(lChild, lRV);
                    lRotationOrder.Add(lAxisIndex);
                    pNode->LclRotation.Set(lRV);
                    pNode->SetRotationActive(true);

                    const FbxString lAnimationToken = lNodeID + "/" + lSid + ".ANGLE";
                    if (lAxisIndex == 0)
                        ImportPropertyAnimation(pNode->LclRotation, lAnimationToken, FBXSDK_CURVENODE_COMPONENT_X);
                    else if (lAxisIndex == 1)
                        ImportPropertyAnimation(pNode->LclRotation, lAnimationToken, FBXSDK_CURVENODE_COMPONENT_Y);
                    else if (lAxisIndex == 2)
                        ImportPropertyAnimation(pNode->LclRotation, lAnimationToken, FBXSDK_CURVENODE_COMPONENT_Z);
                }
                else
                {
                    FbxVector4 lData;
                    DAE_GetElementContent(lChild, lData);
                    if (lData[3] != 0)
                    {
                        const FbxString lWarning = FbxString("Unknown <rotate> element with SID \"") + lSid + "\"";
                        AddNotificationWarning(lWarning);
                    }
                }
            }
            else
            {
                FbxDouble4 lRotation;
                DAE_GetElementContent(lChild, lRotation);

                if (lRotation[3] == 0)
                    continue;

                double lAngle = lRotation[3] * FBXSDK_PI_DIV_180 / 2;
                double lCos = cos(lAngle);
                double lSin = sin(lAngle);
                FbxQuaternion lQ(lRotation[0] * lSin, lRotation[1] * lSin, lRotation[2] * lSin, lCos);
                FbxAMatrix lRotationMatrix;
                lRotationMatrix.SetQ(lQ);
                lTransformStack = lRotationMatrix * lTransformStack;
            }
        }
        else if (structure == COLLADA_SCALE_STRUCTURE)
        {
            FbxDouble3 lScaling;
            DAE_GetElementContent(lChild, lScaling);

            if (lFBXCompatible)
            {
                if (lSid == COLLADA_SCALE_STRUCTURE || lSid.IsEmpty())
                {
                    pNode->LclScaling.Set(lScaling);

                    const FbxString lAnimationToken = lNodeID + "/" + lSid;
                    ImportPropertyAnimation(pNode->LclScaling, lAnimationToken);
                    ImportPropertyAnimation(pNode->LclScaling, lAnimationToken + ".X", FBXSDK_CURVENODE_COMPONENT_X);
                    ImportPropertyAnimation(pNode->LclScaling, lAnimationToken + ".Y", FBXSDK_CURVENODE_COMPONENT_Y);
                    ImportPropertyAnimation(pNode->LclScaling, lAnimationToken + ".Z", FBXSDK_CURVENODE_COMPONENT_Z);
                }
                else
                {
                    const FbxString lWarning = FbxString("Unknown <scale> element with SID \"") + lSid + "\"";
                    AddNotificationWarning(lWarning);
                }
            }
            else
            {
                if (lScaling == FbxDouble3(1, 1, 1))
                    continue;

                FbxAMatrix lScalingMatrix;
                lScalingMatrix.SetS(lScaling);
                lTransformStack = lScalingMatrix * lTransformStack;
            }
        }
        else if (structure == COLLADA_TRANSLATE_STRUCTURE)
        {
            FbxDouble3 lTV;
            DAE_GetElementContent(lChild, lTV);

            if (lFBXCompatible)
            {
                /* WARNING this code identifies a pivot based solely on its <sid>. This will not work
                   if the <sid> of a pivot had a value different than expected. 
                   Pivots are not defined as such in COLLADA 1.3.1. */

			    // If lSid indicates a pivot, update the pivot.
			    // If lSid indicates a pivot inverse, do nothing.
			    // Normal case: update the transform matrix.
			    if (lSid == COLLADA_ROTATE_PIVOT) {
				    pNode->SetRotationPivot(FbxNode::eSourcePivot,lTV);
			    } else if (lSid == COLLADA_SCALE_PIVOT) {
				    pNode->SetScalingPivot(FbxNode::eSourcePivot,lTV);
			    } else if (lSid == COLLADA_ROTATE_PIVOT_OFFSET) {
				    pNode->SetRotationOffset(FbxNode::eSourcePivot,lTV);
			    } else if (lSid == COLLADA_SCALE_PIVOT_OFFSET) {
				    pNode->SetScalingOffset(FbxNode::eSourcePivot,lTV);
			    } else if (lSid == COLLADA_ROTATE_PIVOT_INVERSE || lSid == COLLADA_SCALE_PIVOT_INVERSE) {
				    // do nothing, the pivot is (supposedly) already taken care of.
			    } else if (lSid == COLLADA_TRANSLATE_ORIGIN)
                {
                    //A fix for poser, they add an entry "origin" for translation.
                    FbxVector4 lLclTranslate = pNode->LclTranslation.Get();
                    lLclTranslate += lTV;
                    pNode->LclTranslation.Set(lLclTranslate);

                    IncreaseLclTranslationAnimation(pNode, lTV);
                }
                else if (lSid == COLLADA_TRANSLATE_STRUCTURE || lSid == COLLADA_TRANSLATION_STRUCTURE || 
					     lSid == COLLADA_TRANSLATE_LOCATION || lSid.IsEmpty())
                {
                    FbxVector4 lLclTranslate = pNode->LclTranslation.Get();
                    lLclTranslate += lTV;
                    pNode->LclTranslation.Set(lLclTranslate);

                    const FbxString lAnimationToken = lNodeID + "/" + lSid;
                    ImportPropertyAnimation(pNode->LclTranslation, lAnimationToken);
                    ImportPropertyAnimation(pNode->LclTranslation, lAnimationToken + ".X", FBXSDK_CURVENODE_COMPONENT_X);
                    ImportPropertyAnimation(pNode->LclTranslation, lAnimationToken + ".Y", FBXSDK_CURVENODE_COMPONENT_Y);
                    ImportPropertyAnimation(pNode->LclTranslation, lAnimationToken + ".Z", FBXSDK_CURVENODE_COMPONENT_Z);
                }
                else
                {
                    if (lTV != FbxDouble3(0, 0, 0))
                    {
                        const FbxString lWarning = FbxString("Unknown <translate> element with SID \"") + lSid + "\"";
                        AddNotificationWarning(lWarning);
                    }
                }
            }
            else
            {
                if (lTV == FbxDouble3(0, 0, 0))
                    continue;

                FbxAMatrix lTranslationMatrix;
                lTranslationMatrix.SetT(lTV);
                lTransformStack = lTranslationMatrix * lTransformStack;
            }
        }
        else if (structure == COLLADA_INSTANCE_GEOMETRY_ELEMENT ||
            structure == COLLADA_EXTRA_STRUCTURE ||
            structure == COLLADA_INSTANCE_CAMERA_ELEMENT ||
            structure == COLLADA_INSTANCE_LIGHT_ELEMENT ||
            structure == COLLADA_INSTANCE_CONTROLLER_ELEMENT ||
            structure == COLLADA_BOUNDINGBOX_STRUCTURE ||
            structure == COLLADA_NODE_STRUCTURE ||
            structure == COLLADA_LOOKAT_STRUCTURE ||
            structure == COLLADA_INSTANCE_NODE_ELEMENT)
        {
			// not treated in this function.
			continue;
		} else {
			FbxString msg = FbxString("Structure ") + structure + " unknown\n";
			AddNotificationError( msg );
			return false;
		}
	}

    if (!lFBXCompatible)
    {
        pNode->LclTranslation.Set(lTransformStack.GetT());
        pNode->LclRotation.Set(lTransformStack.GetR());
        pNode->SetRotationActive(true);
        pNode->LclScaling.Set(lTransformStack.GetS());

        const FbxString lMessage = "The transform of node \"" + lNodeID + "\" is not compatible with FBX, so it is baked into TRS.";
        AddNotificationWarning(lMessage);
    }

    // Import visibility animation if exists
    ImportPropertyAnimation(pNode->Visibility, lNodeID + "/" + "visibility");

    SetRotationOrder(pNode, lRotationOrder);
    pNode->UpdatePropertiesFromPivotsAndLimits();

	return true;
}

void FbxReaderCollada::SetRotationOrder(FbxNode * pNode, const FbxArray<int> & pRotationOrder)
{
    FBX_ASSERT(pRotationOrder.Size() <= 3);
    if (pRotationOrder.Size() != 3)
    {
        pNode->SetRotationOrder(FbxNode::eSourcePivot, eEulerXYZ);
        return;
    }

    if (pRotationOrder[0] == 0 && pRotationOrder[1] == 1 && pRotationOrder[2] == 2)
    {
        pNode->SetRotationOrder(FbxNode::eSourcePivot, eEulerXYZ);
    }
    else if (pRotationOrder[0] == 0 && pRotationOrder[1] == 2 && pRotationOrder[2] == 1)
    {
        pNode->SetRotationOrder(FbxNode::eSourcePivot, eEulerXZY);
    }
    else if (pRotationOrder[0] == 1 && pRotationOrder[1] == 0 && pRotationOrder[2] == 2)
    {
        pNode->SetRotationOrder(FbxNode::eSourcePivot, eEulerYXZ);
    }
    else if (pRotationOrder[0] == 1 && pRotationOrder[1] == 2 && pRotationOrder[2] == 0)
    {
        pNode->SetRotationOrder(FbxNode::eSourcePivot, eEulerYZX);
    }
    else if (pRotationOrder[0] == 2 && pRotationOrder[1] == 0 && pRotationOrder[2] == 1)
    {
        pNode->SetRotationOrder(FbxNode::eSourcePivot, eEulerZXY);
    }
    else if (pRotationOrder[0] == 2 && pRotationOrder[1] == 1 && pRotationOrder[2] == 0)
    {
        pNode->SetRotationOrder(FbxNode::eSourcePivot, eEulerZYX);
    }
    else
    {
        pNode->SetRotationOrder(FbxNode::eSourcePivot, eEulerXYZ);
    }
}

int FbxReaderCollada::ImportRotationElement(xmlNode* pRotationElement, FbxVector4& pRotationVector)
{
    FbxDouble4 lValue;
    DAE_GetElementContent(pRotationElement, lValue);

    const FbxDouble3 lAxis = FbxDouble3(lValue[0], lValue[1], lValue[2]);

    int lRotationAxisIndex = 0;
    if (lAxis == FbxDouble3(1, 0, 0))
        lRotationAxisIndex = 0;
    else if (lAxis == FbxDouble3(0, 1, 0))
        lRotationAxisIndex = 1;
    else if (lAxis == FbxDouble3(0, 0, 1))
        lRotationAxisIndex = 2;
    else
    {
        FbxQuaternion lQ;
        const double lDegToRad = .017453292519943295769236907684886127134428718885417;
        const double lCoefficient = sin((lValue[3] / 2.0) * lDegToRad);
        lQ.Set(lAxis[0] * lCoefficient, lAxis[1] * lCoefficient, lAxis[2] * lCoefficient);
        lQ[3] = cos((lValue[3] / 2.0) * lDegToRad);
        pRotationVector.SetXYZ(lQ);
        return 0;
    }
    pRotationVector[lRotationAxisIndex] = lValue[3];
    return lRotationAxisIndex;
}

bool FbxReaderCollada::ImportLookAt(xmlNode* pXmlNode, FbxVector4& pCameraPosition, FbxVector4& pInterestPosition,
                                     FbxVector4& pUpVector, FbxAMatrix& pTransformMatrix)
{
    xmlNode *lLookAtNode = DAE_FindChildElementByTag(pXmlNode, COLLADA_LOOKAT_STRUCTURE);
    if (!lLookAtNode)
        return false;

    ElementContentAccessor lLookAtAccessor(lLookAtNode);
    lLookAtAccessor.GetNext(&pCameraPosition);
    lLookAtAccessor.GetNext(&pInterestPosition);
    lLookAtAccessor.GetNext(&pUpVector);

    pUpVector.Normalize();
    FbxVector4 lFront = pCameraPosition - pInterestPosition;
    lFront.Normalize();
    FbxVector4 lSide = pUpVector.CrossProduct(lFront);
    lSide.Normalize();

    pTransformMatrix.mData[0][0] = lSide[0];
    pTransformMatrix.mData[0][1] = lSide[1];
    pTransformMatrix.mData[0][2] = lSide[2];
    pTransformMatrix.mData[1][0] = pUpVector[0];
    pTransformMatrix.mData[1][1] = pUpVector[1];
    pTransformMatrix.mData[1][2] = pUpVector[2];
    pTransformMatrix.mData[2][0] = lFront[0];
    pTransformMatrix.mData[2][1] = lFront[1];
    pTransformMatrix.mData[2][2] = lFront[2];
    pTransformMatrix.mData[3][0] = pInterestPosition[0];
    pTransformMatrix.mData[3][1] = pInterestPosition[1];
    pTransformMatrix.mData[3][2] = pInterestPosition[2];

    return true;
}

bool FbxReaderCollada::IsNodeExportable(FbxString lId) 
{
	// Return false if we do not want to import a node with this id.

	// Do not import camera nodes with ids persp, top, bottom,
	// left, right, side, front or back since they are global cameras already
	// created in FBX (Note: the global camera settings will not be kept).
	if (lId == "persp" || lId == "top" || lId == "bottom" || lId == "left"
		|| lId == "right" || lId == "side" || lId == "front" || lId == "back") {
		FbxString msg = FbxString("Camera ") + lId + " will not be imported.";
		AddNotificationWarning( msg );
		return false;
	}

	return true;
}

bool FbxReaderCollada::CheckColladaVersion(const FbxString & pVersionString)
{
    // mColladaVersion has just been read.
    // Verify that the version is 1.4.*, else warning.
    if (strcmp(pVersionString.Buffer(), "1.4") < 0)
    {
        FbxString msg = FbxString("Old Collada format (") + pVersionString + "). ";
        msg += "This format is weakly supported and could cause read errors. ";
        msg += "There is strong support for Collada format 1.4.*.";
        AddNotificationWarning( msg );
    }
    else if (strcmp(pVersionString.Buffer(), "1.5") >= 0)
    {
        FbxString msg = FbxString("Collada format (") + pVersionString + ") ";
        msg += " newer than supported format 1.4.*. Might be weakly supported.";
        AddNotificationWarning( msg );
    }
    
    return true;
}

void FbxReaderCollada::AddNotificationError( FbxString pError )
{
    FbxUserNotification * lUserNotification = mManager.GetUserNotification();
	if ( lUserNotification ) {
		FbxString lError = "ERROR: " + pError;
		lUserNotification->AddDetail( FbxUserNotification::eFileIONotify, lError );
	}
}

void FbxReaderCollada::AddNotificationWarning( FbxString pWarning )
{
    FbxUserNotification * lUserNotification = mManager.GetUserNotification();
	if ( lUserNotification ) {
		FbxString lWarning = "Warning: " + pWarning;
		lUserNotification->AddDetail( FbxUserNotification::eFileIONotify, lWarning );
	}
}

bool FbxReaderCollada::ImportScene(xmlNode * pColladaNode)
{
    xmlNode * lSceneElement = DAE_FindChildElementByTag(pColladaNode, COLLADA_SCENE_STRUCTURE);
    if (!lSceneElement)
    {
        AddNotificationError("There is no <scene> element in this COLLADA file.");
        return false;
    }

    xmlNode * lInstanceVisualScene = DAE_FindChildElementByTag(lSceneElement, COLLADA_INSTANCE_VSCENE_ELEMENT);
    if (!lInstanceVisualScene)
    {
        const FbxString lMessage = FbxString("There is no <instance_visual_scene> element in the <scene> element.");
        AddNotificationError(lMessage);
        return false;
    }

	FbxString lExternalRef;
    const FbxString lVisualSceneID = DAE_GetIDFromUrlAttribute(lInstanceVisualScene, lExternalRef);
    xmlNode * lLibraryVisualSceneElement = DAE_FindChildElementByTag(pColladaNode, COLLADA_LIBRARY_VSCENE_ELEMENT);
    if (!lLibraryVisualSceneElement)
    {
        AddNotificationError("There is no <library_visual_scene> element in this COLLADA file.");
        return false;
    }
    xmlNode * lVisualSceneElement = DAE_FindChildElementByAttribute(lLibraryVisualSceneElement, COLLADA_ID_PROPERTY, lVisualSceneID);
    if (!lVisualSceneElement)
    {
        const FbxString lMessage = FbxString("There is no <visual_scene> element with ID \"")
            + lVisualSceneID + "\".";
        AddNotificationError(lMessage);
        return false;
    }
    return ImportVisualScene(lVisualSceneElement, mScene);
}

void FbxReaderCollada::BuildUpLibraryMap()
{
    LibraryTypeTraits * lTraits[] = {&mEffectTypeTraits, &mMaterialTypeTraits, &mImageTypeTraits, &mGeometryTypeTraits,
        &mControllerTypeTraits, &mLightTypeTraits, &mCameraTypeTraits, &mNodeTypeTraits, &mAnimationTypeTraits};
    const int lTraitCount = sizeof(lTraits) / sizeof(LibraryTypeTraits *);
    for (int lTraitIndex = 0; lTraitIndex < lTraitCount; ++lTraitIndex)
    {
        xmlNode * lLibraryElement = DAE_FindChildElementByTag(mColladaElement, lTraits[lTraitIndex]->library_tag);
        if (lLibraryElement)
        {
            BuildUpLibraryMap(lLibraryElement, lTraits[lTraitIndex]->element_tag);
        }
    }
}

void FbxReaderCollada::BuildUpLibraryMap(xmlNode * pElement, const FbxString & pElementTag)
{
    if (pElement)
    {
        xmlNode * lChildElement = DAE_FindChildElementByTag(pElement, pElementTag);
        while (lChildElement)
        {
            FbxString lID = DAE_GetElementAttributeValue(lChildElement, COLLADA_ID_PROPERTY);
                mColladaElements[lID] = ColladaElementData(lChildElement);

            BuildUpLibraryMap(lChildElement, pElementTag);

            lChildElement = DAE_FindChildElementByTag(pElement, pElementTag, lChildElement);
        }
    }
}

FbxObject * FbxReaderCollada::GetLibrary(const LibraryTypeTraits & type_traits, const FbxString & pID)
{
	ColladaElementMapType::RecordType* lIter = mColladaElements.Find(pID);
    if (!lIter)
    {
        const FbxString lMessage = "No <" + type_traits.element_tag + "> element with ID \"" + pID + "\" exists.";
        AddNotificationError(lMessage);
        return NULL;
    }
    else
    {
		if (lIter->GetValue().mFBXObject)
        {
            return lIter->GetValue().mFBXObject;
        }
        else
        {
            xmlNode * lElement = lIter->GetValue().mColladaElement;
            FbxObject * lObject = GetLibrary(type_traits, lElement);
            // At present, node instances are converted to different copies
            if (type_traits.library_tag != COLLADA_LIBRARY_NODE_ELEMENT)
                lIter->GetValue().mFBXObject = lObject;
            return lObject;
        }
    }
}

FbxObject * FbxReaderCollada::GetLibrary(const LibraryTypeTraits & type_traits, xmlNode * pElement)
{
    FbxObject * lObject = NULL;

    if (type_traits.library_tag == COLLADA_LIBRARY_EFFECT_ELEMENT)
        lObject = ImportEffect(pElement);
    else if (type_traits.library_tag == COLLADA_LIBRARY_MATERIAL_ELEMENT)
        lObject = ImportMaterial(pElement);
    else if (type_traits.library_tag == COLLADA_LIBRARY_IMAGE_ELEMENT)
        lObject = ImportImage(pElement);
    else if (type_traits.library_tag == COLLADA_LIBRARY_LIGHT_ELEMENT)
        lObject = ImportLight(pElement);
    else if (type_traits.library_tag == COLLADA_LIBRARY_CAMERA_ELEMENT)
        lObject = ImportCamera(pElement);
    else if (type_traits.library_tag == COLLADA_LIBRARY_NODE_ELEMENT)
        lObject = ImportNode(pElement);

    return lObject;
}

bool FbxReaderCollada::ImportMatrixAnimation(FbxNode * pNode, const FbxString & pAnimationChannelID)
{
	AnimationMapType::RecordType* iter = mAnimationElements.Find(pAnimationChannelID);
    if (!iter)
        return true;
	
	FbxArray<xmlNode*> & lAnimationElements = iter->GetValue();
    const int lAnimationElementCount = (int)(lAnimationElements.Size());
    for (int lAnimationElementIndex = 0; lAnimationElementIndex < lAnimationElementCount; ++lAnimationElementIndex)
    {
        AnimationElement lAnimationElement;
        if (lAnimationElement.FromCOLLADA(lAnimationElements[lAnimationElementIndex], mSourceElements) == false)
			return false;

        double lLocalUnitConversion = 1.0;
        const FbxSystemUnit * lLocalUnit = lAnimationElement.GetUnit();
        if (lLocalUnit)
            lLocalUnitConversion = lLocalUnit->GetConversionFactorTo(mGlobalSettings->GetSystemUnit());

        FbxAnimLayer * lAnimLayer = GetAnimLayer(lAnimationElement.GetID());
        if (lAnimationElement.ToFBX(pNode, lAnimLayer, lLocalUnitConversion) == false)
			return false;
    }

    return true;
}

bool FbxReaderCollada::ImportPropertyAnimation(FbxProperty & pProperty, const FbxString & pAnimationChannelID,
                                                const char * pChannelName)
{
	AnimationMapType::RecordType* iter = mAnimationElements.Find(pAnimationChannelID);
    if (!iter)
        return false;
 
	bool ret = true;
	FbxArray<xmlNode*> & lAnimationElements = iter->GetValue();
    const int lAnimationElementCount = (int)(lAnimationElements.Size());
    for (int lAnimationElementIndex = 0; lAnimationElementIndex < lAnimationElementCount; ++lAnimationElementIndex)
    {
        AnimationElement lAnimationElement;
        lAnimationElement.FromCOLLADA(lAnimationElements[lAnimationElementIndex], mSourceElements);

        FbxAnimLayer * lAnimLayer = GetAnimLayer(lAnimationElement.GetID());
        double lLocalUnitConversion = 1.0;
        const FbxSystemUnit * lLocalUnit = lAnimationElement.GetUnit();
        if (lLocalUnit)
            lLocalUnitConversion = lLocalUnit->GetConversionFactorTo(mGlobalSettings->GetSystemUnit());

        if (lAnimationElement.GetChannelCount() == 1) // Single channel
        {
            FbxAnimCurve * lCurve = pProperty.GetCurve(lAnimLayer, pChannelName, true);
            if (lAnimationElement.ToFBX(lCurve, 0, lLocalUnitConversion) == false)
				ret = false;
        }
        else if (lAnimationElement.GetChannelCount() == 3) // Multiple channel, just support three channels for vector now
        {
            FbxAnimCurve * lCurve = pProperty.GetCurve(lAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
            if (lAnimationElement.ToFBX(lCurve, 0, lLocalUnitConversion) == false)
				ret = false;

            lCurve = pProperty.GetCurve(lAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
            if (lAnimationElement.ToFBX(lCurve, 1, lLocalUnitConversion) == false)
				ret = false;

            lCurve = pProperty.GetCurve(lAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
            if (lAnimationElement.ToFBX(lCurve, 2, lLocalUnitConversion) == false)
				ret = false;
        }
        else
        {
            FBX_ASSERT(0);
            ret = false;
        }
    }

    return ret;
}

void FbxReaderCollada::Preprocess(xmlNode * pColladaElement)
{
    FbxArray<xmlNode*> lSourceElements;
    FbxArray<xmlNode*> lAnimationElements;

    xmlNode * lLibraryAnimationsElement = DAE_FindChildElementByTag(pColladaElement, COLLADA_LIBRARY_ANIMATION_ELEMENT);
    if (lLibraryAnimationsElement)
    {
        RecursiveSearchElement(lLibraryAnimationsElement, COLLADA_ANIMATION_STRUCTURE, lAnimationElements);
        RecursiveSearchElement(lLibraryAnimationsElement, COLLADA_SOURCE_STRUCTURE, lSourceElements);
    }

    xmlNode * lLibraryGeometriesElement = DAE_FindChildElementByTag(pColladaElement, COLLADA_LIBRARY_GEOMETRY_ELEMENT);
    if (lLibraryGeometriesElement)
    {
        RecursiveSearchElement(lLibraryGeometriesElement, COLLADA_SOURCE_STRUCTURE, lSourceElements);
    }
    
    xmlNode * lLibraryControllersElement = DAE_FindChildElementByTag(pColladaElement, COLLADA_LIBRARY_CONTROLLER_ELEMENT);
    if (lLibraryControllersElement)
    {
        RecursiveSearchElement(lLibraryControllersElement, COLLADA_SOURCE_STRUCTURE, lSourceElements);
    }

    const int lSourceElementCount = lSourceElements.GetCount();
    for (int lIndex = 0; lIndex < lSourceElementCount; ++lIndex)
    {
        xmlNode * lElement = lSourceElements[lIndex];
        const FbxString lName = DAE_GetElementAttributeValue(lElement, COLLADA_ID_PROPERTY);
        if (!lName.IsEmpty())
			mSourceElements[lName] = lElement;
    }

    // Update map between channel and animation element
    const int lAnimationElementCount = lAnimationElements.GetCount();
    for (int lIndex = 0; lIndex < lAnimationElementCount; ++lIndex)
    {
        xmlNode * lElement = lAnimationElements[lIndex];
        xmlNode * lChannelElement = DAE_FindChildElementByTag(lElement, COLLADA_CHANNEL_STRUCTURE);
        while (lChannelElement)
        {
            const FbxString lAttributeID = DAE_GetIDFromTargetAttribute(lChannelElement);
            mAnimationElements[lAttributeID].Add(lElement);
            lChannelElement = DAE_FindChildElementByTag(lElement, COLLADA_CHANNEL_STRUCTURE, lChannelElement);
        }
    }
}

FbxAnimLayer * FbxReaderCollada::GetAnimLayer(const FbxString & lAnimationID)
{
	const int lAnimationClipCount = (int)(mAnimationClipData.Size());
    for (int lAnimationClipIndex = 0; lAnimationClipIndex < lAnimationClipCount; ++lAnimationClipIndex)
    {
        AnimationClipData & lAnimClipData = mAnimationClipData[lAnimationClipIndex];
		FbxSet<FbxString>::RecordType* iter = lAnimClipData.mAnimationElementIDs.Find(lAnimationID);
        if (iter)
        {
            if (lAnimClipData.mAnimLayer == NULL)
            {
                FbxAnimStack* lAnimStack = FbxAnimStack::Create(mScene, lAnimClipData.mID);
                lAnimClipData.mAnimLayer = FbxAnimLayer::Create(lAnimStack, "Layer0");
            }
            return lAnimClipData.mAnimLayer;
        }
    }
    return mAnimLayer;
}

double FbxReaderCollada::GetLocalUnitConversion(xmlNode * pElement)
{
    double lLocalUnitConversion = 1;
    if (pElement)
    {
        xmlNode * lAssetElement = DAE_FindChildElementByTag(pElement, COLLADA_ASSET_STRUCTURE);
        if (lAssetElement)
        {
            xmlNode * lUnitElement = DAE_FindChildElementByTag(lAssetElement, COLLADA_UNIT_STRUCTURE);
            if (lUnitElement)
            {
                FbxSystemUnit lLocalUnit = DAE_ImportUnit(lUnitElement);
                FbxSystemUnit lDocumentUnit = mGlobalSettings->GetSystemUnit();
                lLocalUnitConversion = lLocalUnit.GetConversionFactorTo(lDocumentUnit);
            }
        }
    }
    return lLocalUnitConversion;
}

void FbxReaderCollada::ImportPropertyValue(FbxObject * pObject,
                                            const char * pPropertyName,
                                            xmlNode * pPropertyValueElement)
{
    const char * lTag = (const char *)(pPropertyValueElement->name);
    if (strcmp(lTag, COLLADA_INT_TYPE) == 0)
    {
        FbxProperty lProp = FbxProperty::Create(pObject, FbxIntDT,
            pPropertyName, pPropertyName);
        int lValue = 0;
        DAE_GetElementContent(pPropertyValueElement, lValue);
        lProp.Set(lValue);
    }
    else if (strcmp(lTag, COLLADA_FLOAT_TYPE) == 0)
    {
        FbxProperty lProp = FbxProperty::Create(pObject, FbxFloatDT,
            pPropertyName, pPropertyName);
        double lValue = 0;
        DAE_GetElementContent(pPropertyValueElement, lValue);
        float lFloatValue = (float)lValue;
        lProp.Set(lFloatValue);
    }
    else if (strcmp(lTag, COLLADA_FLOAT3_TYPE) == 0)
    {
        FbxProperty lProp = FbxProperty::Create(pObject, FbxDouble3DT,
            pPropertyName, pPropertyName);
        FbxDouble3 lValue;
        DAE_GetElementContent(pPropertyValueElement, lValue);
        lProp.Set(lValue);
    }
    else if (strcmp(lTag, COLLADA_MATRIX_TYPE) == 0)
    {
        FbxProperty lProp = FbxProperty::Create(pObject, FbxTransformMatrixDT,
            pPropertyName, pPropertyName);
        FbxAMatrix lValue;
        DAE_GetElementContent(pPropertyValueElement, lValue);
        lProp.Set(lValue);
    }
    else if (strcmp(lTag, COLLADA_STRING_TYPE) == 0)
    {
        FbxProperty lProp = FbxProperty::Create(pObject, FbxStringDT,
            pPropertyName, pPropertyName);
        FbxString lValue;
        DAE_GetElementContent(pPropertyValueElement, lValue);
        lProp.Set(lValue);
    }
    else if (strcmp(lTag, COLLADA_FXCMN_SURFACE_ELEMENT) == 0)
    {
        FbxProperty lProp = FbxProperty::Create(pObject, FbxDouble3DT,
            pPropertyName, pPropertyName);
        xmlNode * lInitFromElement = DAE_FindChildElementByTag(
            pPropertyValueElement, COLLADA_INITFROM_ELEMENT);
        FbxString lTextureID;
        DAE_GetElementContent(lInitFromElement, lTextureID);
        FbxFileTexture * lImage = FbxCast<FbxFileTexture>(
            GetLibrary(mImageTypeTraits, lTextureID));
        if (lImage)
            lProp.ConnectSrcObject(lImage);
    }
    else
    {
        FBX_ASSERT(0);
    }
}

#include <fbxsdk/fbxsdk_nsend.h>
