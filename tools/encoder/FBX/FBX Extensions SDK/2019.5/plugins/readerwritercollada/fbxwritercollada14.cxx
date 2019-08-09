/****************************************************************************************
 
   Copyright (C) 2017 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/scene/animation/fbxanimcurve.h>
#include <fbxsdk/scene/animation/fbxanimutilities.h>
#include <fbxsdk/fileio/collada/fbxwritercollada14.h>
#include <fbxsdk/fileio/collada/fbxcolladaanimationelement.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

namespace
{
    const double FLOAT_TOLERANCE = 0.000001;
    const double RADIAN_TO_DEGREE = 57.295779513082320876798154814105170332405472466565;

    const char * AMBIENT_LIGHT_NAME = "SceneAmbient";

    const char * VERTEX_POSTFIX = "-VERTEX";
    const char * POSITION_POSTFIX = "-POSITION";
    const char * NORMAL_POSTFIX = "-Normal";
    const char * UV_POSTFIX = "-UV";
    const char * VERTEX_COLOR_POSTFIX = "-VERTEX_COLOR";
};

//
//Constructor
//
FbxWriterCollada::FbxWriterCollada(FbxManager& pManager,int pID, FbxStatus& pStatus) : 
    FbxWriter(pManager, pID, pStatus), 
    mFileName(""),
    mXmlDoc(NULL),
    mLibraryAnimation(NULL),
    mLibraryCamera(NULL),
    mLibraryController(NULL),
    mLibraryGeometry(NULL),
    mLibraryImage(NULL),
    mLibraryLight(NULL),
    mLibraryMaterial(NULL),
    mLibraryEffect(NULL),
    mLibraryTexture(NULL),
    mLibraryVisualScene(NULL),
    mTriangulate(false),
    mSingleMatrix(false)
{
    mSamplingPeriod.SetSecondDouble( 1./30. );
    mFileObject         = FbxNew< FbxFile >();
    mShapeMeshesList    = FbxNew< FbxStringList >();
}

//
//Destructor
//
FbxWriterCollada::~FbxWriterCollada()
{
    if (mFileObject->IsOpen() )
    {
        FileClose();
    }

    FBX_SAFE_DELETE(mFileObject);
    FBX_SAFE_DELETE(mShapeMeshesList);
}

//
//Create and open file with the given name.
//Return true on success, false otherwise.
//
bool FbxWriterCollada::FileCreate(char* pFileName)
{
    if (mFileObject->IsOpen() )
    {
        FileClose();
    }

    mFileName = FbxPathUtils::Clean(pFileName);
    FbxPathUtils::Create(FbxPathUtils::GetFolderName(mFileName));
    
	if (!mFileObject->Open(mFileName.Buffer(), FbxFile::eCreateWriteOnly, false) )
    {
        GetStatus().SetCode(FbxStatus::eFailure, "File not opened"); 
        mFileName="";
        return false;
    }

    return true;
}

//
//Close file.
//Return true on success, false otherwise.
//
bool FbxWriterCollada::FileClose()
{
    if( mFileObject->IsOpen() )
    {
        mFileObject->Close();
    }
    mFileName = "";
    return true;
}

//
//Check if current file is open.
//Return true if file is open, false otherwise.
//
bool FbxWriterCollada::IsFileOpen()
{
    return mFileObject->IsOpen();
}


//
//Export the FBX document to Collada file, according to the given options settings.
//1.  Get FBX scene from FBX document (pDocument);
//2.  Preprocess FBX scene, so as to correctly export FBX nodes to Collada XML nodes;
//3.  Create XML document (create Collada XML root node);
//4.  Setup XML header;
//5.  Export FBX scene info to Collada asset;
//6.  Export FBX scene(all FBX nodes) to Collada XML nodes tree;
//7.  Export the global ambient to Collada XML light node;
//8.  Update XML mesh library with the shapes found, and add shapes as nodes in the scene;
//9.  Export all animations;
//10. Export all Libraries to the Collada document;
//11. Write out the XML document;
//12. Free XML document;
//13. Exporting FBX to Collada is done.
//
bool FbxWriterCollada::Write(FbxDocument* pDocument)
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

    mTriangulate  = IOS_REF.GetBoolProp(EXP_COLLADA_TRIANGULATE,  true);
    mSingleMatrix = IOS_REF.GetBoolProp(EXP_COLLADA_SINGLEMATRIX, true);

    double lFrameRate = IOS_REF.GetDoubleProp(EXP_COLLADA_FRAME_RATE,  30.0);
    mSamplingPeriod.SetSecondDouble(1 / lFrameRate); 

    mStatus = PreprocessScene( *lScene );
    if (mStatus == false)
        return mStatus;

    // Pointer to scene
    mScene = lScene;

    // Create xml document
    xmlNode* xmlRoot = NULL;
    if (!(xmlRoot = xmlNewNode(NULL, XML_STR COLLADA_DOCUMENT_STRUCTURE)))
    {
        mStatus = false;
        return mStatus;
    }

    // Setup header 
    xmlNewProp(xmlRoot, XML_STR "xmlns", XML_STR COLLADA_SCHEMA);
    xmlNewProp(xmlRoot, XML_STR "version", XML_STR COLLADA_VERSION);

    FbxDocumentInfo *lSceneInfo = lScene->GetSceneInfo();

    char lPrevious_Locale_LCNUMERIC[100];
    memset(lPrevious_Locale_LCNUMERIC, 0, 100);
    FBXSDK_strcpy(lPrevious_Locale_LCNUMERIC, 100, setlocale(LC_NUMERIC, 0)); // query current setting for LC_NUMERIC
    setlocale(LC_NUMERIC, "C");  // set locale using period as decimal separator

    xmlNode * lAssetNode = ExportAsset(xmlRoot, lSceneInfo);
    mStatus = lAssetNode != NULL;

    if (mStatus)
    {
        xmlNode * lSceneElement = ExportScene(lScene);
        if (lSceneElement)
            xmlAddChild(xmlRoot, lSceneElement);
        else
            mStatus = false;
    }

    if (mStatus)
    {
        mStatus = ExportAnimation(lScene->GetRootNode());
    }

    if (mStatus)
    {
        mStatus = ExportLibraries(lAssetNode);
    }

    if (mStatus)
    {
        mXmlDoc = xmlNewDoc(XML_STR "1.0");
        mStatus = mXmlDoc != NULL;
        if (mXmlDoc)
        {
            xmlDocSetRootElement(mXmlDoc, xmlRoot);
            xmlSaveFormatFileEnc(mFileName.Buffer(), mXmlDoc, "utf-8", 1);
        }
    }

    if (mXmlDoc)
    {
        xmlFreeDoc(mXmlDoc);
        mXmlDoc = NULL;
    }

    // set numeric locale back
    setlocale(LC_NUMERIC, lPrevious_Locale_LCNUMERIC);

    return true;
} // Write

xmlNode* FbxWriterCollada::ExportAsset(xmlNode* pXmlNode, FbxDocumentInfo* pSceneInfo)
{
    xmlNode *lAssetElement = DAE_AddChildElement(pXmlNode, COLLADA_ASSET_STRUCTURE);
    if (!lAssetElement)
        return lAssetElement;

    xmlNode *lContributorElement = DAE_AddChildElement(lAssetElement, COLLADA_CONTRIBUTOR_ASSET_ELEMENT);
    if (pSceneInfo && lContributorElement)
    {
        DAE_AddChildElement(lContributorElement, COLLADA_AUTHOR_STRUCTURE, pSceneInfo->mAuthor);
        DAE_AddChildElement(lContributorElement, COLLADA_AUTHORING_TOOL_STRUCTURE, "FBX COLLADA exporter");
        DAE_AddChildElement(lContributorElement, COLLADA_COMMENTS_STRUCTURE, pSceneInfo->mComment);
    }

    // created and modified at the same time
    // modified is mandatory.
    // Example of the ISO 8601 format: 2005-11-09T21:51:09Z
    // Get current UTC time
    time_t currentTime;
    time(&currentTime);
    struct tm *utcTime = NULL;
    FBXSDK_gmtime(utcTime, &currentTime);    

    // generate modified string
    char lLocalTime[32];
    FBXSDK_sprintf(lLocalTime, 32, "%d-%02d-%02dT%02d:%02d:%02dZ", 
        utcTime->tm_year + 1900, utcTime->tm_mon + 1, utcTime->tm_mday, 
        utcTime->tm_hour, utcTime->tm_min, utcTime->tm_sec);

    DAE_AddChildElement(lAssetElement, COLLADA_CREATED_STRUCTURE, lLocalTime);
    if (pSceneInfo)
        DAE_AddChildElement(lAssetElement, COLLADA_KEYWORDS_STRUCTURE, pSceneInfo->mKeywords);
    DAE_AddChildElement(lAssetElement, COLLADA_MODIFIED_STRUCTURE, lLocalTime);
    if (pSceneInfo)
    {
        DAE_AddChildElement(lAssetElement, COLLADA_REVISION_STRUCTURE, pSceneInfo->mRevision);
        DAE_AddChildElement(lAssetElement, COLLADA_SUBJECT_STRUCTURE, pSceneInfo->mSubject);
        DAE_AddChildElement(lAssetElement, COLLADA_TITLE_STRUCTURE, pSceneInfo->mTitle);
    }

    //unit - <meter> and <name>.
    // In FBX we always work in centimeters.
    double lValue = mScene->GetGlobalSettings().GetSystemUnit().GetScaleFactor() / 100.;
    FbxString lConversionFactorStr( lValue );

    xmlNode * lUnitElement = DAE_AddChildElement(lAssetElement, COLLADA_UNIT_STRUCTURE);
    if (lUnitElement)
    {
        DAE_AddAttribute(lUnitElement, COLLADA_METER_PROPERTY, lConversionFactorStr);
        DAE_AddAttribute(lUnitElement, COLLADA_NAME_PROPERTY, "centimeter");
    }

    // Up axis
    FbxAxisSystem lAxisSystem = mScene->GetGlobalSettings().GetAxisSystem();
    FbxString lUpVectorStr( COLLADA_Y_UP );
    int lUpAxisSign;
    switch(lAxisSystem.GetUpVector(lUpAxisSign))
    {
    case FbxAxisSystem::eXAxis:
        lUpVectorStr = COLLADA_X_UP;
        break;
    case FbxAxisSystem::eYAxis:
        lUpVectorStr = COLLADA_Y_UP;
        break;
    case FbxAxisSystem::eZAxis:
        lUpVectorStr = COLLADA_Z_UP;
        break;
    default:
        {
            FbxString msg = FbxString("Invalid up-axis: default Y up is used");
            AddNotificationWarning( msg );
        }
        break;
    }
    
    if ( lUpAxisSign < 0 )
    {
        FbxString msg = FbxString("Invalid direction for up-axis: exporter should convert scene.");
        AddNotificationWarning( msg );
    }

    if ( lAxisSystem.GetCoorSystem() != FbxAxisSystem::eRightHanded )
    {
        FbxString msg = FbxString("Axis system is Left Handed: exporter should convert scene.");
        AddNotificationWarning( msg );
    }

    DAE_AddChildElement(lAssetElement, COLLADA_UP_AXIS_STRUCTURE, lUpVectorStr);

    return lAssetElement;
}

xmlNode * FbxWriterCollada::ExportScene(FbxScene* pScene)
{
    XmlNodePtr lVisualSceneElement(DAE_NewElement(COLLADA_VSCENE_ELEMENT));
    const char * lSceneName = pScene->GetName();
    DAE_AddAttribute(lVisualSceneElement, COLLADA_ID_PROPERTY, lSceneName);
    DAE_AddAttribute(lVisualSceneElement, COLLADA_NAME_PROPERTY, lSceneName);

    // Create the scene node
    const FbxNode* lRootNode = pScene->GetRootNode();
    const int lChildCount = lRootNode->GetChildCount();
    for (int i = 0; i < lChildCount; i++)
        ExportNodeRecursive(lVisualSceneElement, lRootNode->GetChild(i));

    UpdateMeshLibraryWithShapes(lVisualSceneElement);

    // Add extra element containing visual_scene MAX3D & FCOLLADA extensions
    xmlNode * lExtraElement = DAE_AddChildElement(lVisualSceneElement, COLLADA_EXTRA_STRUCTURE);
    ExportVisualSceneMAX3DExtension(lExtraElement, pScene);
    ExportVisualSceneFCOLLADAExtension(lExtraElement, pScene);

    XmlNodePtr lSceneElement(DAE_NewElement(COLLADA_SCENE_STRUCTURE));
    xmlNode * lInstanceVisualSceneElement = DAE_AddChildElement(lSceneElement, COLLADA_INSTANCE_VSCENE_ELEMENT);
    const FbxString lUrlStr = FbxString("#") + lSceneName;
    DAE_AddAttribute(lInstanceVisualSceneElement, COLLADA_URL_PROPERTY, lUrlStr.Buffer());

    // Export ambient light
    ExportSceneAmbient(lVisualSceneElement);

    // add the node to the camera library. The libraries will be added to the document later.
    if (!mLibraryVisualScene) 
    {
        // If the visual scene library doesn't exist yet, create it.
        mLibraryVisualScene = DAE_NewElement(COLLADA_LIBRARY_VSCENE_ELEMENT);
    }
    xmlAddChild(mLibraryVisualScene, lVisualSceneElement.Release());

    return lSceneElement.Release();
}

bool FbxWriterCollada::ExportLibraries(xmlNode* pXmlNode) {
    // Hypothesis: the pXmlNode given as argument is the <asset> node.
    // Libraries are the siblings just after this node.

    if (mLibraryImage) {
        pXmlNode = xmlAddNextSibling(pXmlNode, mLibraryImage);
    }
    if (mLibraryTexture) {
        pXmlNode = xmlAddNextSibling(pXmlNode, mLibraryTexture);
    }
    if (mLibraryMaterial) {
        pXmlNode = xmlAddNextSibling(pXmlNode, mLibraryMaterial);
    }
    if (mLibraryEffect) {
        pXmlNode = xmlAddNextSibling(pXmlNode, mLibraryEffect);
    }
    if (mLibraryGeometry) {
        pXmlNode = xmlAddNextSibling(pXmlNode, mLibraryGeometry);
    }
    if (mLibraryController) {
        pXmlNode = xmlAddNextSibling(pXmlNode, mLibraryController);
    }
    if (mLibraryAnimation) {
        pXmlNode = xmlAddNextSibling(pXmlNode, mLibraryAnimation);
    }
    if (mLibraryLight) {
        pXmlNode = xmlAddNextSibling(pXmlNode, mLibraryLight);
    }
    if (mLibraryCamera) {
        pXmlNode = xmlAddNextSibling(pXmlNode, mLibraryCamera);
    }
    if (mLibraryVisualScene) {
        pXmlNode = xmlAddNextSibling(pXmlNode, mLibraryVisualScene);
    }

    return true;
} // ExportLibraries


xmlNode* FbxWriterCollada::ExportNodeRecursive(xmlNode* pXmlNode, const FbxNode* pNode) {
    int i;

    xmlNode *lXmlNode = ExportNode(pXmlNode, pNode);
    if (!lXmlNode) {
        FbxString msg = FbxString("Could not export node ") + pNode->GetName();
        AddNotificationError( msg );
        return NULL;
    }

    for (i = 0; i < pNode->GetChildCount(); i++) {
        xmlNode *lXmlChildNode = ExportNodeRecursive(lXmlNode, pNode->GetChild(i));
    }

    return lXmlNode;
} // ExportNodeRecursive

void FbxWriterCollada::ExportVisualSceneMAX3DExtension(xmlNode * pExtraElement, FbxScene * pScene)
{
    const FbxTime::EMode lTimeMode = pScene->GetGlobalSettings().GetTimeMode();
    double lTimeModeDouble;
    if (lTimeMode == FbxTime::eCustom)
    {
        lTimeModeDouble = pScene->GetGlobalSettings().GetCustomFrameRate();
    }
    else
    {
        lTimeModeDouble = FbxTime::GetFrameRate(lTimeMode);
    }
    
    xmlNode * lTechniqueElement = DAE_AddChildElement(pExtraElement, COLLADA_TECHNIQUE_STRUCTURE);
    DAE_AddAttribute(lTechniqueElement, COLLADA_PROFILE_PROPERTY, COLLADA_MAX3D_PROFILE);
    DAE_AddChildElement(lTechniqueElement, COLLADA_MAX3D_FRAMERATE_ELEMENT, lTimeModeDouble);
}

void FbxWriterCollada::ExportVisualSceneFCOLLADAExtension(xmlNode * pExtraElement, FbxScene * pScene)
{
    FbxTimeSpan lTimeSpan;
    pScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeSpan);

    xmlNode * lTechniqueElement = DAE_AddChildElement(pExtraElement, COLLADA_TECHNIQUE_STRUCTURE);
    DAE_AddAttribute(lTechniqueElement, COLLADA_PROFILE_PROPERTY, COLLADA_FCOLLADA_PROFILE);
    DAE_AddChildElement(lTechniqueElement, COLLADA_FCOLLADA_STARTTIME_ELEMENT, lTimeSpan.GetStart().GetSecondDouble());
    DAE_AddChildElement(lTechniqueElement, COLLADA_FCOLLADA_ENDTIME_ELEMENT, lTimeSpan.GetStop().GetSecondDouble());
}

xmlNode* FbxWriterCollada::ExportNode(xmlNode* pXmlNode, const FbxNode* pNode)
{
    xmlNode *lXmlNode = DAE_AddChildElement(pXmlNode, COLLADA_NODE_STRUCTURE);
    if (!lXmlNode) return NULL;

    FbxString lNameWithoutSpacePrefix = pNode->GetNameWithoutNameSpacePrefix();
    DAE_AddAttribute(lXmlNode, COLLADA_NAME_PROPERTY, lNameWithoutSpacePrefix);
    FbxProperty lIDProperty = pNode->FindProperty(COLLADA_ID_PROPERTY_NAME);
    if (lIDProperty.IsValid())
    {
        FbxString lID = lIDProperty.Get<FbxString>();
        DAE_AddAttribute(lXmlNode, COLLADA_ID_PROPERTY, lID);
    }
    else
    {
        DAE_AddAttribute(lXmlNode, COLLADA_ID_PROPERTY, lNameWithoutSpacePrefix);
    }

    if (pNode->GetDstObjectCount<FbxDisplayLayer>())
    {
        FbxDisplayLayer * lLayer = pNode->GetDstObject<FbxDisplayLayer>();
        DAE_AddAttribute(lXmlNode, COLLADA_LAYER_PROPERTY, lLayer->GetName());
    }

    // Because FBX COLLADA writer use Name_array for controllers, the sid of nodes should be set.
    DAE_AddAttribute(lXmlNode, COLLADA_SUBID_PROPERTY, lNameWithoutSpacePrefix);

    mStatus = ExportTransform(lXmlNode, pNode);
    if (!mStatus) return lXmlNode;

    xmlNode *lXmlPivotNode = lXmlNode;
    FbxVector4 lPivotPos = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
    FbxVector4 lPivotRot = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
    FbxVector4 lPivotScale = pNode->GetGeometricScaling(FbxNode::eSourcePivot);
    bool lScaleNotId = NotValue( lPivotScale, 1 );
    if ( NotZero( lPivotPos )
        || NotZero( lPivotRot[0] ) || NotZero( lPivotRot[1] ) || NotZero( lPivotRot[2] )
        || lScaleNotId )
    {
        lXmlPivotNode = xmlNewChild(lXmlNode, NULL, XML_STR COLLADA_NODE_STRUCTURE, XML_STR "");
        if (!lXmlPivotNode) return lXmlNode;

        FbxString lPivotName = pNode->GetNameWithoutNameSpacePrefix() + FbxString("-Pivot");
        xmlNewProp(lXmlPivotNode, XML_STR COLLADA_ID_PROPERTY, XML_STR lPivotName.Buffer());
        xmlNewProp(lXmlPivotNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR lPivotName.Buffer());

        FbxString lTStr = FbxString(lPivotPos[0]) + FbxString(" ") + FbxString(lPivotPos[1]) + FbxString(" ") + FbxString(lPivotPos[2]);
        xmlNode *lXmlPivotNodeT = xmlNewChild(lXmlPivotNode, NULL, XML_STR COLLADA_TRANSLATE_STRUCTURE, XML_STR lTStr.Buffer());

        FbxQuaternion lQuat;
        lQuat.ComposeSphericalXYZ( lPivotRot );

        // Translate to axis angle
        lPivotRot.mData[3] = acos( lQuat.mData[3] );
        lPivotRot.mData[3] *= 2.0;
        lPivotRot.mData[3] *= RADIAN_TO_DEGREE;
        double lNorm = sqrt( lQuat.mData[0]*lQuat.mData[0] + lQuat.mData[1]*lQuat.mData[1] + lQuat.mData[2]*lQuat.mData[2] );
        if (fabs(lNorm) > 0.000001f)
        {
            lPivotRot.mData[0] = (float)(lQuat.mData[0] / lNorm);
            lPivotRot.mData[1] = (float)(lQuat.mData[1] / lNorm);
            lPivotRot.mData[2] = (float)(lQuat.mData[2] / lNorm);
        }
        else
        {
            lPivotRot.mData[0] = 0;
            lPivotRot.mData[1] = 0;
            lPivotRot.mData[2] = 0;
        }

        FbxString lRStr = FbxString(lPivotRot[0]) + FbxString(" ") + FbxString(lPivotRot[1]) + FbxString(" ") + FbxString(lPivotRot[2]) + FbxString(" ") + FbxString(lPivotRot[3]);
        xmlNode *lXmlPivotNodeR = xmlNewChild(lXmlPivotNode, NULL, XML_STR COLLADA_ROTATE_STRUCTURE, XML_STR lRStr.Buffer());

        if ( lScaleNotId )
        {
            FbxString lSStr = FbxString(lPivotScale[0]) + FbxString(" ") + FbxString(lPivotScale[1]) + FbxString(" ") + FbxString(lPivotScale[2]);
            xmlNode *lXmlPivotNodeS = xmlNewChild(lXmlPivotNode, NULL, XML_STR COLLADA_SCALE_STRUCTURE, XML_STR lSStr.Buffer());
        }
    }

    mStatus = ExportNodeAttribute(lXmlPivotNode, pNode);
    if (!mStatus) return lXmlNode;

    // Add visibility attribute
    xmlNode * lExtra = DAE_AddChildElement(lXmlNode, COLLADA_EXTRA_STRUCTURE);
    xmlNode * lFCOLLADATechnique = DAE_AddChildElement(lExtra, COLLADA_TECHNIQUE_STRUCTURE);
    DAE_AddAttribute(lFCOLLADATechnique, COLLADA_PROFILE_PROPERTY, COLLADA_FCOLLADA_PROFILE);
    DAE_AddChildElement(lFCOLLADATechnique, COLLADA_FCOLLADA_VISIBILITY_ELEMENT, pNode->Visibility.Get());

    // If this node has a target (usually when this node contains camera or light)
    // save the url of the target node in FBX extension
    if (pNode->GetTarget())
    {
        xmlNode * lFBXTechnique = DAE_AddChildElement(lExtra, COLLADA_TECHNIQUE_STRUCTURE);
        DAE_AddAttribute(lFBXTechnique, COLLADA_PROFILE_PROPERTY, COLLADA_FBX_PROFILE);
        DAE_AddChildElement(lFBXTechnique, COLLADA_FBX_TARGET_ELEMENT, FbxString("#") + pNode->GetTarget()->GetName());
    }

    return lXmlNode;
} // ExportNode


bool FbxWriterCollada::ExportTransform(xmlNode* pXmlNode, const FbxNode* pNode) {
    int i;
    // Export the node's default transforms

    // If the mesh is a skin binded to a skeleton, the bind pose will include its transformations.
    // In that case, do not export the transforms twice.
    if (pNode->GetNodeAttribute()
        && pNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh)
    {
        int lDeformerCount = ((FbxMesh*)pNode->GetNodeAttribute())->GetDeformerCount(FbxDeformer::eSkin);
        if(lDeformerCount > 1)
            FBX_ASSERT_NOW("Unexpected number of skin greater than 1");
        int lClusterCount=0;
        //it is expected for lDeformerCount to be equal to 1
        for(i=0; i<lDeformerCount; ++i)
            lClusterCount+=((FbxSkin*)((FbxMesh*)pNode->GetNodeAttribute())->GetDeformer(i, FbxDeformer::eSkin))->GetClusterCount();
        if(lClusterCount)    
            return true;
    }

    if ( mSingleMatrix )
    {
        FbxAMatrix lIdentity;
        lIdentity.SetIdentity();

        FbxAMatrix lThisLocal;
        //For Single Matrix situation, obtain transfrom matrix from eDestinationPivot, which include pivot offsets and pre/post rotations.
        FbxAMatrix& lThisGlobal = const_cast<FbxNode*>(pNode)->EvaluateGlobalTransform(FBXSDK_TIME_ZERO, FbxNode::eDestinationPivot);
        const FbxNode* lParentNode = pNode->GetParent();
        if( lParentNode )
        {
            //For Single Matrix situation, obtain transfrom matrix from eDestinationPivot, which include pivot offsets and pre/post rotations.
            FbxAMatrix & lParentGlobal = const_cast<FbxNode*>(lParentNode)->EvaluateGlobalTransform(FBXSDK_TIME_ZERO, FbxNode::eDestinationPivot);
            FbxAMatrix lParentInverted = lParentGlobal.Inverse();
            lThisLocal = lParentInverted * lThisGlobal;
        }
        else
        {
            lThisLocal = lThisGlobal;
        }

        //COLLADA_MATRIX_STRUCTURE
        FbxString lTStr = FbxString(lThisLocal.mData[0][0]) + FbxString(" ") + FbxString(lThisLocal.mData[1][0]) + FbxString(" ") + FbxString(lThisLocal.mData[2][0]) + FbxString(" ") + FbxString(lThisLocal.mData[3][0])
            + FbxString(" ") + FbxString(lThisLocal.mData[0][1]) + FbxString(" ") + FbxString(lThisLocal.mData[1][1]) + FbxString(" ") + FbxString(lThisLocal.mData[2][1]) + FbxString(" ") + FbxString(lThisLocal.mData[3][1])
            + FbxString(" ") + FbxString(lThisLocal.mData[0][2]) + FbxString(" ") + FbxString(lThisLocal.mData[1][2]) + FbxString(" ") + FbxString(lThisLocal.mData[2][2]) + FbxString(" ") + FbxString(lThisLocal.mData[3][2])
            + FbxString(" ") + FbxString(lThisLocal.mData[0][3]) + FbxString(" ") + FbxString(lThisLocal.mData[1][3]) + FbxString(" ") + FbxString(lThisLocal.mData[2][3]) + FbxString(" ") + FbxString(lThisLocal.mData[3][3]);
        xmlNode *lXmlNodeMat = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_MATRIX_STRUCTURE, XML_STR lTStr.Buffer());
        xmlNewProp(lXmlNodeMat, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_MATRIX_STRUCTURE);
        return true;
    }

    FbxVector4 lT, lR, lS, lRP, lSP, lRPo, lSPo, lPreR, lPostR;

    // Local translation, rotation and scaling
    lT = pNode->LclTranslation.Get();
    lR = pNode->LclRotation.Get();
    lS = pNode->LclScaling.Get();
    
    // If the node has pivots, set them as translations.
    lRP = pNode->GetRotationPivot(FbxNode::eSourcePivot);
    lSP = pNode->GetScalingPivot(FbxNode::eSourcePivot);
    lRPo = pNode->GetRotationOffset(FbxNode::eSourcePivot);
    lSPo = pNode->GetScalingOffset(FbxNode::eSourcePivot);

    // Pre- and post- rotations
    if (!pNode->GetUseRotationSpaceForLimitOnly(FbxNode::eSourcePivot)) {
        lPreR = pNode->GetPreRotation(FbxNode::eSourcePivot);
        lPostR = pNode->GetPostRotation(FbxNode::eSourcePivot);
    }
    
    // translation
    if (NotZero(lT) || IsTranslationAnimated(pNode)) {
        FbxString lTStr = FbxString(lT[0]) + FbxString(" ") + FbxString(lT[1]) + FbxString(" ") + FbxString(lT[2]);
        xmlNode *lXmlNodeT = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_TRANSLATE_STRUCTURE, XML_STR lTStr.Buffer());
        xmlNewProp(lXmlNodeT, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_TRANSLATE_STRUCTURE);
    }
    // rotation pivot offset
    if (NotZero(lRPo)) {
        FbxString lRPoStr = FbxString(lRPo[0]) + FbxString(" ") + FbxString(lRPo[1]) + FbxString(" ") + FbxString(lRPo[2]);
        xmlNode *lXmlNodeRPo = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_TRANSLATE_STRUCTURE, XML_STR lRPoStr.Buffer());
        xmlNewProp(lXmlNodeRPo, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_ROTATE_PIVOT_OFFSET);
    }
    // rotation pivot
    if (NotZero(lRP)) {
        FbxString lRPStr = FbxString(lRP[0]) + FbxString(" ") + FbxString(lRP[1]) + FbxString(" ") + FbxString(lRP[2]);
        xmlNode *lXmlNodeRP = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_TRANSLATE_STRUCTURE, XML_STR lRPStr.Buffer());
        xmlNewProp(lXmlNodeRP, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_ROTATE_PIVOT);
    }
    // pre rotation in Z
    if (NotZero(lPreR[2])) {
        FbxString lPreRStr = FbxString("0 0 1 ") + FbxString(lPreR[2]);
        xmlNode *lXmlNodePreR = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_ROTATE_STRUCTURE, XML_STR lPreRStr.Buffer());
        xmlNewProp(lXmlNodePreR, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_PRE_ROTATION_Z);
    }
    // pre rotation in Y
    if (NotZero(lPreR[1])) {
        FbxString lPreRStr = FbxString("0 1 0 ") + FbxString(lPreR[1]);
        xmlNode *lXmlNodePreR = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_ROTATE_STRUCTURE, XML_STR lPreRStr.Buffer());
        xmlNewProp(lXmlNodePreR, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_PRE_ROTATION_Y);
    }
    // pre rotation in X
    if (NotZero(lPreR[0])) {
        FbxString lPreRStr = FbxString("1 0 0 ") + FbxString(lPreR[0]);
        xmlNode *lXmlNodePreR = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_ROTATE_STRUCTURE, XML_STR lPreRStr.Buffer());
        xmlNewProp(lXmlNodePreR, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_PRE_ROTATION_X);
    }

    EFbxRotationOrder    lRO;
    xmlNode*        lXmlNodeR;
    FbxString            lAxisStr[3];
    lAxisStr[0] = FbxString("1 0 0 ");
    lAxisStr[1] = FbxString("0 1 0 ");
    lAxisStr[2] = FbxString("0 0 1 ");
    FbxString lRStr;

    pNode->GetRotationOrder(FbxNode::eSourcePivot, lRO);

    int lIndex[3] = {0, 1, 2 };

    switch (lRO) {
        case eEulerXZY:

            // rotation Y-Z-X
            lIndex[0] = 1;
            lIndex[1] = 2;
            lIndex[2] = 0;

            break;

        case eEulerYZX:

            // rotation in X-Z-Y
            lIndex[0] = 0;
            lIndex[1] = 2;
            lIndex[2] = 1;

            break;

        case eEulerYXZ:
            
            // rotation in Z-X-Y
            lIndex[0] = 2;
            lIndex[1] = 0;
            lIndex[2] = 1;

            break;

        case eEulerZXY:
            
            // rotation in Y-X-Z
            lIndex[0] = 1;
            lIndex[1] = 0;
            lIndex[2] = 2;

            break;

        case eEulerZYX:

            // rotation in X-Y-Z
            lIndex[0] = 0;
            lIndex[1] = 1;
            lIndex[2] = 2;

            break;

        case eEulerXYZ:
        default:

            // rotation in Z-Y-X
            lIndex[0] = 2;
            lIndex[1] = 1;
            lIndex[2] = 0;

            break;
    }

    const char * rotationAxis[] = {
        COLLADA_ROTATE_X,
        COLLADA_ROTATE_Y,
        COLLADA_ROTATE_Z
    };
    for ( i = 0; i < 3; ++i )
    {
        if ( NotZero(lR[lIndex[i]]) || IsRotationAnimated( pNode, i ) || lRO != eEulerXYZ )
        {
            lRStr = lAxisStr[lIndex[i]] + FbxString(lR[lIndex[i]]);
            lXmlNodeR = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_ROTATE_STRUCTURE, XML_STR lRStr.Buffer());
            xmlNewProp(lXmlNodeR, XML_STR COLLADA_SUBID_PROPERTY, XML_STR rotationAxis[lIndex[i]]);
        }
    }
    // post rotation in Z
    if (NotZero(lPostR[2])) {
        FbxString lPostRStr = FbxString("0 0 1 ") + FbxString(lPostR[2]);
        xmlNode *lXmlNodePostR = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_ROTATE_STRUCTURE, XML_STR lPostRStr.Buffer());
        xmlNewProp(lXmlNodePostR, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_POST_ROTATION_Z);
    }
    // post rotation in Y
    if (NotZero(lPostR[1])) {
        FbxString lPostRStr = FbxString("0 1 0 ") + FbxString(lPostR[1]);
        xmlNode *lXmlNodePostR = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_ROTATE_STRUCTURE, XML_STR lPostRStr.Buffer());
        xmlNewProp(lXmlNodePostR, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_POST_ROTATION_Y);
    }
    // post rotation in X
    if (NotZero(lPostR[0])) {
        FbxString lPostRStr = FbxString("1 0 0 ") + FbxString(lPostR[0]);
        xmlNode *lXmlNodePostR = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_ROTATE_STRUCTURE, XML_STR lPostRStr.Buffer());
        xmlNewProp(lXmlNodePostR, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_POST_ROTATION_X);
    }
    // rotation pivot inverse
    if (NotZero(lRP)) {
        FbxString lRPStr = FbxString(-lRP[0]) + FbxString(" ") + FbxString(-lRP[1]) + FbxString(" ") + FbxString(-lRP[2]);
        xmlNode *lXmlNodeRP = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_TRANSLATE_STRUCTURE, XML_STR lRPStr.Buffer());
        xmlNewProp(lXmlNodeRP, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_ROTATE_PIVOT_INVERSE);
    }
    // scaling pivot offset
    if (NotZero(lSPo)) {
        FbxString lSPoStr = FbxString(lSPo[0]) + FbxString(" ") + FbxString(lSPo[1]) + FbxString(" ") + FbxString(lSPo[2]);
        xmlNode *lXmlNodeSPo = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_TRANSLATE_STRUCTURE, XML_STR lSPoStr.Buffer());
        xmlNewProp(lXmlNodeSPo, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_SCALE_PIVOT_OFFSET);
    }
    // scaling pivot
    if (NotZero(lSP)) {
        FbxString lSPStr = FbxString(lSP[0]) + FbxString(" ") + FbxString(lSP[1]) + FbxString(" ") + FbxString(lSP[2]);
        xmlNode *lXmlNodeSP = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_TRANSLATE_STRUCTURE, XML_STR lSPStr.Buffer());
        xmlNewProp(lXmlNodeSP, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_SCALE_PIVOT);
    }

    // scaling
	if (NotValue(lS,1) || IsScaleAnimated(pNode)) {
        FbxString lSStr = FbxString(lS[0]) + FbxString(" ") + FbxString(lS[1]) + FbxString(" ") + FbxString(lS[2]);
        xmlNode *lXmlNodeS = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_SCALE_STRUCTURE, XML_STR lSStr.Buffer());
        xmlNewProp(lXmlNodeS, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_SCALE_STRUCTURE);
    }

    // scaling pivot inverse
    if (NotZero(lSP)) {
        FbxString lSPStr = FbxString(-lSP[0]) + FbxString(" ") + FbxString(-lSP[1]) + FbxString(" ") + FbxString(-lSP[2]);
        xmlNode *lXmlNodeSP = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_TRANSLATE_STRUCTURE, XML_STR lSPStr.Buffer());
        xmlNewProp(lXmlNodeSP, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_SCALE_PIVOT_INVERSE);
    }
        

    return true;
} // ExportTransform


bool FbxWriterCollada::ExportNodeAttribute(xmlNode* pXmlNode, const FbxNode* pNode)
{
    int i;
    const FbxNodeAttribute *lNodeAttribute = pNode->GetNodeAttribute();

    if (lNodeAttribute) {
        FbxNodeAttribute::EType lNodeAttributeType = lNodeAttribute->GetAttributeType();
        if (lNodeAttributeType == FbxNodeAttribute::eNull) {
            // Nothing to do. In COLLADA there is no NULL as such.
            FbxString msg = FbxString("'Null' type not supported by COLLADA. ");
            msg += FbxString("Only the transform data of node: '") + pNode->GetName() + "' will be saved.";
            AddNotificationWarning( msg );

        } else if (lNodeAttributeType == FbxNodeAttribute::eMarker) {
            // Not implemented in COLLADA.
            FbxString msg = FbxString("'Marker' type not supported by COLLADA. ");
            msg += FbxString("Only the transform data of node: '") + pNode->GetName() + "' will be saved.";
            AddNotificationWarning( msg );

        } else if (lNodeAttributeType == FbxNodeAttribute::eLight) {
            // Create a light library for every light in the FBX scene.
            xmlNode* lLightLibraryNode = CreateLightLibrary(pNode);
            if (!lLightLibraryNode) return false;

            // Add the instance refering to the light library to the node.
            xmlNode *lXmlInstance = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_INSTANCE_LIGHT_ELEMENT, NULL);          
            FbxString lUrlStr = FbxString("#") + DAE_GetElementAttributeValue(lLightLibraryNode, COLLADA_ID_PROPERTY);
            xmlNewProp(lXmlInstance, XML_STR COLLADA_URL_PROPERTY, XML_STR lUrlStr.Buffer());

        } else if (lNodeAttributeType == FbxNodeAttribute::eCamera) {
            // Create a camera library for every camera in the FBX scene.
            xmlNode* lCamLibraryNode = CreateCameraLibrary(pNode);
            if (!lCamLibraryNode) return false;

            // Add the instance refering to the camera library to the node.
            xmlNode *lXmlInstance = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_INSTANCE_CAMERA_ELEMENT, NULL);
            xmlAddChild(pXmlNode, lXmlInstance);
            FbxString lUrlStr = FbxString("#") + DAE_GetElementAttributeValue(lCamLibraryNode, COLLADA_ID_PROPERTY);
            xmlNewProp(lXmlInstance, XML_STR COLLADA_URL_PROPERTY, XML_STR lUrlStr.Buffer());

        } else if (lNodeAttributeType == FbxNodeAttribute::eCameraSwitcher) {
            // Not implemented in COLLADA.
            FbxString msg = FbxString("'CameraSwitcher' type not supported by COLLADA. ");
            msg += FbxString("Only the transform data of node: '") + pNode->GetName() + "' will be saved.";
            AddNotificationWarning( msg );

        } else if (lNodeAttributeType == FbxNodeAttribute::eSkeleton) {
            // The only difference between a node containing a NULL and one containing a SKELETON
            // is the property type JOINT. Add it.
            xmlNewProp(pXmlNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR COLLADA_JOINT_NODE_TYPE);

        } else if (lNodeAttributeType == FbxNodeAttribute::eMesh
            || lNodeAttributeType == FbxNodeAttribute::eNurbs
            || lNodeAttributeType == FbxNodeAttribute::ePatch)
        {
            // triangulate if needed
            bool lTriangulate = false;
            if (lNodeAttributeType == FbxNodeAttribute::eMesh )
            {
                if (mTriangulate == true)
                    lTriangulate = true;
            } else
            {
                // Nurbs and patches Not implemented in COLLADA: triangulate
                FbxString msg = FbxString("Nurbs and Patches not supported by COLLADA. ");
                msg += FbxString("Node '") + pNode->GetName() + "' will be triangulated.";
                AddNotificationWarning( msg );
            }

            if ( lTriangulate )
            {
                FbxGeometryConverter lConverter(&mManager);
                lNodeAttribute = lConverter.Triangulate(const_cast<FbxNodeAttribute*>(pNode->GetNodeAttribute()), true);
            }

            // Create a mesh library for every mesh in the FBX scene.
            xmlNode* lMeshLibraryNode = CreateMeshLibrary(pNode);
            if (!lMeshLibraryNode) return false;
            
            // If mesh has links, or shapes, it is defined by a controller.
            // Else, it is defined by a geometry.
            FbxMesh *lMesh = (FbxMesh*)lNodeAttribute;
            xmlNode *lXmlInstance;

            int lDeformerCount = lMesh->GetDeformerCount(FbxDeformer::eSkin);
            if(lDeformerCount > 1)
                FBX_ASSERT_NOW("Unexpected number of skin greater than 1");
            int lClusterCount=0;
            //it is expected for lDeformerCount to be equal to 1
            for(i=0; i<lDeformerCount; ++i)
                lClusterCount+=((FbxSkin*)lMesh->GetDeformer(i, FbxDeformer::eSkin))->GetClusterCount();

            if (lClusterCount || lMesh->GetShapeCount()) {
                // Add the instance refering to the mesh library to the node.
                lXmlInstance = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_INSTANCE_CONTROLLER_ELEMENT, NULL);
                xmlAddChild(pXmlNode, lXmlInstance);

                FbxString lUrlStr;
                FbxString lString;
                if (lClusterCount)
                {
                    lString = pNode->GetNameWithoutNameSpacePrefix();
                    lUrlStr = FbxString("#") + FbxString(lString.Buffer()) + "Controller";
                }
                else
                {
                    lString=pNode->GetNameWithoutNameSpacePrefix();
                    lUrlStr = FbxString("#") + FbxString(lString.Buffer()) + "-lib-morph";
                }
                xmlNewProp(lXmlInstance, XML_STR COLLADA_URL_PROPERTY, XML_STR lUrlStr.Buffer());
            } else {
                // Add the instance refering to the mesh library to the node.
                lXmlInstance = xmlNewChild(pXmlNode, NULL, XML_STR COLLADA_INSTANCE_GEOMETRY_ELEMENT, NULL);
                xmlAddChild(pXmlNode, lXmlInstance);

                FbxString lUrlStr = FbxString("#") + DAE_GetElementAttributeValue(lMeshLibraryNode, COLLADA_ID_PROPERTY);
                xmlNewProp(lXmlInstance, XML_STR COLLADA_URL_PROPERTY, XML_STR lUrlStr.Buffer());
            }

            // If mesh has materials, export bind_material.
            // If mesh has textures, and if those textures do not use the
            // mesh materials, use the texture materials instead.
            // Note: for now, use only layer 0, this is bad and should be corrected
            // to use all layers.
            FbxLayerElementMaterial *lLayerElementMaterial = NULL;
            FbxLayerElementTexture *lLayerElementTexture = NULL;
            if (lMesh->GetLayer(0)) {
                lLayerElementMaterial = lMesh->GetLayer(0)->GetMaterials();
                lLayerElementTexture = lMesh->GetLayer(0)->GetTextures(FbxLayerElement::eTextureDiffuse);
            }
            int lMaterialCount = 0;
            int lTextureCount = 0;
            if (lLayerElementMaterial) lMaterialCount = pNode->GetMaterialCount();
            if (lLayerElementTexture) lTextureCount = lLayerElementTexture->GetDirectArray().GetCount();

            if (lTextureCount || lMaterialCount) {
                xmlNode *lBindMaterial = xmlNewChild(lXmlInstance, NULL, XML_STR COLLADA_BINDMATERIAL_ELEMENT, NULL);
                xmlNode *lTechnique = xmlNewChild(lBindMaterial, NULL, XML_STR COLLADA_TECHNIQUE_COMMON_ELEMENT, NULL);
                
                if (lTextureCount) {
                    // for each texture, if it does not use the material, use its material as material.
                    for (i = 0; i < lTextureCount; i++) {
                        FbxFileTexture *lTexture = FbxCast<FbxFileTexture>(lLayerElementTexture->GetDirectArray().GetAt(i));
                        FbxString lTexName = lTexture->GetNameWithoutNameSpacePrefix();
                        if (lTexture && lTexture->GetMaterialUse() == FbxFileTexture::eDefaultMaterial) { // Texture does not use Model material
                            FbxString lTexMatName = lTexName + "-" + COLLADA_DIFFUSE_MATERIAL_PARAMETER + "-Material";
                            FbxString lTexMatUrl = FbxString("#") + lTexMatName;
                            xmlNode *lXmlMaterial = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_INSTANCE_MATERIAL_ELEMENT, NULL);
                            xmlNewProp(lXmlMaterial, XML_STR COLLADA_SYMBOL_PROPERTY, XML_STR lTexMatName.Buffer());
                            xmlNewProp(lXmlMaterial, XML_STR COLLADA_TARGET_PROPERTY, XML_STR lTexMatUrl.Buffer());

                        } else {
                            // Texture use Model material
                            // Do nothing here because model material will be taken care by ExportMesh()                            
                        }
                    }
                }
                /* else */ 
                if (lMaterialCount) {
                for (i = 0; i < lMaterialCount; i++) {
                    // Use the material name.
                    FbxSurfaceMaterial* lMaterial = pNode->GetMaterial(i); 
                        FbxString lMaterialName = lMaterial->GetNameWithoutNameSpacePrefix();
                        FbxString lMaterialUrl = FbxString("#") + lMaterialName;
                        xmlNode *lXmlMaterial = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_INSTANCE_MATERIAL_ELEMENT, NULL);
                        xmlNewProp(lXmlMaterial, XML_STR COLLADA_SYMBOL_PROPERTY, XML_STR lMaterialName.Buffer());
                        xmlNewProp(lXmlMaterial, XML_STR COLLADA_TARGET_PROPERTY, XML_STR lMaterialUrl.Buffer());
                    }
                }
            }

        } else {
            FbxString msg = FbxString("Unknown type. ");
            msg += FbxString("Only the transform data of node: '") + pNode->GetName() + "' will be saved.";
            AddNotificationWarning( msg );
        }
    }

    return true;
} // ExportNodeAttribute


xmlNode* FbxWriterCollada::CreateMeshLibrary(const FbxNode* pNode)
{
    // Create the geometry node
    xmlNode* lXmlGeometryNode = xmlNewNode(NULL, XML_STR COLLADA_GEOMETRY_STRUCTURE);
    if (!lXmlGeometryNode) return NULL;
    FbxString lModelNameStr = pNode->GetNameWithoutNameSpacePrefix();
    FbxString lIdStr = lModelNameStr + "-lib";
    FbxString lNameStr = lModelNameStr + "Mesh";
    xmlNewProp(lXmlGeometryNode, XML_STR COLLADA_ID_PROPERTY, XML_STR lIdStr.Buffer());
    xmlNewProp(lXmlGeometryNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR lNameStr.Buffer());

    // Create the mesh node
    xmlNode *lXmlMesh = ExportMesh(pNode);
    if (!lXmlMesh) return NULL;
    xmlAddChild(lXmlGeometryNode, lXmlMesh);

    // add the node to the geometry library. The libraries will be added to the document later.
    if (!mLibraryGeometry) {
        // If the geometry library doesn't exist yet, create it.
        mLibraryGeometry = xmlNewNode(NULL, XML_STR COLLADA_LIBRARY_GEOMETRY_ELEMENT);
    }
    xmlAddChild(mLibraryGeometry, lXmlGeometryNode);

    return lXmlGeometryNode;
} // CreateMeshLibrary


xmlNode* FbxWriterCollada::CreateCameraLibrary(const FbxNode* pNode)
{
    XmlNodePtr lCameraElement(ExportCamera(pNode));
    if (!lCameraElement)
        return NULL;

    // add the node to the camera library. The libraries will be added to the document later.
    if (!mLibraryCamera)
    {
        // If the camera library doesn't exist yet, create it.
        mLibraryCamera = DAE_NewElement(COLLADA_LIBRARY_CAMERA_ELEMENT);
    }
    xmlAddChild(mLibraryCamera, lCameraElement);

    return lCameraElement.Release();
} // CreateCameraLibrary


xmlNode* FbxWriterCollada::CreateLightLibrary(const FbxNode* pNode) {

    // Create the light node
    xmlNode *lXmlLight = ExportLight(pNode);
    if (!lXmlLight) return NULL;

    // add the node to the light library. The libraries will be added to the document later.
    if (!mLibraryLight) {
        // If the light library doesn't exist yet, create it.
        mLibraryLight = xmlNewNode(NULL, XML_STR COLLADA_LIBRARY_LIGHT_ELEMENT);
    }
    xmlAddChild(mLibraryLight, lXmlLight);

    return lXmlLight;
} // CreateLightLibrary

void FbxWriterCollada::ExportSceneAmbient(xmlNode * pVisualSceneElement)
{
    FbxColor lGlobalAmbient( mScene->GetGlobalSettings().GetAmbientColor() );
    if ( lGlobalAmbient.mRed || lGlobalAmbient.mGreen || lGlobalAmbient.mBlue )
    {
        XmlNodePtr lLightElement(ExportLight(NULL));
        if (!lLightElement) return;

        // add the node to the light library. The libraries will be added to the document later.
        if (!mLibraryLight)
        {
            // If the light library doesn't exist yet, create it.
            mLibraryLight = DAE_NewElement(COLLADA_LIBRARY_LIGHT_ELEMENT);
        }
        xmlAddChild(mLibraryLight, lLightElement.Release());

        // Create a dummy node for ambient light in the scene
        XmlNodePtr lNodeElement(DAE_NewElement(COLLADA_NODE_STRUCTURE));
        xmlNode * lInstanceLightElement = DAE_AddChildElement(lNodeElement, COLLADA_INSTANCE_LIGHT_ELEMENT);
        const FbxString lURLStr = FbxString("#") + AMBIENT_LIGHT_NAME;
        DAE_AddAttribute(lInstanceLightElement, COLLADA_URL_PROPERTY, lURLStr.Buffer());
        xmlAddChild(pVisualSceneElement, lNodeElement.Release());
    }
} // ExportSceneAmbient


xmlNode* FbxWriterCollada::ExportMesh(const FbxNode* pNode)
{
    int i;

    xmlNode* lXmlMesh = xmlNewNode(NULL, XML_STR COLLADA_MESH_STRUCTURE);
    if (!lXmlMesh) return NULL;

    FbxMesh* lMesh = const_cast<FbxNode*>(pNode)->GetMesh();
    if (!lMesh) {
        FbxString msg = FbxString("Could not get mesh for node ") + pNode->GetName();
        AddNotificationError( msg );
        return NULL;
    }

    int lDeformerCount = lMesh->GetDeformerCount(FbxDeformer::eSkin);
    if(lDeformerCount > 1)
        FBX_ASSERT_NOW("Unexpected number of skin greater than 1");
    int lNbLink=0;
    //it is expected for lDeformerCount to be equal to 1
    for(i=0; i<lDeformerCount; ++i)
        lNbLink+=((FbxSkin*)lMesh->GetDeformer(i, FbxDeformer::eSkin))->GetClusterCount();
    
    int lNbShapes = lMesh->GetShapeCount();

    // Vertices Positions
    FbxString lNameWithoutNameSpacePrefix = pNode->GetNameWithoutNameSpacePrefix();
    xmlNode* lXmlPositions = ExportVertexPositions(lXmlMesh, lMesh, lNameWithoutNameSpacePrefix.Buffer(), true, (lNbLink == 0));
    if (!lXmlPositions) return NULL;
    xmlAddChild(lXmlMesh, lXmlPositions);

    // Ensure we covered all layer elements of layer 0.
    ExportLayerElements(lXmlMesh, lMesh, lNameWithoutNameSpacePrefix.Buffer());

    // Vertices
    xmlNode* lXmlVertices = ExportVertices(lXmlMesh, lMesh, lNameWithoutNameSpacePrefix.Buffer());
    if (!lXmlVertices) return NULL;
    xmlAddChild(lXmlMesh, lXmlVertices);

    // For each material of the mesh we have to export polygons node.
    // If the polygons have a texture, and if this texture does not use the
    // polygon material, use the texture material instead.
    // Note: for now, use only layer 0, this is bad and should be corrected
    // to use all layers.
    FbxLayerElementMaterial *lLayerElementMaterial = NULL;
    FbxLayerElementTexture *lLayerElementTexture = NULL;
    if (lMesh->GetLayer(0)) 
    {
        lLayerElementMaterial = lMesh->GetLayer(0)->GetMaterials();
        lLayerElementTexture = lMesh->GetLayer(0)->GetTextures(FbxLayerElement::eTextureDiffuse);
    }
    int lMaterialCount = 0;
    int lTextureCount = 0;
    if (lLayerElementMaterial) lMaterialCount = pNode->GetMaterialCount();
    if (lLayerElementTexture) lTextureCount = lLayerElementTexture->GetDirectArray().GetCount();

    if (lLayerElementTexture && lTextureCount > 0) 
    {
        // for each texture, if it does not use the material, use its material as material.
        for (i = 0; i < lTextureCount; i++) 
        {
            FbxFileTexture *lTexture = FbxCast<FbxFileTexture>(lLayerElementTexture->GetDirectArray().GetAt(i));
            if (lTexture->GetMaterialUse() == FbxFileTexture::eDefaultMaterial) 
            {
                FbxString lTexName = lTexture->GetNameWithoutNameSpacePrefix(); 
                FbxString lTexMatName = lTexName + "-" + COLLADA_DIFFUSE_MATERIAL_PARAMETER + "-Material";
                
                ExportPolygons(lXmlMesh, lMesh, lTexMatName, 0, lNameWithoutNameSpacePrefix.Buffer());
            }
        }
    }
    
    if (lLayerElementMaterial && lMaterialCount) 
    {
        for (i = 0; i < lMaterialCount; i++) 
        {
            // Use the material name.
            FbxSurfaceMaterial* lMaterial = pNode->GetMaterial(i);
            FbxString lMaterialName = lMaterial->GetNameWithoutNameSpacePrefix();

            ExportPolygons(lXmlMesh, lMesh, lMaterialName, i, lNameWithoutNameSpacePrefix.Buffer());
            
            //Export mesh material
            ExportMaterial(lMaterial);
        }
    } 
    else 
    {
        // No material
        ExportPolygons(lXmlMesh, lMesh, "", 0,lNameWithoutNameSpacePrefix.Buffer());
    }

    // Export textures used by the mesh
    mStatus = ExportMeshTextures(lMesh);
    if (!mStatus) return NULL;

    // Export mesh links
    if (lNbLink > 0) {
        // Create a controller
        mStatus = ExportController(lMesh);
        if (!mStatus) return NULL;
    }

    // Export mesh shapes, if any
    if (lNbShapes > 0) {
        // Create a controller
        mStatus = ExportControllerShape(lMesh);
        if (!mStatus) return NULL;
    }

    return lXmlMesh;
} // ExportMesh


xmlNode* FbxWriterCollada::ExportShapeGeometry(FbxMesh* pMeshShape, FbxString pShapeId) {
    // Create the geometry node
    xmlNode* lXmlGeometryNode = xmlNewNode(NULL, XML_STR COLLADA_GEOMETRY_STRUCTURE);
    if (!lXmlGeometryNode) return NULL;
    FbxString lModelNameStr = pShapeId;
    int lLibIndex = pShapeId.Find("-lib");
    if (lLibIndex >= 0) { lModelNameStr = pShapeId.Left(lLibIndex); }
    FbxString lNameStr = lModelNameStr + "Mesh";
    xmlNewProp(lXmlGeometryNode, XML_STR COLLADA_ID_PROPERTY, XML_STR pShapeId.Buffer());
    xmlNewProp(lXmlGeometryNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR lNameStr.Buffer());

    xmlNode* lXmlMesh = xmlNewChild(lXmlGeometryNode, NULL, XML_STR COLLADA_MESH_STRUCTURE, NULL);
    if (!lXmlMesh) return NULL;

    // Vertices Positions
    xmlNode* lXmlPositions = ExportVertexPositions(lXmlMesh, pMeshShape, lModelNameStr, true, true);
    if (!lXmlPositions) return NULL;
    xmlAddChild(lXmlMesh, lXmlPositions);

    // Ensure we covered all layer elements of layer 0.
    ExportLayerElements(lXmlMesh, pMeshShape, lModelNameStr);

    // Vertices
    xmlNode* lXmlVertices = ExportVertices(lXmlMesh, pMeshShape, lModelNameStr);
    if (!lXmlVertices) return NULL;
    xmlAddChild(lXmlMesh, lXmlVertices);

    // Export no material for shapes
    ExportPolygons(lXmlMesh, pMeshShape, "", 0, lModelNameStr, true);

    return lXmlGeometryNode;
} // ExportShapeGeometry


xmlNode* FbxWriterCollada::ExportVertexPositions(xmlNode* pXmlNode, FbxMesh* pMesh, FbxString pMeshName, bool pInGeometry, bool pExportControlPoints) {
    // In an ordinary geometry, export the control points.
    // In a binded geometry, export transformed control points...
    // In a controller, export the control points.
    int i, j, lIndexLink;

    pExportControlPoints = true;

    FbxString lStr = pMeshName;
    if (pInGeometry) {
        lStr += POSITION_POSTFIX; // Normal geometry case
    } else {
        lStr += "-BindPos"; // inside a controller
    }

    FbxArray<FbxVector4> lControlPoints;
    // Get Control points.
    // Translate a FbxVector4* into FbxArray<FbxVector4>
    FbxVector4* lTemp = pMesh->GetControlPoints();
    int lNbControlPoints = pMesh->GetControlPointsCount();
    for (i = 0; i < lNbControlPoints; i++) {
        lControlPoints.Add(lTemp[i]);
    }

    FbxArray<FbxVector4> lPositions;

    if (pExportControlPoints) {
        lPositions = lControlPoints;
        if (!pInGeometry) {
            FbxAMatrix lTransform = pMesh->GetNode()->EvaluateGlobalTransform();
            for (i = 0; i < lNbControlPoints; i++) {
                lPositions[i] = lTransform.MultT(lPositions[i]);
            }
        }

    } else {
        // Initialize positions
        lPositions.Resize(lNbControlPoints);

        // Get the transformed control points.
            
        int lDeformerCount = pMesh->GetDeformerCount(FbxDeformer::eSkin);
        if(lDeformerCount > 1)
            FBX_ASSERT_NOW("Unexpected number of skin greater than 1");
        //int lNbLink=0;
        //it is expected for lDeformerCount to be equal to 1
        for(i=0; i<lDeformerCount; ++i)
        {
            for (lIndexLink = 0; lIndexLink < ((FbxSkin*)pMesh->GetDeformer(i, FbxDeformer::eSkin))->GetClusterCount(); lIndexLink++) 
            {
                FbxCluster* lLink = ((FbxSkin*)pMesh->GetDeformer(i, FbxDeformer::eSkin))->GetCluster(lIndexLink);
                
            FbxAMatrix lJointPosition = lLink->GetLink()->EvaluateGlobalTransform();
            FbxAMatrix lTransformLink;
            lLink->GetTransformLinkMatrix(lTransformLink);
            FbxAMatrix lM = lTransformLink.Inverse() * lJointPosition;

                for (j = 0; j < lLink->GetControlPointIndicesCount(); j++) 
                {
                int lIndex = lLink->GetControlPointIndices()[j];
                FbxVector4 lControlPoint = lControlPoints[lIndex];
                double lWeight = lLink->GetControlPointWeights()[j];
                FbxVector4 lPos = lM.MultT(lControlPoint);
                lPos = lPos * lWeight;
                lPositions[lIndex] = lPositions[lIndex] + lPos;
            }
            }//lIndex For
        }//lDeformer For
    }//if else


    xmlNode* lXmlSource = DAE_ExportSource14(pXmlNode, lStr.Buffer(), lPositions);

    return lXmlSource;
} // ExportVertexPositions


xmlNode* FbxWriterCollada::ExportLayerElements(xmlNode* pXmlMesh, FbxMesh* pMesh, FbxString pName) {
    // Ensure we covered all layer elements of layer 0:
    // - Normals
    // - UVs
    // - Vertex Colors
    // - Materials and Textures are covered when the mesh is exported.
    // - Warnings for unsupported layer element types: polygon groups, undefined

    int lNbLayers = pMesh->GetLayerCount();
    int i;

    // Normals
    for (i = 0; i < lNbLayers; i++) {
        if (!pMesh->GetLayer(i)->GetNormals()) continue;
        xmlNode* lXmlNormals = ExportNormals(pXmlMesh, pMesh, pName, NORMAL_POSTFIX, i);
        if (!lXmlNormals) return NULL;
        xmlAddChild(pXmlMesh, lXmlNormals);
    }

    // UVs
    for (i = 0; i < lNbLayers; i++) {
        if (!pMesh->GetLayer(i)->GetUVs()) continue;
        xmlNode* lXmlUVs = ExportUVs(pXmlMesh, pMesh, pName, i);
        if (!lXmlUVs) return NULL;
        xmlAddChild(pXmlMesh, lXmlUVs);
    }

    // Vertex Colors
    for (i = 0; i < lNbLayers; i++) {
        if (!pMesh->GetLayer(i)->GetVertexColors()) continue;
        xmlNode* lXmlVertexColors = ExportVertexColors(pXmlMesh, pMesh, pName, i);
        if (!lXmlVertexColors) return NULL;
        xmlAddChild(pXmlMesh, lXmlVertexColors);
    }

    // Polygon Groups are unsupported
    for (i = 0; i < lNbLayers; i++) {
        if (pMesh->GetLayer(i)->GetPolygonGroups()) {
            FbxString msg = FbxString("Polygon groups are not supported (node ") + pName + ")";
            AddNotificationWarning( msg );
        }
    }

    // Undefined layer element type are unsupported.
    for (i = 0; i < lNbLayers; i++) {
        if (pMesh->GetLayer(i)->GetLayerElementOfType(FbxLayerElement::eUnknown)) {
            FbxString msg = FbxString("Layer elements of undefined type are not supported (node ") + pName + ")";
            AddNotificationWarning( msg );
        }
    }

    return pXmlMesh;
} // ExportLayerElements


xmlNode* FbxWriterCollada::ExportNormals(xmlNode* pXmlNode, FbxMesh* pMesh, FbxString pName, FbxString pExt, int pLayerIndex) {
    // Export the normals of the pLayerIndex'th layer of a mesh into an xml node.

    FbxLayer *lLayer = pMesh->GetLayer(pLayerIndex);
    FbxLayerElementNormal *lLayerElementNormals = lLayer->GetNormals();
    if (!lLayerElementNormals) return NULL;

    FbxArray<FbxVector4> lNormals;
    lLayerElementNormals->GetDirectArray().CopyTo(lNormals);

    FbxString lStr = FbxString(pName) + pExt + pLayerIndex;
    xmlNode* lXmlSource = DAE_ExportSource14(pXmlNode, lStr.Buffer(), lNormals);

    return lXmlSource;

} // ExportNormals


xmlNode* FbxWriterCollada::ExportUVs(xmlNode* pXmlNode, FbxMesh* pMesh, FbxString pName, int pLayerIndex) {
    // Export the UVs of the pLayerIndex'th layer of a mesh into an xml node
    
    FbxLayer *lLayer = pMesh->GetLayer(pLayerIndex);
    FbxLayerElementUV *lLayerElementUV = lLayer->GetUVs();
    if (!lLayerElementUV) return NULL;

    FbxArray<FbxVector2> lUVs;
    lLayerElementUV->GetDirectArray().CopyTo(lUVs);

    FbxString lStr = FbxString(pName) + UV_POSTFIX + pLayerIndex;
    xmlNode* lXmlSource = DAE_ExportSource14(pXmlNode, lStr.Buffer(), lUVs);

    return lXmlSource;
} // ExportUVs


xmlNode* FbxWriterCollada::ExportVertexColors(xmlNode* pXmlNode, FbxMesh* pMesh, FbxString pName, int pLayerIndex) {
    // Export Vertex colors of the pLayerIndex'th layer of a mesh into an xml node
    
    FbxLayer *lLayer = pMesh->GetLayer(pLayerIndex);
    FbxLayerElementVertexColor *lLayerElementVertexColor = lLayer->GetVertexColors();
    if (!lLayerElementVertexColor) return NULL;

    FbxArray<FbxColor> lVertexColors;
    lLayerElementVertexColor->GetDirectArray().CopyTo(lVertexColors);

    FbxString lStr = FbxString(pName) + VERTEX_COLOR_POSTFIX + pLayerIndex;
    xmlNode* lXmlSource = DAE_ExportSource14(pXmlNode, lStr.Buffer(), lVertexColors);

    return lXmlSource;
} // ExportVertexColors


xmlNode* FbxWriterCollada::ExportVertices(xmlNode* pXmlNode, FbxMesh* pMesh, FbxString pName)
{
    // Export <vertices> inside a mesh.
    xmlNode* lXmlVertices = xmlNewNode(NULL, XML_STR COLLADA_VERTICES_STRUCTURE);
    FbxString lNameStr = pName;
    FbxString lIdStr = lNameStr + VERTEX_POSTFIX;
    FbxString lSourceStr = lNameStr + POSITION_POSTFIX;
    xmlNewProp(lXmlVertices, XML_STR COLLADA_ID_PROPERTY, XML_STR lIdStr.Buffer());
    DAE_AddInput14(lXmlVertices, COLLADA_POSITION_INPUT, lSourceStr);

    const int lLayerCount = pMesh->GetLayerCount();
    for (int lLayerIndex = 0; lLayerIndex < lLayerCount; ++lLayerIndex)
    {
        FbxLayer * lLayer = pMesh->GetLayer(lLayerIndex);
        if (lLayer->GetNormals() && lLayer->GetNormals()->GetMappingMode() == FbxLayerElement::eByControlPoint)
        {
            FbxString lStr = pName + NORMAL_POSTFIX + FbxString(lLayerIndex);
            DAE_AddInput14(lXmlVertices, COLLADA_NORMAL_INPUT, lStr);
        }
        if (lLayer->GetUVs() && lLayer->GetUVs()->GetMappingMode() == FbxLayerElement::eByControlPoint)
        {
            FbxString lStr = pName + UV_POSTFIX + FbxString(lLayerIndex);
            DAE_AddInput14(lXmlVertices, COLLADA_TEXCOORD_INPUT, lStr);
        }
        if (lLayer->GetVertexColors() && lLayer->GetVertexColors()->GetMappingMode() == FbxLayerElement::eByControlPoint)
        {
            FbxString lStr = pName + VERTEX_COLOR_POSTFIX + FbxString(lLayerIndex);
            DAE_AddInput14(lXmlVertices, COLLADA_COLOR_INPUT, lStr);
        }
    }

    return lXmlVertices;
} // ExportVertices


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Export Mesh polygons, normals and texture coordinates.  Here is an example of the COLLADA format (for a single mesh):
// <triangles count="10720" material="sMetal"><input semantic="VERTEX" offset="0" source="#pSpaceShip-VERTEX"/><input semantic="TEXCOORD" offset="1" set="0" source="#pSpaceShip-UV0"/><p> 2604 5123 7082 2762 2363 5122 2604 5123 2363 5122 1 21 703 1239 (...)
// <triangles count="4480" material="sWhite"><input semantic="VERTEX" offset="0" source="#pSpaceShip-VERTEX"/><input semantic="TEXCOORD" offset="1" set="0" source="#pSpaceShip-UV0"/><p> 362 417 7242 2922 3787 5486 7242 2922 
// (...)
// <triangles>: Means that the exported polygons are triangles.  COLLADA exports triangulated polygons if we use the default preset.
// <count>: Number of polygons (in our case, triangles) that will follow in the definition. 
// <material>: Material's name.  Only one material can be assigned to a <triangles> XML element.  Therefore, when we export a mesh
//            that contains multiple materials, we will have as many <triangles> elements as we have materials.
// <input>: Defines the nature of the following values.  In our case, we have 2 input elements, one is VERTEX and the other is TEXCOORD
//          VERTEX has an offset of 0 and TEXCOORD has an offset of 1.  This means that the following values will have this ordeR:
//          VERTEX TEXCOORD VERTEX TEXCOORD VERTEX TEXCOORD (...)
//          So, since we are using triangles, our first triangle's vertices will be values [0; 2; 4]
//          and its texture (material) coordinates will be values [1; 3; 5]
xmlNode* FbxWriterCollada::ExportPolygons(xmlNode* pMeshElement, FbxMesh* pMesh, FbxString pMaterialName, int pMaterialIndexInNode, FbxString pName, bool pShape)
{  
    // we need the node to write out the materials.   
    FbxNode* lNode = pMesh ? pMesh->GetNode() : NULL;
    if(lNode == NULL && !pShape)
    {
        FBX_ASSERT( lNode );
        return NULL;
    }

    // If user check "Triangulate" option in COLLADA group, use <triangles>;
    // If not, use <polylist>
    xmlNode * lParentElement = NULL;
    if (mTriangulate)
        lParentElement = DAE_AddChildElement(pMeshElement, COLLADA_TRIANGLES_STRUCTURE);
    else
        lParentElement = DAE_AddChildElement(pMeshElement, COLLADA_POLYLIST_STRUCTURE);

    // In COLLADA, we only support Layer 0 for now.  
    // TODO-pk this needs to be updated eventually.
    FbxLayer* lLayerMaterial = pMesh ? pMesh->GetLayer(0, FbxLayerElement::eMaterial) : NULL;
    FbxLayerElementMaterial* lGlobalLayerMaterials = lLayerMaterial ? lLayerMaterial->GetMaterials() : NULL;

    // Determine the list of polygons that we want to export.    
    // If we are exporting a mesh that contains multiple materials, we need to make an array
    // of the polygons we want to export for that particular material.
    FbxArray<int> lPolygonIndicesToExport;

    // when pShape is true, we now that pMesh has been created locally and IS NOT connected to an FbxNode, so lNode 
    // is NULL, we still want to "export" the polygon and we simulate that they all use the same material.
    if( !pShape && lGlobalLayerMaterials && lNode->GetMaterialCount() > 1 )
    {
        for( int i = 0; i < lGlobalLayerMaterials->GetIndexArray().GetCount(); ++i )
        {
            if( lGlobalLayerMaterials->GetIndexArray().GetAt(i) == pMaterialIndexInNode )
            {
                lPolygonIndicesToExport.Add(i);        
            }
        }   
    }
    else
    {
        for(int i = 0; i < pMesh->GetPolygonCount(); ++i)
        {
            lPolygonIndicesToExport.Add(i);
        }
    }

    DAE_AddAttribute(lParentElement, COLLADA_COUNT_PROPERTY, lPolygonIndicesToExport.GetCount());
    
    // If the polygons have material, the material name is an attribute.
    if (pMaterialName.GetLen() > 0)
        DAE_AddAttribute(lParentElement, COLLADA_MATERIAL_PROPERTY, pMaterialName.Buffer());

    // inputs.
    // vertices are mandatory
    int offset = 0;
    DAE_AddInput14(lParentElement, COLLADA_VERTEX_INPUT, pName + VERTEX_POSTFIX, offset);
    // Normals (all layers... does COLLADA support this?);
    // UVs on all layers; 
    // Vertex Colors on all layers.
    const int lLayerCount = pMesh->GetLayerCount();
    for (int lLayerIndex = 0; lLayerIndex < lLayerCount; ++lLayerIndex)
    {
        FbxLayer * lLayer = pMesh->GetLayer(lLayerIndex);
        if (lLayer->GetNormals() && lLayer->GetNormals()->GetMappingMode() != FbxLayerElement::eByControlPoint)
        {
            FbxString lStr = pName + NORMAL_POSTFIX + FbxString(lLayerIndex);
            ++offset;
            DAE_AddInput14(lParentElement, COLLADA_NORMAL_INPUT, lStr, offset);
        }
        if (lLayer->GetUVs() && lLayer->GetUVs()->GetMappingMode() != FbxLayerElement::eByControlPoint)
        {
            FbxString lStr = pName + UV_POSTFIX + FbxString(lLayerIndex);
            ++offset;
            DAE_AddInput14(lParentElement, COLLADA_TEXCOORD_INPUT, lStr, offset, lLayerIndex); // Using TEXCOORD b/c UV not supported by ColladaMaya
        }
        if (lLayer->GetVertexColors() && lLayer->GetVertexColors()->GetMappingMode() != FbxLayerElement::eByControlPoint)
        {
            FbxString lStr = pName + VERTEX_COLOR_POSTFIX + FbxString(lLayerIndex);
            ++offset;
            DAE_AddInput14(lParentElement, COLLADA_COLOR_INPUT, lStr, offset, lLayerIndex);
        }
    }

    // Export <vcount> for <polylist>
    if (!mTriangulate)
    {
        FbxString lContent;
        for (int lPolygonIndex = 0; lPolygonIndex < lPolygonIndicesToExport.GetCount(); ++lPolygonIndex)
        {
			int ps = pMesh->GetPolygonSize(lPolygonIndicesToExport.GetAt(lPolygonIndex));
			if (ps < 0)
			{
				GetStatus().SetCode(FbxStatus::eFailure, "Bad Polygon Size");
				return NULL;
			}
			lContent += ps;
            lContent += " ";
        }
        DAE_AddChildElement(lParentElement, COLLADA_VERTEXCOUNT_ELEMENT, lContent);
    }

    int lNormalIndex = 0;
    int lUVIndex = 0;
    int lVertexColorIndex = 0;
    int lPVIndex = 0; // Index of the polygon_vertex
	FbxArray<FbxString*> lPolygonStrs;

    for (int lPolygonIndex = 0; lPolygonIndex < lPolygonIndicesToExport.GetCount(); lPolygonIndex++)
    {
		FbxString* lPolygonStrPtr = FbxNew<FbxString>();
		lPolygonStrs.Add(lPolygonStrPtr);
		#define lPolygonStr (*lPolygonStrPtr)

        const int lPolygonSize = pMesh->GetPolygonSize(lPolygonIndicesToExport.GetAt(lPolygonIndex));
        for (int lPositionIndex = 0; lPositionIndex < lPolygonSize; lPositionIndex++)
        {
            int lVertexIndex = pMesh->GetPolygonVertex(lPolygonIndicesToExport.GetAt(lPolygonIndex), lPositionIndex);
			lPVIndex = pMesh->GetPolygonVertexIndex(lPolygonIndicesToExport.GetAt(lPolygonIndex));
			if (lPVIndex > -1) lPVIndex += lPositionIndex;

			// Check the returned values of the GetPolygonVertex and GetPolygonVertexIndex and skip
			// if they are negative. This may produce and invalid collada file.
			FBX_ASSERT(lVertexIndex > -1 && lPVIndex > -1);
			if (lVertexIndex < 0 || lPVIndex < 0) continue;

			lPolygonStr += " ";
			lPolygonStr += lVertexIndex;
            
            for (int lLayerIndex = 0; lLayerIndex < pMesh->GetLayerCount(); lLayerIndex++)
            {
                // Normals indices
                FbxLayerElementNormal *lNormals = pMesh->GetLayer(lLayerIndex)->GetNormals();
                if (lNormals && lNormals->GetMappingMode() != FbxLayerElement::eByControlPoint)
                {
                    // find normal index for this polygon and vertex
                    FbxLayerElement::EMappingMode lMappingMode = lNormals->GetMappingMode();
                    FbxLayerElement::EReferenceMode lReferenceMode = lNormals->GetReferenceMode();
                    if (lMappingMode == FbxLayerElement::eByPolygonVertex) {
                        if (   lReferenceMode == FbxLayerElement::eIndex
                            || lReferenceMode == FbxLayerElement::eIndexToDirect) 
                        {
                            lNormalIndex = lNormals->GetIndexArray().GetAt(lPVIndex);
							lPolygonStr += FbxString(" ") + FbxString(lNormalIndex);
                        } 
                        else if (lReferenceMode == FbxLayerElement::eDirect) 
                        {
							lPolygonStr += FbxString(" ") + FbxString(lPVIndex);
                        }        
                    } 
                    else 
                    {
                        // unsupported mapping mode.
                        FbxString msg = FbxString("layer element type not supported for normals: ") + lMappingMode;
                        AddNotificationWarning( msg );
                        // use control points indices.
                        lNormalIndex = lVertexIndex;
						lPolygonStr += FbxString(" ") + FbxString(lNormalIndex);
                    }
                }

                FbxLayerElementUV *lUVs = pMesh->GetLayer(lLayerIndex)->GetUVs();
                // UVs indices
                if (lUVs && lUVs->GetMappingMode() != FbxLayerElement::eByControlPoint) 
                {
                    // find UV index for this polygon and vertex
                    FbxLayer* lLayer = pMesh->GetLayer(0);
                    FbxLayerElement::EMappingMode lMappingMode = FbxLayerElement::eNone;
                    
                    if(lLayer && lLayer->GetUVs(FbxLayerElement::eTextureDiffuse))
                    {
                        lMappingMode = lLayer->GetUVs(FbxLayerElement::eTextureDiffuse)->GetMappingMode();
                    }
                    
                    if (lMappingMode == FbxLayerElement::eByPolygonVertex) 
                    {
                        lUVIndex = lUVs->GetIndexArray().GetAt(lPVIndex);
						lPolygonStr += FbxString(" ") + FbxString(lUVIndex);
                    } 
                    else 
                    {
                        // unsupported mapping mode.
                        FbxString msg = FbxString("layer element type not supported for UVs: ") + lMappingMode;
                        AddNotificationWarning( msg );
                        // use control points indices.
                        lUVIndex = lVertexIndex;
						lPolygonStr += FbxString(" ") + FbxString(lUVIndex);
                    }
                }

                FbxLayerElementVertexColor *lVertexColors = pMesh->GetLayer(lLayerIndex)->GetVertexColors();
                // Vertex Colors indices
                if (lVertexColors && lVertexColors->GetMappingMode() != FbxLayerElement::eByControlPoint) {
                    // find Vertex Color index for this polygon and vertex
                    FbxLayerElement::EMappingMode lMappingMode = lVertexColors->GetMappingMode();
                    if (lMappingMode == FbxLayerElement::eByPolygonVertex) {
                        lVertexColorIndex = lVertexColors->GetIndexArray().GetAt(lPVIndex);
						lPolygonStr += FbxString(" ") + FbxString(lVertexColorIndex);
                    } else {
                        // unsupported mapping mode.
                        FbxString msg = FbxString("layer element type not supported for vertex colors: ") + lMappingMode;
                        AddNotificationWarning( msg );
                        // use control points indices.
                        lVertexColorIndex = lVertexIndex;
						lPolygonStr += FbxString(" ") + FbxString(lVertexColorIndex);
                    }
                }
            }
        }
    }

	// calculate the total amount of space to concatente all the temporary strings stored in lPolygonStrs
	size_t buffSize = 0;
	for (int i = 0; i < lPolygonStrs.GetCount(); i++) 
		buffSize += lPolygonStrs[i]->GetLen();

	// allocate one single buffer big enough to make the full string
	char* buffer = (char*)FbxMalloc(buffSize + 1);
	char* p = buffer;
	for (int i = 0; i < lPolygonStrs.GetCount(); i++)
	{
		size_t l = lPolygonStrs[i]->GetLen();
		memcpy(p, lPolygonStrs[i]->Buffer(), l);
		p += l;
	}
	// make sure we are NULL terminated
	*p = '\0';

	// store the COLLADA data
	DAE_AddChildElement(lParentElement, COLLADA_P_STRUCTURE, buffer);

	// free temporary memory
	FbxArrayDelete(lPolygonStrs);
	FbxFree(buffer);
	    
    return lParentElement;
}

bool FbxWriterCollada::ExportMeshMaterials(FbxMesh *pMesh, int pNbMat) {

    FbxNode* lNode = pMesh->GetNode();
    FBX_ASSERT( lNode );
    if( !lNode ) return false;

    // Export materials used by the mesh pMesh (number of material in the mesh is pNbMat).

    int lMatIndex;

    for (lMatIndex = 0; lMatIndex < pNbMat; lMatIndex++) {
        FbxSurfaceMaterial *lMaterial = lNode->GetMaterial(lMatIndex);

        ExportMaterial(lMaterial);
    }

    return true;
} // ExportMeshMaterials


xmlNode* FbxWriterCollada::ExportMaterial(FbxSurfaceMaterial *pMaterial) {
    // Add this material to the library mLibraryMaterial - if it is not already there!
    // Either way, return the xml node.

    FbxString lMaterialName = pMaterial->GetNameWithoutNameSpacePrefix(); // Material do not support namespaces.
    FbxString lEffectName = lMaterialName + "-fx";
    FbxString lEffectUrl = FbxString("#") + lEffectName;

    // Look if this material is already in the materials library.
    xmlNode *lMatNode = DAE_FindChildElementByAttribute(mLibraryMaterial, COLLADA_ID_PROPERTY, lMaterialName);
    if (!lMatNode) {
        // Material not already in the library. Add it.
        // If the Material library is not already created, create it.
        if (!mLibraryMaterial) {
            mLibraryMaterial = xmlNewNode(NULL, XML_STR COLLADA_LIBRARY_MATERIAL_ELEMENT);
        }

        // add material to the library
        lMatNode = xmlNewChild(mLibraryMaterial, NULL, XML_STR COLLADA_MATERIAL_STRUCTURE, NULL);
        xmlNewProp(lMatNode, XML_STR COLLADA_ID_PROPERTY, XML_STR lMaterialName.Buffer());
        xmlNewProp(lMatNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR lMaterialName.Buffer());

        // Material is an instance of an effects.
        xmlNode* lInstanceEffect = xmlNewChild(lMatNode, NULL, XML_STR COLLADA_INSTANCE_EFFECT_ELEMENT, NULL);
        xmlNewProp(lInstanceEffect, XML_STR COLLADA_URL_PROPERTY, XML_STR lEffectUrl.Buffer());

        // If the material has a CGFX implementation, export the bind table
        const FbxImplementation * lImpl = pMaterial->GetImplementation(0);
        if (lImpl && lImpl->Language.Get() == FBXSDK_SHADING_LANGUAGE_CGFX)
        {
            const FbxBindingTable * lTable = lImpl->GetRootTable();
            size_t lEntryCount = lTable->GetEntryCount();
            for (size_t lEntryIndex = 0; lEntryIndex < lEntryCount; ++lEntryIndex)
            {
                const FbxBindingTableEntry & lEntry =
                    lTable->GetEntry(lEntryIndex);
                const char * lDest = lEntry.GetDestination();
                FbxProperty lSourceProperty = 
                    pMaterial->FindPropertyHierarchical(lEntry.GetSource());
                FBX_ASSERT(lSourceProperty.IsValid());

                xmlNode * lSetParamElement = DAE_AddChildElement(
                    lInstanceEffect, COLLADA_FXCMN_SETPARAM_ELEMENT);
                DAE_AddAttribute(lSetParamElement, COLLADA_REF_PROPERTY, lDest);

                ExportPropertyValue(lSourceProperty, lSetParamElement);
            }
        }

        ExportEffect(pMaterial, lEffectName);
    }

    return lMatNode;
} // ExportMaterial


xmlNode* FbxWriterCollada::ExportEffect(FbxSurfaceMaterial *pMaterial, FbxString pEffectId) {
    // Add this effect to the library mLibraryEffect - if it is not already there!
    // Either way, return the xml node.

    xmlNode *lEffectNode = DAE_FindChildElementByAttribute(mLibraryMaterial, COLLADA_ID_PROPERTY, pEffectId);
    if (!lEffectNode) {
        // Effect not already in the library. Add it.
        // If the Effect library is not already created, create it.
        if (!mLibraryEffect) {
            mLibraryEffect = xmlNewNode(NULL, XML_STR COLLADA_LIBRARY_EFFECT_ELEMENT);
        }

        // add effect to the library
        lEffectNode = xmlNewChild(mLibraryEffect, NULL, XML_STR COLLADA_EFFECT_ELEMENT, NULL);
        xmlNewProp(lEffectNode, XML_STR COLLADA_ID_PROPERTY, XML_STR pEffectId.Buffer());
        FbxString lNameWithoutNameSpacePrefix = pMaterial->GetNameWithoutNameSpacePrefix();
        xmlNewProp(lEffectNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR lNameWithoutNameSpacePrefix.Buffer());

        // effect has a common profile
        xmlNode* lCommonProfile = xmlNewChild(lEffectNode, NULL, XML_STR COLLADA_FX_PROFILE_COMMON_ELEMENT, NULL);
        // common profile has a technique
        xmlNode* lTechnique = xmlNewChild(lCommonProfile, NULL, XML_STR COLLADA_TECHNIQUE_STRUCTURE, NULL);
        xmlNewProp(lTechnique, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_TECHNIQUE_STANDARD_PARAMETER);

        //IMPORTANT NOTE:
        //Always check for the most complex class before the less one. In this case, Phong inherit from Lambert,
        //so if we would be testing for lambert classid before phong, we would never enter the phong case.
        if( pMaterial->Is<FbxSurfacePhong>() )
        {
            FbxSurfacePhong* lPhongSurface = FbxCast<FbxSurfacePhong>(pMaterial);
            xmlNode* lPhong = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_FXSTD_PHONG_ELEMENT, NULL);

            FbxDouble3 lColor;
            double lFactor;
            
            // emissive color
            lColor = lPhongSurface->Emissive.Get();
            lFactor = lPhongSurface->EmissiveFactor.Get();
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            DAE_AddParameter(lPhong, COLLADA_EMISSION_MATERIAL_PARAMETER, lColor);

            // ambient color
            lColor = lPhongSurface->Ambient.Get();
            lFactor = lPhongSurface->AmbientFactor.Get();
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            DAE_AddParameter(lPhong, COLLADA_AMBIENT_MATERIAL_PARAMETER, lColor);    

            // diffuse color
            lColor = lPhongSurface->Diffuse.Get();
            lFactor = lPhongSurface->DiffuseFactor.Get();
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            DAE_AddParameter(lPhong, COLLADA_DIFFUSE_MATERIAL_PARAMETER, lColor);

            // specular color
            lColor = lPhongSurface->Specular.Get();
            lFactor = lPhongSurface->SpecularFactor.Get();
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            DAE_AddParameter(lPhong, COLLADA_SPECULAR_MATERIAL_PARAMETER, lColor);
            
            // shininess
            double lShininess;
            lShininess = lPhongSurface->Shininess.Get();
            DAE_AddParameter(lPhong, COLLADA_SHININESS_MATERIAL_PARAMETER, lShininess);

            // reflective / reflectivity
            lColor = lPhongSurface->Reflection.Get();
            DAE_AddParameter(lPhong, COLLADA_REFLECTIVE_MATERIAL_PARAMETER, lColor);
            lFactor = lPhongSurface->ReflectionFactor.Get();
            DAE_AddParameter(lPhong, COLLADA_REFLECTIVITY_MATERIAL_PARAMETER, lFactor);

            // opacity / transparency
            lColor = lPhongSurface->TransparentColor.Get();
            xmlNode * lTransparentElement = DAE_AddParameter(lPhong, COLLADA_TRANSPARENT_MATERIAL_PARAMETER, lColor);
            lFactor = lPhongSurface->TransparencyFactor.Get();
            DAE_AddParameter(lPhong, COLLADA_TRANSPARENCY_MATERIAL_PARAMETER, lFactor);
            DAE_AddAttribute(lTransparentElement, COLLADA_OPAQUE_MODE_ATTRIBUTE, COLLADA_OPAQUE_MODE_RGB_ZERO);

            // Note: 
            // COLLADA_INDEXOFREFRACTION_MATERIAL_PARAMETER,
            // are not supported by FBX.
        }
        else if( pMaterial->Is<FbxSurfaceLambert>() )
        {
            FbxSurfaceLambert* lLambertSurface = FbxCast<FbxSurfaceLambert>(pMaterial);
            xmlNode* lLambert = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_FXSTD_LAMBERT_ELEMENT, NULL);

            FbxDouble3 lColor;
            double lFactor;
            
            // emissive color
            lColor = lLambertSurface->Emissive.Get();
            lFactor = lLambertSurface->EmissiveFactor.Get();
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            DAE_AddParameter(lLambert, COLLADA_EMISSION_MATERIAL_PARAMETER, lColor);

            // ambient color
            lColor = lLambertSurface->Ambient.Get();
            lFactor = lLambertSurface->AmbientFactor.Get();
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            DAE_AddParameter(lLambert, COLLADA_AMBIENT_MATERIAL_PARAMETER, lColor);    

            // diffuse color
            lColor = lLambertSurface->Diffuse.Get();
            lFactor = lLambertSurface->DiffuseFactor.Get();
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            DAE_AddParameter(lLambert, COLLADA_DIFFUSE_MATERIAL_PARAMETER, lColor);

            // opacity / transparency
            lColor = lLambertSurface->TransparentColor.Get();
            xmlNode * lTransparentElement = DAE_AddParameter(lLambert, COLLADA_TRANSPARENT_MATERIAL_PARAMETER, lColor);
            lFactor = lLambertSurface->TransparencyFactor.Get();
            DAE_AddParameter(lLambert, COLLADA_TRANSPARENCY_MATERIAL_PARAMETER, lFactor);
            DAE_AddAttribute(lTransparentElement, COLLADA_OPAQUE_MODE_ATTRIBUTE, COLLADA_OPAQUE_MODE_RGB_ZERO);

            // Note: 
            // COLLADA_REFLECTIVITY_MATERIAL_PARAMETER,
            // COLLADA_INDEXOFREFRACTION_MATERIAL_PARAMETER,
            // are not supported by FBX.
        }
        else // use shading model
        {
            FbxString lShadingModel = pMaterial->ShadingModel.Get();

            // shading models supported here are: "constant", "blinn"
            // note that "lambert" and "phong" should have been treated above
            // all others default to "phong"
            if (lShadingModel == COLLADA_FXSTD_CONSTANT_ELEMENT) 
            {

                // technique has a constant
                xmlNode* lConstant = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_FXSTD_CONSTANT_ELEMENT, NULL);

                // constant has the effect's parameters

                FbxProperty lPropColor;
                FbxProperty lPropFactor;
                FbxDouble3 lColor;
                double lFactor;

                // emissive color
                lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sEmissive, FbxColor3DT, false);
                lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sEmissiveFactor, FbxDoubleDT, false);

                if (lPropFactor.IsValid())
                {
                    lFactor = lPropFactor.Get<FbxDouble>();
                }
                else
                {
                    lFactor = 1.;
                }
                if (lPropColor.IsValid())
                {
                    lColor = lPropColor.Get<FbxDouble3>();
                    lColor[0] *= lFactor;
                    lColor[1] *= lFactor;
                    lColor[2] *= lFactor;
                }
                else
                {
                    if (!lPropFactor.IsValid())
                    {
                        lFactor = 0.;
                    }
                    lColor[0]=lFactor;
                    lColor[0]=lFactor;
                    lColor[0]=lFactor;
                }

                DAE_AddParameter(lConstant, COLLADA_EMISSION_MATERIAL_PARAMETER, lColor);

                // reflectivity
                lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sReflectionFactor, FbxDoubleDT, false);

                if (lPropFactor.IsValid())
                {
                    lFactor = lPropFactor.Get<FbxDouble>();
                    DAE_AddParameter(lConstant, COLLADA_REFLECTIVITY_MATERIAL_PARAMETER, lFactor);
                }

                // opacity / transparency
                lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sTransparentColor, FbxColor3DT, false);

                if (lPropColor.IsValid())
                {
                    lColor = lPropColor.Get<FbxDouble3>();
                    lColor[0] = lColor[0];
                    lColor[1] = lColor[1];
                    lColor[2] = lColor[2];
                    DAE_AddParameter(lConstant, COLLADA_TRANSPARENT_MATERIAL_PARAMETER, lColor);
                }

                lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sTransparencyFactor, FbxDoubleDT, false);

                if (lPropFactor.IsValid())
                {
                    lFactor = lPropFactor.Get<FbxDouble>();
                    DAE_AddParameter(lConstant, COLLADA_TRANSPARENCY_MATERIAL_PARAMETER, lFactor);
                }

                // Note: 
                // COLLADA_REFLECTIVE_MATERIAL_PARAMETER,
                // COLLADA_INDEXOFREFRACTION_MATERIAL_PARAMETER
                // are not supported by FBX.
            }
            else if (lShadingModel == COLLADA_FXSTD_BLINN_ELEMENT) 
            {

                // technique has a blinn
                xmlNode* lBlinn = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_FXSTD_BLINN_ELEMENT, NULL);

                // blinn has the effect's parameters
                FbxProperty lPropColor;
                FbxProperty lPropFactor;
                FbxDouble3 lColor;
                double lFactor;

                // emissive color
                lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sEmissive, FbxColor3DT, false);
                lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sEmissiveFactor, FbxDoubleDT, false);

                if (lPropFactor.IsValid())
                {
                    lFactor = lPropFactor.Get<FbxDouble>();
                }
                else
                {
                    lFactor = 1.;
                }

                if (lPropColor.IsValid())
                {
                    lColor = lPropColor.Get<FbxDouble3>();
                    lColor[0] *= lFactor;
                    lColor[1] *= lFactor;
                    lColor[2] *= lFactor;
                }
                else
                {
                    if (!lPropFactor.IsValid())
                    {
                        lFactor = 0.;
                    }
                    lColor[0]=lFactor;
                    lColor[0]=lFactor;
                    lColor[0]=lFactor;
                }

                DAE_AddParameter(lBlinn, COLLADA_EMISSION_MATERIAL_PARAMETER, lColor);

                // ambient color
                lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sAmbient, FbxColor3DT, false);
                lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sAmbientFactor, FbxDoubleDT, false);

                if (lPropFactor.IsValid())
                {
                    lFactor = lPropFactor.Get<FbxDouble>();
                }
                else
                {
                    lFactor = 1.;
                }
                if (lPropColor.IsValid())
                {
                    lColor = lPropColor.Get<FbxDouble3>();
                    lColor[0] *= lFactor;
                    lColor[1] *= lFactor;
                    lColor[2] *= lFactor;
                }
                else
                {
                    if (!lPropFactor.IsValid())
                    {
                        lFactor = 0.;
                    }
                    lColor[0] = lFactor;
                    lColor[1] = lFactor;
                    lColor[2] = lFactor;
                }
                DAE_AddParameter(lBlinn, COLLADA_AMBIENT_MATERIAL_PARAMETER, lColor);    

                // diffuse color
                lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse, FbxColor3DT, false);
                lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sDiffuseFactor, FbxDoubleDT, false);

                if (lPropFactor.IsValid())
                {
                    lFactor = lPropFactor.Get<FbxDouble>();
                }
                else
                {
                    lFactor = 1.;
                }
                if (lPropColor.IsValid())
                {
                    lColor = lPropColor.Get<FbxDouble3>();
                    lColor[0] *= lFactor;
                    lColor[1] *= lFactor;
                    lColor[2] *= lFactor;
                }
                else
                {
                    if (!lPropFactor.IsValid())
                    {
                        lFactor = 0.;
                    }
                    lColor[0] = lFactor;
                    lColor[1] = lFactor;
                    lColor[2] = lFactor;
                }
                DAE_AddParameter(lBlinn, COLLADA_DIFFUSE_MATERIAL_PARAMETER, lColor);

                // specular color
                lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sSpecular, FbxColor3DT, false);
                lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sSpecularFactor, FbxDoubleDT, false);

                if (lPropFactor.IsValid())
                {
                    lFactor = lPropFactor.Get<FbxDouble>();
                }
                else
                {
                    lFactor = 1.;
                }
                if (lPropColor.IsValid())
                {
                    lColor = lPropColor.Get<FbxDouble3>();
                    lColor[0] *= lFactor;
                    lColor[1] *= lFactor;
                    lColor[2] *= lFactor;
                }
                else
                {
                    if (!lPropFactor.IsValid())
                    {
                        lFactor = 0.;
                    }
                    lColor[0] = lFactor;
                    lColor[1] = lFactor;
                    lColor[2] = lFactor;
                }
                DAE_AddParameter(lBlinn, COLLADA_SPECULAR_MATERIAL_PARAMETER, lColor);

                // shininess
                lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sShininess, FbxDoubleDT, false);

                if (lPropFactor.IsValid())
                {
                    lFactor = lPropFactor.Get<FbxDouble>();
                    DAE_AddParameter(lBlinn, COLLADA_SHININESS_MATERIAL_PARAMETER, lFactor);
                }

                // reflective
                lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sReflection, FbxColor3DT, false);
                if (lPropColor.IsValid())
                {
                    lColor = lPropColor.Get<FbxDouble3>();
                    DAE_AddParameter(lBlinn, COLLADA_REFLECTIVE_MATERIAL_PARAMETER, lColor);
                }

                // reflectivity
                lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sReflectionFactor, FbxDoubleDT, false);

                if (lPropFactor.IsValid())
                {
                    lFactor = lPropFactor.Get<FbxDouble>();
                    DAE_AddParameter(lBlinn, COLLADA_REFLECTIVITY_MATERIAL_PARAMETER, lFactor);
                }

                // opacity / transparency
                lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sTransparentColor, FbxColor3DT, false);

                if (lPropColor.IsValid())
                {
                    lColor = lPropColor.Get<FbxDouble3>();
                    lColor[0] = lColor[0];
                    lColor[1] = lColor[1];
                    lColor[2] = lColor[2];
                    DAE_AddParameter(lBlinn, COLLADA_TRANSPARENT_MATERIAL_PARAMETER, lColor);
                }

                lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sTransparencyFactor, FbxDoubleDT, false);

                if (lPropFactor.IsValid())
                {
                    lFactor = lPropFactor.Get<FbxDouble>();
                    DAE_AddParameter(lBlinn, COLLADA_TRANSPARENCY_MATERIAL_PARAMETER, lFactor);
                }

                // Note: 
                // COLLADA_INDEXOFREFRACTION_MATERIAL_PARAMETER
                // are not supported by FBX.
            }
            else
            {
                // If has a CGFX implementation, export constant in profile common;
                // And set path with NVidia extension
                FbxImplementation * lImpl = pMaterial->GetDefaultImplementation();
                if (lImpl && lImpl->Language.Get() == FBXSDK_SHADING_LANGUAGE_CGFX)
                {
                    // Set default material to constant, in case when the reader
                    // of COLLADA don't support NVidia extension.
                    DAE_AddChildElement(lTechnique,
                        COLLADA_FXSTD_CONSTANT_ELEMENT);

                    // Save the path in NVidia extension.
                    xmlNode * lExtraElement = DAE_AddChildElement(lEffectNode,
                        COLLADA_EXTRA_STRUCTURE);
                    DAE_AddAttribute(lExtraElement, COLLADA_TYPE_PROPERTY,
                        COLLADA_NVIDIA_FXCOMPOSER_IMPORT_ELEMENT);

                    xmlNode * lTechniqueElement = DAE_AddChildElement(
                        lExtraElement, COLLADA_TECHNIQUE_STRUCTURE);
                    DAE_AddAttribute(lTechniqueElement, COLLADA_PROFILE_PROPERTY,
                        COLLADA_NVIDIA_FXCOMPOSER_PROFILE);

                    xmlNode * lImportElement = DAE_AddChildElement(
                        lTechniqueElement,
                        COLLADA_NVIDIA_FXCOMPOSER_IMPORT_ELEMENT);
                    FbxString lURL = lImpl->GetRootTable()->DescAbsoluteURL.Get();
                    if (lURL.IsEmpty())
                        lURL = lImpl->GetRootTable()->DescRelativeURL.Get();
                    DAE_AddAttribute(lImportElement,
                        COLLADA_URL_PROPERTY, lURL);
                    DAE_AddAttribute(lImportElement,
                        COLLADA_NVIDIA_FXCOMPOSER_COMPILER_OPTIONS_ATTRIBUTE,
                        "");
                    DAE_AddAttribute(lImportElement,
                        COLLADA_NVIDIA_FXCOMPOSER_PROFILE_ATTRIBUTE,
                        FbxPathUtils::GetExtensionName(lURL));
                }
                else
                {
                    xmlNode* lPhong = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_FXSTD_PHONG_ELEMENT, NULL);

                    FbxProperty lPropColor;
                    FbxProperty lPropFactor;
                    FbxDouble3 lColor;
                    double lFactor;

                    // emissive color
                    lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sEmissive, FbxColor3DT, false);
                    lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sEmissiveFactor, FbxDoubleDT, false);

                    if (lPropFactor.IsValid())
                    {
                        lFactor = lPropFactor.Get<FbxDouble>();
                    }
                    else
                    {
                        lFactor = 1.;
                    }
                    if (lPropColor.IsValid())
                    {
                        lColor = lPropColor.Get<FbxDouble3>();
                        lColor[0] *= lFactor;
                        lColor[1] *= lFactor;
                        lColor[2] *= lFactor;
                    }
                    else
                    {
                        if (!lPropFactor.IsValid())
                        {
                            lFactor = 0.;
                        }
                        lColor[0]=lFactor;
                        lColor[0]=lFactor;
                        lColor[0]=lFactor;
                    }

                    DAE_AddParameter(lPhong, COLLADA_EMISSION_MATERIAL_PARAMETER, lColor);

                    // ambient color
                    lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sAmbient, FbxColor3DT, false);
                    lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sAmbientFactor, FbxDoubleDT, false);

                    if (lPropFactor.IsValid())
                    {
                        lFactor = lPropFactor.Get<FbxDouble>();
                    }
                    else
                    {
                        lFactor = 1.;
                    }
                    if (lPropColor.IsValid())
                    {
                        lColor = lPropColor.Get<FbxDouble3>();
                        lColor[0] *= lFactor;
                        lColor[1] *= lFactor;
                        lColor[2] *= lFactor;
                    }
                    else
                    {
                        if (!lPropFactor.IsValid())
                        {
                            lFactor = 0.;
                        }
                        lColor[0]=lFactor;
                        lColor[0]=lFactor;
                        lColor[0]=lFactor;
                    }

                    DAE_AddParameter(lPhong, COLLADA_AMBIENT_MATERIAL_PARAMETER, lColor);    

                    // diffuse color
                    lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse, FbxColor3DT, false);
                    lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sDiffuseFactor, FbxDoubleDT, false);

                    if (lPropFactor.IsValid())
                    {
                        lFactor = lPropFactor.Get<FbxDouble>();
                    }
                    else
                    {
                        lFactor = 1.;
                    }
                    if (lPropColor.IsValid())
                    {
                        lColor = lPropColor.Get<FbxDouble3>();
                        lColor[0] *= lFactor;
                        lColor[1] *= lFactor;
                        lColor[2] *= lFactor;
                    }
                    else
                    {
                        if (!lPropFactor.IsValid())
                        {
                            lFactor = 0.;
                        }
                        lColor[0]=lFactor;
                        lColor[0]=lFactor;
                        lColor[0]=lFactor;
                    }

                    DAE_AddParameter(lPhong, COLLADA_DIFFUSE_MATERIAL_PARAMETER, lColor);

                    // specular color
                    lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sSpecular, FbxColor3DT, false);
                    lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sSpecularFactor, FbxDoubleDT, false);

                    if (lPropFactor.IsValid())
                    {
                        lFactor = lPropFactor.Get<FbxDouble>();
                    }
                    else
                    {
                        lFactor = 1.;
                    }
                    if (lPropColor.IsValid())
                    {
                        lColor = lPropColor.Get<FbxDouble3>();
                        lColor[0] *= lFactor;
                        lColor[1] *= lFactor;
                        lColor[2] *= lFactor;
                    }
                    else
                    {
                        if (!lPropFactor.IsValid())
                        {
                            lFactor = 0.;
                        }
                        lColor[0]=lFactor;
                        lColor[0]=lFactor;
                        lColor[0]=lFactor;
                    }

                    DAE_AddParameter(lPhong, COLLADA_SPECULAR_MATERIAL_PARAMETER, lColor);

                    // shininess
                    lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sShininess, FbxDoubleDT, false);

                    if (lPropFactor.IsValid())
                    {
                        lFactor = lPropFactor.Get<FbxDouble>();
                        DAE_AddParameter(lPhong, COLLADA_SHININESS_MATERIAL_PARAMETER, lFactor);
                    }

                    // reflective
                    lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sReflection, FbxColor3DT, false);
                    if (lPropColor.IsValid())
                    {
                        lColor = lPropColor.Get<FbxDouble3>();
                        DAE_AddParameter(lPhong, COLLADA_REFLECTIVE_MATERIAL_PARAMETER, lColor);
                    }

                    // reflectivity
                    lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sReflectionFactor, FbxDoubleDT, false);

                    if (lPropFactor.IsValid())
                    {
                        lFactor = lPropFactor.Get<FbxDouble>();
                        DAE_AddParameter(lPhong, COLLADA_REFLECTIVITY_MATERIAL_PARAMETER, lFactor);
                    }

                    // opacity / transparency
                    lPropColor = pMaterial->FindProperty(FbxSurfaceMaterial::sTransparentColor, FbxColor3DT, false);

                    if (lPropColor.IsValid())
                    {
                        lColor = lPropColor.Get<FbxDouble3>();
                        lColor[0] = lColor[0];
                        lColor[1] = lColor[1];
                        lColor[2] = lColor[2];
                        DAE_AddParameter(lPhong, COLLADA_TRANSPARENT_MATERIAL_PARAMETER, lColor);
                    }

                    lPropFactor = pMaterial->FindProperty(FbxSurfaceMaterial::sTransparencyFactor, FbxDoubleDT, false);

                    if (lPropFactor.IsValid())
                    {
                        lFactor = lPropFactor.Get<FbxDouble>();
                        DAE_AddParameter(lPhong, COLLADA_TRANSPARENCY_MATERIAL_PARAMETER, lFactor);
                    }

                    // Note: 
                    // COLLADA_INDEXOFREFRACTION_MATERIAL_PARAMETER,
                    // are not supported by FBX.
                }
            }

        }
    }

    return lEffectNode;
} // ExportEffect


// COLLADA: ambient, diffuse, emission, reflective, specular, transparent
static const FbxLayerElement::EType sLayerTypes[] =
{
    FbxLayerElement::eTextureAmbient
    , FbxLayerElement::eTextureDiffuse
    , FbxLayerElement::eTextureEmissive
    , FbxLayerElement::eTextureReflection
    , FbxLayerElement::eTextureSpecular
    , FbxLayerElement::eTextureTransparency
};

void GetColladaName( int pLayerElementType, FbxString & pString )
{
    // same order as previous array
    switch( pLayerElementType )
    {
    case 0:
        pString = COLLADA_AMBIENT_MATERIAL_PARAMETER;
        break;
    case 1:
        pString = COLLADA_DIFFUSE_MATERIAL_PARAMETER;
        break;
    case 2:
        pString = COLLADA_EMISSION_MATERIAL_PARAMETER;
        break;
    case 3:
        pString = COLLADA_REFLECTIVE_MATERIAL_PARAMETER;
        break;
    case 4:
        pString = COLLADA_SPECULAR_MATERIAL_PARAMETER;
        break;
    case 5:
        pString = COLLADA_TRANSPARENT_MATERIAL_PARAMETER;
        break;
    default:
        FBX_ASSERT_NOW("Unexpected layer element type");
        pString = "INVALID";
    }
}

bool FbxWriterCollada::AddMaterialTextureInput(xmlNode *pXmlMaterial, FbxFileTexture *pTexture, FbxString pImageId, int pLayerIndex, int pLayerElementType)
{
    // Add the texture to the right effect.

    int i;
    FbxString lColladaName;
    GetColladaName( pLayerElementType, lColladaName );
    FbxString lName = pTexture->GetNameWithoutNameSpacePrefix(); 
    FbxString lTextureName = lName + "-" + COLLADA_IMAGE_STRUCTURE;

    // Get the instance_effect url in the material.
    xmlNode *lInstanceEffect = DAE_FindChildElementByTag(pXmlMaterial, COLLADA_INSTANCE_EFFECT_ELEMENT);
    if (!lInstanceEffect) {
        FbxString lMaterialNodeName = (const char*)pXmlMaterial->name;
        FbxString msg = FbxString("could not find instance effect in material node ") + lMaterialNodeName + ".";
        msg += " Could not export texture " + lTextureName + ".";
        AddNotificationError( msg );
        return false;
    }

    FbxString url = DAE_GetElementAttributeValue(lInstanceEffect, COLLADA_URL_PROPERTY);

    // Get the effect with this url as id
    xmlNode *lEffect = DAE_FindChildElementByAttribute(mLibraryEffect, COLLADA_ID_PROPERTY, url.Buffer()+1);
    if (!lEffect) {
        FbxString msg = FbxString("could not find library effect ") + url + ".";
        msg += " Could not export texture " + lTextureName + ".";
        AddNotificationError( msg );
        return false;
    }

    // Get the common profile
    xmlNode *lCommonProfile = DAE_FindChildElementByTag(lEffect, COLLADA_FX_PROFILE_COMMON_ELEMENT);
    if (!lCommonProfile) {
        FbxString msg = FbxString("Could not find common profile in library effect ") + url + ".";
        msg += " Could not export texture " + lTextureName + ".";
        AddNotificationWarning( msg );
        return true;
    }
    // Get the technique
    xmlNode *lTechnique = DAE_FindChildElementByTag(lCommonProfile, COLLADA_TECHNIQUE_STRUCTURE);
    if (!lTechnique) {
        FbxString msg = FbxString("Could not find technique in library effect ") + url + ".";
        msg += " Could not export texture " + lTextureName + ".";
        AddNotificationWarning( msg );
        return true;
    }
    // Get the material type node (lambert or phong: In our case, always phong, since we are exporting from FBX.)
    xmlNode *lMaterialType = DAE_FindChildElementByTag(lTechnique, COLLADA_FXSTD_PHONG_ELEMENT);
    if (!lMaterialType)
    {
        lMaterialType = DAE_FindChildElementByTag(lTechnique, COLLADA_FXSTD_LAMBERT_ELEMENT);
    }
    if (!lMaterialType)
    {
        lMaterialType = DAE_FindChildElementByTag(lTechnique, COLLADA_FXSTD_BLINN_ELEMENT);
    }
    if (!lMaterialType) {
        FbxString msg = FbxString("Could not find phong node in library effect ") + url + ".";
        msg += " Could not export texture " + lTextureName + ".";
        AddNotificationWarning( msg );
        return true;
    }
    
    // Get the related node
    xmlNode * lPropertyNode = DAE_FindChildElementByTag(lMaterialType, lColladaName);
    if (!lPropertyNode) {
        lPropertyNode = xmlNewChild(lMaterialType, NULL, XML_STR lColladaName.Buffer(), NULL);
    }

    // Is the texture node with this id already there?
    CNodeList lTextureNodes;
    findChildrenByType(lPropertyNode, COLLADA_FXSTD_SAMPLER_ELEMENT, lTextureNodes);
    xmlNode *lTextureNode = NULL;
    for (i = 0; i < lTextureNodes.GetCount(); i++) {
        lTextureNode = lTextureNodes[i];
        FbxString lTextureAttribute = DAE_GetElementAttributeValue(lTextureNode, COLLADA_FXSTD_TEXTURE_ATTRIBUTE);
        if (lTextureAttribute == pImageId)
            break;
        else
            lTextureNode = NULL;
    }

    if (!lTextureNode) {
        lTextureNode = ExportTexture(pTexture, lTextureName, pLayerIndex);
        xmlAddChild(lPropertyNode, lTextureNode);
        xmlNode *lColor = DAE_FindChildElementByTag( lPropertyNode, COLLADA_FXSTD_COLOR_ELEMENT );
        xmlUnlinkNode( lColor );
        xmlFreeNode( lColor );
    }

    return true;
} // AddMaterialTextureInput


xmlNode* FbxWriterCollada::ExportTexture(FbxFileTexture *pTexture, FbxString pImageId, int pLayerIndex) {

    xmlNode *lTextureNode = xmlNewNode(NULL, XML_STR COLLADA_FXSTD_SAMPLER_ELEMENT);
    xmlNewProp(lTextureNode, XML_STR COLLADA_FXSTD_TEXTURE_ATTRIBUTE, XML_STR pImageId.Buffer());
    FbxString lBindChannel( "CHANNEL" );
    lBindChannel += FbxString(pLayerIndex);
    xmlNewProp(lTextureNode, XML_STR COLLADA_FXSTD_TEXTURESET_ATTRIBUTE, XML_STR lBindChannel.Buffer());

    // add an extra node and a "MAYA" technique for extra parameter 
    xmlNode *lExtra = xmlNewChild(lTextureNode, NULL, XML_STR COLLADA_EXTRA_STRUCTURE, NULL);
    xmlNode *lMayaTechnique = xmlNewChild(lExtra, NULL, XML_STR COLLADA_TECHNIQUE_STRUCTURE, NULL);
    xmlNewProp(lMayaTechnique, XML_STR COLLADA_PROFILE_PROPERTY, XML_STR COLLADA_MAYA_PROFILE);

    xmlNode* lParam;
    FbxString lSid;
    
    // Parameters both supported by COLLADA and FBX:
    // - blend mode
    // - wrap in U and in V
    // the rest is not supported.
    FbxTexture::EWrapMode lWrapU, lWrapV;
    FbxTexture::EBlendMode lBlendMode;

    lWrapU = pTexture->GetWrapModeU();
    lWrapV = pTexture->GetWrapModeV();
    lBlendMode = pTexture->GetBlendMode();

    // WrapU
    lParam = xmlNewChild(lMayaTechnique, NULL, XML_STR COLLADA_TEXTURE_WRAPU_PARAMETER, 
            (lWrapU == FbxTexture::eClamp) ? XML_STR COLLADA_FALSE_KEYWORD : XML_STR COLLADA_TRUE_KEYWORD);
    lSid = FbxString(COLLADA_TEXTURE_WRAPU_PARAMETER) + pLayerIndex;
    xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR FbxString(lSid).Buffer());

    // WrapV
    lParam = xmlNewChild(lMayaTechnique, NULL, XML_STR COLLADA_TEXTURE_WRAPV_PARAMETER, 
        (lWrapV == FbxTexture::eClamp) ? XML_STR COLLADA_FALSE_KEYWORD : XML_STR COLLADA_TRUE_KEYWORD);
    lSid = FbxString(COLLADA_TEXTURE_WRAPV_PARAMETER) + pLayerIndex;
    xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR FbxString(lSid).Buffer());

    // Blend mode
    if (lBlendMode == FbxTexture::eTranslucent) {
        lParam = xmlNewChild(lMayaTechnique, NULL, XML_STR COLLADA_TEXTURE_BLEND_MODE_PARAMETER_14, XML_STR "NONE");
    } else if (lBlendMode == FbxTexture::eAdditive) {
        lParam = xmlNewChild(lMayaTechnique, NULL, XML_STR COLLADA_TEXTURE_BLEND_MODE_PARAMETER_14, XML_STR "ADD");
    } else {
        lParam = xmlNewChild(lMayaTechnique, NULL, XML_STR COLLADA_TEXTURE_BLEND_MODE_PARAMETER_14, XML_STR "ADD");
    }

    return lTextureNode;
}

const FbxString FbxWriterCollada::ExportImage(FbxFileTexture * pTexture)
{
    FbxString lName = pTexture->GetNameWithoutNameSpacePrefix();
    FbxString lImageId = lName + "-" + COLLADA_IMAGE_STRUCTURE;

    if (!mLibraryImage)
    {
        mLibraryImage = xmlNewNode(NULL, XML_STR COLLADA_LIBRARY_IMAGE_ELEMENT);
    }

    xmlNode* lImageNode = DAE_FindChildElementByAttribute(mLibraryImage,
        COLLADA_ID_PROPERTY, lImageId);

    if (!lImageNode)
    {
        lImageNode = DAE_AddChildElement(mLibraryImage, COLLADA_IMAGE_STRUCTURE);
        DAE_AddAttribute(lImageNode, COLLADA_ID_PROPERTY, lImageId);
        DAE_AddAttribute(lImageNode, COLLADA_NAME_PROPERTY, lName);
        FbxString lSourceStr = FbxString("file://") + pTexture->GetFileName();
        // Replace pipe in strings of type "C|\Documents\... " by "C:\Documents\... "
        lSourceStr.FindAndReplace("|", ":");
        DAE_AddChildElement(lImageNode, COLLADA_INITFROM_ELEMENT, lSourceStr);
    }

    return lImageId;
}

void FbxWriterCollada::ExportPropertyValue(const FbxProperty & pProperty,
                                            xmlNode * pParentElement)
{
    int lTextureCount = pProperty.GetSrcObjectCount<FbxFileTexture>();
    if (lTextureCount)
    {
        xmlNode * lSurfaceElement = DAE_AddChildElement(pParentElement,
            COLLADA_FXCMN_SURFACE_ELEMENT);
        DAE_AddAttribute(lSurfaceElement, COLLADA_TYPE_PROPERTY, "2D");

        FbxFileTexture * lTexture = pProperty.GetSrcObject<FbxFileTexture>();
        FbxString lImageID = ExportImage(lTexture);
        DAE_AddChildElement(lSurfaceElement, COLLADA_INITFROM_ELEMENT, lImageID);
        return;
    }

    FbxDataType lType = pProperty.GetPropertyDataType();
    const char * lTypeName = lType.GetName();
    if (lType == FbxIntDT)
    {
        int lValue = pProperty.Get<FbxInt>();
        DAE_AddChildElement(pParentElement, COLLADA_INT_TYPE, lValue);
    }
    else if (lType == FbxDouble3DT)
    {
        FbxDouble3 lValue = pProperty.Get<FbxDouble3>();
        FbxString lValueRep = FbxString(lValue[0]) + " " +
            FbxString(lValue[1]) + " " + FbxString(lValue[2]);
        DAE_AddChildElement(pParentElement, COLLADA_FLOAT3_TYPE, lValueRep);
    }
    else if (lType == FbxFloatDT)
    {
        FbxFloat lValue = pProperty.Get<FbxFloat>();
        DAE_AddChildElement(pParentElement, COLLADA_FLOAT_TYPE, lValue);
    }
    else if (lType == FbxTransformMatrixDT)
    {
        FbxAMatrix lValue = pProperty.Get<FbxAMatrix>();
        DAE_AddChildElement(pParentElement, COLLADA_MATRIX_TYPE, lValue);
    }
    else if (lType == FbxStringDT)
    {
        FbxString lValue = pProperty.Get<FbxString>();
        DAE_AddChildElement(pParentElement, COLLADA_STRING_TYPE, lValue);
    }
    else
    {
        FBX_ASSERT_NOW("Unknown property type");
    }
}

bool FbxWriterCollada::ExportMeshTextures(FbxMesh *pMesh) {
    // Export textures used by the mesh pMesh.
    int i, lLayerIndex, lTexIndex, lNbTex, lPolygonTextureIndex, lMaterialIndex;
    FbxLayer *lLayer;
    FbxLayerElement::EMappingMode lMappingMode;
    FbxLayerElement::EReferenceMode lReferenceMode;
    FbxFileTexture *lTexture;
    FbxSurfaceMaterial* lMaterial;
    FbxString lImageId, lTextureName;
    FbxArray<FbxTexture*> lTextureArray;
    FbxArray<FbxSurfaceMaterial*> lUsedMaterialArray;
    FbxNode* lNode = pMesh->GetNode();

    int lNbLayer = pMesh->GetLayerCount();
    for (lLayerIndex = 0; lLayerIndex < lNbLayer; lLayerIndex++) 
    {
        lLayer = pMesh->GetLayer(lLayerIndex);
        if (!lLayer) continue;

        int lTextureTypeCount = sizeof( sLayerTypes ) / sizeof( FbxLayerElement::EType );
        int lTextureTypeIndex;
        for ( lTextureTypeIndex = 0; lTextureTypeIndex < lTextureTypeCount; ++lTextureTypeIndex )
        {
            FbxLayerElementTexture *lLayerElementTexture = lLayer->GetTextures( sLayerTypes[lTextureTypeIndex]);
            if (!lLayerElementTexture) 
                continue;

            FbxString lColladaName;
            GetColladaName( lTextureTypeIndex, lColladaName );

            lMappingMode = lLayerElementTexture->GetMappingMode();
            lReferenceMode = lLayerElementTexture->GetReferenceMode();

             // Number of textures in this current layer. Some textures could be used more than once.
            if (   lReferenceMode == FbxLayerElement::eDirect
                || lReferenceMode == FbxLayerElement::eIndexToDirect) {
                        lNbTex = lLayerElementTexture->GetDirectArray().GetCount();
            } else {
                mScene->FillTextureArray(lTextureArray);
                lNbTex = lTextureArray.GetCount();
            }

            for (lTexIndex = 0; lTexIndex < lNbTex; lTexIndex++) 
            {
                if (   lReferenceMode == FbxLayerElement::eDirect
                    || lReferenceMode == FbxLayerElement::eIndexToDirect) {
                    lTexture = FbxCast<FbxFileTexture>(lLayerElementTexture->GetDirectArray().GetAt(lTexIndex));
                } else {
                    lTexture = FbxCast<FbxFileTexture>(lTextureArray[lTexIndex]);
                }

                if( !lTexture )
                {
                    FbxString msg = FbxString("Could not find texture ") + lTexIndex;
                    msg += FbxString(" of layer ") + lLayerIndex;
                    if (pMesh->GetNode()) {
                        msg += " in mesh for node " + pMesh->GetNode()->GetNameWithoutNameSpacePrefix() + ".";
                    }
                    AddNotificationWarning( msg );
                    continue;
                }

                FbxString lName = lTexture->GetNameWithoutNameSpacePrefix();
                lTextureName = lName + "-" + lColladaName; // To not confuse texture id with image id
                lImageId = ExportImage(lTexture);

                // The texture has to be linked to a material in Collada.
                // If the texture does not use a material, create a new material.
                // Else, if the layer already has a material, link the texture to it. 
                // Else, if a previous layer already has a material, link the texture to it.
                // Else, create a new material.
                lUsedMaterialArray.Clear();

                if (lTexture->GetMaterialUse() == FbxFileTexture::eDefaultMaterial) {
                    // Texture does not use material, so create a new material.
                    // Actually the material should already have been created and it
                    // would be more efficient to just find it and use it. TODO.
                    FbxString lMaterialName = lTextureName + "-Material";
                    lMaterial = FbxSurfaceMaterial::Create(&mManager, lMaterialName.Buffer());
                    lUsedMaterialArray.Add(lMaterial);
                    if (!lNode->IsConnectedSrcObject(lMaterial))
                    {
                        lNode->ConnectSrcObject(lMaterial);
                    }

                } else {
                    // Find the right material to export the texture
                    FbxLayerElementMaterial *lLayerElementMaterial = lLayer->GetMaterials();
                    if (!lLayerElementMaterial) {
                        // No materials on this layer.
                        // Look for materials on previous layers.
                        for (i = 0; i < lLayerIndex; i++) {
                            lLayerElementMaterial = pMesh->GetLayer(i)->GetMaterials();
                            if (lLayerElementMaterial) break;
                        }
                    }

                    if (!lLayerElementMaterial) {
                        // No materials at all on this mesh, this means we have to create a material to support the texture.
                        FbxString lMaterialName = lTextureName + "-Material";
                        // Actually the material should already have been created and it
                        // would be more efficient to just find it and use it. TODO.
                        lMaterial = FbxSurfaceMaterial::Create(&mManager, lMaterialName.Buffer());
                        lUsedMaterialArray.Add(lMaterial);
                        if (!lNode->IsConnectedSrcObject(lMaterial))
                        {
                            lNode->ConnectSrcObject(lMaterial);
                        }
                    } 
                    else 
                    {
                        FbxLayerElement::EMappingMode lMaterialMappingMode = lLayerElementMaterial->GetMappingMode();
                        // Get the material for the texture
                        // if the texture mapping is ALL_SAME, the first material will do.
                        // if the texture mapping is BY_POLYGON, find all polygons using that texture;
                        // then find which material each polygon is using.
                        if (lMappingMode == FbxLayerElement::eAllSame || lMaterialMappingMode == FbxLayerElement::eAllSame) 
                        {
                            if (lLayerElementMaterial->GetReferenceMode() == FbxLayerElement::eDirect)
                            {                                
                                lMaterial = lNode->GetMaterial(0);
                            } 
                            //To include eIndexToDirect case, where GetDirectArray().GetCount() > 0
                            else if(lLayerElementMaterial->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                            {
                                lMaterialIndex = lLayerElementMaterial->GetIndexArray().GetAt(0);                                
                                lMaterial = lNode->GetMaterial(lMaterialIndex);
                            }//~
                            else 
                            {                                
                                if (lNode->GetMaterialCount() > 0)
                                {                                   
                                    FbxArray<FbxSurfaceMaterial*> lMaterialArray;                
                                    mScene->FillMaterialArray(lMaterialArray);
                                    lMaterialIndex = lLayerElementMaterial->GetIndexArray().GetAt(0);
                                    lMaterial = lMaterialArray.GetAt(lMaterialIndex);

                                    FBX_ASSERT_NOW("What the ....!");
                              }
                                else
                                {
                                    lMaterialIndex = lLayerElementMaterial->GetIndexArray().GetAt(0);                                    
                                    lMaterial = lNode->GetMaterial(lMaterialIndex);
                                }
                            }
                            lUsedMaterialArray.Add(lMaterial);

                        }

                        //we should check if the material is by polygon, we don't really care
                        //if the textures are by polygon as the mapping mode for the material and the texture
                        //are independant...
                        else if (lMaterialMappingMode == FbxLayerElement::eByPolygon) 
                        {
                            for (i = 0; i < pMesh->GetPolygonCount(); i++) {
                                if (lReferenceMode == FbxLayerElement::eDirect) {
                                    lPolygonTextureIndex = lTexIndex;
                                } else {
                                    lPolygonTextureIndex = lLayerElementTexture->GetIndexArray().GetAt(i);
                                }
                                if (lPolygonTextureIndex == lTexIndex) { 
                                    // found first polygon using the texture
                                    if (lLayerElementMaterial->GetReferenceMode() == FbxLayerElement::eDirect) {                                        
                                        lMaterial = lNode->GetMaterial(i);
                                    } 
                                    else if (lLayerElementMaterial->GetReferenceMode() == FbxLayerElement::eIndexToDirect) 
                                    {
                                        lMaterialIndex = lLayerElementMaterial->GetIndexArray().GetAt(i);                                       
                                        lMaterial = lNode->GetMaterial(lMaterialIndex);
                                    } 
                                    else 
                                    {
                                        FbxArray<FbxSurfaceMaterial*> lMaterialArray;                
                                        mScene->FillMaterialArray(lMaterialArray);
                                        lMaterialIndex = lLayerElementMaterial->GetIndexArray().GetAt(i);
                                        lMaterial = lMaterialArray[lMaterialIndex];
                                    }
                                    lUsedMaterialArray.Add(lMaterial);
                                }
                            }
                        } 
                        else 
                        {
                            // unsupported mapping mode.
                            FbxString msg = FbxString("Mapping mode not supported for textures: ") + lMappingMode;
                            AddNotificationWarning( msg );
                            // use first material of the mesh, if available                           
                            if (lNode->GetMaterialCount() > 0) {                      
                               
                                lMaterial = lNode->GetMaterial(0);
                            } 
                            else 
                            {
                                msg = FbxString("Could not find a material to export the texture ") + lTexIndex;
                                msg += FbxString(" of layer ") + lLayerIndex;
                                if (pMesh->GetNode()) 
                                {
                                    msg += " in mesh " + pMesh->GetNode()->GetNameWithoutNameSpacePrefix() + ".";
                                }
                                AddNotificationError( msg );
                                return false;
                            }
                        }
                    }
                }

                // Export the materials if needed
                for (lMaterialIndex = 0; lMaterialIndex < lUsedMaterialArray.GetCount(); lMaterialIndex++) 
                {
                    lMaterial = lUsedMaterialArray[lMaterialIndex];
                    xmlNode *lXmlMaterial = ExportMaterial(lMaterial);
                    if (!lXmlMaterial) return false;
                    // Add the texture as input to the effect.
                    mStatus = AddMaterialTextureInput(lXmlMaterial, lTexture, lImageId, lLayerIndex, lTextureTypeIndex);
                    if (!mStatus) return false;
                }//for lmaterial
            }//for ltexindex
        }//for texture type index
    }//for layer index

    return true;
} // ExportMeshTextures

xmlNode* FbxWriterCollada::ExportCamera(const FbxNode* pNode)
{
    XmlNodePtr lCameraElement(DAE_NewElement(COLLADA_CAMERA_STRUCTURE));
    if (!lCameraElement) return NULL;

    const FbxCamera* lCamera = pNode->GetCamera();
    if (!lCamera) {
        FbxString msg = FbxString("Could not get camera for node ") + pNode->GetName();
        AddNotificationError( msg );
        return NULL;
    }
    
    FbxString lCameraName(lCamera->GetName());
    if (lCameraName.IsEmpty())
        lCameraName = FbxString(pNode->GetName()) + "-camera";

    DAE_AddAttribute(lCameraElement, COLLADA_ID_PROPERTY, lCameraName.Buffer());
    DAE_AddAttribute(lCameraElement, COLLADA_NAME_PROPERTY, lCameraName.Buffer());

    // camera has an optics
    xmlNode* lOptics = xmlNewChild(lCameraElement, NULL, XML_STR COLLADA_OPTICS_STRUCTURE, NULL);
    // optics has a common technique
    xmlNode* lTechnique = xmlNewChild(lOptics, NULL, XML_STR COLLADA_TECHNIQUE_COMMON_ELEMENT, NULL);

    // technique has a perspective or orthographic depending on the
    // camera's projection type
    xmlNode* lProjectionType;
    if (lCamera->ProjectionType.Get() == FbxCamera::eOrthogonal) {
        lProjectionType = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_CAMERA_ORTHO_ELEMENT, NULL);
    } else {
        lProjectionType = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_CAMERA_PERSP_ELEMENT, NULL);
    } 

    // lProjectionType has the camera's parameters
    xmlNode* lParam;
    
    // Export field of view.
    if (lCamera->ProjectionType.Get() == FbxCamera::ePerspective) {

        FbxCamera::EApertureMode lCamApertureMode = lCamera->GetApertureMode();

        switch ( lCamApertureMode )
        {
        case FbxCamera::eHorizAndVert:
            {
                double lXFOV = lCamera->FieldOfViewX.Get();
                lParam = xmlNewChild(lProjectionType, NULL, XML_STR COLLADA_XFOV_CAMERA_PARAMETER, XML_STR FbxString(lXFOV).Buffer());
                xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_XFOV_CAMERA_PARAMETER);
                double lYFOV = lCamera->FieldOfViewY.Get();
                lParam = xmlNewChild(lProjectionType, NULL, XML_STR COLLADA_YFOV_CAMERA_PARAMETER, XML_STR FbxString(lYFOV).Buffer());
                xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_YFOV_CAMERA_PARAMETER);
            }
            break;
        case FbxCamera::eHorizontal:
            {
                double lXFOV = lCamera->FieldOfView.Get();
                lParam = xmlNewChild(lProjectionType, NULL, XML_STR COLLADA_XFOV_CAMERA_PARAMETER, XML_STR FbxString(lXFOV).Buffer());
                xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_XFOV_CAMERA_PARAMETER);
            }
            break;
        case FbxCamera::eVertical:
            {
                double lYFOV = lCamera->FieldOfView.Get();
                lParam = xmlNewChild(lProjectionType, NULL, XML_STR COLLADA_YFOV_CAMERA_PARAMETER, XML_STR FbxString(lYFOV).Buffer());
                xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_YFOV_CAMERA_PARAMETER);
            }
            break;
        case FbxCamera::eFocalLength:
            {
                double lXFOV = lCamera->ComputeFieldOfView( lCamera->FocalLength.Get() );
                lParam = xmlNewChild(lProjectionType, NULL, XML_STR COLLADA_XFOV_CAMERA_PARAMETER, XML_STR FbxString(lXFOV).Buffer());
                xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_XFOV_CAMERA_PARAMETER);
            }
            break;
        default:
            {
                FbxString msg = FbxString("Warning: unknown camera aperture mode for camera: ");
                msg += lCameraName;
                AddNotificationWarning( msg );
            }
            break;
        }
    }

    // Horizontal magnification, in case of an orthogonal camera
    if (lCamera->ProjectionType.Get() == FbxCamera::eOrthogonal) {
        double lOrthoZoom = lCamera->OrthoZoom.Get();
        lParam = xmlNewChild(lProjectionType, NULL, XML_STR COLLADA_XMAG_CAMERA_PARAMETER, XML_STR FbxString(lOrthoZoom).Buffer());
        xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_XMAG_CAMERA_PARAMETER);
    }

    // Aspect Ratio
    double lAspectRatio = lCamera->GetApertureWidth() / lCamera->GetApertureHeight();
    lParam = xmlNewChild(lProjectionType, NULL, XML_STR COLLADA_ASPECT_CAMERA_PARAMETER, XML_STR FbxString(lAspectRatio).Buffer());

    // Near and Far planes
    double lNearPlane = lCamera->GetNearPlane();
    lParam = xmlNewChild(lProjectionType, NULL, XML_STR COLLADA_ZNEAR_CAMERA_PARAMETER, XML_STR FbxString(lNearPlane).Buffer());
    xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_ZNEAR_CAMERA_PARAMETER);

    double lFarPlane = lCamera->GetFarPlane();
    lParam = xmlNewChild(lProjectionType, NULL, XML_STR COLLADA_ZFAR_CAMERA_PARAMETER, XML_STR FbxString(lFarPlane).Buffer());
    xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_ZFAR_CAMERA_PARAMETER);

    // "FCOLLADA" technique has the camera's parameters: Horizontal and vertical apertures;
    // FBX is in centimeters, Maya in inches.
    xmlNode * lFCOLLADATechnique = DAE_AddChildElement(lOptics, COLLADA_TECHNIQUE_STRUCTURE);
    DAE_AddAttribute(lFCOLLADATechnique, COLLADA_PROFILE_PROPERTY, COLLADA_FCOLLADA_PROFILE);

    double lApertureHeight = centimetersToInches(lCamera->GetApertureHeight());
    DAE_AddChildElement(lFCOLLADATechnique, COLLADA_CAMERA_VERTICAL_APERTURE_PARAMETER, lApertureHeight);
    double lApertureWidth = centimetersToInches(lCamera->GetApertureWidth());
    DAE_AddChildElement(lFCOLLADATechnique, COLLADA_CAMERA_HORIZONTAL_APERTURE_PARAMETER, lApertureWidth);
    double lSqueezeRatio = lCamera->GetSqueezeRatio();
    DAE_AddChildElement(lFCOLLADATechnique, COLLADA_CAMERA_LENS_SQUEEZE_PARAMETER, lSqueezeRatio);

    return lCameraElement.Release();
}

xmlNode* FbxWriterCollada::ExportLight(const FbxNode* pNode)
{
    XmlNodePtr lLightElement(DAE_NewElement(COLLADA_LIGHT_STRUCTURE));
    if (!lLightElement) return NULL;

    const FbxLight* lLight = NULL;
    if (pNode)
    {
        lLight = pNode->GetLight();
        if (!lLight) {
            FbxString msg = FbxString("Could not get light for node ") + pNode->GetName();
            AddNotificationError( msg );
            return NULL;
        }
    }

    FbxString lLightName(lLight ? lLight->GetName() : AMBIENT_LIGHT_NAME);
    if (lLightName.IsEmpty())
        lLightName = FbxString(pNode->GetName()) + "-light";

    DAE_AddAttribute(lLightElement, COLLADA_ID_PROPERTY, lLightName.Buffer());
    DAE_AddAttribute(lLightElement, COLLADA_NAME_PROPERTY, lLightName.Buffer());

    // light has a common technique
    xmlNode* lTechnique = xmlNewChild(lLightElement, NULL, XML_STR COLLADA_TECHNIQUE_COMMON_ELEMENT, NULL);

    // Light property: DIRECTIONAL, POINT, SPOT
    xmlNode *lXmlLightType = NULL;
    if ( lLight )
    {
        FbxLight::EType lLightType = lLight->LightType.Get();
        if (lLightType == FbxLight::eDirectional) {
            lXmlLightType = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_LIGHT_DIRECTIONAL_ELEMENT, NULL);
        } else if (lLightType == FbxLight::ePoint) {
            lXmlLightType = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_LIGHT_POINT_ELEMENT, NULL);
        } else if (lLightType == FbxLight::eSpot) {
            lXmlLightType = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_LIGHT_SPOT_ELEMENT, NULL);
        } else {
            FbxString msg = FbxString("Unknown light type: ") + lLightType;
            AddNotificationError( msg );
            FbxDelete(lTechnique);
            return NULL;
        }

        // light type has the light's parameters
        xmlNode* lParam;
        FbxVector4 lColor;

        lColor = lLight->Color.Get();
        FbxString lStrColor = FbxString(lColor[0]) + " " + FbxString(lColor[1]) + " " + FbxString(lColor[2]);
        lParam = xmlNewChild(lXmlLightType, NULL, XML_STR COLLADA_COLOR_LIGHT_PARAMETER, XML_STR lStrColor.Buffer());
        xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR "color");

        if (lLightType == FbxLight::eSpot) {
            double lConeAngle = lLight->OuterAngle.Get();
            FbxString    lConeAngleString = FbxString(lConeAngle);
            lParam = xmlNewChild(lXmlLightType, NULL, XML_STR COLLADA_FALLOFFANGLE_LIGHT_PARAMETER, XML_STR lConeAngleString.Buffer());
            xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR "angle");
        }

        // add a "MAYA" technique for intensity parameter 
        xmlNode* lMayaTechnique = xmlNewChild(lLightElement, NULL, XML_STR COLLADA_TECHNIQUE_STRUCTURE, NULL);
        xmlNewProp(lMayaTechnique, XML_STR COLLADA_PROFILE_PROPERTY, XML_STR COLLADA_MAYA_PROFILE);

        // FBX units are 100x those of Maya
        double lIntensity = lLight->Intensity.Get() / 100.0;
        FbxString lIntensityString = FbxString(lIntensity);
        lParam = xmlNewChild(lMayaTechnique, NULL, XML_STR COLLADA_LIGHT_INTENSITY_PARAMETER_14, XML_STR lIntensityString.Buffer());
        xmlNewProp(lParam, XML_STR COLLADA_SUBID_PROPERTY, XML_STR COLLADA_LIGHT_INTENSITY_PARAMETER_14);
    }
    else
    {
        // We are exporting the global ambient
        FbxColor lGlobalAmbient( mScene->GetGlobalSettings().GetAmbientColor() );
        lXmlLightType = xmlNewChild(lTechnique, NULL, XML_STR COLLADA_LIGHT_AMBIENT_ELEMENT, NULL);

        FbxString lStrColor = FbxString(lGlobalAmbient.mRed) + " " + FbxString(lGlobalAmbient.mGreen) + " " + FbxString(lGlobalAmbient.mBlue);
        xmlNode* lParam = xmlNewChild(lXmlLightType, NULL, XML_STR COLLADA_COLOR_LIGHT_PARAMETER, XML_STR lStrColor.Buffer());
    }

    return lLightElement.Release();
}

bool FbxWriterCollada::ExportController(FbxMesh *pMesh) {
    // Export controller in case of a binded skin.
    // For a morph controller, use ExportControllerShape.
    int i, j, k, lControlPointIndex, lLinkIndex, lJointIndex;

    // Only one controller will be created per mesh.
    // We can thus give the mesh name to the controller.
    FbxString lName                = pMesh->GetNode()->GetNameWithoutNameSpacePrefix();
    FbxString lId                    = lName + "Controller";
    FbxString lTargetUrl            = FbxString("#") + lName + "-lib";
    FbxString    lJointsId            = lId + "-Joints";
    FbxString    lMatricesId            = lId + "-Matrices";
    FbxString    lWeightsId            = lId + "-Weights";

    // Update the CONTROLLER library.
    // If the Controller library is not already created, create it.
    if (!mLibraryController) {
        mLibraryController = xmlNewNode(NULL, XML_STR COLLADA_LIBRARY_CONTROLLER_ELEMENT);
    }

    // Create controller and add it to the library
    xmlNode* lController = xmlNewChild(mLibraryController, NULL, XML_STR COLLADA_CONTROLLER_STRUCTURE, NULL);
    xmlNewProp(lController, XML_STR COLLADA_ID_PROPERTY, XML_STR lId.Buffer());

    // Controller has a skin
    //According to Collada Specification 1.4.1, skin controller should target at morph controller if any Morph. 
    if(pMesh->GetShapeCount()>0){
        lTargetUrl = FbxString("#") + lName + "-lib-morph";
    }
    //~
    xmlNode* lSkin = xmlNewChild(lController, NULL, XML_STR COLLADA_SKIN_STRUCTURE, NULL);
    xmlNewProp(lSkin, XML_STR COLLADA_SOURCE_PROPERTY, XML_STR lTargetUrl.Buffer());

    // Bind shape matrix
    FbxAMatrix lTransformMatrix;

    //on cluster 0 of skin 0
    ((FbxSkin *)pMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetTransformMatrix(lTransformMatrix);
    
    
    FbxString lStrMatrix = matrixToString(lTransformMatrix);
    xmlNode *lBindShapeMatrix = xmlNewChild(lSkin, NULL, XML_STR COLLADA_BINDSHAPEMX_SKIN_PARAMETER, XML_STR lStrMatrix.Buffer());

    // Joints source
    FbxStringList lJointsNames;
    bool lWarningLinkShowed = false;

    int lDeformerCount = pMesh->GetDeformerCount(FbxDeformer::eSkin);
    if(lDeformerCount > 1)
        FBX_ASSERT_NOW("Unexpected number of skin greater than 1");
    //it is expected for lDeformerCount to be equal to 1
    for(i=0; i<lDeformerCount; ++i)
    {
        
        for (lJointIndex = 0; lJointIndex < ((FbxSkin*)pMesh->GetDeformer(i, FbxDeformer::eSkin))->GetClusterCount(); lJointIndex++) 
        {
            FbxCluster* lLink= ((FbxSkin*)pMesh->GetDeformer(i, FbxDeformer::eSkin))->GetCluster(lJointIndex);
        if (!lLink || !lLink->GetLink()) {
            if (!lWarningLinkShowed) {
                FbxString msg = FbxString("Not able to export all links for mesh ") + lName;
                AddNotificationWarning( msg );
                lWarningLinkShowed = true;
            }
            continue;
        }

        FbxString lJointName = lLink->GetLink()->GetNameWithoutNameSpacePrefix();

        if (lLink->GetLinkMode() != FbxCluster::eTotalOne &&
            lLink->GetLinkMode() != FbxCluster::eNormalize) 
        {
            FbxString msg = FbxString("link ") + lJointName;
            msg += "'s mode is not supported. It will be treated as mode eTotalOne.";
            AddNotificationWarning( msg );
            lWarningLinkShowed = true;
        }

        lJointsNames.Add(lJointName.Buffer());
        }//lJointIndex for 
    }//lDeformer for
    xmlNode *lXmlJointsSource = DAE_ExportSource14(lSkin, lJointsId.Buffer(), lJointsNames, COLLADA_NAME_TYPE, true); 

    // Link matrices source
    FbxArray<FbxMatrix> lMatrices;
    for(k=0; k<lDeformerCount; ++k)
    {
        for (lJointIndex = 0; lJointIndex < ((FbxSkin*)pMesh->GetDeformer(k, FbxDeformer::eSkin))->GetClusterCount(); lJointIndex++) {
            FbxCluster* lLink=((FbxSkin*)pMesh->GetDeformer(k, FbxDeformer::eSkin))->GetCluster(lJointIndex);
        if (!lLink || !lLink->GetLink()) continue;

        FbxAMatrix lTransformLink;
        lLink->GetTransformLinkMatrix(lTransformLink);
        FbxAMatrix lTransformLinkInverse = lTransformLink.Inverse();
        // We have to convert the FbxAMatrix to a FbxMatrix or else we lose information when transposing.
        FbxMatrix lMatrix;
        for (i = 0; i < 4; i++)
            for (j = 0; j < 4; j++)
                lMatrix.mData[i][j] = lTransformLinkInverse.mData[i][j];
        lMatrix = lMatrix.Transpose();
        lMatrices.Add(lMatrix);
        }//end lJointIndex for
    }//end lDeformer for
    xmlNode *lXmlMatricesSource = DAE_ExportSource14(lSkin, lMatricesId.Buffer(), lMatrices);

    // Weights source; also gather information (vcount and indices) that will be used for vertex weights.
    FbxArray<double> lWeights;
    lWeights.Add(1.0);
    int lVCount = 0;
    int lNbVCount = 0;
    int lNbControlPoints = pMesh->GetControlPointsCount();
    FbxString lStrVCount, lStrWeights;

    for (lControlPointIndex = 0; lControlPointIndex < lNbControlPoints; lControlPointIndex++) 
    {
        
        lVCount = 0;

        for (k=0; k<lDeformerCount; ++k)
        {

            for (lLinkIndex = 0; lLinkIndex < ((FbxSkin*)pMesh->GetDeformer(k, FbxDeformer::eSkin))->GetClusterCount(); lLinkIndex++) 
            {
                
                FbxCluster* lLink=((FbxSkin*)pMesh->GetDeformer(k, FbxDeformer::eSkin))->GetCluster(lLinkIndex);
            if (!lLink || !lLink->GetLink()) continue;

            // Loop on control points used by the link?
            int* lLinkControlPointIndices = lLink->GetControlPointIndices();
            if (!lLinkControlPointIndices) continue;

            // Find if this control point is used by this link... Not optimized, but should work.
            int lIndex = -1;
            for (i = 0; i < lLink->GetControlPointIndicesCount(); i++) {
                if (lLinkControlPointIndices[i] == lControlPointIndex) {
                    lIndex = i;
                    break;
                }
            }
            if (lIndex == -1) continue;

            double* lLinkWeights = lLink->GetControlPointWeights();
            if (!lLinkWeights) continue;

            int idx;
            float weight = (float) lLinkWeights[lIndex];
            idx = lWeights.GetCount();
            lWeights.Add(weight);
            
            lVCount++;
            if (lStrWeights.GetLen() > 0) lStrWeights += ' ';
            lStrWeights += FbxString(lLinkIndex) + ' ' + idx;
            }//end for lLinkIndex
        }//end lDeformer for
        if (lVCount > 0) {
            if (lStrVCount.GetLen() > 0) lStrVCount += ' ';
            lStrVCount += FbxString(lVCount);
            lNbVCount++;
        }
    }

    FbxStringList lAccessorParams;
    xmlNode *lXmlWeightsSource = DAE_ExportSource14(lSkin, lWeightsId.Buffer(), lAccessorParams, lWeights);

    // Joints and their matrices
    xmlNode *lXmlJoints = xmlNewChild(lSkin, NULL, XML_STR COLLADA_JOINTS_STRUCTURE, NULL);
    DAE_AddInput14(lXmlJoints, COLLADA_JOINT_SEMANTIC, lJointsId.Buffer());
    DAE_AddInput14(lXmlJoints, COLLADA_BIND_MATRIX_SEMANTIC, lMatricesId.Buffer());

    // Vertex weights
    xmlNode* lXmlVertexWeights = xmlNewChild(lSkin, NULL, XML_STR COLLADA_WEIGHTS_ELEMENT, NULL);
    FbxString lNbVCountString = FbxString(lNbVCount);
    xmlNewProp(lXmlVertexWeights, XML_STR COLLADA_COUNT_PROPERTY, XML_STR lNbVCountString.Buffer());
    DAE_AddInput14(lXmlVertexWeights, COLLADA_JOINT_SEMANTIC, lJointsId.Buffer(), 0);
    DAE_AddInput14(lXmlVertexWeights, COLLADA_WEIGHT_PARAMETER, lWeightsId.Buffer(), 1);
    xmlNode *lXmlVCount = xmlNewChild(lXmlVertexWeights, NULL, XML_STR COLLADA_VERTEXCOUNT_ELEMENT, XML_STR lStrVCount.Buffer());
    xmlNode *lXmlV = xmlNewChild(lXmlVertexWeights, NULL, XML_STR COLLADA_VALUE_STRUCTURE, XML_STR lStrWeights.Buffer());

    return true;
} // ExportController


bool FbxWriterCollada::ExportControllerShape(FbxMesh *pMesh) {
    // Export controller in case of a morph controller (shape deformer).

    FbxString lName                = pMesh->GetNode()->GetNameWithoutNameSpacePrefix();
    FbxString lId                    = lName + "-lib-morph";
    FbxString lTargetUrl            = FbxString("#") + lName + "-lib";
    FbxString    lShapesId            = lId + "-targets"; // yes, this always mixes me up too
    FbxString    lWeightsId            = lId + "-weights";

    int lVertexIndex;

    // Update the CONTROLLER library.
    // If the Controller library is not already created, create it.
    if (!mLibraryController) {
        mLibraryController = xmlNewNode(NULL, XML_STR COLLADA_LIBRARY_CONTROLLER_ELEMENT);
    }

    // Create controller and add it to the library
    xmlNode* lController = xmlNewChild(mLibraryController, NULL, XML_STR COLLADA_CONTROLLER_STRUCTURE, NULL);
    xmlNewProp(lController, XML_STR COLLADA_ID_PROPERTY, XML_STR lId.Buffer());

    // Controller has a morph
    xmlNode* lMorph = xmlNewChild(lController, NULL, XML_STR COLLADA_CONTROLLER_MORPH_ELEMENT, NULL);
    xmlNewProp(lMorph, XML_STR COLLADA_SOURCE_PROPERTY, XML_STR lTargetUrl.Buffer());
    // There are 2 methods: 
    // Normalized: (1-w1-w2-...)*BaseMesh + w1*Target1 + w2*Target2 + ...
    // Relative: BaseMesh + w1*Target1 + w2*Target2 + ...
    // FBX is Relative.
    xmlNewProp(lMorph, XML_STR "method", XML_STR "RELATIVE");

    // Targets (shapes) sources and
    // Shape weights.
    FbxStringList lShapesNames;
    FbxArray<double> lShapesWeights;

    //Collada does not support In-Between blend shape by default;
    //So for each blend shape channel, only take account of the first target shape.
    int lBlendShapeDeformerCount = pMesh->GetDeformerCount(FbxDeformer::eBlendShape);
    for(int lBlendShapeIndex = 0; lBlendShapeIndex<lBlendShapeDeformerCount; ++lBlendShapeIndex)
    {
        FbxBlendShape* lBlendShape = (FbxBlendShape*)pMesh->GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);

        int lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();
        for(int lChannelIndex = 0; lChannelIndex<lBlendShapeChannelCount; ++lChannelIndex)
        {
            FbxBlendShapeChannel* lChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);

            FbxShape* lShape = lChannel->GetTargetShape(0);
            FbxString lShapeName = lShape->GetName();
            lShapeName += "-lib";
            lShapesNames.Add(lShapeName.Buffer());

            double lWeight = lChannel->DeformPercent.Get();
            lShapesWeights.Add(lWeight/100);

        }//For each blend shape channel
    }//For each blend shape deformer

    xmlNode *lXmlShapesSource = DAE_ExportSource14(lMorph, lShapesId.Buffer(), lShapesNames, COLLADA_NAME_TYPE, true);
    FbxStringList lAccessorParams;
    lAccessorParams.Add("WEIGHT", 0);
    xmlNode *lXmlShapesWeights = DAE_ExportSource14(lMorph, lWeightsId.Buffer(), lAccessorParams, lShapesWeights);

    // Targets
    xmlNode* lXmlTargets = xmlNewChild(lMorph, NULL, XML_STR COLLADA_TARGETS_ELEMENT, NULL);
    DAE_AddInput14(lXmlTargets, COLLADA_MORPH_TARGET_SEMANTIC, lShapesId.Buffer() );
    DAE_AddInput14(lXmlTargets, COLLADA_MORPH_WEIGHT_SEMANTIC, lWeightsId.Buffer() );

    // Update internal info for later export of shape geometries, required for COLLADA.
    lBlendShapeDeformerCount = pMesh->GetDeformerCount(FbxDeformer::eBlendShape);
    for(int lBlendShapeIndex = 0; lBlendShapeIndex<lBlendShapeDeformerCount; ++lBlendShapeIndex)
    {
        FbxBlendShape* lBlendShape = (FbxBlendShape*)pMesh->GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);

        int lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();
        for(int lChannelIndex = 0; lChannelIndex<lBlendShapeChannelCount; ++lChannelIndex)
        {
            FbxBlendShapeChannel* lChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);
            FbxShape* lShape = lChannel->GetTargetShape(0);

            // Create a mesh deformed by the shape.
            FbxMesh *lMeshShape = FbxMesh::Create(&mManager, "");
            CopyMesh(lMeshShape, pMesh);

            // Deform the mesh vertices with the shape.
            int lNbVertex = lMeshShape->GetControlPointsCount();
            FbxVector4 *lShapeControlPoints = lShape->GetControlPoints();

            FbxVector4 *lShapeNormals = NULL;
            FbxLayerElementArrayTemplate<FbxVector4>* direct;
            lShape->GetNormals(&direct);
            if (direct)
                lShapeNormals = direct->GetLocked(lShapeNormals, FbxLayerElementArray::eReadLock);

            for (lVertexIndex = 0; lVertexIndex < lNbVertex; lVertexIndex++) {
                FbxVector4 lNewControlPoint = lShapeControlPoints[lVertexIndex];
                if (lShapeNormals)
                {
                    FbxVector4 lNewNormal = lShapeNormals[lVertexIndex];
                    lMeshShape->SetControlPointAt(lNewControlPoint, lNewNormal, lVertexIndex);
                }
                else
                    lMeshShape->SetControlPointAt(lNewControlPoint, lVertexIndex);
            }

            if (direct)
                direct->Release(&lShapeNormals, lShapeNormals);

            FbxString lTargetId = FbxString(lShape->GetName()) + "-lib";
            mShapeMeshesList->Add(lTargetId.Buffer(), (FbxHandle)lMeshShape);
        }//For each blend shape channel
    }//For each blend shape deformer

    return true;
} // ExportControllerShape


bool FbxWriterCollada::UpdateMeshLibraryWithShapes(xmlNode *pXmlNode) {
    // Create a mesh library for every shape, as needed.

    int lShapeIndex;

    int lShapeCount = mShapeMeshesList->GetCount();
    if (lShapeCount == 0) return true;

    // Find geometry library
    if (!mLibraryGeometry) {
        FbxString msg = FbxString("Could not find geometry library");
        AddNotificationError( msg );
        return false;
    }

    for (lShapeIndex = 0; lShapeIndex < mShapeMeshesList->GetCount(); lShapeIndex++)
    {
        FbxString lShapeId = mShapeMeshesList->GetStringAt(lShapeIndex);
        FbxString lModelNameStr = lShapeId;
        int lLibIndex = lShapeId.Find("-lib");
        if (lLibIndex >= 0) { lModelNameStr = lShapeId.Left(lLibIndex); }
        FbxMesh *lMeshShape = (FbxMesh*)mShapeMeshesList->GetReferenceAt(lShapeIndex);
        // Look if a mesh with this id already exists.
        xmlNode* lXmlGeometry = DAE_FindChildElementByAttribute(mLibraryGeometry, COLLADA_ID_PROPERTY, lShapeId);
        if (lXmlGeometry) continue;

        // Create a new geometry with this id.
        lXmlGeometry = ExportShapeGeometry(lMeshShape, lShapeId);
        xmlAddChild(mLibraryGeometry, lXmlGeometry);
    }

    return true;
} // UpdateMeshLibraryWithShapes


bool FbxWriterCollada::ExportAnimation(FbxNode* pNode) {

    int i;

    // If the node is animated, export its animation.
    bool isAnimated = FbxAnimUtilities::IsAnimated(pNode);
    if (isAnimated == false && pNode->GetNodeAttribute())
        isAnimated = FbxAnimUtilities::IsAnimated(pNode->GetNodeAttribute());

    if (isAnimated) {
        // If the geometry library doesn't exist yet, create it.
        if (!mLibraryAnimation) {
            mLibraryAnimation = xmlNewNode(NULL, XML_STR COLLADA_LIBRARY_ANIMATION_ELEMENT);
        }

        // Create an animation node
        FbxString lName = pNode->GetNameWithoutNameSpacePrefix();
        FbxString lId = lName + "-anim";
        xmlNode* lAnimationNode = xmlNewChild(mLibraryAnimation, NULL, XML_STR COLLADA_ANIMATION_STRUCTURE, XML_STR "");
        xmlNewProp(lAnimationNode, XML_STR COLLADA_ID_PROPERTY, XML_STR lId.Buffer());
        xmlNewProp(lAnimationNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR lName.Buffer());

        ExportAnimationCurves(pNode, lAnimationNode);

    }

    // Export animation recursively
    for (i = 0; i < pNode->GetChildCount(); i++) {
        mStatus = ExportAnimation(pNode->GetChild(i));
        if (!mStatus) return false;
    }

    return true;
} // ExportAnimation


bool FbxWriterCollada::ExportAnimationCurves(FbxNode* pNode, xmlNode* pAnimationNode) 
{
    FbxAnimCurve* lCurve;

    if ( mSingleMatrix )
    {
        if ( IsTranslationAnimated( pNode ) || IsRotationAnimated( pNode ) || IsScaleAnimated( pNode ))
        {
            // export sampled matrix animation
        
            FbxTimeSpan lTimeInterval(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
            pNode->GetAnimationInterval( lTimeInterval, mAnimStack );

            FbxTime lTime = lTimeInterval.GetStart();
            FbxNode * lParentNode = pNode->GetParent();
            xmlNode* lSubAnimationNode = xmlNewChild(pAnimationNode, NULL, XML_STR COLLADA_ANIMATION_STRUCTURE, XML_STR "");
            //int lKeyIndex;
            FbxString lNodeName        = DAE_GetElementAttributeValue(pAnimationNode, COLLADA_NAME_PROPERTY);

            FbxArray<double> lInputs;
            FbxArray<FbxAMatrix> lOutputs;
            FbxStringList lInterpolations;

            while(lTime < lTimeInterval.GetStop() + FBXSDK_TIME_EPSILON) //Don't miss the last frame
            {
                double lKeyT = lTime.GetSecondDouble();
                lInputs.Add( lKeyT );
                lInterpolations.Add(COLLADA_INTERPOLATION_TYPE_LINEAR);

                FbxAMatrix lThisLocal;
                //For Single Matrix situation, obtain transform matrix from eDestinationPivot, which include pivot offsets and pre/post rotations.
                FbxAMatrix& lThisGlobal = pNode->EvaluateGlobalTransform(lTime, FbxNode::eDestinationPivot);
                if ( lParentNode )
                {
                    //For Single Matrix situation, obtain transform matrix from eDestinationPivot, which include pivot offsets and pre/post rotations.
                    FbxAMatrix& lParentGlobal = lParentNode->EvaluateGlobalTransform(lTime, FbxNode::eDestinationPivot);
                    FbxAMatrix lParentInverted = lParentGlobal.Inverse();
                    lThisLocal = lParentInverted * lThisGlobal;
                }
                else
                {
                    lThisLocal = lThisGlobal;
                }

                // target all COLLADA_MATRIX_STRUCTURE - single matrix is written in row-major order in COLLADA document for human readability
                // when represents a transformation (TRS), the last column of the matrix represents the translation part of the transformation.
                lOutputs.Add( lThisLocal );

                lTime += mSamplingPeriod;
            }

            FbxStringList lAccessorParams;
            xmlNode* lXmlSource;

            // Source: Input - values

            FbxString lInputId = lNodeName + "-Matrix-animation-input";
            lAccessorParams.Add("TIME", 0);
            lXmlSource = DAE_ExportSource14(lSubAnimationNode, lInputId.Buffer(), lAccessorParams, lInputs);

            // Source: Output - values

            FbxString lOutputId = lNodeName + "-Matrix-animation-output-transform";
            lAccessorParams.Clear();
            lAccessorParams.Add("transform", 0); // stays at that value for the other sources
            lXmlSource = DAE_ExportSource14(lSubAnimationNode, lOutputId.Buffer(), lOutputs);

            // Source: Interpolation - values

            FbxString lInterpolationsId = lNodeName + "-Interpolations";
            lAccessorParams.Clear();
            lAccessorParams.Add("INTERPOLATION", 0); 
            lXmlSource = DAE_ExportSource14(lSubAnimationNode, lInterpolationsId.Buffer(), lInterpolations, COLLADA_NAME_TYPE, true);
            
            // Sampler

            FbxString lSamplerId = lNodeName + "-Matrix-animation-transform";
            xmlNode* lSampler = xmlNewChild(lSubAnimationNode, NULL, XML_STR COLLADA_SAMPLER_STRUCTURE, NULL);
            xmlNewProp(lSampler, XML_STR COLLADA_ID_PROPERTY, XML_STR lSamplerId.Buffer());
            DAE_AddInput14(lSampler, COLLADA_INPUT_SEMANTIC, lInputId);
            DAE_AddInput14(lSampler, COLLADA_OUTPUT_SEMANTIC, lOutputId);
            DAE_AddInput14(lSampler, COLLADA_INTERPOLATION_SEMANTIC, lInterpolationsId);

            // Channel to the animated part

            xmlNode* lChannel = xmlNewChild(lSubAnimationNode, NULL, XML_STR COLLADA_CHANNEL_STRUCTURE, NULL);
            FbxString lSamplerUrl = FbxString("#") + lSamplerId;
            FbxString lTarget = lNodeName + "/" + COLLADA_MATRIX_STRUCTURE;
            xmlNewProp(lChannel, XML_STR COLLADA_SOURCE_PROPERTY, XML_STR lSamplerUrl.Buffer());
            xmlNewProp(lChannel, XML_STR COLLADA_TARGET_PROPERTY, XML_STR lTarget.Buffer());

        }
    }
    else
    {
        const char * PropertyMapping[][4] = 
        {
            {"Lcl Translation", "X", COLLADA_TRANSLATE_STRUCTURE, "X"},
            {"Lcl Translation", "Y", COLLADA_TRANSLATE_STRUCTURE, "Y"},
            {"Lcl Translation", "Z", COLLADA_TRANSLATE_STRUCTURE, "Z"},
            {"Lcl Rotation", "X", COLLADA_ROTATE_X, "ANGLE"},
            {"Lcl Rotation", "Y", COLLADA_ROTATE_Y, "ANGLE"},
            {"Lcl Rotation", "Z", COLLADA_ROTATE_Z, "ANGLE"},
            {"Lcl Scaling", "X", COLLADA_SCALE_STRUCTURE, "X"},
            {"Lcl Scaling", "Y", COLLADA_SCALE_STRUCTURE, "Y"},
            {"Lcl Scaling", "Z", COLLADA_SCALE_STRUCTURE, "Z"},
        };
        const int lPropertyMappingEntryCount = sizeof(PropertyMapping) /
            sizeof(const char *[4]);
        
        // Only export curves with at least 2 keys.
        for (int lIndex = 0; lIndex < lPropertyMappingEntryCount; ++lIndex)
        {
            FbxProperty lProperty = pNode->FindProperty(PropertyMapping[lIndex][0]);
            FBX_ASSERT(lProperty.IsValid());
            lCurve = lProperty.GetCurve(mAnimLayer, PropertyMapping[lIndex][1]);
            if (lCurve && lCurve->KeyGetCount() >= 2)
            {
                AnimationElement lAnimationElement;
                lAnimationElement.FromFBX(lCurve);
                lAnimationElement.ToCOLLADA(mLibraryAnimation,
                    pNode->GetNameWithoutNameSpacePrefix(),
                    FbxString(PropertyMapping[lIndex][2]) + "."  + PropertyMapping[lIndex][3]);
            }
        }
    }

    // Visibility is not supported by Collada.
    lCurve = pNode->Visibility.GetCurve(mAnimLayer);
    if (lCurve && lCurve->KeyGetCount() >= 2) {
        ExportCurve(pAnimationNode, lCurve, "visibility", "");
    }

    FbxNodeAttribute *lNodeAttribute = pNode->GetNodeAttribute();
    if (!lNodeAttribute) 
    {
        return true; // finish here if no node attribute.
    }

    // Now export curves specific to certain node attribute types.

    // For the lights
    if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eLight) {

        FbxLight* light = pNode->GetLight();

        // Light color.
        lCurve = light->Color.GetCurve(mAnimLayer, FBXSDK_CURVENODE_COLOR_RED);
        if (lCurve && lCurve->KeyGetCount() >= 2) {
            ExportCurve(pAnimationNode, lCurve, COLLADA_COLOR_LIGHT_PARAMETER, "R", 0, 0, 1);
        }

        lCurve = light->Color.GetCurve(mAnimLayer, FBXSDK_CURVENODE_COLOR_GREEN);
        if (lCurve && lCurve->KeyGetCount() >= 2) {
            ExportCurve(pAnimationNode, lCurve, COLLADA_COLOR_LIGHT_PARAMETER, "G", 0, 0, 1);
        }

        lCurve = light->Color.GetCurve(mAnimLayer, FBXSDK_CURVENODE_COLOR_BLUE);
        if (lCurve && lCurve->KeyGetCount() >= 2) {
            ExportCurve(pAnimationNode, lCurve, COLLADA_COLOR_LIGHT_PARAMETER, "B", 0, 0, 1);
        }

        // Light intensity
        lCurve = light->Intensity.GetCurve(mAnimLayer);
        if (lCurve && lCurve->KeyGetCount() >= 2) {
            ExportCurve(pAnimationNode, lCurve, COLLADA_LIGHT_INTENSITY_PARAMETER_14, "", 0, 1, 1);
        }

        // Light outer angle
        lCurve = light->OuterAngle.GetCurve(mAnimLayer);
        if (lCurve && lCurve->KeyGetCount() >= 2) {
            ExportCurve(pAnimationNode, lCurve, "angle", "", 0, 0, 1);
        }
    }

    // For the cameras
    if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eCamera) {

        FbxCamera* lCamera = pNode->GetCamera();

        if (lCamera->ProjectionType.Get() == FbxCamera::ePerspective) {
            FbxCamera::EApertureMode lCamApertureMode = lCamera->GetApertureMode();

            // Camera Field of View
            if ( FbxCamera::eVertical == lCamApertureMode ) {
                lCurve = lCamera->FieldOfView.GetCurve(mAnimLayer);
                if (lCurve && lCurve->KeyGetCount() >= 2) {
                    ExportCurve(pAnimationNode, lCurve, COLLADA_YFOV_CAMERA_PARAMETER, "", 0, 0, 1);
                }
            }
            else if ( FbxCamera::eHorizontal == lCamApertureMode ) {
                lCurve = lCamera->FieldOfView.GetCurve(mAnimLayer);
                if (lCurve && lCurve->KeyGetCount() >= 2) {
                    ExportCurve(pAnimationNode, lCurve, COLLADA_XFOV_CAMERA_PARAMETER, "", 0, 0, 1);
                }
            }
            else if ( FbxCamera::eHorizAndVert == lCamApertureMode ) {
                lCurve = lCamera->FieldOfViewX.GetCurve(mAnimLayer);
                if (lCurve && lCurve->KeyGetCount() >= 2) {
                    ExportCurve(pAnimationNode, lCurve, COLLADA_XFOV_CAMERA_PARAMETER, "", 0, 0, 1);
                }

                lCurve = lCamera->FieldOfViewY.GetCurve(mAnimLayer);
                if (lCurve && lCurve->KeyGetCount() >= 2) {
                    ExportCurve(pAnimationNode, lCurve, COLLADA_YFOV_CAMERA_PARAMETER, "", 0, 0, 1);
                }
            }
            else if ( FbxCamera::eFocalLength == lCamApertureMode ) {
                lCurve = lCamera->FocalLength.GetCurve(mAnimLayer);
                if (lCurve && lCurve->KeyGetCount() >= 2) {
                    // Convert the focal length curve to a FOV curve.
                    FbxAnimCurve* lFOVCurve = FbxAnimCurve::Create(pNode->GetFbxManager(), "tmpFOV");
                    ConvertFocalLengthCurveToFOV(lFOVCurve, lCurve, (FbxCamera*)lNodeAttribute);
                    FbxString msg = FbxString("Camera Focal Length converted to Field Of View");
                    AddNotificationWarning( msg );
                    ExportCurve(pAnimationNode, lFOVCurve, COLLADA_YFOV_CAMERA_PARAMETER, "", 0, 0, 1);
                    lFOVCurve->Destroy();
                }
            }
        }
    }

    // Get the geometry to get the shapes.
    if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh) 
    {
        FbxGeometry* lGeometry = (FbxGeometry*)pNode->GetNodeAttribute();
        int lBlendShapeDeformerCount = lGeometry->GetDeformerCount(FbxDeformer::eBlendShape);
        for(int lBlendShapeIndex = 0; lBlendShapeIndex<lBlendShapeDeformerCount; ++lBlendShapeIndex)
        {
            FbxBlendShape* lBlendShape = (FbxBlendShape*)lGeometry->GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);

            int lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();
            for(int lChannelIndex = 0; lChannelIndex<lBlendShapeChannelCount; ++lChannelIndex)
            {
                FbxBlendShapeChannel* lChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);

                if(lChannel)
                {
                    lCurve = lGeometry->GetShapeChannel(lBlendShapeIndex, lChannelIndex, mAnimLayer, false);
                    if (lCurve && lCurve->KeyGetCount() >= 2) {
                        FbxString lStr1 = lBlendShapeIndex;
                        FbxString lStr2 = lChannelIndex;

                        FbxString lParamName( "Weight" );
                        lParamName += "_";
                        lParamName += lStr1;
                        lParamName += "_";
                        lParamName += lStr2;
                        ExportCurve(pAnimationNode, lCurve, "morph-weights", lParamName.Buffer(), true);
                    }
                }//If lChannel is valid
            }//For each blend shape channel
        }//For each blend shape deformer
    }

    return true;
} // ExportAnimationCurves


bool FbxWriterCollada::ExportCurve(xmlNode* pAnimationNode, FbxAnimCurve* pCurve,
                                    const char* pChannelName, const char* pSubChannelName,
                                    bool pExportShape, bool pExportIntensity, bool pExportLib) {
    // When pExportShape is true, Id nomenclature is a bit different.
    // When pExportShape or pExportIntensity is true, values are divided by 100.

    xmlNode* lSubAnimationNode = xmlNewChild(pAnimationNode, NULL, XML_STR COLLADA_ANIMATION_STRUCTURE, XML_STR "");

    int lKeyIndex;

    FbxString lNodeName        = DAE_GetElementAttributeValue(pAnimationNode, COLLADA_NAME_PROPERTY);
    if (pExportShape || pExportLib) {
        lNodeName = lNodeName + "-lib";
    }
    FbxString lInputId        = lNodeName + "-" + pChannelName + "-animation-input" + pSubChannelName;
    FbxString lOutputId        = lNodeName + "-" + pChannelName + "-animation-output" + pSubChannelName;
    FbxString lInTanId        = lNodeName + "-" + pChannelName + "-animation-intan" + pSubChannelName;
    FbxString lOutTanId        = lNodeName + "-" + pChannelName + "-animation-outtan" + pSubChannelName;
    FbxString lInterpolationId    = lNodeName + "-" + pChannelName + "-animation-interpolation" + pSubChannelName;
    FbxString lSamplerId        = lNodeName + "-" + pChannelName + "-animation" + pSubChannelName;
    FbxString lChannelId        = lNodeName + "-" + pChannelName + "-animation-channel" + pSubChannelName;
    FbxString lTarget            = lNodeName + "/" + pChannelName;
    FbxString lInTanWeightsId = lNodeName + "-" + pChannelName + "-animation-intan-weights" + pSubChannelName;
    FbxString lOutTanWeightsId = lNodeName + "-" + pChannelName + "-animation-outtan-weights" + pSubChannelName;
    if (FbxString(pSubChannelName).GetLen() > 0)    lTarget += FbxString(".") + pSubChannelName;

    if (pExportShape) {
        // Target nomenclature is a bit different for shapes.
        lTarget                = lNodeName + "-" + pChannelName + "(" + pSubChannelName + ")";
    }

    FbxArray<double> lInputs;
    FbxArray<double> lOutputs;
    FbxArray<double> lInTan;
    FbxArray<double> lOutTan;
    FbxStringList lInterpolations;
    FbxArray<double> lInTanWeights;
    FbxArray<double> lOutTanWeights;
    bool lIsWeighted = false;

    int lKeyCount = pCurve->KeyGetCount();
    for (lKeyIndex = 0; lKeyIndex < lKeyCount; lKeyIndex++) {
        // Key times and values
        double lKeyT = pCurve->KeyGetTime(lKeyIndex).GetSecondDouble();
        lInputs.Add(lKeyT);
        double lOutput = pCurve->KeyGetValue(lKeyIndex);
        if (pExportShape || pExportIntensity)
            lOutputs.Add(lOutput/100.0);
        else
            lOutputs.Add(lOutput);

        // Key tangents
        if (lKeyIndex > 0) {
            // Left Derivative
            double lLeftW            = pCurve->KeyGetLeftTangentWeight(lKeyIndex);
            double lLeftInterval    = lKeyT - pCurve->KeyGetTime(lKeyIndex-1).GetSecondDouble();
            double lLeftD            = pCurve->KeyGetLeftDerivative(lKeyIndex);        
            double lLeftATanV        = lLeftD * lLeftW * lLeftInterval;
            if (pExportShape || pExportIntensity) lLeftATanV /= 100.0;
                    
            lInTan.Add(lLeftATanV);
        } else {
            lInTan.Add(0);
        }
        if (lKeyIndex + 1 < lKeyCount) {
            // Right Derivative
            double lRightW            = pCurve->KeyGetRightTangentWeight(lKeyIndex);
            double lRightInterval    = pCurve->KeyGetTime(lKeyIndex+1).GetSecondDouble() - lKeyT;
            double lRightD            = pCurve->KeyGetRightDerivative(lKeyIndex);
            double lRightATanV        = lRightD * lRightW * lRightInterval;
            if (pExportShape || pExportIntensity) lRightATanV /= 100.0;
                    
            lOutTan.Add(lRightATanV);
        } else {
            lOutTan.Add(0);
        }


        // Key interpolations
        FbxAnimCurveDef::EInterpolationType lInterp = pCurve->KeyGetInterpolation(lKeyIndex);
        FbxAnimCurveDef::ETangentMode lTangentMode = pCurve->KeyGetTangentMode(lKeyIndex);
        if(lInterp == FbxAnimCurveDef::eInterpolationConstant) {
            lInterpolations.Add(COLLADA_INTERPOLATION_TYPE_STEP);
        } else if (lInterp == FbxAnimCurveDef::eInterpolationLinear) {
            // The interpretation of "LINEAR" is different in COLLADAMaya than in FBX.
            // For now, keep to LINEAR, but this will result in changes in the fcurves.
            lInterpolations.Add(COLLADA_INTERPOLATION_TYPE_LINEAR);
        } else if (lInterp == FbxAnimCurveDef::eInterpolationCubic) {
            // Whatever the tangent mode, Break, User, Auto_Break, Auto or TCB, 
            // the curve interpolation type is set to BEZIER.
            lInterpolations.Add(COLLADA_INTERPOLATION_TYPE_BEZIER);
        } else {
            FBX_ASSERT_NOW("Unexpected interpolation type");
        }

        // Key weights
        if (lInterp == FbxAnimCurveDef::eInterpolationCubic && pCurve->KeyIsLeftTangentWeighted(lKeyIndex)) {
            double lLeftWeight = pCurve->KeyGetLeftTangentWeight(lKeyIndex);
            lInTanWeights.Add(lLeftWeight);
            lIsWeighted = true;
        } else {
            lInTanWeights.Add(0);
        }

        if (lInterp == FbxAnimCurveDef::eInterpolationCubic && pCurve->KeyIsRightTangentWeighted(lKeyIndex)) {
            double lRightWeight = pCurve->KeyGetRightTangentWeight(lKeyIndex);
            lOutTanWeights.Add(lRightWeight);
            lIsWeighted = true;
        } else {
            lOutTanWeights.Add(0);
        }

    }
    
    FbxStringList lAccessorParams;
    xmlNode* lXmlSource;

    // Source: Input - values

    lAccessorParams.Add("TIME", 0);
    lXmlSource = DAE_ExportSource14(lSubAnimationNode, lInputId.Buffer(), lAccessorParams, lInputs);

    // Source: Output - values

    lAccessorParams.Clear();
    lAccessorParams.Add(pSubChannelName, 0); // stays at that value for the other sources
    lXmlSource = DAE_ExportSource14(lSubAnimationNode, lOutputId.Buffer(), lAccessorParams, lOutputs);

    // Source: Left tangents
    lXmlSource = DAE_ExportSource14(lSubAnimationNode, lInTanId.Buffer(), lAccessorParams, lInTan);

    // Source: Right tangents
    lXmlSource = DAE_ExportSource14(lSubAnimationNode, lOutTanId.Buffer(), lAccessorParams, lOutTan);

    // Source: Interpolation types
    //lXmlSource = DAE_ExportSource14(lSubAnimationNode, lInterpolationId.Buffer(), lInterpolations, pSubChannelName, COLLADA_NAME_TYPE, true);
    lXmlSource = DAE_ExportSource14(lSubAnimationNode, lInterpolationId.Buffer(), lInterpolations, COLLADA_NAME_TYPE, true);


    // Sampler bundles it all together
    xmlNode* lSampler = xmlNewChild(lSubAnimationNode, NULL, XML_STR COLLADA_SAMPLER_STRUCTURE, NULL);
    xmlNewProp(lSampler, XML_STR COLLADA_ID_PROPERTY, XML_STR lSamplerId.Buffer());
    DAE_AddInput14(lSampler, COLLADA_INPUT_SEMANTIC, lInputId);
    DAE_AddInput14(lSampler, COLLADA_OUTPUT_SEMANTIC, lOutputId);
    DAE_AddInput14(lSampler, COLLADA_IN_TANGENT_SEMANTIC, lInTanId);
    DAE_AddInput14(lSampler, COLLADA_OUT_TANGENT_SEMANTIC, lOutTanId);
    DAE_AddInput14(lSampler, COLLADA_INTERPOLATION_SEMANTIC, lInterpolationId);

    // Channel to the animated part
    xmlNode* lChannel = xmlNewChild(lSubAnimationNode, NULL, XML_STR COLLADA_CHANNEL_STRUCTURE, NULL);
    FbxString lSamplerUrl = FbxString("#") + lSamplerId;
    xmlNewProp(lChannel, XML_STR COLLADA_SOURCE_PROPERTY, XML_STR lSamplerUrl.Buffer());
    xmlNewProp(lChannel, XML_STR COLLADA_TARGET_PROPERTY, XML_STR lTarget.Buffer());


    return true;
} // ExportCurve


bool FbxWriterCollada::NotZero(FbxVector4 pV) {
    if (NotZero(pV[0]) || NotZero(pV[1]) || NotZero(pV[2])) return true;
    return false;
}


bool FbxWriterCollada::NotValue(FbxVector4 pV, double pValue) {
    if (NotZero(pV[0] - pValue) || NotZero(pV[1] - pValue) || NotZero(pV[2] - pValue)) return true;
    return false;
}


bool FbxWriterCollada::NotZero(double pD) {
    if (fabs(pD) < FLOAT_TOLERANCE) return false;
    return true;
}


bool FbxWriterCollada::IsTranslationAnimated(const FbxNode *pNode)
{
    // If null, there is no animation for sure
    FbxAnimCurveNode* acn = const_cast<FbxNode*>(pNode)->LclTranslation.GetCurveNode(mAnimLayer);
    if (acn == NULL) return false;
    
    for( unsigned int i = 0; i < acn->GetChannelsCount(); i++ )
    {
        FbxAnimCurve* fc = acn->GetCurve(i);
        if (fc && fc->KeyGetCount() > 0)
            return true;
    }

    return false;

} // IsTranslationAnimated


bool FbxWriterCollada::IsRotationAnimated(const FbxNode *pNode)
{
    FbxAnimCurveNode* acn = const_cast<FbxNode*>(pNode)->LclRotation.GetCurveNode(mAnimLayer);
    if (acn == NULL) return false;
    
    for( unsigned int i = 0; i < acn->GetChannelsCount(); i++ )
    {
        FbxAnimCurve* fc = acn->GetCurve(i);
        if (fc && fc->KeyGetCount() > 0)
            return true;
    }

    return false;

} // IsRotationAnimated

bool FbxWriterCollada::IsRotationAnimated(const FbxNode *pNode, int pAxis)
{
    FbxAnimCurveNode* acn = const_cast<FbxNode*>(pNode)->LclRotation.GetCurveNode(mAnimLayer);
    if (acn == NULL) return false;
    
    for( unsigned int i = 0; i < acn->GetChannelsCount(); i++ )
    {
        FbxAnimCurve* fc = acn->GetCurve(i);
        if (fc && fc->KeyGetCount() > 0 && i == pAxis)
            return true;
    }
    return false;
}

bool FbxWriterCollada::IsScaleAnimated(const FbxNode *pNode) 
{
    FbxAnimCurveNode* acn = const_cast<FbxNode*>(pNode)->LclScaling.GetCurveNode(mAnimLayer);
    if (acn == NULL) return false;
    
    for( unsigned int i = 0; i < acn->GetChannelsCount(); i++ )
    {
        FbxAnimCurve* fc = acn->GetCurve(i);
        if (fc && fc->KeyGetCount() > 0)
            return true;
    }

    return false;
}

void FbxWriterCollada::CopyMesh(FbxMesh *pNewMesh, FbxMesh *pRefMesh) {
    // Copy mesh parameters from pRefMesh to pNewMesh.
    // If pOnlyLayer0 is true, only layer 0 is copied. Else, all layers are copied.
    int i, lPolygonIndex, lVertexIndex, lLayerIndex;

    // Vertices
    int lControlPointsCount = pRefMesh->GetControlPointsCount();
    FbxVector4* lRefControlPoints = pRefMesh->GetControlPoints();
    FbxVector4* lRefNormals = NULL;
    FbxLayerElementArrayTemplate<FbxVector4>* direct;
    pRefMesh->GetNormals(&direct);
    if (direct)
        lRefNormals = direct->GetLocked(lRefNormals, FbxLayerElementArray::eReadLock);

    pNewMesh->InitControlPoints(lControlPointsCount);
    pNewMesh->InitNormals(lControlPointsCount);

    for (i = 0; i < lControlPointsCount; i++) {
        FbxVector4 lControlPoint = lRefControlPoints[i];
        FbxVector4 lNormal = lRefNormals[i];
        pNewMesh->SetControlPointAt(lControlPoint, lNormal, i);
    }
    if (direct)
        direct->Release(&lRefNormals, lRefNormals);

    // Polygons
    int lPolygonCount = pRefMesh->GetPolygonCount();
    int* lRefPolygonVertices = pRefMesh->GetPolygonVertices();
    int lMaterialIndex = -1;
    int lTextureIndex = -1;
    int lIndex = 0;
    for (lPolygonIndex = 0; lPolygonIndex < lPolygonCount; lPolygonIndex++) {
        int lPolygonSize = pRefMesh->GetPolygonSize(lPolygonIndex);
        pNewMesh->BeginPolygon(lMaterialIndex, lTextureIndex);
        for (lVertexIndex = 0; lVertexIndex < lPolygonSize; lVertexIndex++) {
            pNewMesh->AddPolygon(lRefPolygonVertices[lIndex]);
            lIndex++;
        }
        pNewMesh->EndPolygon();
    }

    // Layer elements: Normals, UVs, Vertex colors, Materials, Textures and Polygon groups (the latter is not supported by COLLADA).
    for (lLayerIndex = 0; lLayerIndex < pRefMesh->GetLayerCount(); lLayerIndex++) {
        FbxLayer* lRefLayer = pRefMesh->GetLayer(lLayerIndex);
        FbxLayer* lNewLayer = pNewMesh->GetLayer(lLayerIndex);
        while (!lNewLayer) {
            pNewMesh->CreateLayer();
            lNewLayer = pNewMesh->GetLayer(lLayerIndex);
        }
        lNewLayer->Clone(*lRefLayer);
    }

} // CopyMesh

//
//Process FBX scene before exporting FBX scene to Collada file.
//Here we can process name clash, special transformation conversion, material conversion etc.
//
bool FbxWriterCollada::PreprocessScene(FbxScene &pScene)
{
    // Rename ALL the nodes from FBX to Collada
    FbxRenamingStrategyCollada lRenamer;
    lRenamer.EncodeScene(&pScene);

    FbxNode* lRootNode = pScene.GetRootNode();
    PreprocessNodeRecursive( lRootNode );

    if ( mSingleMatrix )
    {
        lRootNode->ResetPivotSetAndConvertAnimation( 1. / mSamplingPeriod.GetSecondDouble() );
    }

    // convert to the old material system for now
    FbxMaterialConverter lConv( *pScene.GetFbxManager() );
    lConv.AssignTexturesToLayerElements( pScene );

    FbxString lActiveAnimStackName = pScene.ActiveAnimStackName;
    mAnimStack = pScene.FindMember<FbxAnimStack>(lActiveAnimStackName.Buffer());
    if (!mAnimStack)
    {
         // the application has an invalid ActiveAnimStackName, we fallback by using the 
         // first animStack.
         mAnimStack = pScene.GetMember<FbxAnimStack>();
    }
    
    // If none of the above method succeed in returning an anim stack, we create 
    // a dummy one to avoid crashes. The correctness of the exported values cannot 
    // be guaranteed in this case
    if (mAnimStack == NULL)
    {
        mAnimStack = FbxAnimStack::Create(&pScene, "dummy");
        mAnimLayer = FbxAnimLayer::Create(&pScene, "dummyL");
        mAnimStack->AddMember(mAnimLayer);
    }

    mAnimLayer = mAnimStack->GetMember<FbxAnimLayer>();

    // Make sure the scene has a name. If it does not, we try to use the filename
    // and, as last resort a dummy name.
    if (strlen(pScene.GetName()) == 0)
    {
        FbxDocumentInfo *lSceneInfo = pScene.GetSceneInfo();
        FbxString lFilename("dummy");
        if (lSceneInfo)
        {
            lFilename = lSceneInfo->Original_FileName.Get();
            if (lFilename.GetLen() > 0)
            {
                FbxString lFn = FbxPathUtils::GetFileName(lFilename.Buffer(), false);
                if (lFn.GetLen() > 0)
                    lFilename = lFn;
            }
        }
        pScene.SetName(lFilename.Buffer());
    }
    return true;
}


void FbxWriterCollada::PreprocessNodeRecursive(FbxNode* pNode)
{
    FbxVector4 lPostR;
    FbxNodeAttribute const * lNodeAttribute = pNode->GetNodeAttribute();
    //Set PivotState to active to ensure ConvertPivotAnimationRecursive() execute correctly. 
    if(pNode){
        pNode->SetPivotState(FbxNode::eSourcePivot, FbxNode::ePivotActive);
        pNode->SetPivotState(FbxNode::eDestinationPivot, FbxNode::ePivotActive); 
    }
    //~
    if ( lNodeAttribute )
    {
        // Special transformation conversion cases.
        // If spotlight or directional light, 
        // rotate node so spotlight is directed at the X axis (was Z axis).
        if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eLight)
        {
            FbxLight *lLight = (FbxLight*)pNode->GetNodeAttribute();
            if (lLight->LightType.Get() == FbxLight::eSpot
                || lLight->LightType.Get() == FbxLight::eDirectional)
            {
                lPostR = pNode->GetPostRotation(FbxNode::eSourcePivot);
                lPostR[0] += 90;
                pNode->SetPostRotation(FbxNode::eSourcePivot, lPostR);
            }
        } 
        // If camera, rotate node so camera is directed at the -Z axis (was X axis).
        else if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eCamera)
        {
            lPostR = pNode->GetPostRotation(FbxNode::eSourcePivot);
            lPostR[1] += 90;
            pNode->SetPostRotation(FbxNode::eSourcePivot, lPostR);
        }

    }

    int i;
    for (i = 0; i < pNode->GetChildCount(); ++i) {
        PreprocessNodeRecursive(pNode->GetChild(i));
    }

}

//
//Process FBX scene after exporting FBX scene to Collada file.
//
//
bool FbxWriterCollada::PostprocessScene(FbxScene &pScene)
{
    //When we convert the fbx7 to the older version, we will lose the connections between textures and materials.
    //So If the want to write the scene using fbx7 again, we need to reconnect them at first.
    FbxMaterialConverter lConv( *pScene.GetFbxManager() );
    lConv.ConnectTexturesToMaterials(pScene);
    return false;
}


void FbxWriterCollada::ConvertFocalLengthCurveToFOV(FbxAnimCurve *pFOVCurve, FbxAnimCurve *pFLCurve, FbxCamera *pCamera)
{
    //Convert a curve that represents camera focal length animation (pFLCurve) to a curve that represents camera field of view (pFOVCurve).
    for( int i = 0; i < pFLCurve->KeyGetCount(); i++ )
    {
        FbxTime lKeyTime = pFLCurve->KeyGetTime(i);
        double lKeyValue = pCamera->ComputeFieldOfView(pFLCurve->KeyGetValue(i));
        pFOVCurve->KeyAdd(lKeyTime);
        pFOVCurve->KeySetValue(i, float(lKeyValue));

        if( pFLCurve->KeyGetInterpolation(i) == FbxAnimCurveDef::eInterpolationCubic && (pFLCurve->KeyGetTangentMode(i) & FbxAnimCurveDef::eTangentUser) )
        {
            if( i+1 < pFLCurve->KeyGetCount() )
            {
                double lKeyV = pFLCurve->KeyGetValue(i);
                double lKeyT = pFLCurve->KeyGetTime(i).GetSecondDouble();
                double lRightD = pFLCurve->KeyGetRightDerivative(i);
                double lRightW = pFLCurve->KeyGetRightTangentWeight(i);                        
                double lInterval = (pFLCurve->KeyGetTime(i+1) - pFLCurve->KeyGetTime(i)).GetSecondDouble();
                double lHandleT = lKeyT + lRightW * lInterval;
                double lHandleV = (lRightD * (lHandleT-lKeyT)) + lKeyV;
                double lKeyVMod = pCamera->ComputeFieldOfView(lKeyV);
                double lHandleVMod = pCamera->ComputeFieldOfView(lHandleV);                        
                double lRightDMod = (lHandleVMod-lKeyVMod) / (lHandleT-lKeyT);
                pFOVCurve->KeySetRightDerivative(i, float(lRightDMod));
            }
            if( i > 0 )
            {
                double lKeyV = pFLCurve->KeyGetValue(i);
                double lKeyT = pFLCurve->KeyGetTime(i).GetSecondDouble();
                double lLeftD = pFLCurve->KeyGetLeftDerivative(i);
                double lLeftW = pFLCurve->KeyGetLeftTangentWeight(i);
                double lInterval = (pFLCurve->KeyGetTime(i) - pFLCurve->KeyGetTime(i-1)).GetSecondDouble();
                double lHandleT = lKeyT - (lLeftW * lInterval);
                double lHandleV = lKeyV - (lLeftD * (lKeyT-lHandleT));
                double lKeyVMod = pCamera->ComputeFieldOfView(lKeyV);
                double lHandleVMod = pCamera->ComputeFieldOfView(lHandleV);
                double lLeftDMod = (lKeyVMod-lHandleVMod) / (lKeyT-lHandleT);                        
                pFOVCurve->KeySetLeftDerivative(i, float(lLeftDMod));    
            }
        }
    }
}

void FbxWriterCollada::AddNotificationError( FbxString pError )
{
    FbxUserNotification * lUserNotification = mManager.GetUserNotification();
    if ( lUserNotification ) {
        FbxString lError = "ERROR: " + pError;
        lUserNotification->AddDetail( FbxUserNotification::eFileIONotify, lError );
    }
}

void FbxWriterCollada::AddNotificationWarning( FbxString pWarning )
{
    FbxUserNotification * lUserNotification = mManager.GetUserNotification();
    if ( lUserNotification ) {
        FbxString lWarning = "Warning: " + pWarning;
        lUserNotification->AddDetail( FbxUserNotification::eFileIONotify, lWarning );
    }
}

#include <fbxsdk/fbxsdk_nsend.h>

