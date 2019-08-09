/****************************************************************************************
 
   Copyright (C) 2016 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/fileio/fbx/fbxreaderfbx6.h>
#include <fbxsdk/fileio/fbxprogress.h>
#include <fbxsdk/fileio/fbxglobalsettings.h>
#include <fbxsdk/scene/shading/fbxshadingconventions.h>
#include <fbxsdk/scene/animation/fbxanimutilities.h>
#include <fbxsdk/utils/fbxscenecheckutility.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

static FbxLayerElement::EMappingMode       ConvertMappingModeToken(const char* pToken);
static FbxLayerElement::EReferenceMode     ConvertReferenceModeToken(const char* pToken);
static FbxLayerElementTexture::EBlendMode  ConvertBlendModeToken(const char* pToken);

static FbxVector4 sgLastCameraPos;
static FbxVector4 sgLastCameraUpVec;
static FbxVector4 sgLastCameraLookAt;

//
// Local class
//
class Fbx6ObjectTypeInfo
{
public:
    FbxString mType;
    FbxString mSubType;
    FbxString mName;
};

//
// Local struct
// used to manage external references
//
struct Fbx6TypeReadReferenceInfo
{
    bool        mReferenceIsExternal;
    FbxString     mReferenceName;
    FbxString     mReferencedObjectName;
    FbxString     mReferencedDocumentRootName;
    FbxString     mReferencedDocumentPathName;
    FbxObject* mReferencedObject;
};

//
// Local class
// used to manage external references
//
class Fbx6TypeReadReferences
{
public:

    Fbx6TypeReadReferences(void) {};
    virtual ~Fbx6TypeReadReferences(void) { FbxArrayDelete(mReferences); };
    int AddReference(
        bool  pExternalRef,
        char* pReferenceName,
        char* pReferencedObjectName,
        char* pReferencedDocumentRootName,
        char* pReferencedDocumentPathName
        );
    int ResolveForDocument(FbxDocument* pReferencingDocument, FbxDocument* pReferencedDocument, bool pForceExternal = false);
    bool AreAllExternalReferencesResolved(void);
    bool GetReferenceResolution(char* pRefName, FbxString& pRefObjName, bool& pRefExternal, FbxObject* &pRefObj);

private:
    FbxArray< Fbx6TypeReadReferenceInfo* >   mReferences;
};

bool Fbx6TypeReadReferences::AreAllExternalReferencesResolved(void)
{
    Fbx6TypeReadReferenceInfo* lRefInfo;
    int                     i, lCount = mReferences.GetCount();

    for (i = 0; i < lCount; i++)
    {
        lRefInfo = mReferences[i];
        if (lRefInfo->mReferenceIsExternal)
        {
            if (lRefInfo->mReferencedObject == NULL)
            {
                return false;
            }
        }
    }

    return true;
}

int Fbx6TypeReadReferences::ResolveForDocument(FbxDocument* pReferencingDocument, FbxDocument* pReferencedDocument, bool pForceExternal)
{
    if ((pReferencingDocument == NULL) || (pReferencedDocument == NULL))
    {
        return 0;
    }

    FbxObject* lObj;
    int         lResult = 0;
    int         i, lMemberCount = pReferencedDocument->GetMemberCount();
    bool        lIsReferenceExternal = pForceExternal || (pReferencedDocument->GetRootDocument() != pReferencingDocument->GetRootDocument());

    for (i = 0; i < lMemberCount; i++)
    {
        lObj = pReferencedDocument->GetMember(i);

        // Recursion for sub-documents
        FbxDocument* lSubDoc = FbxCast<FbxDocument>(lObj);
        if (lSubDoc != NULL)
        {
            lResult += ResolveForDocument(pReferencingDocument, lSubDoc, pForceExternal);
        }

        FbxString lDocPathToRoot = pReferencedDocument->GetPathToRootDocument();
        FbxString lObjName = lObj->GetNameWithNameSpacePrefix();
        int     j, lCount = mReferences.GetCount();

        for (j = 0; j < lCount; j++)
        {
            if (mReferences[j]->mReferencedDocumentPathName == lDocPathToRoot)
            {
                if (mReferences[j]->mReferencedObjectName == lObjName)
                {
                    mReferences[j]->mReferencedObject = lObj;
                    mReferences[j]->mReferenceIsExternal = lIsReferenceExternal;
                }
            }
        }
    }
    return lResult;
}

int Fbx6TypeReadReferences::AddReference
(
 bool    pExternalRef,
 char*   pReferenceName,
 char*   pReferencedObjectName,
 char*   pReferencedDocumentRootName,
 char*   pReferencedDocumentPathName
 )
{
    Fbx6TypeReadReferenceInfo* lRefInfo = FbxNew< Fbx6TypeReadReferenceInfo >();
    lRefInfo->mReferenceIsExternal = pExternalRef;
    lRefInfo->mReferenceName = pReferenceName;
    lRefInfo->mReferencedObjectName = pReferencedObjectName;
    lRefInfo->mReferencedDocumentRootName = pReferencedDocumentRootName;
    lRefInfo->mReferencedDocumentPathName = pReferencedDocumentPathName;
    lRefInfo->mReferencedObject = NULL;

    return mReferences.Add(lRefInfo);
}

bool Fbx6TypeReadReferences::GetReferenceResolution
(
 char*           pRefName,
 FbxString&        pRefObjName,
 bool&           pRefExternal,
 FbxObject*&    pRefObj
 )
{
    pRefObj         = NULL;
    pRefExternal    = false;

    int     i, lCount = mReferences.GetCount();
    FbxString lReferenceName(pRefName);

    Fbx6TypeReadReferenceInfo* lRefInfo;

    for (i = 0; i < lCount; i++)
    {
        lRefInfo = mReferences[i];
        if (lReferenceName == lRefInfo->mReferenceName)
        {
            pRefObjName     = lRefInfo->mReferencedObjectName;
            pRefObj         = lRefInfo->mReferencedObject;
            pRefExternal    = lRefInfo->mReferenceIsExternal;
            return true;
        }
    }
    return false;
}

Fbx6ClassTemplateMap::Fbx6ClassTemplateMap()
{

}

Fbx6ClassTemplateMap::~Fbx6ClassTemplateMap()
{
    Clear();
}

bool Fbx6ClassTemplateMap::AddClassId( FbxClassId pId, FbxObject* pTemplateObj )
{
    if( !pId.IsValid() )
    {
        pTemplateObj->Destroy();
        pTemplateObj = NULL;
        return false;
    }

    mClassMap.Insert( pId, pTemplateObj );

    return true;
}

bool Fbx6ClassTemplateMap::HasModifiedFlags(FbxProperty lProp) const
{
    for( int i = 0; i < FbxPropertyFlags::eFlagCount; ++i )
    {
        if( lProp.ModifiedFlag(IndexToFlag(i)) )
            return true;
    }
    return false;
}

bool Fbx6ClassTemplateMap::MergeWithTemplate( FbxObject* pObject ) const
{
    if( !pObject )
        return false;

    FbxClassId lId = pObject->GetRuntimeClassId();
    if( !lId.IsValid() )
        return false;

    const MapType::RecordType* lRec = mClassMap.Find(lId);
    if( !lRec || !lRec->GetValue() )
        return false;

    const FbxObject* lTemplateObj = lRec->GetValue();
    FBX_ASSERT( lTemplateObj );

    if( !lTemplateObj )
        return false;

    // sanity check
    FBX_ASSERT( pObject->GetClassId().Is(lTemplateObj->GetClassId()) );

    FbxProperty::FbxPropertyNameCache lObjCache( pObject->RootProperty );

    for( FbxProperty lTemplateProp = lTemplateObj->RootProperty.GetFirstDescendent();
        lTemplateProp.IsValid();
        lTemplateProp = lTemplateObj->RootProperty.GetNextDescendent( lTemplateProp ) )
    {
        const bool lSetValue = lTemplateProp.GetValueInheritType() == FbxPropertyFlags::eOverride;
        const bool lSetFlags = HasModifiedFlags( lTemplateProp );

        // if the template hasn't overriden either of these, it means the template
        // and the in memory class root are the same, so skip this property.
        if( !lSetValue && !lSetFlags )
            continue;

        FbxProperty lObjProp = pObject->RootProperty.Find( lTemplateProp.GetHierarchicalName() );

        if( !lObjProp.IsValid() )
        {
            // parents are visited before children, so the parent of this one
            // should exist.
            FbxProperty lTemplatePropParent = lTemplateProp.GetParent();
            FbxProperty lObjPropParent = FbxProperty();

            if( lTemplatePropParent.IsValid() )
                lObjPropParent = lTemplatePropParent.IsRoot() ? pObject->RootProperty : pObject->RootProperty.Find( lTemplatePropParent.GetHierarchicalName() );

            FBX_ASSERT( lObjPropParent.IsValid() );
            if( lObjPropParent.IsValid() )
			{
                lObjProp = FbxProperty::CreateFrom(lObjPropParent, lTemplateProp, false);
				lObjProp.CopyValue(lTemplateProp);
			}
        }
        else
        {
            // only override if lObjProp, and all of its references do not override.
            if( !lObjProp.Modified() && lSetValue )
                lObjProp.CopyValue( lTemplateProp );

            if( lSetFlags )
            {
                for( int i = 0; i < FbxPropertyFlags::eFlagCount; ++i )
                {
                    FbxPropertyFlags::EFlags lFlag = IndexToFlag(i);
                    if( !lObjProp.ModifiedFlag( lFlag ) )   // don't set if already overridden
                    {
                        if( lTemplateProp.ModifiedFlag(lFlag) )    // set if modified.
                            lObjProp.ModifyFlag( lFlag, lTemplateProp.GetFlag( lFlag ) );
                    }
                }
            }
        }
    }
    return true;
}

void Fbx6ClassTemplateMap::Clear()
{
    MapType::RecordType* lRec = mClassMap.Minimum();

    while( lRec )
    {
        if( lRec->GetValue() )
            lRec->GetValue()->Destroy();
        lRec = lRec->Successor();
    }

    mClassMap.Clear();
}

//
// Local utility functions
//
static int FindTypeIndex(FbxString& pTypeName, FbxArray<Fbx6ObjectTypeInfo*>& pTypeList);

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

template< class T > static T* CreateOrCloneReference
(
 FbxManager& pManager,
 FbxString&        pNewObjectName,
 FbxObject*     pReferencedObject,
 Fbx6ClassTemplateMap& pTemplateMap,
 const char*        pAssetClassName = ""
 )
{
    T* lObject = FbxCast<T>(pReferencedObject);
    if (lObject != NULL)
    {
        // we need to make sure the object is loaded,
        if( lObject->ContentIsLoaded() || lObject->ContentLoad() != 0 )
        {
            lObject = FbxCast<T>(lObject->Clone(FbxObject::eReferenceClone));
            {
                lObject->SetInitialName(pNewObjectName.Buffer());
                lObject->SetName(pNewObjectName.Buffer());
            }
        }
        else
            FBX_ASSERT_NOW("Failed to load content");
    }

    if (lObject == NULL)
    {
        FbxClassId lClassId = pManager.FindClass( pAssetClassName );
        if( lClassId.IsValid())
        {
            lObject = FbxCast<T>( pManager.CreateNewObjectFromClassId(lClassId, pNewObjectName.Buffer()) );        
        }
        else
        {
        lObject = T::Create(&pManager, pNewObjectName.Buffer());
    }
    }

    FBX_ASSERT( lObject );
    pTemplateMap.MergeWithTemplate( lObject );

    return lObject;
}

static FbxObject* CreateOrCloneReference
(
 FbxManager& pManager,
 FbxString&        pNewObjectName,
 FbxObject*     pReferencedObject,
 FbxClassId     pClassId,
 Fbx6ClassTemplateMap& pTemplateMap
 )
{
    FbxObject* lObject = pReferencedObject;
    if (lObject != NULL)
    {
        // we need to make sure the object is loaded,
        if( lObject->ContentIsLoaded() || lObject->ContentLoad() != 0 )
        {
            lObject = pReferencedObject->Clone(FbxObject::eReferenceClone);
            {
                lObject->SetInitialName(pNewObjectName.Buffer());
                lObject->SetName(pNewObjectName.Buffer());
            }
        }
        else
            FBX_ASSERT_NOW("Failed to load content");
    }

    if (lObject == NULL)
    {
        if (pClassId.IsValid())
        {
            lObject= FbxCast <FbxObject> (pClassId.Create(pManager, pNewObjectName.Buffer(),NULL));
        }
    }

    FBX_ASSERT( lObject );
    pTemplateMap.MergeWithTemplate( lObject );

    return lObject;
}

template<typename T>
FbxClassId CheckRuntimeClass(T* pParentClass, const FbxString& pObjectType, const FbxString& pObjectSubType, FbxManager& mManager)
{
    FbxClassId ObjectClassId;
    if( !strcmp(pObjectType.Buffer(), FBXSDK_TYPE_PROCEDURALGEOMETRY) )
    {
        ObjectClassId = mManager.FindClass(ADSK_TYPE_PROCEDURALGEOMETRY);
    }
    else if( !strcmp(pObjectType.Buffer(), FBXSDK_TYPE_ENVIRONMENT) )
    {
        ObjectClassId = mManager.FindClass(ADSK_TYPE_ENVIRONMENT);
    }
    else if( !strcmp(pObjectType.Buffer(), FBXSDK_TYPE_SWATCHSCENE) )
    {
        ObjectClassId = mManager.FindClass(ADSK_TYPE_SWATCHSCENE);
    }

    if( !ObjectClassId.IsValid() )
    {
        ObjectClassId = mManager.FindFbxFileClass(pObjectType.Buffer(), pObjectSubType.Buffer());
    }
    if( !ObjectClassId.IsValid() )
    {
        // Try to look for subtype since it's use for runtime class type
        ObjectClassId = mManager.FindClass(pObjectSubType.Buffer());
    }
        // If there were no subtype, than try to get the base class type
        if( !ObjectClassId.IsValid() )
        {
            ObjectClassId = mManager.FindClass(pObjectType.Buffer());
        }
        if( !ObjectClassId.IsValid() )
        {
            FbxString lClassName = pObjectType + "_" + pObjectSubType;
            ObjectClassId = mManager.RegisterRuntimeFbxClass( lClassName,
                                                           pParentClass,
                                                           pObjectType.Buffer(),
                                                           pObjectSubType.Buffer() );
        }
    return ObjectClassId;
}

// *******************************************************************************************************
//  Constructor and File management
// *******************************************************************************************************

// Construction
FbxReaderFbx6::FbxReaderFbx6(FbxManager& pManager, FbxImporter& pImporter, int pID, FbxStatus& pStatus) : 
    FbxReader(pManager, pID, pStatus),
    mFileObject(NULL),
    mImporter(pImporter),
    mAxisSystem(FbxAxisSystem::MayaYUp),
    mSystemUnit(FbxSystemUnit::cm),
    mDefinitionsStatistics(NULL),
    mSceneInfo( NULL ),
    mAnimLayer(NULL),
    mProgress(NULL),
    mProgressPause(true)
{
    mNodeArrayName.SetCaseSensitive(true);
    mParseGlobalSettings = false;
    mRetrieveStats = true;
	mFrameRate = FbxTime::eDefaultMode;
	SetIOSettings(pImporter.GetIOSettings());
}

// Destruction
FbxReaderFbx6::~FbxReaderFbx6()
{
    mAnimLayer = NULL;
    FbxDelete(mDefinitionsStatistics);

    if (mFileObject)
    {
        FileClose();
    }

    FBX_SAFE_DESTROY(mSceneInfo);

    FbxArrayDelete(mTakeInfo);
}

//
// File open
//
bool FbxReaderFbx6::FileOpen(char* pFileName, EFileOpenSpecialFlags pFlags)
{
    mParseGlobalSettings = (pFlags & eParseForGlobalSettings);
    mRetrieveStats = (pFlags & eParseForStatistics) != 0;

    return FileOpen(pFileName);
}

//
// Open fbx file to read.
// Try to Get file header information and open the file with fbx file object. If there is no error, read the global settings and definition section.
//
bool FbxReaderFbx6::FileOpen(char* pFileName)
{
    bool lCheckCRC = false;
    bool lParse = false;

    mData->Reset();

    if (!mFileObject)
    {
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

        // Get the Axis Information
        if (mParseGlobalSettings)
            ReadGlobalSettingsInMainSection();

        // Get the Definition Section
        if (mRetrieveStats)
            ReadDefinitionSectionForStats();

    }

    return true;
}

bool FbxReaderFbx6::FileOpen(FbxFile * pFile)
{
    bool lCheckCRC = false, lParse = false;
    if( !mFileObject )
    {
        mFileObject = FbxIO::Create(FbxIO::BinaryNormal, GetStatus());       
        mFileObject->CacheSize(IOS_REF.GetIntProp(IMP_CACHE_SIZE, 8));
    }

    if( !mFileObject->ProjectOpen(pFile, this, lCheckCRC, lParse, mImporter.GetFileHeaderInfo()) )
    {
        return false;
    }
	else
	{        
        // Get the Axis Information
        if (mParseGlobalSettings)
            ReadGlobalSettingsInMainSection();

        // Get the Definition Section
        if (mRetrieveStats)
            ReadDefinitionSectionForStats();

    }
    return true;
}

bool FbxReaderFbx6::FileOpen(FbxStream * pStream, void* pStreamData)
{
    bool lCheckCRC = false, lParse = false;
    if( !mFileObject )
    {
        mFileObject = FbxIO::Create(FbxIO::BinaryNormal, GetStatus());       
        mFileObject->CacheSize(IOS_REF.GetIntProp(IMP_CACHE_SIZE, 8));
    }

    if( !mFileObject->ProjectOpen(pStream, pStreamData, this, lCheckCRC, lParse, mImporter.GetFileHeaderInfo()) )
    {
        return false;
    }
	else
	{        
        // Get the Axis Information
        if (mParseGlobalSettings)
            ReadGlobalSettingsInMainSection();

        // Get the Definition Section
        if (mRetrieveStats)
            ReadDefinitionSectionForStats();

    }
    return true;
}

//
// Close file stream
//
bool FbxReaderFbx6::FileClose()
{
    if (!mFileObject)
    {
        GetStatus().SetCode(FbxStatus::eFailure, "File not opened");
        return false;
    }

    if (!mFileObject->ProjectClose())
    {
        FBX_SAFE_DELETE(mFileObject);
        return false;
    }
    else
    {
        FBX_SAFE_DELETE(mFileObject);
        return true;
    }
}

//
// Check whether file is ready to read
//
bool FbxReaderFbx6::IsFileOpen()
{
    return mFileObject != NULL;
}

//
// Get current file import mode
//
FbxReaderFbx6::EImportMode FbxReaderFbx6::GetImportMode()
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

//
// Get current file version
//
void FbxReaderFbx6::GetVersion(int& pMajor, int& pMinor, int& pRevision)
{
    FBX_ASSERT(mFileObject);

    FbxIO::ProjectConvertVersionNumber(mFileObject->ProjectGetSectionVersion(FBX_MAIN_SECTION),
        pMajor,
        pMinor,
        pRevision);

}

//
// Get file axis information
//

bool FbxReaderFbx6::GetAxisInfo(FbxAxisSystem* pAxisSystem, FbxSystemUnit* pSystemUnits)
{
    if (!pAxisSystem || !pSystemUnits)
        return false;

    *pAxisSystem = mAxisSystem;
    *pSystemUnits = mSystemUnit;
    return true;
}

//
// Get frame rate
//

bool FbxReaderFbx6::GetFrameRate(FbxTime::EMode &pTimeMode)
{
	if(mFrameRate == FbxTime::eDefaultMode) 
	    return false;

	pTimeMode = mFrameRate; 
	return true;
}


//
// Get file statistics
//
bool FbxReaderFbx6::GetStatistics(FbxStatistics* pStats)
{
    if (!pStats)
        return false;

    if (!mDefinitionsStatistics)
    {
        pStats->Reset();
        return false;
    }

    *pStats = *mDefinitionsStatistics;
    return true;
}

//
// Get Read options
//
bool FbxReaderFbx6::GetReadOptions(bool pParseFileAsNeeded)
{
    return GetReadOptions(NULL, pParseFileAsNeeded);
}

bool FbxReaderFbx6::GetReadOptions(FbxIO* pFbx, bool pParseFileAsNeeded)
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


// ************************************************************************************************
// Utility functions
// ************************************************************************************************

//
// Find string from string array
//
int FbxReaderFbx6::FindString(FbxString pString, FbxArray<FbxString*>& pStringArray)
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

//
// Read password from string
//
bool FbxReaderFbx6::ReadPassword(FbxString pPassword)
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

//
// Get type index from type info array
//
int FindTypeIndex(FbxString& pTypeName, FbxArray<Fbx6ObjectTypeInfo*>& pTypeList)
{
    int i, lCount = pTypeList.GetCount();

    for (i=0; i<lCount; i++)
    {
        if (pTypeList[i]->mType == pTypeName) return i;
    }

    return -1;
}

//
// Convert EMappingMode from string
//
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
    else if(!strcmp(pToken, TOKEN_KFBXGEOMETRYMESH_BY_EDGE))
    {
        lMappingMode = FbxLayerElement::eByEdge;
    }

    return lMappingMode;
}

//
// Convert EReferenceMode from string
//
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

//
// Convert BlendMode from string
//
FbxLayerElementTexture::EBlendMode ConvertBlendModeToken(const char* pToken)
{
    FbxLayerElementTexture::EBlendMode lBlendMode = FbxLayerElementTexture::eNormal;

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
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_NORMAL))
    {
        lBlendMode = FbxLayerElementTexture::eNormal;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_DISSOLVE))
    {
        lBlendMode = FbxLayerElementTexture::eDissolve;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_DARKEN))
    {
        lBlendMode = FbxLayerElementTexture::eDarken;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_COLORBURN))
    {
        lBlendMode = FbxLayerElementTexture::eColorBurn;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_LINEARBURN))
    {
        lBlendMode = FbxLayerElementTexture::eLinearBurn;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_DARKERCOLOR))
    {
        lBlendMode = FbxLayerElementTexture::eDarkerColor;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_LIGHTEN))
    {
        lBlendMode = FbxLayerElementTexture::eLighten;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_SCREEN))
    {
        lBlendMode = FbxLayerElementTexture::eScreen;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_COLORDODGE))
    {
        lBlendMode = FbxLayerElementTexture::eColorDodge;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_LINEARDODGE))
    {
        lBlendMode = FbxLayerElementTexture::eLinearDodge;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_LIGHTERCOLOR))
    {
        lBlendMode = FbxLayerElementTexture::eLighterColor;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_SOFTLIGHT))
    {
        lBlendMode = FbxLayerElementTexture::eSoftLight;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_HARDLIGHT))
    {
        lBlendMode = FbxLayerElementTexture::eHardLight;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_VIVIDLIGHT))
    {
        lBlendMode = FbxLayerElementTexture::eVividLight;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_LINEARLIGHT))
    {
        lBlendMode = FbxLayerElementTexture::eLinearLight;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_PINLIGHT))
    {
        lBlendMode = FbxLayerElementTexture::ePinLight;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_HARDMIX))
    {
        lBlendMode = FbxLayerElementTexture::eHardMix;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_DIFFERENCE))
    {
        lBlendMode = FbxLayerElementTexture::eDifference;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_EXCLUSION))
    {
        lBlendMode = FbxLayerElementTexture::eExclusion;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_SUBTRACT))
    {
        lBlendMode = FbxLayerElementTexture::eSubtract;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_DIVIDE))
    {
        lBlendMode = FbxLayerElementTexture::eDivide;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_HUE))
    {
        lBlendMode = FbxLayerElementTexture::eHue;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_SATURATION))
    {
        lBlendMode = FbxLayerElementTexture::eSaturation;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_COLOR))
    {
        lBlendMode = FbxLayerElementTexture::eColor;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_LUMINOSITY))
    {
        lBlendMode = FbxLayerElementTexture::eLuminosity;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_OVERLAY))
    {
        lBlendMode = FbxLayerElementTexture::eOverlay;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_MAXBLEND))
    {
        lBlendMode = FbxLayerElementTexture::eBlendModeCount;
    }

    return lBlendMode;
}

//
// Fix names in document
//
static void FixDocumentNames(FbxDocument* pDocument)
{
    if (pDocument == NULL)
    {
        return;
    }

    int             i, lCount = pDocument->GetMemberCount<FbxDocument>();
    FbxDocument*   lSubDoc;

    // First, recurse on sub-documents
    for (i = 0; i < lCount; i++)
    {
        lSubDoc = pDocument->GetMember<FbxDocument>(i);
        FixDocumentNames(lSubDoc);
    }

    if (pDocument->GetDocument() == NULL)
    {
        return;
    }

    FbxString  lSep("::");
    FbxString         lDocName = pDocument->GetNameOnly() + lSep;

    // Then correct names of all contained objects
    lCount = pDocument->GetMemberCount();

    for (i = 0; i < lCount; i++)
    {
        FbxObject* lObj = pDocument->GetMember(i);
        FbxString     lObjName = lObj->GetNameOnly();

        if (lObjName.Find(lDocName) == 0)
        {
            FbxString lObjNewName = lObjName.Right(lObjName.GetLen() - lDocName.GetLen());
            lObj->SetInitialName(lObjNewName.Buffer());
            lObj->SetName(lObjNewName.Buffer());
        }
    }
}

// *******************************************************************************************************
//  Constructor and reader
// *******************************************************************************************************

//
// Read current file data to document
//
bool FbxReaderFbx6::Read(FbxDocument* pDocument)
{
    if( !pDocument )
    {
        GetStatus().SetCode(FbxStatus::eFailure, "Invalid document handle");
        return false;
    }

	//Notify plugins before reading FBX file
	FbxScene* lScene = FbxCast<FbxScene>(pDocument);
#ifndef FBXSDK_ENV_WINSTORE
	if( lScene )
	{
		PluginsReadBegin(*lScene);
	}
#endif

	bool Result = Read(pDocument, NULL);

	//Notify plugins after reading FBX file
#ifndef FBXSDK_ENV_WINSTORE
	if( lScene )
	{
		PluginsReadEnd(*lScene);
	}
#endif

    return Result;
}

//
//     Read data to document by file object.
//
// 1.  Perform pre-import events to notify plugins
// 2.  Clean up template, object and attribute map to avoid confliction
// 3.  Create stream option for fbx reader
// 4.  Read document description, external reference section
// 5.  Read object section
// 6.  Read embedded media file
// 7.  Read global settings
// 8.  Read connection section
// 9.  Read animation
// 10. Connect textures to materials
// 11. Clean up
// 12. Perform post-import events to notify plugins

bool FbxReaderFbx6::Read(FbxDocument* pDocument, FbxIO* pFbx)
{
    if (!pDocument)
    {
        GetStatus().SetCode(FbxStatus::eFailure, "Invalid document handle");
        return false;
    }

	// note: sprintf() use the locale to find the decimal separator
	// French, Italian, German, ... use the comma as decimal separator
	// so we need a way to be un-localized into writing/reading our files formats

	// force usage of a period as decimal separator
	char lPrevious_Locale_LCNUMERIC[100]; memset(lPrevious_Locale_LCNUMERIC, 0, 100);
	FBXSDK_strcpy(lPrevious_Locale_LCNUMERIC, 100, setlocale(LC_NUMERIC, 0  ));	// query current setting for LC_NUMERIC
	char *lCurrent_Locale_LCNUMERIC  = setlocale(LC_NUMERIC, "C");  // set locale using period as decimal separator

    FbxScene*      lScene = FbxCast<FbxScene>(pDocument);
    bool            lIsAScene = (lScene != NULL);

    FbxIO*           lInternalFbx = NULL;
    bool            lResult = true;
    bool            lDontResetPosition = false;

#ifndef FBXSDK_ENV_WINSTORE
    // Notify the plugins we are about to do an import.
    FbxEventPreImport lPreEvent( pDocument );
    pDocument->GetFbxManager()->EmitPluginsEvent( lPreEvent );
#endif

    mClassTemplateMap.Clear();
    mObjectMap.Clear();
    
    if (lResult)
    {
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
    }

    FbxIO::FbxAutoResetXRefManager lSetXRefManager(mFileObject, pDocument->GetFbxManager()->GetXRefManager());

    if( lResult )
    {
        if( mFileObject->ProjectGetCurrentSection() == FBX_NO_SECTION )
        {
			if (!mFileObject->ProjectOpenMainSection())
			{
                GetStatus().SetCode(FbxStatus::eInvalidFile, "File is corrupted %s", mFileObject->GetFilename());
                lResult = false;
			}
        }
        else
        {
            if( !lDontResetPosition ) mFileObject->FieldReadResetPosition();
        }
    }

    if( lResult )
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

    if( lIsAScene )
    {
        lScene->Clear();
        mNodeArrayName.Clear();
    }

    //
    // Read document description section
    //
    if( lResult )
    {
        FbxString lDocumentName;

        lResult = ReadDescriptionSection(pDocument, lDocumentName);

        if (lResult)
        {
            pDocument->SetInitialName(lDocumentName.Buffer());
            pDocument->SetName(lDocumentName.Buffer());
        }
    }

    Fbx6TypeReadReferences lDocumentReferences;

    //
    // Read document external references section
    //
    if( lResult )
    {
        lResult = ReadReferenceSection(pDocument, lDocumentReferences);

        if (lResult)
        {

            int i, lCount = mManager.GetDocumentCount();

            FbxLibrary* lLibrary = FbxCast<FbxLibrary>(pDocument);

            for (i = 0; i < lCount; i++)
            {
                FbxDocument* lExtDoc = mManager.GetDocument(i);
                
                if( lExtDoc != pDocument )
                {
                    if( lExtDoc->GetRootDocument() != pDocument->GetRootDocument() )
                    {
                        lDocumentReferences.ResolveForDocument(pDocument, lExtDoc);
                    } else if( lLibrary )
                    {
                        FbxLibrary* lExtLibrary = FbxCast<FbxLibrary>(lExtDoc);

                        if( lExtLibrary && lExtLibrary != mManager.GetRootLibrary() &&
                            lExtLibrary != mManager.GetUserLibraries() &&
                            lExtLibrary != mManager.GetSystemLibraries() )
                        {
                            lDocumentReferences.ResolveForDocument(pDocument, lExtDoc, true);
                        }
                    }
                }
            }

            if (lDocumentReferences.AreAllExternalReferencesResolved() == false)
            {             
                GetStatus().SetCode(FbxStatus::eFailure, "Unresolved external references");
            }
        }
    }

    FbxArray<Fbx6ObjectTypeInfo*> lObjectDefinitionContent;

    if( lResult )
    {
        lResult = ReadDefinitionSection(pDocument, lObjectDefinitionContent);
    }

    if (lResult)
    {
        if (lIsAScene)
        {
            ReadCameraSwitcher(*lScene);
        }

        // Always read the nodes.
        mFileObject->FieldReadResetPosition();

        mProgressPause = false;

        lResult = ReadObjectSection(pDocument, lObjectDefinitionContent, lDocumentReferences);

        mProgressPause = true;

        // make sure the root node is registered as Model::Scene, so
        // it match the System object of MB.
        if (lIsAScene && lScene->GetRootNode())
        {
            mObjectMap.Remove(mObjectMap.Find("Model::Scene"));
            mObjectMap.Add("Model::Scene",lScene->GetRootNode());
        }
    }

    if (lResult)
    {
        if (IOS_REF.GetBoolProp(IMP_FBX_TEXTURE, true))
        {            
            ReadMedia(pDocument);
        }
    }

    if (lIsAScene && lResult)
    {
        if(IOS_REF.GetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true))
        {
            if (mFileObject->FieldReadBegin("Version5"))
            {
                if (mFileObject->FieldReadBlockBegin())
                {                   
                    ReadGlobalLightSettings(*lScene);
                    ReadGlobalTimeSettings(*lScene);
                    ReadGlobalCameraSettings(*lScene);
                    mFileObject->FieldReadBlockEnd();
                }
                mFileObject->FieldReadEnd();
            }
        }
    }

    if (lResult)
    {
        ReadConnectionSection(pDocument);
    }

    if (lResult)
    {
        // Import the animation.
        if (IOS_REF.GetBoolProp(IMP_FBX_ANIMATION, true) && mTakeInfo.GetCount() > 0)
        {
            lResult = ReadDocumentAnimation(pDocument);
        }
    }

	if (lResult)
	{
		if (!IOS_REF.GetBoolProp(IMP_RELAXED_FBX_CHECK, false))
		{
			FbxSceneCheckUtility lSceneCheck(lScene, &mStatus);
			lResult = lSceneCheck.Validate();
		}
	}

    if (lIsAScene && lResult)
    {
        // Let's try to give the PRODUCER cameras to the globalCameraSettings object.
        // Actually we are going to copy the camera attributes.
        int i;
        FbxArray<int> cameraSwitcherId;
        FbxArray<int> lProducerCamerasId;

        // scan all the nodes and only process cameras
        for (i = 0; i < mNodeArrayName.GetCount(); i++)
        {
            FbxNode* lCurrentNode = (FbxNode*) mNodeArrayName[i];
            FbxCamera* cam = lCurrentNode->GetCamera(); // ignore anything that is not a camera
            if (cam)
            {
                // the CopyProducerCameras can ignore the cam argument if the name is not one of the
                // producer cameras
                if (lScene->GlobalCameraSettings().CopyProducerCamera(lCurrentNode->GetNameWithoutNameSpacePrefix(), cam, IOS_REF.GetBoolProp(IMP_KEEP_PRODUCER_CAM_SRCOBJ, false)))
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
				FbxCameraSwitcher* lPreviousCam = lScene->GlobalCameraSettings().GetCameraSwitcher();
				if (lPreviousCam)
				{
					FbxNode* lPreviousNode = lPreviousCam->GetNode();
					if (lPreviousNode)
					{
						mObjectMap.Remove(mObjectMap.Find(lPreviousNode->GetName()));
					}
				}
				
                lScene->GlobalCameraSettings().SetCameraSwitcher(cams);                
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
		        
        // Post-process command goes here       
        lScene->ReindexMaterialConnections();
        lScene->BuildTextureLayersDirectArray();
        lScene->FixInheritType(lScene->GetRootNode());

        ReorderCameraSwitcherIndices(*lScene);
        RebuildTrimRegions(*lScene);
        SetSubdivision(*lScene);
		ConvertShapeDeformProperty(*lScene);

        FbxMaterialConverter lConv( mManager );
        lConv.ConnectTexturesToMaterials( *lScene );   

        RebuildLayeredTextureAlphas(*lScene);

        FbxRenamingStrategyFbx6 lRenaming;
        lRenaming.DecodeScene(lScene);

		//Normally, we would have put this code in ReadLight, but since the properties are read
		//after ReadLight, they wouldn't be initialized with file value. So we have no choice
		//to process this after the file is done reading properties.
        int lCount;
		for( i = 0, lCount = lScene->GetSrcObjectCount<FbxLight>(); i < lCount; i++ )
		{
			FbxLight* lLight = lScene->GetSrcObject<FbxLight>(i);
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
		for (i = 0, lCount = lScene->GetSrcObjectCount<FbxCharacter>(); i < lCount; i++)
		{
			FbxCharacter* lCharacter = lScene->GetSrcObject<FbxCharacter>(i);
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
		for( i = 0, lCount = lScene->GetSrcObjectCount<FbxConstraint>(); i < lCount; ++i )
		{
			FbxConstraint* lConstraint = lScene->GetSrcObject<FbxConstraint>(i);
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
		for( i = 0, lCount = lProperties.Size(); i < lCount; ++i )
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

		FbxGlobalCameraSettings& lGCS = lScene->GlobalCameraSettings();
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerPerspective());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerBack());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerBottom());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerFront());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerLeft());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerRight());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerTop());
    }

    {
        // If we've created an fbm folder, let the client know about it.
        FbxString lDefaultPath = "";
        FbxString lUserDefinePath = IOS_REF.GetStringProp(IMP_EXTRACT_FOLDER, lDefaultPath);
        const char* lUserDefinePathBuffer = (lUserDefinePath == lDefaultPath) ? NULL : lUserDefinePath.Buffer();
        FbxString lMediaFolder = mFileObject->GetMediaDirectory(false, lUserDefinePathBuffer);

        if( !lMediaFolder.IsEmpty() )
        {
            FbxDocumentInfo* lDocumentInfo = pDocument->GetDocumentInfo();

            if( !lDocumentInfo )
            {
                lDocumentInfo = FbxDocumentInfo::Create(pDocument->GetFbxManager(), "");
                pDocument->SetDocumentInfo(lDocumentInfo);
            }

            if( lDocumentInfo )
            {
                lDocumentInfo->EmbeddedUrl.Set(lMediaFolder);
            }
        }
    }

    // At this step, the hierarchy of embedded documents has been reconstructed
    // because their are stored in a flat manner in the file, we now have to
    // correct their names
    if (lResult)
    {
        FixDocumentNames(pDocument);
    }

    if (pFbx)
    {
        mFileObject = lInternalFbx;
    }

    FbxArrayDelete(lObjectDefinitionContent);

#ifndef FBXSDK_ENV_WINSTORE
    // once we are done post-processing the scene notify plugins.
    if( lResult )
    {
        FbxEventPostImport lEvent( pDocument );
        pDocument->GetFbxManager()->EmitPluginsEvent( lEvent );
    }
#endif

	// set numeric locale back
	setlocale(LC_NUMERIC, lPrevious_Locale_LCNUMERIC);

    if(!IOS_REF.GetBoolProp(IMP_RELAXED_FBX_CHECK, false))
    {
  	    // if at some point the eInvalidFile error has been raised inside a block that still returned true,
	    // we should consider the file corrupted
    	if (lResult && mStatus.GetCode() == FbxStatus::eInvalidFile)
    		lResult = false;
    }
    return lResult;
}

void FbxReaderFbx6::SetProgressHandler(FbxProgress *pProgress)
{
    mProgress = pProgress;
}

void FbxReaderFbx6::SetEmbeddingExtractionFolder(const char* pExtractFolder)
{
	mFileObject->SetEmbeddingExtractionFolder(pExtractFolder);
}

//
// Read properties settings and flags
//
void FbxReaderFbx6::PluginReadParameters(FbxObject& pParams)
{
    ReadPropertiesAndFlags(&pParams, mFileObject, false);
}

// ****************************************************************************************
// Read Headers and sections
// ****************************************************************************************
void FbxReaderFbx6::ReadGlobalSettingsInMainSection()
{
    mFileObject->ProjectOpenMainSection();

    if (mFileObject->FieldReadBegin(FIELD_OBJECT_PROPERTIES))
    {
        if (mFileObject->FieldReadBlockBegin())
        {
            FbxGlobalSettings* gs = FbxGlobalSettings::Create(&mManager,"TempGlobalSettings");
            FBX_ASSERT(gs != NULL);
            if (gs)
            {
                if (mFileObject->FieldReadBegin(FIELD_GLOBAL_SETTINGS))
                {
                    if (mFileObject->FieldReadBlockBegin())
                    {
                        ReadGlobalSettings(*gs);

                        mAxisSystem = gs->GetAxisSystem();
                        mSystemUnit = gs->GetSystemUnit();
						mFrameRate  = gs->GetTimeMode();

                        gs->Destroy(); 

                        mFileObject->FieldReadBlockEnd();
                    }
                    mFileObject->FieldReadEnd ();
                }
            }
            mFileObject->FieldReadBlockEnd();
        }
        mFileObject->FieldReadEnd ();
    }

    mFileObject->FieldReadResetPosition();
    mFileObject->ProjectCloseSection();
}

void FbxReaderFbx6::ReadDefinitionSectionForStats()
{
    // will get deleted by the destructor
    if (!mDefinitionsStatistics)
        mDefinitionsStatistics = FbxNew< FbxStatisticsFbx >();

    mFileObject->ProjectOpenMainSection();

    if (mFileObject->FieldReadBegin(FIELD_OBJECT_DEFINITION))
    {
        if(mFileObject->FieldReadBlockBegin())
        {
            mFileObject->FieldReadI(FIELD_OBJECT_DEFINITION_VERSION);
            while (mFileObject->FieldReadBegin(FIELD_OBJECT_DEFINITION_OBJECT_TYPE))
            {
                FbxString lType = mFileObject->FieldReadC();
                if( mFileObject->FieldReadBlockBegin() )
                {
                    int count = mFileObject->FieldReadI(FIELD_OBJECT_DEFINITION_COUNT);
                    ((FbxStatisticsFbx*)mDefinitionsStatistics)->AddItem(lType, count);
                    mFileObject->FieldReadBlockEnd();
                }
                mFileObject->FieldReadEnd();
            }

            mFileObject->FieldReadBlockEnd();
        }
        mFileObject->FieldReadEnd();
    }

    mFileObject->FieldReadResetPosition();
    mFileObject->ProjectCloseSection();

    // In case we have a summary section in the file, we fill the statistics (as much as we can) with 
    // the information in the there
    int  lDummy;
    if (ReadOptionsInExtensionSection(lDummy))
    {
        FbxString lSummaryType[6];
        int lSummaryValue[6];

        // create type strings to avoid compiler error on linux: the argument of AddItem is a reference to a FbxString!
        lSummaryType[0] = FbxString(FIELD_SUMMARY_CONTENT_COUNT_MODEL);
        lSummaryType[1] = FbxString(FIELD_SUMMARY_CONTENT_COUNT_DEVICE);
        lSummaryType[2] = FbxString(FIELD_SUMMARY_CONTENT_COUNT_CHARACTER);
        lSummaryType[3] = FbxString(FIELD_SUMMARY_CONTENT_COUNT_ACTOR);
        lSummaryType[4] = FbxString(FIELD_SUMMARY_CONTENT_COUNT_CONSTRAINT);
        lSummaryType[5] = FbxString(FIELD_SUMMARY_CONTENT_COUNT_MEDIA);

        lSummaryValue[0] = IOS_REF.GetIntProp(IMP_FBX_MODEL_COUNT, 0);
        lSummaryValue[1] = IOS_REF.GetIntProp(IMP_FBX_DEVICE_COUNT, 0);
        lSummaryValue[2] = IOS_REF.GetIntProp(IMP_FBX_CHARACTER_COUNT, 0);
        lSummaryValue[3] = IOS_REF.GetIntProp(IMP_FBX_ACTOR_COUNT, 0);
        lSummaryValue[4] = IOS_REF.GetIntProp(IMP_FBX_CONSTRAINT_COUNT, 0);
        lSummaryValue[5] = IOS_REF.GetIntProp(IMP_FBX_MEDIA_COUNT, 0);
    
        for (int i = 0; i < 6; i++)
        {
            if (lSummaryValue[i] > 0)
            {
                FbxStatisticsFbx* lStats = ((FbxStatisticsFbx*)mDefinitionsStatistics);

                // check if we already filled the stats from the Defintions section
                bool needToAdd = true;
                for (int j = 0; j < lStats->GetNbItems(); j++)
                {
                    FbxString lItemName;
                    int lItemCount;
                    lStats->GetItemPair(j, lItemName, lItemCount);
                    if (lSummaryType[i] == lItemName || (i==5 && lItemName=="Video"))
                    {
                        if (lItemCount > 0)
                        {
                            // use the value retrieved from the Definition section
                            needToAdd = false;
                            break;
                        }
                    }
                }

                if (needToAdd)
                    lStats->AddItem(lSummaryType[i], lSummaryValue[i]);
            }
        }
    }
}

void FbxReaderFbx6::ReadOptionsInMainSection()
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

    IOS_REF.SetIntProp(IMP_FBX_MODEL_COUNT, lContentCount );

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

    IOS_REF.SetIntProp(IMP_FBX_DEVICE_COUNT, lContentCount );

    ReadTakeOptions();

    mFileObject->FieldReadResetPosition();
}

//
// Read take options
//
void FbxReaderFbx6::ReadTakeOptions()
{
    FbxString lString;

    FbxArrayDelete(mTakeInfo);

    IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString("") );

    //Retrieve fbx info for getting some takes infos out
    if (mFileObject->FieldReadBegin ("Takes"))
    {
        bool lCurrentTakeFound = false; // Used to validate if the CurrentTake name is valid

        if (mFileObject->FieldReadBlockBegin())
        {
            lString=mFileObject->FieldReadC ("Current");

            IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, lString );

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

                        if (lTakeFbxObject.ProjectOpenDirect(lFullFileName.Buffer(), this))
                        {
                            lNewInfo->mDescription = lTakeFbxObject.FieldReadC ("Comments");
                            lNewInfo->mLocalTimeSpan = lTakeFbxObject.FieldReadTS("LocalTime");
                            lNewInfo->mReferenceTimeSpan = lTakeFbxObject.FieldReadTS("ReferenceTime");
                            lTakeFbxObject.ProjectClose ();
                        }
                        else if (mFileObject->IsEmbedded ())
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

                if(IOS_REF.GetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString("")) == lNewInfo->mName)
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
				IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, mTakeInfo[0]->mName );
            }
            else
            {
                IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString(""));
            }
        }
    }
}

//
// Read options in certain section
//
bool FbxReaderFbx6::ReadOptionsInExtensionSection(int& pSectionIndex)
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

        IOS_REF.SetBoolProp(IMP_FBX_TEMPLATE, (bool)mFileObject->FieldReadB(FIELD_SUMMARY_TEMPLATE));
        IOS_REF.SetBoolProp(IMP_FBX_PASSWORD_ENABLE, (bool)mFileObject->FieldReadB(FIELD_SUMMARY_PASSWORD_PROTECTION));

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
			if(mSceneInfo) mSceneInfo->Destroy();
			mSceneInfo = ReadSceneInfo();
        }

        ReadTakeOptions();

        mFileObject->FieldReadBlockEnd();
    }
    mFileObject->FieldReadEnd();

    mFileObject->ProjectCloseSection();

    return true;
}

//
// Write options in extension section
//
bool FbxReaderFbx6::WriteOptionsInExtensionSection(bool pOverwriteLastExtensionSection)
{
    if (!mFileObject->ProjectCreateExtensionSection(pOverwriteLastExtensionSection))
    {
        // Clear last error if function failed.
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
    //      - added the scene info
    //      - added block version number for FIELD_SUMMARY_TAKES
    mFileObject->FieldWriteI(FIELD_SUMMARY_VERSION, 101);

    mFileObject->FieldWriteB(FIELD_SUMMARY_TEMPLATE, IOS_REF.GetBoolProp(IMP_FBX_TEMPLATE, false)) ;

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

    if (mSceneInfo)
    {
        WriteSceneInfo(mSceneInfo);
    }

    mFileObject->FieldWriteBegin(FIELD_SUMMARY_TAKES);
    mFileObject->FieldWriteBlockBegin();
   
    // The FIELD_SUMMARY_TAKES_VERSION field did not exist in
    // the initial block. It has been added in v101 of FIELD_SUMMARY_VERSION.
    // v100 has been left to represent the "original" block.
    //
    // Version 101: added the take thumbnail
    mFileObject->FieldWriteI(FIELD_SUMMARY_TAKES_VERSION, 101);

    FbxString lString = IOS_REF.GetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString(""));
    mFileObject->FieldWriteC(FIELD_SUMMARY_TAKES_CURRENT, lString.Buffer());
   
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


// ****************************************************************************************
// Global
// ****************************************************************************************

//
// Read thumbnail from file
//
FbxThumbnail* FbxReaderFbx6::ReadThumbnail()
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

            ReadPropertiesAndFlags( lThumbnail, mFileObject );

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

//
// Write thumbnail
//
bool FbxReaderFbx6::WriteThumbnail(FbxThumbnail* pThumbnail)
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

//
// Read scene info
//
FbxDocumentInfo* FbxReaderFbx6::ReadSceneInfo(FbxString& pType)
{
    FbxDocumentInfo* lSceneInfo = NULL;
    if (pType.CompareNoCase("UserData") == 0)
    {
        lSceneInfo = FbxDocumentInfo::Create(&mManager, "");

        mFileObject->FieldReadI(FIELD_SCENEINFO_VERSION);

        // Read the scene thumbnail
        lSceneInfo->SetSceneThumbnail(ReadThumbnail());

        // Read the Meta-Data
        if (mFileObject->FieldReadBegin(FIELD_SCENEINFO_METADATA))
        {
            if (mFileObject->FieldReadBlockBegin())
            {
                mFileObject->FieldReadI(FIELD_SCENEINFO_METADATA_VERSION);

                lSceneInfo->mTitle      = mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_TITLE);
                lSceneInfo->mSubject    = mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_SUBJECT);
                lSceneInfo->mAuthor     = mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_AUTHOR);
                lSceneInfo->mKeywords   = mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_KEYWORDS);
                lSceneInfo->mRevision   = mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_REVISION);
                lSceneInfo->mComment    = mFileObject->FieldReadS(FIELD_SCENEINFO_METADATA_COMMENT);

                mFileObject->FieldReadBlockEnd();
            }
            mFileObject->FieldReadEnd();
        }

        // Get the properties
        ReadPropertiesAndFlags( lSceneInfo,mFileObject );
    }

    return lSceneInfo;
}

FbxDocumentInfo* FbxReaderFbx6::ReadSceneInfo()
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

//
// Write scene info
//
void FbxReaderFbx6::WriteSceneInfo(FbxDocumentInfo* pSceneInfo)
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

//
// Read description section and assign the document name
//
bool FbxReaderFbx6::ReadDescriptionSection(FbxDocument *pDocument, FbxString& pDocumentName)
{
    bool lRet = true;

    if (mFileObject->FieldReadBegin(FIELD_OBJECT_DESCRIPTION))
    {
        if(mFileObject->FieldReadBlockBegin())
        {
            pDocumentName = mFileObject->FieldReadC(FIELD_OBJECT_DESCRIPTION_NAME);
            mFileObject->FieldReadBlockEnd();
        }
        mFileObject->FieldReadEnd();
    }

    return lRet;
}

//
// Read reference section section and fill the external references
//
bool FbxReaderFbx6::ReadReferenceSection(FbxDocument *pDocument, Fbx6TypeReadReferences& pDocReferences)
{
    FbxString lExternal("External");
    bool            lRet = true;

    if (mFileObject->FieldReadBegin(FIELD_OBJECT_REFERENCES))
    {
        if(mFileObject->FieldReadBlockBegin())
        {
            while(mFileObject->FieldReadBegin(FIELD_OBJECT_REFERENCES_FILE_PATH_URL))
            {
                FbxEventReferencedDocument lEvent;

                lEvent.mFilePathUrl = mFileObject->FieldReadC();
                if (mFileObject->FieldReadBlockBegin())
                {
                    if (mFileObject->FieldReadBegin(FIELD_OBJECT_DEFINITION_OBJECT_TYPE))
                    {
                        lEvent.mClassName = mFileObject->FieldReadC();
                        mFileObject->FieldReadEnd();
                    }

                    int lNumDocRead = 0;

                    while (mFileObject->FieldReadBegin("Document"))
                    {
                        FbxString lRefDocName = mFileObject->FieldReadC();

                        if (mFileObject->FieldReadBlockBegin())
                        {
                            ++lNumDocRead;

                            // Accumulate the name of the parent: every time
                            // we find a document name, before overwriting
                            // the previous one we had found (if any),
                            // we add that previously found name to the parent
                            // full document path.
                            if (! lEvent.mDocumentName.IsEmpty())
                            {
                                if( lEvent.mParentFullName.IsEmpty() )
                                {
                                    lEvent.mParentFullName = lEvent.mDocumentName;
                                }
                                else
                                {
                                    FbxString lSep("::");
                                    lEvent.mParentFullName += lSep;
                                    lEvent.mParentFullName += lEvent.mDocumentName;
                                }
                            }

                            // Simple document name is always the last one found.
                            lEvent.mDocumentName = lRefDocName;
                        }
                    }

                    for (int i = 0; i < lNumDocRead; i++)
                    {
                        mFileObject->FieldReadBlockEnd();
                    }

                    mFileObject->FieldReadBlockEnd();
                }
                pDocument->Emit(lEvent);

                mFileObject->FieldReadEnd();
            }

            while (mFileObject->FieldReadBegin(FIELD_OBJECT_REFERENCES_REFERENCE))
            {
                FbxString lRefName = mFileObject->FieldReadC();
                FbxString lRefTypeName = mFileObject->FieldReadC();
                bool    lRefIsExternal = (lExternal == lRefTypeName);
                FbxString lRefObjName;
                FbxString lRefDocRootName;
                FbxString lRefDocPathName;

                if (mFileObject->FieldReadBlockBegin())
                {

                    if (mFileObject->FieldReadBegin("Object"))
                    {
                        lRefObjName = mFileObject->FieldReadC();
                        mFileObject->FieldReadEnd();
                    }

                    int i, lNumDocRead = 0;
                    while (mFileObject->FieldReadBegin("Document"))
                    {
                        FbxString lRefDocName = mFileObject->FieldReadC();

                        if (mFileObject->FieldReadBlockBegin())
                        {
                            if (lNumDocRead == 0)
                            {
                                lRefDocRootName = lRefDocName;
                                lRefDocPathName = lRefDocName;
                            }
                            else
                            {
                                FbxString lSep("::");
                                lRefDocPathName += lSep;
                                lRefDocPathName += lRefDocName;
                            }
                            lNumDocRead++;
                        }

                    }
                    for (i = 0; i < lNumDocRead; i++)
                    {
                        mFileObject->FieldReadBlockEnd();
                    }

                    pDocReferences.AddReference(lRefIsExternal, lRefName.Buffer(), lRefObjName.Buffer(), lRefDocRootName.Buffer(), lRefDocPathName.Buffer());

                    mFileObject->FieldReadBlockEnd();
                }

                mFileObject->FieldReadEnd();
            }

            mFileObject->FieldReadBlockEnd();
        }
        mFileObject->FieldReadEnd();
    }

    return lRet;
}

//
// Read definition section
//
bool FbxReaderFbx6::ReadDefinitionSection(FbxDocument* pDocument, FbxArray<Fbx6ObjectTypeInfo*>& pObjectContent)
{
    bool lRet = true;

    if (mFileObject->FieldReadBegin(FIELD_OBJECT_DEFINITION))
    {
        if(mFileObject->FieldReadBlockBegin())
        {
            mFileObject->FieldReadI(FIELD_OBJECT_DEFINITION_VERSION);
            while (mFileObject->FieldReadBegin(FIELD_OBJECT_DEFINITION_OBJECT_TYPE))
            {
                FbxString lType = mFileObject->FieldReadC();
                // We need to bring the "Character" back as a constraint (it was extracted so we can count
                // the number of characters in the file - required by MoBu) but in fact, it has to be 
                // processed as a constraint regardless.
                if (lType == TOKEN_KFBXCONSTRAINT_CHARACTER)
                    lType = FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONSTRAINT;

                if (FindTypeIndex(lType, pObjectContent) == -1)
                {
                    Fbx6ObjectTypeInfo* lTypeInfo = FbxNew< Fbx6ObjectTypeInfo >();
                    lTypeInfo->mType = lType;

                    pObjectContent.Add(lTypeInfo);

                    if( mFileObject->FieldReadBlockBegin() )
                    {
                        // property templates for this object definiton
                        // and yes, there might be more than one, since some
                        // kfbxnodeattributes get merged with their nodes.
                        while (mFileObject->FieldReadBegin(FIELD_OBJECT_PROPERTY_TEMPLATE))
                        {
                            FbxString lFbxClassName = mFileObject->FieldReadC();

                            if(mFileObject->FieldReadBlockBegin())
                            {
                                FbxClassId lId = mManager.FindClass( lFbxClassName );

                                if( lId.IsValid() )
                                {
                                    FbxObject* lTemplateObj = static_cast<FbxObject*>(mManager.CreateNewObjectFromClassId( lId, lFbxClassName + "_TemplateObject" ));
                                    mManager.UnregisterObject(lTemplateObj);

                                    if( ReadProperties( lTemplateObj, mFileObject ) )
                                    {
                                        bool lSuccess = mClassTemplateMap.AddClassId( lId, lTemplateObj );
                                        FBX_ASSERT( lSuccess );
                                    }
                                }
                                mFileObject->FieldReadBlockEnd();
                            }
                            mFileObject->FieldReadEnd();
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

    return lRet;
}

//
// Read global light settings from scene
//
void FbxReaderFbx6::ReadGlobalLightSettings(FbxScene& pScene)
{
    pScene.GlobalLightSettings().RestoreDefaultSettings();
    ReadAmbientColor(pScene);
    ReadFogOption(pScene);
    ReadShadowPlane(pScene);
}

//
// Read global time settings from scene
//
void FbxReaderFbx6::ReadGlobalTimeSettings(FbxScene& pScene)
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
				lTimeModeValue = FbxGetTimeModeFromOldValue ((FbxTime::EOldMode) mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_TIME_MODE, pScene.GetGlobalSettings().GetTimeMode()) );
            }
            else
            {
                lTimeModeValue = FbxGetTimeModeFromFrameRate ( lFrameRate.Buffer() );
            }

            lGlobalSettings.SetTimeMode((FbxTime::EMode)lTimeModeValue );
            lGlobalSettings.SetTimeProtocol((FbxTime::EProtocol) mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_TIME_PROTOCOL, lGlobalSettings.GetTimeProtocol()));
            lGlobalSettings.SetSnapOnFrameMode((FbxGlobalSettings::ESnapOnFrameMode) mFileObject->FieldReadI(FIELD_KFBXGLOBALTIMESETTINGS_SNAP_ON_FRAMES, lGlobalSettings.GetSnapOnFrameMode()));

            FbxTime lStartTime = FBXSDK_TIME_ZERO, lStopTime = FBXSDK_TIME_ONE_SECOND;
            lStartTime = mFileObject->FieldReadLL(FIELD_KFBXGLOBALTIMESETTINGS_TIMELINE_START_TIME, lStartTime.Get());
            lStopTime  = mFileObject->FieldReadLL(FIELD_KFBXGLOBALTIMESETTINGS_TIMELINE_STOP_TIME,  lStopTime.Get());
            lGlobalSettings.SetTimelineDefaultTimeSpan(FbxTimeSpan(lStartTime, lStopTime));

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

//
// Read global camera settings from scene
//
void FbxReaderFbx6::ReadGlobalCameraSettings(FbxScene& pScene)
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

//
// Read shadow plane from scene
//
void FbxReaderFbx6::ReadShadowPlane(FbxScene& pScene)
{
    int          lShadowPlaneCount;
    int          lCount;
    FbxVector4  lPlaneOrigin;
    FbxVector4  lPlaneNormal;

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
        // Read throught all the Shadow Planes in the section...
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

//
// Read ambient color from scene
//
void FbxReaderFbx6::ReadAmbientColor(FbxScene& pScene)
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

//
// Read fog option from scene
//
void FbxReaderFbx6::ReadFogOption(FbxScene& pScene)
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

// ****************************************************************************************
// Read Objects sections
// ****************************************************************************************
bool FbxReaderFbx6::ReadObjectSection
(
 FbxDocument*                       pDocument,
 FbxArray<Fbx6ObjectTypeInfo*>&    pObjectContent,
 Fbx6TypeReadReferences&                pDocReferences
 )
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
                    FbxString lObjectUniqueId  = mFileObject->FieldReadC();
                    FbxString lObjectName      = FbxObject::StripPrefix(lObjectUniqueId);
                    FbxString lObjectSubType = mFileObject->FieldReadC();

                    // Check if this object is a reference to another one
                    FbxString lIsARefStr = mFileObject->FieldReadC();

                    FbxObject* lReferencedObject = NULL;
                    if (!lIsARefStr.IsEmpty() && lIsARefStr.Compare(FIELD_KFBXOBECT_REFERENCE_TO) == 0)
                    {
                        FbxObject* lTmpObj;
                        FbxString     lRefStr = mFileObject->FieldReadC();
                        FbxString     lRefObjStr;
                        bool        lRefExternal;

                        if (pDocReferences.GetReferenceResolution(lRefStr.Buffer(), lRefObjStr, lRefExternal, lTmpObj))
                        {
                            if (lRefExternal)
                            {
                                lReferencedObject = lTmpObj;
                            }
                            else
                            {
                                lReferencedObject = mObjectMap.Get(mObjectMap.Find(lRefObjStr.Buffer()));
                            }
                        }
                    }

                    if (mFileObject->FieldReadBlockBegin())
                    {
                        ReadObject(pDocument, lObjectType, lObjectSubType, lObjectName, lObjectUniqueId, lReferencedObject, pDocReferences);

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

//
// Read object information from file
//
bool FbxReaderFbx6::ReadObject(
                                FbxDocument*   pDocument,
                                FbxString&        pObjectType,
                                FbxString&        pObjectSubType,
                                FbxString&        pObjectName,
                                FbxString&        pObjectUniqueId,
                                FbxObject*     pReferencedObject,
                                Fbx6TypeReadReferences& pDocReferences
                                )
{
    FbxScene*      lScene = FbxCast<FbxScene>(pDocument);
    bool            lIsAScene = (lScene != NULL);
    FbxClassId     ObjectClassId;
    bool            InstanciateUnknownObject=false;

	FbxStatus lPrevStatus = mStatus; // Because the file can still be read even if errors are
	// encountered, we remember the current status (in case of important error such as: file corrupted")

    if( pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_PLUGIN_PARAMS )
    {
#ifndef FBXSDK_ENV_WINSTORE
        PluginsRead(pObjectName.Buffer(), pObjectSubType.Buffer());
#endif
    }
    else if( pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_SCENEINFO )
    {
        FbxDocumentInfo* lDocumentInfo = ReadSceneInfo(pObjectSubType);
        if( lDocumentInfo )    //Read a SceneInfo
        {
            pDocument->SetDocumentInfo(lDocumentInfo);
        }
    }
    else if( pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MODEL )
    {
        // Note: figure why when not a scene those object used to be not loaded.
        //       Currently, this would fail to load the camera in render context
        //       file, which was completely beside the whole point.               

        {
            // Note: for version 6.0 and previous
            if (pObjectSubType.IsEmpty())
            {
                if (strcmp (pObjectName.Buffer(), "Camera Switcher") == 0)
                {
                    pObjectSubType = "CameraSwitcher";
                }
                else
                {
                    pObjectSubType = "Mesh";
                }
            }
            // Read a FbxNode
            FbxNode* lNode = CreateOrCloneReference<FbxNode>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

            mNodeArrayName.Add (const_cast<char*>(lNode->GetName()), (FbxHandle)lNode);

            ReadNode(*lNode, pObjectSubType, pDocReferences);

            mObjectMap.Add(pObjectUniqueId.Buffer(),lNode);
            pDocument->ConnectSrcObject(lNode);
        }
    }
    else if( pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_NODE_ATTRIBUTE )
    {
        FbxNodeAttribute* lAttr = ReadNodeAttribute( pObjectSubType, pObjectName, pObjectUniqueId, pReferencedObject );
        if( lAttr )
        {
            ReadPropertiesAndFlags(lAttr, mFileObject);
            mObjectMap.Add(pObjectUniqueId.Buffer(),lAttr);
            pDocument->ConnectSrcObject(lAttr);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GEOMETRY)
    {
        FbxGeometry* lGeometry = NULL;

        if (pObjectSubType == FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_MESH)
        {
            FbxMesh* lMesh = CreateOrCloneReference<FbxMesh>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
            ReadMesh(*lMesh);
            lGeometry = lMesh;

        }
        if (pObjectSubType == FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_SUBDIV)
        {
            FbxSubDiv* lSubdiv = CreateOrCloneReference<FbxSubDiv>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
            ReadSubdiv(*lSubdiv);
            lGeometry = lSubdiv;

        }
        else if (pObjectSubType == FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_NURB)
        {
            FbxNurbs* lNurbs = CreateOrCloneReference<FbxNurbs>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
            ReadNurb(*lNurbs);
            lGeometry = lNurbs;
        }
        else if (pObjectSubType == FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_NURBS_SURFACE)
        {
            FbxNurbsSurface* lNurbsSurface = CreateOrCloneReference<FbxNurbsSurface>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
            ReadNurbsSurface(*lNurbsSurface);
            lGeometry = lNurbsSurface;
        }
        else if (pObjectSubType == FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_PATCH)
        {
            FbxPatch* lPatch = CreateOrCloneReference<FbxPatch>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
            ReadPatch(*lPatch);
            lGeometry = lPatch;
        }
        else if (pObjectSubType == FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_NURBS_CURVE)
        {
            FbxNurbsCurve* lNurbsCurve = CreateOrCloneReference<FbxNurbsCurve>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
            ReadNurbsCurve(*lNurbsCurve);
            lGeometry = lNurbsCurve;
        }
        else if (pObjectSubType == FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_TRIM_NURB_SURFACE)
        {
            FbxTrimNurbsSurface* lNurbs = CreateOrCloneReference<FbxTrimNurbsSurface>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
            ReadTrimNurbsSurface(*lNurbs);
            lGeometry = lNurbs;
        }
        else if (pObjectSubType == FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_BOUNDARY)
        {
            FbxBoundary* lBoundary = CreateOrCloneReference<FbxBoundary>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
            ReadBoundary( *lBoundary );
            lGeometry = lBoundary;
        }
        else
        {
            InstanciateUnknownObject = true;
            ObjectClassId = CheckRuntimeClass(FBX_TYPE(FbxGeometry), pObjectType, pObjectSubType, mManager);
        }

        if (lGeometry != NULL)
        {
            mObjectMap.Add(pObjectUniqueId.Buffer(),lGeometry);

            if (lIsAScene)
            {
                lScene->AddGeometry(lGeometry);
            }
            else
            {
                pDocument->ConnectSrcObject( lGeometry );
            }
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MATERIAL)
    {
        // Read a FbxSurfaceMaterial
        if (!mObjectMap.Find(pObjectUniqueId.Buffer())) {
            FbxSurfaceMaterial* lMaterial = ReadSurfaceMaterial(pObjectName, pObjectSubType, FbxCast<FbxSurfaceMaterial>(pReferencedObject));
            mObjectMap.Add(pObjectUniqueId.Buffer(),lMaterial);
            pDocument->ConnectSrcObject(lMaterial);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TEXTURE)
    {
        FbxFileTexture* lTexture = CreateOrCloneReference<FbxFileTexture>(mManager, pObjectName, pReferencedObject, mClassTemplateMap, ADSK_TYPE_TEXTURE);

        // Read a FbxFileTexture
        ReadFileTexture(*lTexture);

        mObjectMap.Add(pObjectUniqueId.Buffer(),lTexture);
        pDocument->ConnectSrcObject(lTexture);
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_THUMBNAIL)
    {
        FbxThumbnail* lThumbnail = CreateOrCloneReference<FbxThumbnail>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        // Read a FbxThumbnail
        ReadThumbnail(*lThumbnail);

        mObjectMap.Add(pObjectUniqueId.Buffer(),lThumbnail);
        pDocument->ConnectSrcObject(lThumbnail);
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_VIDEO)
    {
        FbxVideo* lVideo = CreateOrCloneReference<FbxVideo>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        // Read a FbxVideo
        ReadVideo(*lVideo);

        mObjectMap.Add(pObjectUniqueId.Buffer(),lVideo);
        pDocument->ConnectSrcObject(lVideo);
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_DEFORMER)
    {
        if (lIsAScene)
        {
            // Read a FbxDeformer
            if (pObjectSubType == "Skin")
            {
                FbxSkin* lSkin = CreateOrCloneReference<FbxSkin>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

                ReadSkin(*lSkin);
                pDocument->ConnectSrcObject(lSkin);
                mObjectMap.Add(pObjectUniqueId.Buffer(),lSkin);
            }
            else if (pObjectSubType == "Cluster")
            {
                FbxCluster* lCluster = CreateOrCloneReference<FbxCluster>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

                ReadCluster(*lCluster);
                pDocument->ConnectSrcObject(lCluster);
                mObjectMap.Add(pObjectUniqueId.Buffer(),lCluster);
            }
            else if (pObjectSubType == "VertexCacheDeformer")
            {
                FbxVertexCacheDeformer* lDeformer = CreateOrCloneReference<FbxVertexCacheDeformer>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

                ReadVertexCacheDeformer(*lDeformer);
                pDocument->ConnectSrcObject(lDeformer);
                mObjectMap.Add(pObjectUniqueId.Buffer(),lDeformer);
            }
            else
            {
                InstanciateUnknownObject = true;
                ObjectClassId = CheckRuntimeClass(FBX_TYPE(FbxDeformer), pObjectType, pObjectSubType, mManager);
            }
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_POSE)
    {
        if (lIsAScene)
        {
            // Read bind pose, rest pose and character pose
            if (pObjectSubType == "CharacterPose")
            {
                // CharacterPose are writen in v5 file format for the moment.               
                int lCharacterPoseIndex = lScene->CreateCharacterPose(pObjectName.Buffer());

                if (lCharacterPoseIndex != -1)
                {
                    FbxCharacterPose* lCharacterPose = lScene->GetCharacterPose(lCharacterPoseIndex);

                    if (!ReadCharacterPose(*lCharacterPose))
                    {
                        lScene->DestroyCharacterPose(lCharacterPoseIndex);
                    }
                }
            }
            else if (pObjectSubType == "BindPose" || pObjectSubType == "RestPose")
            {
                bool isBindPose = pObjectSubType == "BindPose";
                FbxPose* lPose = FbxPose::Create(&mManager, pObjectName.Buffer());
                lPose->SetIsBindPose(isBindPose);

                if (!ReadPose(*lScene, lPose, isBindPose))
                {
                    lPose->Destroy();
                }
                else
                {
                    lScene->AddPose(lPose);
                }
            }
        }
    }
    else if (lIsAScene && (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONTAINER))
    {
        // Read a AssetContainer
        FbxContainer* lContainer = FbxContainer::Create(&mManager, pObjectName.Buffer());

        ReadContainer(*lContainer);
        mObjectMap.Add(pObjectUniqueId.Buffer(),lContainer);
        pDocument->ConnectSrcObject(lContainer);
    }
    else if (lIsAScene && (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GENERIC_NODE))
    {
        // Read a FbxGenericNode
        FbxGenericNode* lNode = FbxGenericNode::Create(&mManager, pObjectName.Buffer());

        ReadGenericNode(*lNode);
        lScene->AddGenericNode(lNode);
        mObjectMap.Add(pObjectUniqueId.Buffer(),lNode);
    }
    else if (lIsAScene && (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONSTRAINT))
    {
        //Note: merge those two sections when the character will be added to the entity
        if( !strcmp(pObjectSubType.Buffer(), TOKEN_KFBXCONSTRAINT_CHARACTER) )
        {
            int lInputType;
            int lInputIndex;
            int lCharacterIndex = lScene->CreateCharacter(pObjectName.Buffer());

            if (lCharacterIndex != -1)
            {
                FbxCharacter* lCharacter = lScene->GetCharacter(lCharacterIndex);
                ReadCharacter(*lCharacter, lInputType, lInputIndex);
                pDocument->ConnectSrcObject(lCharacter);
                mObjectMap.Add(pObjectUniqueId.Buffer(),lCharacter);
            }
        }
        else
        {
            FbxConstraint* lConstraint = NULL;
            FbxClassId lClassId = mManager.FindFbxFileClass(pObjectType.Buffer(), pObjectSubType.Buffer());
            if( lClassId.IsValid() )
            {
                lConstraint = FbxCast<FbxConstraint>(lClassId.Create(mManager, pObjectName.Buffer(), NULL));
            }

            if( lConstraint )
            {
                if( ReadConstraint(*lConstraint) )
                {
                    pDocument->ConnectSrcObject(lConstraint);
                    mObjectMap.Add(pObjectUniqueId.Buffer(), lConstraint);
                }
                else
                {
                    lConstraint->Destroy();
                }
            }
            else
            {
                InstanciateUnknownObject = true;
                ObjectClassId = CheckRuntimeClass(FBX_TYPE(FbxConstraint), pObjectType, pObjectSubType, mManager);
            }
        }
    }
    //
    // Note: Actor
    //
    else if (lIsAScene && (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONTROLSET_PLUG))
    {
        if(pObjectSubType == "ControlSetPlug")
        {
            int lControlSetPlugIndex = lScene->CreateControlSetPlug(pObjectName.Buffer());

            if(lControlSetPlugIndex != -1)
            {
                FbxControlSetPlug *lPlug = lScene->GetControlSetPlug(lControlSetPlugIndex);

                mFileObject->FieldReadI(FIELD_KFBXGENERICNODE_VERSION,100);

                ReadPropertiesAndFlags(lPlug,mFileObject);

                mObjectMap.Add(pObjectUniqueId.Buffer(),lPlug);
            }
        }
    }
    else if (lIsAScene && (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CACHE))
    {
        FbxCache* lCache = CreateOrCloneReference<FbxCache>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        ReadCache( *lCache );
        mObjectMap.Add(pObjectUniqueId.Buffer(),lCache);
        pDocument->ConnectSrcObject(lCache);
    }
    else if(lIsAScene && (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GLOBAL_SETTINGS))
    {
        ReadGlobalSettings(lScene->GetGlobalSettings());
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_IMPLEMENTATION)
    {
        FbxImplementation* lNode = CreateOrCloneReference<FbxImplementation>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        ReadImplementation( *lNode );
        mObjectMap.Add(pObjectUniqueId.Buffer(),lNode);
        pDocument->ConnectSrcObject(lNode);
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_BINDINGTABLE)
    {
        FbxBindingTable* lNode = CreateOrCloneReference<FbxBindingTable>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        ReadBindingTable( *lNode );
        mObjectMap.Add(pObjectUniqueId.Buffer(),lNode);
        pDocument->ConnectSrcObject(lNode);
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_BINDINGOPERATOR)
    {
        FbxBindingOperator* lNode = CreateOrCloneReference<FbxBindingOperator>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        ReadBindingOperator( *lNode );
        mObjectMap.Add(pObjectUniqueId.Buffer(),lNode);
        pDocument->ConnectSrcObject(lNode);
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_SELECTIONNODE)
    {
        FbxSelectionNode* lNode = CreateOrCloneReference<FbxSelectionNode>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        ReadSelectionNode( *lNode );
        mObjectMap.Add(pObjectUniqueId.Buffer(),lNode);
        pDocument->ConnectSrcObject(lNode);
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_COLLECTION)
    {
        if(pObjectSubType == "SelectionSet")
        {
            FbxSelectionSet* lNode = CreateOrCloneReference<FbxSelectionSet>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

            ReadSelectionSet( *lNode );
            mObjectMap.Add(pObjectUniqueId.Buffer(),lNode);
            pDocument->ConnectSrcObject(lNode);
        }
        else
        {
            FbxCollection* lNode = CreateOrCloneReference<FbxCollection>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

            ReadCollection( *lNode );
            mObjectMap.Add(pObjectUniqueId.Buffer(),lNode);
            pDocument->ConnectSrcObject(lNode);        
        }
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_DOCUMENT)
    {
        FbxDocument* lNode = CreateOrCloneReference<FbxDocument>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        ReadDocument( *lNode );
        mObjectMap.Add(pObjectUniqueId.Buffer(),lNode);
        pDocument->ConnectSrcObject(lNode);
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_LAYERED_TEXTURE)
    {
        FbxLayeredTexture* lTex = CreateOrCloneReference<FbxLayeredTexture>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        ReadLayeredTexture( *lTex );

        mObjectMap.Add(pObjectUniqueId.Buffer(),lTex);
        pDocument->ConnectSrcObject(lTex);
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_OBJECTMETADATA)
    {
        FbxObjectMetaData* lMeta = CreateOrCloneReference<FbxObjectMetaData>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        ReadPropertiesAndFlags(lMeta,mFileObject);
        mObjectMap.Add(pObjectUniqueId.Buffer(),lMeta);
        pDocument->ConnectSrcObject(lMeta);
    }
	else if( pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_STACK )
	{
		FbxAnimStack* lAnimStack = FbxAnimStack::Create(pDocument, pObjectName);
        ReadPropertiesAndFlags(lAnimStack, mFileObject);
		mObjectMap.Add(pObjectUniqueId.Buffer(), lAnimStack);
	}
	else if( pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_LAYER )
	{
		FbxAnimLayer* lAnimLayer = FbxAnimLayer::Create(pDocument, pObjectName);
        ReadPropertiesAndFlags(lAnimLayer, mFileObject);
		mObjectMap.Add(pObjectUniqueId.Buffer(), lAnimLayer);
	}
    else
    {
        InstanciateUnknownObject = true;
        ObjectClassId = CheckRuntimeClass(FBX_TYPE(FbxObject), pObjectType, pObjectSubType, mManager);
    }

    if (InstanciateUnknownObject) {
        FbxObject* lFbxObject=0;
        // Register a new class type objects that are not known to the SDK
        // The object will be viewed as a generic FbxObject
        if(pReferencedObject == NULL)
        {
            lFbxObject = (FbxObject*)mManager.CreateNewObjectFromClassId( ObjectClassId,pObjectName.Buffer() );
            mClassTemplateMap.MergeWithTemplate(lFbxObject);
        }
        else
        {
            lFbxObject = (FbxObject*)CreateOrCloneReference(mManager,pObjectName,pReferencedObject, ObjectClassId, mClassTemplateMap);
        }

        // Create and register the object
        FBX_ASSERT_MSG( lFbxObject,"Could not create object" );
        if (lFbxObject) {
            ReadPropertiesAndFlags(lFbxObject,mFileObject);
            mObjectMap.Add(pObjectUniqueId.Buffer(),lFbxObject);
            pDocument->ConnectSrcObject(lFbxObject);
        }
    }

	// the mStatus may have been cleared while reading a valid section but a previous one may have been
	// detected as corrupted so let's return the error we had before processing the object
	if (!mStatus.Error() && lPrevStatus.Error())
		mStatus = lPrevStatus;

    return true;
}

//
// Create generic object
//
FbxObject* FbxReaderFbx6::CreateGenericObject(FbxDocument* pDocument, char* pObjectType, char* pObjectSubType, char* pObjectName, FbxObject::EObjectFlag pFlags)
{
    //Register a new class type objects that are not known to the SDK
    FbxClassId lObjectClassId = mManager.FindFbxFileClass(pObjectType, pObjectSubType);
    if( !lObjectClassId.IsValid() )
    {
        FbxString lClassName = pObjectType + FbxString("_") + pObjectSubType;
        lObjectClassId = mManager.RegisterRuntimeFbxClass( lClassName.Buffer(), FBX_TYPE(FbxObject), pObjectType, pObjectSubType );
    }

    //The object will be viewed as a generic FbxObject
    //Create and register the object
    FbxObject* lFbxObject = NULL;
    lFbxObject = (FbxObject*)mManager.CreateNewObjectFromClassId(lObjectClassId, pObjectName);

    FBX_ASSERT_MSG(lFbxObject, "Could not create object");
    if( lFbxObject )
    {
        mObjectMap.Add(pObjectName,lFbxObject);
        pDocument->ConnectSrcObject(lFbxObject);
        lFbxObject->SetObjectFlags(pFlags, true);
    }

    return (FbxObject*)lFbxObject;
}

//
// Read camera switcher
//
void FbxReaderFbx6::ReadCameraSwitcher(FbxScene& pScene)
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

//
// Read fbx camera switcher
//
bool FbxReaderFbx6::ReadCameraSwitcher( FbxCameraSwitcher& pCameraSwitcher )
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

//
// Reorder camera switcher indices in the scene
//
void FbxReaderFbx6::ReorderCameraSwitcherIndices(FbxScene& pScene)
{
    if( pScene.GlobalCameraSettings().GetCameraSwitcher() )
    {
        FbxNode* lCameraSwitcherNode = pScene.GlobalCameraSettings().GetCameraSwitcher()->GetNode();
        FbxCameraSwitcher* lCameraSwitcher = lCameraSwitcherNode->GetCameraSwitcher();
        FbxArray<FbxNode*> lNodeArray;
        FbxArray<int> lCameraIndexArray;

        int lCameraIndexCount = lCameraSwitcher->GetCameraNameCount();
        if( lCameraIndexCount == 0 )
        {
            // FiLMBOX may not save camera names associated with indices
            // if there is not animation data in the camera switcher.
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

        int lTakeNodeCount = pScene.GetMemberCount<FbxAnimStack>();
        for( int lTakeNodeIter = 0; lTakeNodeIter < lTakeNodeCount; ++lTakeNodeIter )
        {
            FbxAnimStack* lAnimStack = pScene.GetMember<FbxAnimStack>(lTakeNodeIter);
            if (lAnimStack)
            {
                for( int lAnimLayerIter = 0; lAnimLayerIter < lAnimStack->GetMemberCount<FbxAnimLayer>(); ++lAnimLayerIter )
                {
                    FbxAnimLayer* lAnimLayer = lAnimStack->GetMember<FbxAnimLayer>(lAnimLayerIter);
                    FBX_ASSERT(lAnimLayer != NULL);

                    FbxAnimCurveNode* lAnimCurveNode = lCameraSwitcher->CameraIndex.GetCurveNode(lAnimLayer);
                    if (lAnimCurveNode)
                    {
                        int lNewCameraIndex;
                        int lCameraIndex = lAnimCurveNode->GetChannelValue<int>(0U, 0);
                        if( lCameraIndex >= 1 && lCameraIndex <= lCameraIndexCount )
                        {
                            lNewCameraIndex = lCameraIndexArray[lCameraIndex-1];
                            if (lNewCameraIndex != -1)
							{
                                lAnimCurveNode->SetChannelValue<int>(0U, lNewCameraIndex);
                            }
                        }

                        FbxAnimCurve* lAnimCurve = lAnimCurveNode->GetCurve(0U);
                        if( lAnimCurve )
                        {
                            int lKeyCount = lAnimCurve->KeyGetCount();
                            for( int lKeyIter = 0; lKeyIter < lKeyCount; ++lKeyIter )
                            {
                                lCameraIndex = (int)lAnimCurve->KeyGetValue(lKeyIter);
                                if( lCameraIndex >= 1 && lCameraIndex <= lCameraIndexCount )
                                {
                                    lNewCameraIndex = lCameraIndexArray[lCameraIndex-1];
                                    if( lNewCameraIndex != -1 )
                                    {
                                        lAnimCurve->KeySetValue(lKeyIter, (float)lNewCameraIndex);
                                    }
                                }
                            }
                        }
                    } // if (AnimCurveNode)
                } // for AnimLayer
            } // if AnimStack
        } // for takeNodeCount
    } // if (GetCameraSwitcher)
}

//
// Read kfbx character
//
void FbxReaderFbx6::ReadCharacter(FbxCharacter& pCharacter, int& pInputType, int& pInputIndex)
{
    // Read the character properties
    ReadPropertiesAndFlags(&pCharacter,mFileObject);

    // Need to publish all the character property
    FbxProperty    lProp;
    bool            lValue;

    //Characterize
    lValue = mFileObject->FieldReadB("CHARACTERIZE");
    lProp = pCharacter.FindProperty("Characterize");
    if( lProp.IsValid() ) lProp.Set(lValue);

    //Lock XForm
    lValue = mFileObject->FieldReadB("LOCK_XFORM");
    lProp = pCharacter.FindProperty("LockXForm");
    if( lProp.IsValid() ) lProp.Set(lValue);

    //Lock Pick
    lValue = mFileObject->FieldReadB("LOCK_PICK");
    lProp = pCharacter.FindProperty("LockPick");
    if( lProp.IsValid() ) lProp.Set(lValue);

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
}

//
// Read kfbx character link group
//
void FbxReaderFbx6::ReadCharacterLinkGroup(FbxCharacter& pCharacter, int pCharacterGroupId)
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

//
// Read fbx character link
//
void FbxReaderFbx6::ReadCharacterLink(FbxCharacter& pCharacter, int pCharacterNodeId)
{
    FbxCharacterLink* lCharacterLink = pCharacter.GetCharacterLinkPtr((FbxCharacter::ENodeId)pCharacterNodeId);
    FBX_ASSERT(lCharacterLink != NULL);
    if (lCharacterLink == NULL)
        return;
    
    FbxString lTemplateName = mFileObject->FieldReadS("NAME");
    if( !lTemplateName.IsEmpty() )
    {
        lCharacterLink->mTemplateName = lTemplateName;
        if( lCharacterLink->mPropertyTemplateName.IsValid() ) lCharacterLink->mPropertyTemplateName.Set(lTemplateName);
    }

    lCharacterLink->mOffsetT[0] = mFileObject->FieldReadD("TOFFSETX", 0.0);
    lCharacterLink->mOffsetT[1] = mFileObject->FieldReadD("TOFFSETY", 0.0);
    lCharacterLink->mOffsetT[2] = mFileObject->FieldReadD("TOFFSETZ", 0.0);

    lCharacterLink->mOffsetR[0] = mFileObject->FieldReadD("ROFFSETX", 0.0);
    lCharacterLink->mOffsetR[1] = mFileObject->FieldReadD("ROFFSETY", 0.0);
    lCharacterLink->mOffsetR[2] = mFileObject->FieldReadD("ROFFSETZ", 0.0);

    lCharacterLink->mOffsetS[0] = mFileObject->FieldReadD("SOFFSETX", 1.0);
    lCharacterLink->mOffsetS[1] = mFileObject->FieldReadD("SOFFSETY", 1.0);
    lCharacterLink->mOffsetS[2] = mFileObject->FieldReadD("SOFFSETZ", 1.0);

    lCharacterLink->mParentROffset[0] = mFileObject->FieldReadD("PARENTROFFSETX", 0.0);
    lCharacterLink->mParentROffset[1] = mFileObject->FieldReadD("PARENTROFFSETY", 0.0);
    lCharacterLink->mParentROffset[2] = mFileObject->FieldReadD("PARENTROFFSETZ", 0.0);

    if( lCharacterLink->mPropertyOffsetT.IsValid() ) lCharacterLink->mPropertyOffsetT.Set(lCharacterLink->mOffsetT);
    if( lCharacterLink->mPropertyOffsetR.IsValid() ) lCharacterLink->mPropertyOffsetR.Set(lCharacterLink->mOffsetR);
    if( lCharacterLink->mPropertyOffsetS.IsValid() ) lCharacterLink->mPropertyOffsetS.Set(lCharacterLink->mOffsetS);
    if( lCharacterLink->mPropertyParentOffsetR.IsValid() ) lCharacterLink->mPropertyParentOffsetR.Set(lCharacterLink->mParentROffset);

    ReadCharacterLinkRotationSpace(*lCharacterLink);
}

//
// Read character link formation in rotation space
//
void FbxReaderFbx6::ReadCharacterLinkRotationSpace(FbxCharacterLink& pCharacterLink)
{
    if(mFileObject->FieldReadBegin("ROTATIONSPACE"))
    {
        pCharacterLink.mHasRotSpace = true;

        FbxDouble3 lLimits;
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
			pCharacterLink.mRLimits.SetMinActive(lActiveness[0], lActiveness[1], lActiveness[2]);

			lActiveness[0] = (mFileObject->FieldReadI("XMAXENABLE") != 0);
			lActiveness[1] = (mFileObject->FieldReadI("YMAXENABLE") != 0);
			lActiveness[2] = (mFileObject->FieldReadI("ZMAXENABLE") != 0);
			pCharacterLink.mRLimits.SetMaxActive(lActiveness[0], lActiveness[1], lActiveness[2]);

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

//
// Read kfbx character pose
//
bool FbxReaderFbx6::ReadCharacterPose(FbxCharacterPose& pCharacterPose)
{
    bool lResult = false;
    if( mFileObject->FieldReadBegin("PoseScene") )
    {
        if( mFileObject->FieldReadBlockBegin() )
        {
            FbxImporter* lImporter = FbxImporter::Create(&mManager, "");
			
			FbxIOSettings* lImpIOSettings = GetIOSettings(); // get current IOSettings of the reader instance
			lImporter->SetIOSettings( lImpIOSettings );

            // Store original values
            bool bModel     = lImpIOSettings->GetBoolProp(IMP_FBX_MODEL,              false);
            bool bMaterial  = lImpIOSettings->GetBoolProp(IMP_FBX_MATERIAL,           false);
            bool bTexture   = lImpIOSettings->GetBoolProp(IMP_FBX_TEXTURE,            false);
            bool bShape     = lImpIOSettings->GetBoolProp(IMP_FBX_SHAPE,              false);
            bool bGobo      = lImpIOSettings->GetBoolProp(IMP_FBX_GOBO,               false);
            bool bPivot     = lImpIOSettings->GetBoolProp(IMP_FBX_PIVOT,              false);
            bool bAnimation = lImpIOSettings->GetBoolProp(IMP_FBX_ANIMATION,          false);
            bool bSettings  = lImpIOSettings->GetBoolProp(IMP_FBX_GLOBAL_SETTINGS,    false);

			// Exclude unused stuff
            lImpIOSettings->SetBoolProp(IMP_FBX_MODEL,				false);
            lImpIOSettings->SetBoolProp(IMP_FBX_MATERIAL,			false);
            lImpIOSettings->SetBoolProp(IMP_FBX_TEXTURE,			false);
            lImpIOSettings->SetBoolProp(IMP_FBX_SHAPE,				false);
            lImpIOSettings->SetBoolProp(IMP_FBX_GOBO,				false);
            lImpIOSettings->SetBoolProp(IMP_FBX_PIVOT,				false);
            lImpIOSettings->SetBoolProp(IMP_FBX_ANIMATION,          false);
            lImpIOSettings->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS,    false);

            mFileObject->SetIsBeforeVersion6(true);
			lResult = lImporter->Import(pCharacterPose.GetPoseScene(), mFileObject);
            mFileObject->SetIsBeforeVersion6(false);

            // Keep original values
            lImpIOSettings->SetBoolProp(IMP_FBX_MODEL,              bModel     );
            lImpIOSettings->SetBoolProp(IMP_FBX_MATERIAL,           bMaterial  );
            lImpIOSettings->SetBoolProp(IMP_FBX_TEXTURE,            bTexture   );
            lImpIOSettings->SetBoolProp(IMP_FBX_SHAPE,              bShape     );
            lImpIOSettings->SetBoolProp(IMP_FBX_GOBO,               bGobo      );
            lImpIOSettings->SetBoolProp(IMP_FBX_PIVOT,              bPivot     );
            lImpIOSettings->SetBoolProp(IMP_FBX_ANIMATION,          bAnimation );
            lImpIOSettings->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS,    bSettings  );

            lImporter->Destroy();

            mFileObject->FieldReadBlockEnd();
        }
        mFileObject->FieldReadEnd();
    }

    return lResult;
}

//
// Read kfbx pose
//
bool FbxReaderFbx6::ReadPose(FbxScene& pScene, FbxPose* pPose, bool pAsBindPose)
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
                    FbxObject* lFbxObject = mObjectMap.Get(mObjectMap.Find(lNodeName));
                    FbxNode* node = NULL;
                    if (lFbxObject != NULL && lFbxObject->Is<FbxNode>())
                    {
                        node = (FbxNode*)lFbxObject;
                    }

                    int index = pPose->Add(node, m, local);
                    if (index == -1)
                    {
                        // an error occurred!
                    }
                }

                mFileObject->FieldReadBlockEnd();
            }
            mFileObject->FieldReadEnd();
        }
    }

    return lResult;
}

bool FbxReaderFbx6::ReadSelectionNode(FbxSelectionNode& pSelectionNode)
{
    mFileObject->FieldReadI(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_SELECTIONNODE, 100);    

    ReadPropertiesAndFlags(&pSelectionNode,mFileObject);

    bool lIsIn = false;
    FbxString lNodeName = mFileObject->FieldReadS("Node");
    lIsIn = mFileObject->FieldReadB("IsTheNodeInSet", false);

    pSelectionNode.mIsTheNodeInSet = lIsIn;

    if (mFileObject->FieldReadBegin (FIELD_KFBXSELECTIONSET_VERTICE_INDEXARRAY))
    {
        int lTotalCount = mFileObject->FieldReadGetCount();

        for (int lCount = 0; lCount < lTotalCount; lCount ++)
        {
            int lVertexIndex = mFileObject->FieldReadI();
            pSelectionNode.mVertexIndexArray.Add(lVertexIndex);
        }
        mFileObject->FieldReadEnd ();
    }
    if (mFileObject->FieldReadBegin (FIELD_KFBXSELECTIONSET_EDGE_INDEXARRAY))
    {
        int lTotalCount = mFileObject->FieldReadGetCount();

        for (int lCount = 0; lCount < lTotalCount; lCount ++)
        {
            int lVertexIndex = mFileObject->FieldReadI();
            pSelectionNode.mEdgeIndexArray.Add(lVertexIndex);
        }

        mFileObject->FieldReadEnd ();
    }
    if (mFileObject->FieldReadBegin (FIELD_KFBXSELECTIONSET_POLYGONVERTICES_INDEXARRAY))
    {
        int lTotalCount = mFileObject->FieldReadGetCount();

        for (int lCount = 0; lCount < lTotalCount; lCount ++)
        {
            int lVertexIndex = mFileObject->FieldReadI();
            pSelectionNode.mPolygonIndexArray.Add(lVertexIndex);
        }

        mFileObject->FieldReadEnd ();
    }

    return true;
}

bool FbxReaderFbx6::ReadSelectionSet(FbxSelectionSet& pSelectionSet)
{
    bool lResult = true;

    mFileObject->FieldReadI(FIELD_KFBXCOLLECTION_COLLECTION, 100);

    // Read the properties
    ReadPropertiesAndFlags(&pSelectionSet,mFileObject);

    int nbNodes = mFileObject->FieldReadI("NbSelectionNodes");

    return lResult;
}

//
// Read media from file
//
bool FbxReaderFbx6::ReadMedia(FbxDocument* pDocument, const char* pEmbeddedMediaDirectory /* = "" */)
{
    FbxScene*                  lScene = FbxCast<FbxScene>(pDocument);
    bool                        lIsAScene = (lScene != NULL);
    FbxArray<FbxString*>    lMediaNames;
    FbxArray<FbxString*>    lFileNames;
    int                         i, lFileTextureCount;

    // Set file names in textures.
	lFileTextureCount = pDocument->GetSrcObjectCount<FbxFileTexture>();

    for (i = 0; i < lFileTextureCount; i++)
    {
        FbxFileTexture* lFileTexture = pDocument->GetSrcObject<FbxFileTexture>(i);

        FbxString lCorrectedFileName(FbxPathUtils::Clean(lFileTexture->GetFileName()));

        lFileTexture->SetFileName(lCorrectedFileName.Buffer());
        if (FbxString(lFileTexture->GetRelativeFileName()) == "")
        {
            lFileTexture->SetRelativeFileName(mFileObject->GetRelativeFilePath(lCorrectedFileName.Buffer()));
        }

        lFileNames.Add(FbxNew< FbxString >(lCorrectedFileName));
        lMediaNames.Add(FbxNew< FbxString >(lFileTexture->GetMediaName()));


    }

    if( lIsAScene )
    {
        // Set file names in cameras.
		FbxCamera*					lCamera = NULL;
		FbxIteratorSrc<FbxCamera>	lCameraIter(lScene);
		FbxForEach(lCameraIter, lCamera)
		{
            //background
			const char* backMediaName = lCamera->GetBackgroundMediaName();
			if( backMediaName )
			{
				int lMediaIndex = FindString(backMediaName, lMediaNames);
				if( lMediaIndex != -1 )
				{
					lCamera->SetBackgroundFileName(lFileNames[lMediaIndex]->Buffer());
				}
			}
            //foreground
            const char* ForeMediaName = lCamera->GetForegroundMediaName();
            if( ForeMediaName )
            {
                int lMediaIndex = FindString(ForeMediaName, lMediaNames);
                if( lMediaIndex != -1 )
                {
                    lCamera->SetForegroundFileName(lFileNames[lMediaIndex]->Buffer());
                }
            }
		}

        // and while at it, do the same on the lights that have gobos
		FbxLight*					lLight = NULL;
		FbxIteratorSrc<FbxLight>	lLightIter(lScene);
		FbxForEach(lLightIter, lLight)
		{
			if(lLight->FileName.Get().IsEmpty())
				continue;

			FbxString lFileNameStr = lLight->FileName.Get();
			const char* fileName = lFileNameStr.Buffer();
			if( fileName )
			{
				int lMediaIndex = FindString(fileName, lMediaNames);
				if( lMediaIndex != -1 )
				{
					lLight->FileName.Set(lFileNames[lMediaIndex]->Buffer());
				}
			}
		}
    }

    // Delete local lists.
    FbxArrayDelete(lMediaNames);
    FbxArrayDelete(lFileNames);

    return true;
}

//
// Read fbx node
//
bool FbxReaderFbx6::ReadNode(FbxNode& pNode, FbxString& pObjectSubType, Fbx6TypeReadReferences& pDocReferences)
{
    int lNodeVersion = mFileObject->FieldReadI(FIELD_KFBXNODE_VERSION, 100);

    if(lNodeVersion < 232)
    {
        pNode.mCorrectInheritType = true;
    }

    ReadNodeShading(pNode);
    ReadNodeCullingType(pNode);

    ReadNodeTarget(pNode);

    bool lCreatedAttribute = false;
    ReadNodeAttribute(pNode, pObjectSubType, lCreatedAttribute, pDocReferences);

    // New properties
    // Those properties replace the old pivots and the old limits.
    // So if we find a file containing old pivots and/or old limits,
    // and properties, the properties will overwrite previous values
    ReadNodeProperties(pNode, lCreatedAttribute);

    // Note: to make sure that properties that should be loaded on the node attribute
    // but were loaded in the node (because the N flag is missing - this is true from
    // files generated by MotionBuilder) get correctly set 
    FbxNodeAttribute* na = pNode.GetNodeAttribute();
    if (na != NULL)
    {
        pNode.RootProperty.BeginCreateOrFindProperty();
        FbxProperty p = na->GetFirstProperty();   
        while (p.IsValid()) 
        {   
            FbxProperty s = pNode.FindProperty(p.GetName(), p.GetPropertyDataType());  
            if (s.IsValid())    
            {
                // It will fail, if they have different types.
                bool lRet = p.CopyValue(s);

                //s.Destroy();
            }   
            p = na->GetNextProperty(p);    
        }   
        pNode.RootProperty.EndCreateOrFindProperty();
    }

    //if it's binary 
    //extract precomp file of stereo camera
    if (mFileObject->IsBinary() && na != NULL && na->GetAttributeType() == FbxNodeAttribute::eCameraStereo)
    {
        FbxCameraStereo* lCameraStereo = (FbxCameraStereo*)na;
        if( lCameraStereo )
            ReadCameraStereoPrecomp(*lCameraStereo);
    }

    //Note: for camera system
    FbxCamera* lCamera = pNode.GetCamera();
    FbxString     lName = pNode.GetName();
    if( lCamera && (lName == FBXSDK_CAMERA_PERSPECTIVE ||
        lName == FBXSDK_CAMERA_TOP ||
        lName == FBXSDK_CAMERA_FRONT ||
        lName == FBXSDK_CAMERA_BACK ||
        lName == FBXSDK_CAMERA_RIGHT ||
        lName == FBXSDK_CAMERA_LEFT ||
        lName == FBXSDK_CAMERA_BOTTOM) )
    {

        FbxVector4 camPos(lCamera->Position.Get());
        if(  camPos != sgLastCameraPos )
        {
            lCamera->Position.Set(sgLastCameraPos);
            lCamera->UpVector.Set(sgLastCameraUpVec);
            lCamera->InterestPosition.Set(sgLastCameraLookAt);
        }
    }

    return true;
}

//
// Read properties and flags for fbx container
//
bool FbxReaderFbx6::ReadContainer(FbxContainer& pContainer)
{
    bool lStatus = true;
    mFileObject->FieldReadI(FIELD_KFBXCONTAINER_VERSION, 100);

    // Read the properties
    ReadPropertiesAndFlags(&pContainer,mFileObject);

    if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true))
    {
        FbxString lFileName, lTemplateFolder, lOriginalFileName; 
        int lTokenCount;
        if (mFileObject->FieldReadBegin(FIELD_MEDIA_CONTENT))    
        {
            //Extract the template file
            lFileName = pContainer.TemplateName.Get();   
            lTemplateFolder = mFileObject->GetContainerTemplateDirectory(lFileName.Buffer(),true);

            //Construct proper template file name. 
            //For example, if template name is "eakes.boy", then the file name should be "boy.template".
            lTokenCount = lFileName.GetTokenCount(".");
            lFileName = lFileName.GetToken(lTokenCount-1,".");
            lFileName += ".template";
            lOriginalFileName = lFileName;

            lStatus = mFileObject->FieldReadEmbeddedFile (lFileName, lFileName, lTemplateFolder);
            mFileObject->FieldReadEnd();

            if (lTemplateFolder.Buffer()[lTemplateFolder.GetLen()-1] != '\\' &&
                lTemplateFolder.Buffer()[lTemplateFolder.GetLen()-1] != '/')
            {
                lTemplateFolder += "/";
            }
            lOriginalFileName = lTemplateFolder + lOriginalFileName;
            pContainer.TemplatePath.Set(lOriginalFileName);

            //Parse the template file to extract the extend template files
            FbxArray<FbxString*> lExtendTemplateNames;
            pContainer.mContainerTemplate->ParseTemplateFile(lOriginalFileName.Buffer(), lExtendTemplateNames);
            int lCount = lExtendTemplateNames.GetCount();

            for(int i = 0; i<lCount;i++)
            {
                if (mFileObject->FieldReadBegin(FIELD_MEDIA_CONTENT))    {
                    lFileName = *(lExtendTemplateNames.GetAt(i));
                    lTemplateFolder = mFileObject->GetContainerTemplateDirectory(lFileName.Buffer(),true);

                    lTokenCount = lFileName.GetTokenCount(".");
                    lFileName = lFileName.GetToken(lTokenCount-1,".");
                    lFileName += ".template";

                    lStatus = mFileObject->FieldReadEmbeddedFile (lFileName, lFileName, lTemplateFolder); 
                    mFileObject->FieldReadEnd();
                }
            }
            FbxArrayDelete(lExtendTemplateNames);
        }
    }
    return lStatus;
}

//
// Read properties and flags for fbx generic node
//
bool FbxReaderFbx6::ReadGenericNode(FbxGenericNode& pNode)
{
    mFileObject->FieldReadI(FIELD_KFBXGENERICNODE_VERSION, 100);

    // Read the properties
    ReadPropertiesAndFlags(&pNode,mFileObject);

    return true;
}

//
// Read shading information of node
//
bool FbxReaderFbx6::ReadNodeShading(FbxNode& pNode)
{
    //
    // Retrieve The Hidden and Shading Informations...
    //
//    pNode.SetVisibility(true);

    if (mFileObject->FieldReadBegin (FIELD_KFBXNODE_HIDDEN))
    {
        FbxString lHidden = mFileObject->FieldReadC ();

//        if (stricmp(lHidden.Buffer(), "True") == 0)
//        {
//            pNode.SetVisibility (false);
//        }

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

//
// Read back-face culling type for node
//
bool FbxReaderFbx6::ReadNodeCullingType(FbxNode& pNode)
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

//
// Read target transform for node
//
bool FbxReaderFbx6::ReadNodeTarget( FbxNode& pNode )
{
    FbxVector4 lPostTargetRotation;
    mFileObject->FieldRead3D(FIELD_KFBXNODE_POST_TARGET_ROTATION, lPostTargetRotation.mData, lPostTargetRotation.mData);
    pNode.SetPostTargetRotation(lPostTargetRotation);

    FbxVector4 lTargetUpVector;
    mFileObject->FieldRead3D(FIELD_KFBXNODE_TARGET_UP_VECTOR, lTargetUpVector.mData, lTargetUpVector.mData);
    pNode.SetTargetUpVector(lTargetUpVector);

    return true;
}

//
// Read node attribute according to object subtype
//
bool FbxReaderFbx6::ReadNodeAttribute(FbxNode& pNode, FbxString& pObjectSubType, bool& pCreatedAttribute,
                                           Fbx6TypeReadReferences& pDocReferences)
{
    // We started to write out the node attribute's name so that
    // we can determine if we are writing out the same attribute multiple times.
    // In that case, we save some time by reading the attribute only once.
    //
    pCreatedAttribute = true;

    // Note the name includes the namespace
    FbxString lRefName    = mFileObject->FieldReadS( FIELD_NODE_ATTRIBUTE_REFTO );    // New field, may be empty.
    FbxString lAttribName = mFileObject->FieldReadS( FIELD_NODE_ATTRIBUTE_NAME );

    FbxObject* lRefObject = NULL;

    if( !lAttribName.IsEmpty() )
    {
        if( !lRefName.IsEmpty() )
        {
            FbxString     lRefObjStr;
            bool        lRefExternal;

            if (pDocReferences.GetReferenceResolution(lRefName.Buffer(), lRefObjStr, lRefExternal, lRefObject))
            {
                if (!lRefExternal)
                {
                    lRefObject = mObjectMap.Get( mObjectMap.Find(lRefName.Buffer()) );
                }
            }
        }
        else
        {
            FbxObject* lObj = mObjectMap.Get( mObjectMap.Find( lAttribName ) );

            if( lObj )
            {
                pCreatedAttribute = false;

                FbxNodeAttribute* lAttr = FbxCast<FbxNodeAttribute>(lObj);
                FBX_ASSERT( lAttr );

                if( lAttr )
                {
                    pNode.SetNodeAttribute( lAttr );
                    return true;
                }
                return false;
            }
        }
    }

    FbxString aname = FbxObject::StripPrefix(lAttribName);
    FbxNodeAttribute* lAttr = ReadNodeAttribute( pObjectSubType, aname, lAttribName, lRefObject);

    if( lAttr )
        pNode.SetNodeAttribute(lAttr);
    else
        pCreatedAttribute = false;

    return true;
}

//
// Read node attribute
//
FbxNodeAttribute* FbxReaderFbx6::ReadNodeAttribute( FbxString& pObjectSubType, FbxString& pObjectName, FbxString& pObjectUniqueId, FbxObject* pReferencedObject)
{

    FbxNodeAttribute* lAttr = NULL;

    if (strcmp (pObjectSubType.Buffer(), "LodGroup") == 0)
    {
        FbxLODGroup* lLodGroup = CreateOrCloneReference<FbxLODGroup>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lAttr = (lLodGroup );
    }
    else if (strcmp (pObjectSubType.Buffer(), "Null") == 0)
    {
        FbxNull* lNull = CreateOrCloneReference<FbxNull>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        ReadNull(*lNull);
        lAttr = (lNull );
    }
    else if (strcmp (pObjectSubType.Buffer(), "Marker") == 0)
    {
        FbxMarker *lMarker = CreateOrCloneReference<FbxMarker>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lMarker->SetType(FbxMarker::eStandard);
        ReadMarker(*lMarker);
        lAttr = (lMarker );
    }
    else if (strcmp (pObjectSubType.Buffer(), "OpticalMarker") == 0)
    {
        FbxMarker *lMarker = CreateOrCloneReference<FbxMarker>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lMarker->SetType(FbxMarker::eOptical);
        ReadMarker(*lMarker);
        lAttr = (lMarker );
    }
    else if (strcmp (pObjectSubType.Buffer(), "IKEffector") == 0)
    {
        FbxMarker *lMarker = CreateOrCloneReference<FbxMarker>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lMarker->SetType(FbxMarker::eEffectorIK);
        ReadMarker(*lMarker);
        lAttr = (lMarker );
    }
    else if (strcmp (pObjectSubType.Buffer(), "FKEffector") == 0)
    {
        FbxMarker *lMarker = CreateOrCloneReference<FbxMarker>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lMarker->SetType(FbxMarker::eEffectorFK);
        ReadMarker(*lMarker);
        lAttr = (lMarker );
    }
    else if (strcmp (pObjectSubType.Buffer(), "Root") == 0)
    {
        FbxSkeleton *lSkeletonRoot = CreateOrCloneReference<FbxSkeleton>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lSkeletonRoot->SetSkeletonType (FbxSkeleton::eRoot);

        // Root information as written by Motion Builder 4.0.
        if (mFileObject->FieldReadBegin(FIELD_PROPERTIES))
        {
            if (mFileObject->FieldReadBlockBegin())
            {
                if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYSKELETON_LIMB_NODE_SIZE))
                {
                    lSkeletonRoot->Size.Set(mFileObject->FieldReadD());
                    mFileObject->FieldReadEnd();
                }

                mFileObject->FieldReadBlockEnd();
            }
            mFileObject->FieldReadEnd();
        }
        lAttr = (lSkeletonRoot );
    }
    else if (strcmp (pObjectSubType.Buffer (), "Limb") == 0)
    {
        FbxSkeleton *lSkeletonLimb = CreateOrCloneReference<FbxSkeleton>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
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
        lAttr = (lSkeletonLimb );
    }
    else if (strcmp (pObjectSubType.Buffer(), "LimbNode") == 0)
    {
        FbxSkeleton *lSkeletonLimbNode = CreateOrCloneReference<FbxSkeleton>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lSkeletonLimbNode->SetSkeletonType(FbxSkeleton::eLimbNode);

        // Limb node information as written by Motion Builder 4.0.
        if (mFileObject->FieldReadBegin(FIELD_PROPERTIES))
        {
            if (mFileObject->FieldReadBlockBegin())
            {               

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
        lAttr = (lSkeletonLimbNode );
    }
    else if (strcmp (pObjectSubType.Buffer(), "Effector") == 0)
    {
        FbxSkeleton *lSkeletonEffector = CreateOrCloneReference<FbxSkeleton>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lSkeletonEffector->SetSkeletonType(FbxSkeleton::eEffector);
        lAttr = (lSkeletonEffector );
    }
    else if (strcmp (pObjectSubType.Buffer(), "Nurb") == 0)
    {
        FbxNurbs* lNurbs = CreateOrCloneReference<FbxNurbs>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        ReadNurb(*lNurbs);
        lAttr = (lNurbs );
    }
    else if (strcmp (pObjectSubType.Buffer(), "Patch") == 0)
    {
        FbxPatch *lPatch = CreateOrCloneReference<FbxPatch>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        ReadPatch(*lPatch);
        lAttr = (lPatch );
    }
    else if (strcmp (pObjectSubType.Buffer(), "Mesh") == 0)
    {
        FbxMesh *lMesh = CreateOrCloneReference<FbxMesh>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        ReadMesh(*lMesh);
        lAttr = (lMesh );
    }
    else if (strcmp (pObjectSubType.Buffer(), "Light") == 0)
    {
        FbxLight* lLight = CreateOrCloneReference<FbxLight>(mManager, pObjectName, pReferencedObject, mClassTemplateMap, ADSK_TYPE_LIGHT);
        ReadLight(*lLight);
        lAttr = (lLight );
    }
    else if (strcmp (pObjectSubType.Buffer(), "Camera") == 0)
    {
        FbxCamera* lCamera = CreateOrCloneReference<FbxCamera>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        ReadCamera(*lCamera);
        lAttr = (lCamera );
    }
    else if (strcmp (pObjectSubType.Buffer(), "CameraStereo") == 0)
    {
        FbxCameraStereo* lCameraStereo = CreateOrCloneReference<FbxCameraStereo>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        ReadCameraStereo(*lCameraStereo);
        lAttr = (lCameraStereo );
    }
    else if (strcmp (pObjectSubType.Buffer(), "CameraSwitcher") == 0)
    {
        // The camera switcher attribute read here only exist from fbx v6.0 files
        FbxCameraSwitcher* lSwitcher = CreateOrCloneReference<FbxCameraSwitcher>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        ReadCameraSwitcher(*lSwitcher);
        lAttr = (lSwitcher );
    }
    else if (strcmp (pObjectSubType.Buffer(), "Optical") == 0)
    {
        FbxOpticalReference *lOpticalReference = CreateOrCloneReference<FbxOpticalReference>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lAttr = (lOpticalReference );
    }
    else if (strcmp (pObjectSubType.Buffer(), "NurbsSurface") == 0 ||
        strcmp (pObjectSubType.Buffer(), "NurbsCurve") == 0 ||
        strcmp (pObjectSubType.Buffer(), "TrimNurbsSurface") == 0 )
    {
        // These new node attributes are not stored directly in the
        // Model:: object's properties, they are stored as geometry
        // objects and will be read and connected to this node, later in the read.
        return NULL;
    }
    else if (strcmp (pObjectSubType.Buffer(), "Subdiv") == 0)
    {
        return NULL;
    }
    else if (strcmp (pObjectSubType.Buffer(), "3D Curve") == 0)
    {
        return NULL;
    }
    else
    {
        FBX_ASSERT_NOW("Unknown subType");
        return NULL;
    }

    if( lAttr && pObjectUniqueId != "" )
    {
        mObjectMap.Add( pObjectUniqueId, lAttr );
    }

    return lAttr;
}

//
// Read node properties
//
bool FbxReaderFbx6::ReadNodeProperties(FbxNode& pNode, bool pReadNodeAttributeProperties)
{
    ReadPropertiesAndFlags(&pNode,mFileObject,pReadNodeAttributeProperties);
    pNode.UpdatePivotsAndLimitsFromProperties();

    return true;
}

//
// Read geometry layer elements
//
bool FbxReaderFbx6::ReadLayerElements(FbxGeometry& pGeometry)
{
    FbxArray<FbxLayerElement*> lElementsMaterial;
    ReadLayerElementsMaterial(&pGeometry, lElementsMaterial);

    FbxArray<FbxLayerElement*> lElementsNormal;
    ReadLayerElementsNormal(&pGeometry, lElementsNormal);

    FbxArray<FbxLayerElement*> lElementsBinormal;
    ReadLayerElementsBinormal(&pGeometry, lElementsBinormal);

    FbxArray<FbxLayerElement*> lElementsTangent;
    ReadLayerElementsTangent(&pGeometry, lElementsTangent);

    FbxArray<FbxLayerElement*> lElementsVertexColor;
    ReadLayerElementsVertexColor(&pGeometry, lElementsVertexColor);

    FbxArray<FbxLayerElement*> lElementsPolygonGroup;
    ReadLayerElementsPolygonGroup(&pGeometry, lElementsPolygonGroup);

    FbxArray<FbxLayerElement*> lElementsSmoothing;
    ReadLayerElementsSmoothing(&pGeometry, lElementsSmoothing);

    FbxArray<FbxLayerElement*> lElementsUserData;
    ReadLayerElementsUserData(&pGeometry, lElementsUserData);

    FbxArray<FbxLayerElement*> lElementsVisibility;
    ReadLayerElementsVisibility(&pGeometry, lElementsVisibility);

    FbxArray<FbxLayerElement*> lElementsEdgeCrease;
    ReadLayerElementEdgeCrease(&pGeometry, lElementsEdgeCrease);

    FbxArray<FbxLayerElement*> lElementsVertexCrease;
    ReadLayerElementVertexCrease(&pGeometry, lElementsVertexCrease);

    FbxArray<FbxLayerElement*> lElementsHole;
    ReadLayerElementHole(&pGeometry, lElementsHole);

    FbxArray<FbxLayerElement*> lElementsTextures[FbxLayerElement::sTypeTextureCount];

    FbxArray<FbxLayerElement*> lElementsTextureUVs[FbxLayerElement::sTypeTextureCount];

    int lLayerIndex;
    FBXSDK_FOR_EACH_TEXTURE(lLayerIndex)
    {
        ReadLayerElementsTexture(&pGeometry, lElementsTextures[lLayerIndex], FBXSDK_TEXTURE_TYPE(lLayerIndex));
        ReadLayerElementsChannelUV(&pGeometry, lElementsTextureUVs[lLayerIndex], FBXSDK_TEXTURE_TYPE(lLayerIndex));
    }

    while (mFileObject->FieldReadBegin(FIELD_KFBXLAYER))
    {
        lLayerIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            mFileObject->FieldReadI();

            while (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT))
            {
                if (mFileObject->FieldReadBlockBegin())
                {
                    if (!pGeometry.GetLayer(lLayerIndex))
                    {
                        int lCreatedLayer = pGeometry.CreateLayer();
                        FBX_ASSERT(lCreatedLayer == lLayerIndex);
                    }
                    FbxLayer *lLayer = pGeometry.GetLayer(lLayerIndex);

                    const char* lLayerElementType = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_TYPE);
                    int         lLayerElementIndex = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_TYPED_INDEX);

                    // skip if lLayerIndex is not consistent
                    if (lLayer && lLayerElementIndex >= 0)
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
                                // This is a fix to correctly retrieve the material. If we don't
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
                        else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_BINORMAL))
                        {
                            if (lElementsBinormal.GetCount() > 0 && lLayerElementIndex < lElementsBinormal.GetCount())
                                lLayer->SetBinormals(static_cast<FbxLayerElementBinormal*>(lElementsBinormal[lLayerElementIndex]));
                        }
                        else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_TANGENT))
                        {
                            if (lElementsTangent.GetCount() > 0 && lLayerElementIndex < lElementsTangent.GetCount())
                                lLayer->SetTangents(static_cast<FbxLayerElementTangent*>(lElementsTangent[lLayerElementIndex]));
                        }
                        else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_COLOR))
                        {
                            if (lElementsVertexColor.GetCount() > 0 && lLayerElementIndex < lElementsVertexColor.GetCount())
                                lLayer->SetVertexColors(static_cast<FbxLayerElementVertexColor*>(lElementsVertexColor[lLayerElementIndex]));
                        }
                        else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_POLYGON_GROUP))
                        {
                            if (lElementsPolygonGroup.GetCount() > 0 && lLayerElementIndex < lElementsPolygonGroup.GetCount())
                                lLayer->SetPolygonGroups(static_cast<FbxLayerElementPolygonGroup*>(lElementsPolygonGroup[lLayerElementIndex]));
                        }
                        else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_SMOOTHING))
                        {
                            if (lElementsSmoothing.GetCount() > 0 && lLayerElementIndex < lElementsSmoothing.GetCount())
                                lLayer->SetSmoothing(static_cast<FbxLayerElementSmoothing*>(lElementsSmoothing[lLayerElementIndex]));
                        }
                        else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_EDGE_CREASE))
                        {
                            if (lElementsEdgeCrease.GetCount() > 0 && lLayerElementIndex < lElementsEdgeCrease.GetCount())
                                lLayer->SetEdgeCrease(static_cast<FbxLayerElementCrease*>(lElementsEdgeCrease[lLayerElementIndex]));
                        }
                        else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_VERTEX_CREASE))
                        {
                            if (lElementsVertexCrease.GetCount() > 0 && lLayerElementIndex < lElementsVertexCrease.GetCount())
                                lLayer->SetVertexCrease(static_cast<FbxLayerElementCrease*>(lElementsVertexCrease[lLayerElementIndex]));
                        }
                        else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_HOLE))
                        {
                            if (lElementsHole.GetCount() > 0 && lLayerElementIndex < lElementsHole.GetCount())
                                lLayer->SetHole(static_cast<FbxLayerElementHole*>(lElementsHole[lLayerElementIndex]));
                        }
                        else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_USER_DATA))
                        {
                            if (lElementsUserData.GetCount() > 0 && lLayerElementIndex < lElementsUserData.GetCount())
                                lLayer->SetUserData(static_cast<FbxLayerElementUserData*>(lElementsUserData[lLayerElementIndex]));
                        }
                        else if (!strcmp(lLayerElementType, FIELD_KFBXLAYER_ELEMENT_VISIBILITY))
                        {
                            if (lElementsVisibility.GetCount() > 0 && lLayerElementIndex < lElementsVisibility.GetCount())
                                lLayer->SetVisibility(static_cast<FbxLayerElementVisibility*>(lElementsVisibility[lLayerElementIndex]));
                        }
                        else
                        {
                            int lLayerIndex1;
                            FBXSDK_FOR_EACH_TEXTURE(lLayerIndex1)
                            {
                                if (!strcmp(lLayerElementType, FbxLayerElement::sTextureNames[lLayerIndex1]))
                                {
                                    if (lElementsTextures[lLayerIndex1].GetCount() > 0 && lLayerElementIndex < lElementsTextures[lLayerIndex1].GetCount())
                                    {
                                        lLayer->SetTextures(FBXSDK_TEXTURE_TYPE(lLayerIndex1), static_cast<FbxLayerElementTexture*>(lElementsTextures[lLayerIndex1][lLayerElementIndex]));
                                    }
                                    else
                                    {
                                        // Should be here only when ALL_SAME and access is DIRECT.
                                        // This is a fix to correctly retrieve the texture. If we don't
                                        // do this, there will be no texture on the model).
                                        if (lLayerElementIndex == 0)
                                        {
                                            FbxLayerElementTexture* let = FbxLayerElementTexture::Create(&pGeometry, "");
                                            let->SetMappingMode(FbxLayerElement::eAllSame);
                                            let->SetReferenceMode(FbxLayerElement::eDirect);
                                            lLayer->SetTextures(FBXSDK_TEXTURE_TYPE(lLayerIndex1), let);
                                        }
                                    }
                                }
                                else if (!strcmp(lLayerElementType, FbxLayerElement::sTextureUVNames[lLayerIndex1]))
                                {
                                    if (lElementsTextureUVs[lLayerIndex1].GetCount() > 0 && lLayerElementIndex < lElementsTextureUVs[lLayerIndex1].GetCount())
                                    {
                                        lLayer->SetUVs(static_cast<FbxLayerElementUV*>(lElementsTextureUVs[lLayerIndex1][lLayerElementIndex]), FBXSDK_TEXTURE_TYPE(lLayerIndex1));
                                    }
                                }
                            }
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

//
// Read layer element material for geometry
//
bool FbxReaderFbx6::ReadLayerElementsMaterial(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsMaterial)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_MATERIAL))
    {
        FbxLayerElementMaterial* lLayerElementMaterial = FbxLayerElementMaterial::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
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

                lIndexArray.Resize(lMaterialCount);

                for(lMaterialCounter = 0 ; lMaterialCounter < lMaterialCount  ; lMaterialCounter ++)
                {
                    int lMaterialIndex = mFileObject->FieldReadI();
                    lIndexArray.SetAt(lMaterialCounter, lMaterialIndex);
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

//
// Read layer element normal for geometry
//
bool FbxReaderFbx6::ReadLayerElementsNormal(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsNormal)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_NORMAL))
    {
        FbxLayerElementNormal* lLayerElementNormal = FbxLayerElementNormal::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            if (lLayerElementVersion >= 101)
            {
                FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
                lLayerElementNormal->SetName(lLayerName.Buffer());
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementNormal->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementNormal->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));

            if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_NORMALS))
            {
                int lNormalCounter ;
                int lNormalCount  = mFileObject->FieldReadGetCount() / 3 ;
                FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementNormal->GetDirectArray();

                lDirectArray.Resize( lNormalCount );

                for(lNormalCounter = 0 ; lNormalCounter < lNormalCount  ; lNormalCounter ++)
                {
                    FbxVector4 lNormal ;
                    mFileObject->FieldRead3D(lNormal);
                    lDirectArray.SetAt(lNormalCounter, lNormal);
                }
                mFileObject->FieldReadEnd();
            }

            if (ConvertReferenceModeToken(lReferenceMode) != FbxLayerElement::eDirect)
            {
                if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_NORMALS_INDEX))
                {
                    int lIndexCounter ;
                    int lIndexCount = mFileObject->FieldReadGetCount();
                    FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementNormal->GetIndexArray();

                    lIndexArray.Resize( lIndexCount );

                    for (lIndexCounter = 0; lIndexCounter < lIndexCount ; lIndexCounter++)
                    {
                        int lCurrentIndex = mFileObject->FieldReadI();
                        lIndexArray.SetAt(lIndexCounter, lCurrentIndex);
                    }
                    mFileObject->FieldReadEnd();
                }
            }

            mFileObject->FieldReadBlockEnd();
        }

        mFileObject->FieldReadEnd();

        int lAddedIndex  = pElementsNormal.Add(lLayerElementNormal);

        FBX_ASSERT( lAddedIndex == lLayerElementIndex);
    }
    return true;
}

//
// Read layer element tangent for geometry
//
bool FbxReaderFbx6::ReadLayerElementsTangent(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsTangent)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_TANGENT))
    {
        FbxLayerElementTangent* lLayerElementTangent = FbxLayerElementTangent::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            if (lLayerElementVersion >= 101)
            {
                FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
                lLayerElementTangent->SetName(lLayerName.Buffer());
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementTangent->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementTangent->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));

            if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_TANGENTS))
            {
                int lTangentCounter ;
                int lTangentCount  = mFileObject->FieldReadGetCount() / 3 ;
                FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementTangent->GetDirectArray();

                lDirectArray.Resize( lTangentCount );

                for(lTangentCounter = 0 ; lTangentCounter < lTangentCount  ; lTangentCounter ++)
                {
                    FbxVector4 lTangent ;
                    mFileObject->FieldRead3D(lTangent);
                    lDirectArray.SetAt(lTangentCounter, lTangent);
                }
                mFileObject->FieldReadEnd();
            }

            if (ConvertReferenceModeToken(lReferenceMode) != FbxLayerElement::eDirect)
            {
                if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_TANGENTS_INDEX))
                {
                    int lIndexCounter ;
                    int lIndexCount = mFileObject->FieldReadGetCount();
                    FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementTangent->GetIndexArray();

                    lIndexArray.Resize( lIndexCount );

                    for (lIndexCounter = 0; lIndexCounter < lIndexCount ; lIndexCounter++)
                    {
                        int lCurrentIndex = mFileObject->FieldReadI();
                        lIndexArray.SetAt(lIndexCounter, lCurrentIndex);
                    }
                    mFileObject->FieldReadEnd();
                }
            }

            mFileObject->FieldReadBlockEnd();
        }

        mFileObject->FieldReadEnd();

        int lAddedIndex  = pElementsTangent.Add(lLayerElementTangent);

        FBX_ASSERT( lAddedIndex == lLayerElementIndex);
    }
    return true;
}

//
// Read layer element binormal for geometry
//
bool FbxReaderFbx6::ReadLayerElementsBinormal(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsBinormal)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_BINORMAL))
    {
        FbxLayerElementBinormal* lLayerElementBinormal = FbxLayerElementBinormal::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            if (lLayerElementVersion >= 101)
            {
                FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
                lLayerElementBinormal->SetName(lLayerName.Buffer());
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementBinormal->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementBinormal->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));

            if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_BINORMALS))
            {
                int lBinormalCounter ;
                int lBinormalCount  = mFileObject->FieldReadGetCount() / 3 ;
                FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementBinormal->GetDirectArray();

                lDirectArray.Resize( lBinormalCount );

                for(lBinormalCounter = 0 ; lBinormalCounter < lBinormalCount  ; lBinormalCounter ++)
                {
                    FbxVector4 lBinormal ;
                    mFileObject->FieldRead3D(lBinormal);
                    lDirectArray.SetAt(lBinormalCounter, lBinormal);
                }
                mFileObject->FieldReadEnd();
            }

            if (ConvertReferenceModeToken(lReferenceMode) != FbxLayerElement::eDirect)
            {
                if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_BINORMALS_INDEX))
                {
                    int lIndexCounter ;
                    int lIndexCount = mFileObject->FieldReadGetCount();
                    FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementBinormal->GetIndexArray();

                    lIndexArray.Resize( lIndexCount );

                    for (lIndexCounter = 0; lIndexCounter < lIndexCount ; lIndexCounter++)
                    {
                        int lCurrentIndex = mFileObject->FieldReadI();
                        lIndexArray.SetAt(lIndexCounter, lCurrentIndex);
                    }
                    mFileObject->FieldReadEnd();
                }
            }

            mFileObject->FieldReadBlockEnd();
        }

        mFileObject->FieldReadEnd();

        int lAddedIndex  = pElementsBinormal.Add(lLayerElementBinormal);

        FBX_ASSERT( lAddedIndex == lLayerElementIndex);
    }
    return true;
}


//
// Read layer element vertex color for geometry
//
bool FbxReaderFbx6::ReadLayerElementsVertexColor(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsVertexColor)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_COLOR))
    {
        FbxLayerElementVertexColor* lLayerElementVertexColor = FbxLayerElementVertexColor::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
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
                    lColor.mRed     = mFileObject->FieldReadD();
                    lColor.mGreen   = mFileObject->FieldReadD();
                    lColor.mBlue    = mFileObject->FieldReadD();
                    lColor.mAlpha   = mFileObject->FieldReadD();
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

//
// Read layer element texture for geometry
//
bool FbxReaderFbx6::ReadLayerElementsTexture( FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsTexture, FbxLayerElement::EType pTextureType )
{
    while(mFileObject->FieldReadBegin(FbxLayerElement::sTextureNames[FBXSDK_TEXTURE_INDEX(pTextureType)]))
    {
        FbxLayerElementTexture* lLayerElementTexture = FbxLayerElementTexture::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();
        bool lValid             = false;

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            if (lLayerElementVersion >= 101)
            {
                FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
                lLayerElementTexture->SetName(lLayerName.Buffer());
            }

            const char* lMappingMode    = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode  = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);
            const char* lBlendMode      = mFileObject->FieldReadC(FIELD_KFBXTEXTURE_BLEND_MODE);
            double   lAlpha             = mFileObject->FieldReadD(FIELD_KFBXTEXTURE_ALPHA);

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

//
// Read layer element UV channel for geometry
//
bool FbxReaderFbx6::ReadLayerElementsChannelUV( FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsUV, FbxLayerElement::EType pTextureType)
{

    while(mFileObject->FieldReadBegin(FbxLayerElement::sTextureUVNames[FBXSDK_TEXTURE_INDEX(pTextureType)]))
    {
        FbxLayerElementUV* lLayerElementUV = FbxLayerElementUV::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            if (lLayerElementVersion >= 101)
            {
                FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
                lLayerElementUV->SetName(lLayerName.Buffer());
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementUV->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementUV->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));

            if (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_UV))
            {
                int lUVCounter ;
                int lUVCount  = mFileObject->FieldReadGetCount() / 2 ;
                FbxLayerElementArrayTemplate<FbxVector2>& lDirectArray = lLayerElementUV->GetDirectArray();

                lDirectArray.Resize( lUVCount );

                for(lUVCounter = 0 ; lUVCounter < lUVCount  ; lUVCounter ++)
                {
                    FbxVector2 lUV;
                    lUV.mData[0] = mFileObject->FieldReadD();
                    lUV.mData[1] = mFileObject->FieldReadD();

                    lDirectArray.SetAt(lUVCounter, lUV);
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

                    lIndexArray.Resize( lIndexCount );

                    for (lIndexCounter = 0; lIndexCounter < lIndexCount ; lIndexCounter++)
                    {
                        int lCurrentIndex = mFileObject->FieldReadI();
                        lIndexArray.SetAt(lIndexCounter, lCurrentIndex);
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

//
// Read layer element polygongroup for geometry
//
bool FbxReaderFbx6::ReadLayerElementsPolygonGroup(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsPolygonGroup)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_POLYGON_GROUP))
    {
        FbxLayerElementPolygonGroup* lLayerElementPolygonGroup = FbxLayerElementPolygonGroup::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
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

                lIndexArray.Resize(lPolygonGroupCount);

                for(lPolygonGroupCounter = 0 ; lPolygonGroupCounter < lPolygonGroupCount  ; lPolygonGroupCounter ++)
                {
                    int lPolygonGroupIndex = mFileObject->FieldReadI();
                    lIndexArray.SetAt(lPolygonGroupCounter, lPolygonGroupIndex);
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

//
// Read layer element smoothing for geometry
//
bool FbxReaderFbx6::ReadLayerElementsSmoothing(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsSmoothing)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_SMOOTHING))
    {
        FbxLayerElementSmoothing* lLayerElementSmoothing = FbxLayerElementSmoothing::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            bool lIsIntArray=false;
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            if (lLayerElementVersion >= 101)
            {
                FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
                lLayerElementSmoothing->SetName(lLayerName.Buffer());
                if(lLayerElementVersion >= 102)
                {
                    lIsIntArray = true;
                }
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementSmoothing->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementSmoothing->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));
            FBX_ASSERT(lLayerElementSmoothing->GetReferenceMode() == FbxLayerElement::eDirect) ;

            // read the direct array data
            if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_SMOOTHING))
            {
                int lSmoothingCounter ;
                int lSmoothingCount  = mFileObject->FieldReadGetCount();
                FbxLayerElementArrayTemplate<int>& lDirectArray = lLayerElementSmoothing->GetDirectArray();

                lDirectArray.Resize( lSmoothingCount );

                for(lSmoothingCounter = 0 ; lSmoothingCounter < lSmoothingCount  ; lSmoothingCounter ++)
                {
                    int lSmoothingValue;
                    if(lIsIntArray)
                        lSmoothingValue = mFileObject->FieldReadI();
                    else
                        lSmoothingValue = (int) mFileObject->FieldReadB();
                    lDirectArray.SetAt(lSmoothingCounter, lSmoothingValue);
                }
                mFileObject->FieldReadEnd();
            }

            mFileObject->FieldReadBlockEnd();
        }

        mFileObject->FieldReadEnd();

        int lAddedIndex  = pElementsSmoothing.Add(lLayerElementSmoothing);

        FBX_ASSERT( lAddedIndex == lLayerElementIndex);
    }
    return true;
}

//
// Read layer element user data for geometry
//
bool FbxReaderFbx6::ReadLayerElementsUserData(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsUserData)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_USER_DATA))
    {
        int lLayerElementIndex = mFileObject->FieldReadI();
        FbxLayerElementUserData* lLayerElementUserData = NULL;

        if (mFileObject->FieldReadBlockBegin())
        {
            // read id
            const int lUserDataId = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYMESH_USER_DATA_ID);

            // read types and names
            FbxArray<FbxDataType> lDataTypes;
            FbxArray<const char*> lDataNames;
            bool lAllDataTypesOk = true;
            while(mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_USER_DATA_ARRAY))
            {
                if(mFileObject->FieldReadBlockBegin())
                {
                    const char* lName = mFileObject->FieldReadC(FIELD_KFBXGEOMETRYMESH_USER_DATA_NAME);
                    const int lNameLength = (int)strlen( lName ) + 1;
                    char* lTempName = FbxNewArray< char >( lNameLength );
                    FBXSDK_strncpy(lTempName, lNameLength, lName, lNameLength);
                    lDataNames.Add( lTempName );

                    const char* lTypeName = mFileObject->FieldReadC(FIELD_KFBXGEOMETRYMESH_USER_DATA_TYPE);
                    lDataTypes.Add( this->mManager.GetDataTypeFromName( lTypeName ) );

                    if( !( lDataTypes[ lDataTypes.GetCount() - 1 ].GetType() == eFbxBool ||
                        lDataTypes[ lDataTypes.GetCount() - 1 ].GetType() == eFbxInt ||
                        lDataTypes[ lDataTypes.GetCount() - 1 ].GetType() == eFbxFloat ||
                        lDataTypes[ lDataTypes.GetCount() - 1 ].GetType() == eFbxDouble ) )
                    {
                        lAllDataTypesOk = false;
                    }

                    mFileObject->FieldReadBlockEnd();
                }
                mFileObject->FieldReadEnd();
            }

            if( !lAllDataTypesOk )
            {
                for( int i = 0; i < lDataNames.GetCount(); ++i )
                {
                    FBX_SAFE_DELETE_ARRAY(lDataNames[i]);
                }

                continue;
            }

            lLayerElementUserData = FbxLayerElementUserData::Create(pGeometry, "", lUserDataId, lDataTypes, lDataNames );

            // read version, name
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            if (lLayerElementVersion >= 101)
            {
                FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
                lLayerElementUserData->SetName(lLayerName.Buffer());
            }

            // read mapping and ref modes
            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementUserData->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementUserData->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));


            // read the actual data
            int lDataIndex = 0;
            mFileObject->FieldReadResetPosition();
            while(mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_USER_DATA_ARRAY))
            {
                if(mFileObject->FieldReadBlockBegin())
                {
                    mFileObject->FieldReadBegin( FIELD_KFBXGEOMETRYMESH_USER_DATA );

                    const int lFieldCount = mFileObject->FieldReadGetCount();

                    lLayerElementUserData->ResizeAllDirectArrays( lFieldCount );

                    for( int i = 0; i < lFieldCount; ++i )
                    {
                        switch( lLayerElementUserData->GetDataType(lDataIndex).GetType() )
                        {
                        case eFbxBool:    FbxGetDirectArray<bool>(lLayerElementUserData,lDataIndex).SetAt(i, mFileObject->FieldReadB());  break;
                        case eFbxInt: FbxGetDirectArray<int>(lLayerElementUserData,lDataIndex).SetAt(i, mFileObject->FieldReadI());   break;
                        case eFbxFloat:   FbxGetDirectArray<float>(lLayerElementUserData,lDataIndex).SetAt(i, mFileObject->FieldReadF());  break;
                        case eFbxDouble:  FbxGetDirectArray<double>(lLayerElementUserData,lDataIndex).SetAt(i, mFileObject->FieldReadD()); break;
                        default: break;
                        }
                    }
                    mFileObject->FieldReadEnd();


                    mFileObject->FieldReadBlockEnd();
                }
                mFileObject->FieldReadEnd();

                lDataIndex++;
            }

            // read the indices
            if (lLayerElementUserData->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
            {
                if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_USER_DATA_INDEX))
                {
                    int lIndexCounter = 0;
                    int lIndexCount = mFileObject->FieldReadGetCount();
                    FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementUserData->GetIndexArray();

                    lIndexArray.Resize( lIndexCount );

                    for (lIndexCounter = 0; lIndexCounter < lIndexCount ; lIndexCounter++)
                    {
                        int lCurrentIndex = mFileObject->FieldReadI();
                        lIndexArray.SetAt(lIndexCounter, lCurrentIndex);
                    }
                    mFileObject->FieldReadEnd();
                }
            }

            // get rid of temp chars for data names
            for( int i = 0; i < lDataNames.GetCount(); ++i )
            {
                FBX_SAFE_DELETE_ARRAY(lDataNames[i]);
            }

            mFileObject->FieldReadBlockEnd();
        }

        mFileObject->FieldReadEnd();

        int lAddedIndex  = pElementsUserData.Add(lLayerElementUserData);
        FBX_ASSERT( lAddedIndex == lLayerElementIndex);
    }
    return true;
}

//
// Read layer element visibility for geometry
//
bool FbxReaderFbx6::ReadLayerElementsVisibility(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsVisibility)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_VISIBILITY))
    {
        FbxLayerElementVisibility* lLayerElementVisibility = FbxLayerElementVisibility::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            if (lLayerElementVersion >= 101)
            {
                FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
                lLayerElementVisibility->SetName(lLayerName.Buffer());
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementVisibility->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementVisibility->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));            

            // read the direct array data
            if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_VISIBILITY))
            {
                int lVisCounter ;
                int lVisCount  = mFileObject->FieldReadGetCount();
                FbxLayerElementArrayTemplate<bool>& lDirectArray = lLayerElementVisibility->GetDirectArray();

                lDirectArray.Resize( lVisCount );

                for(lVisCounter = 0 ; lVisCounter < lVisCount  ; lVisCounter ++)
                {
                    bool lVisValue = mFileObject->FieldReadB();
                    lDirectArray.SetAt(lVisCounter, lVisValue);
                }
                mFileObject->FieldReadEnd();
            }

            mFileObject->FieldReadBlockEnd();
        }

        mFileObject->FieldReadEnd();

        int lAddedIndex  = pElementsVisibility.Add(lLayerElementVisibility);

        FBX_ASSERT( lAddedIndex == lLayerElementIndex);
    }
    return true;
}

//
// Read layer element edge crease for geometry
//
bool FbxReaderFbx6::ReadLayerElementEdgeCrease(FbxGeometry*pGeometry, FbxArray<FbxLayerElement*>& pElementsEdgeCrease)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_EDGE_CREASE))
    {
        FbxLayerElementCrease* lLayerElementCrease = FbxLayerElementCrease::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
            lLayerElementCrease->SetName(lLayerName.Buffer());

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementCrease->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementCrease->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));
            FBX_ASSERT(lLayerElementCrease->GetReferenceMode() == FbxLayerElement::eDirect) ;

            // read the direct array data
            if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_EDGE_CREASE))
            {
                int lCreaseCounter ;
                int lCreaseCount  = mFileObject->FieldReadGetCount();
                FbxLayerElementArrayTemplate<double>& lDirectArray = lLayerElementCrease->GetDirectArray();

                lDirectArray.Resize( lCreaseCount );

                for(lCreaseCounter = 0 ; lCreaseCounter < lCreaseCount  ; lCreaseCounter ++)
                {
                    double lCreaseValue = mFileObject->FieldReadD();
                    lDirectArray.SetAt(lCreaseCounter, lCreaseValue);
                }
                mFileObject->FieldReadEnd();
            }

            mFileObject->FieldReadBlockEnd();
        }

        mFileObject->FieldReadEnd();

        int lAddedIndex  = pElementsEdgeCrease.Add(lLayerElementCrease);

        FBX_ASSERT( lAddedIndex == lLayerElementIndex);
    }
    return true;
}

//
// Read layer element vertex crease for geometry
//
bool FbxReaderFbx6::ReadLayerElementVertexCrease(FbxGeometry*pGeometry, FbxArray<FbxLayerElement*>& pElementsVertexCrease)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_VERTEX_CREASE))
    {
        FbxLayerElementCrease* lLayerElementCrease = FbxLayerElementCrease::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
            lLayerElementCrease->SetName(lLayerName.Buffer());

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementCrease->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementCrease->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));
            FBX_ASSERT(lLayerElementCrease->GetReferenceMode() == FbxLayerElement::eDirect) ;

            // read the direct array data
            if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_VERTEX_CREASE))
            {
                int lCreaseCounter ;
                int lCreaseCount  = mFileObject->FieldReadGetCount();
                FbxLayerElementArrayTemplate<double>& lDirectArray = lLayerElementCrease->GetDirectArray();

                lDirectArray.Resize( lCreaseCount );

                for(lCreaseCounter = 0 ; lCreaseCounter < lCreaseCount  ; lCreaseCounter ++)
                {
                    double lCreaseValue = mFileObject->FieldReadD();
                    lDirectArray.SetAt(lCreaseCounter, lCreaseValue);
                }
                mFileObject->FieldReadEnd();
            }

            mFileObject->FieldReadBlockEnd();
        }

        mFileObject->FieldReadEnd();

        int lAddedIndex  = pElementsVertexCrease.Add(lLayerElementCrease);

        FBX_ASSERT( lAddedIndex == lLayerElementIndex);
    }
    return true;
}


//
// Read layer element hole for geometry
//
bool FbxReaderFbx6::ReadLayerElementHole(FbxGeometry*pGeometry, FbxArray<FbxLayerElement*>& pElementsHole)
{
    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER_ELEMENT_HOLE))
    {
        FbxLayerElementHole* lLayerElementHole = FbxLayerElementHole::Create(pGeometry, "");
        int lLayerElementIndex = mFileObject->FieldReadI();

        if (mFileObject->FieldReadBlockBegin())
        {
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
            lLayerElementHole->SetName(lLayerName.Buffer());

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementHole->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementHole->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));
            FBX_ASSERT(lLayerElementHole->GetReferenceMode() == FbxLayerElement::eDirect) ;

            // read the direct array data
            if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_HOLE))
            {
                int lHoleCounter ;
                int lHoleCount  = mFileObject->FieldReadGetCount();
                FbxLayerElementArrayTemplate<bool>& lDirectArray = lLayerElementHole->GetDirectArray();

                lDirectArray.Resize( lHoleCount );

                for(lHoleCounter = 0 ; lHoleCounter < lHoleCount  ; lHoleCounter ++)
                {
                    bool lHoleFlag = mFileObject->FieldReadB();
                    lDirectArray.SetAt(lHoleCounter, lHoleFlag);
                }
                mFileObject->FieldReadEnd();
            }

            mFileObject->FieldReadBlockEnd();
        }

        mFileObject->FieldReadEnd();

        int lAddedIndex  = pElementsHole.Add(lLayerElementHole);

        FBX_ASSERT( lAddedIndex == lLayerElementIndex);
    }
    return true;
}

//
// Read layer element geometry links for geometry
//
bool FbxReaderFbx6::ReadGeometryLinks (FbxGeometry& pGeometry)
{
    if (IOS_REF.GetBoolProp(IMP_FBX_LINK, true))
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

//
// Read layer element geometry shapes for geometry
//
bool FbxReaderFbx6::ReadGeometryShapes(FbxGeometry& pGeometry)
{
    if (IOS_REF.GetBoolProp(IMP_FBX_SHAPE, true))
    {
		FbxString lBlendShapeName("");
		FbxBlendShape* lBlendShape = FbxBlendShape::Create(&mManager,"");
		if(!lBlendShape)
		{
			return false;
		}

		pGeometry.AddDeformer(lBlendShape);

		int lCounter = 0;
		while(mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRY_SHAPE))
		{
			FbxString lShapeName = FbxObject::StripPrefix(mFileObject->FieldReadC());
			FbxString lBlendShapeChannelName = lShapeName;

			int i = lShapeName.ReverseFind('.');
			size_t lLength = lShapeName.GetLen();

			if (i != -1)
			{
				lBlendShapeChannelName = lShapeName.Right(lLength-i-1);
			}

			//Extract blend shape deformer and channel name from shape name.
			//Only need to extract once, all shapes of one blend shape deformer should have the same prefix name.
			//For example, Dino:bodyBlendShape.legFixL and Dino:bodyBlendShape.legFixR.
			//Dino:bodyBlendShape is the same as blend shape deformer name.
			//legFixL and legFixR are the channels name.
			//This mostly happen in FBX files exported from Maya.
			if(lCounter == 0)
			{
				if (i != -1)
				{
					lBlendShapeName = lShapeName.Left(i);
				}
			}

			FbxShape* lShape = FbxShape::Create(&mManager, lShapeName);
			if(!lShape)
			{
				return false;
			}

			// Before monument, we only support one layer of shapes in parallel.
			// So put each shape on one separate blend shape channel to imitate this behavior.
			if (ReadShape(*lShape, pGeometry))
			{
				FbxBlendShapeChannel* lBlendShapeChannel = FbxBlendShapeChannel::Create(&mManager,lBlendShapeChannelName);
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

			lCounter++;
		}

		// Set proper name for blend shape deformer.
		lBlendShape->SetName(lBlendShapeName.Buffer());

		// If there is no channel is successfully created on this blend shape deformer 
		// Then it is empty, so destroy it.
		int lChannelCount = lBlendShape->GetBlendShapeChannelCount();
		if(lChannelCount == 0)
		{
			lBlendShape->Destroy();
		}
	}

	return true;
}


//
// Read fbx null node
//
bool FbxReaderFbx6::ReadNull(FbxNull& pNull)
{
    // Root information as written by Motion Builder 4.0.
    if (mFileObject->FieldReadBegin(FIELD_PROPERTIES))
    {
        if (mFileObject->FieldReadBlockBegin())
        {
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

//
// Read fbx marker
//
bool FbxReaderFbx6::ReadMarker(FbxMarker& pMarker)
{
    FbxDouble3 c = pMarker.Color.Get();
    FbxColor lColor(c[0], c[1], c[2]);

    // Marker information as written by Motion Builder 4.0.
    if (mFileObject->FieldReadBegin(FIELD_PROPERTIES))
    {
        if (mFileObject->FieldReadBlockBegin())
        {         

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

    c[0] = lColor.mRed;
    c[1] = lColor.mGreen;
    c[2] = lColor.mBlue;
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

//
// Read the camera node from file
//
bool FbxReaderFbx6::ReadCamera(FbxCamera& pCamera)
{
    double X, Y, Z;
    
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

    //Fix for system camera (backup position for check after property load)
    sgLastCameraPos = pCamera.Position.Get();
    sgLastCameraUpVec = pCamera.UpVector.Get();
    sgLastCameraLookAt = pCamera.InterestPosition.Get();

    // Camera View Options
    pCamera.ShowInfoOnMoving.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_SHOW_INFO_ON_MOVING, true));
    pCamera.ShowAudio.Set(mFileObject->FieldReadB(FIELD_KFBXGEOMETRYCAMERA_SHOW_AUDIO, false));

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

    return true;
}

//
// Read fbx stereo camera
//
bool FbxReaderFbx6::ReadCameraStereo( FbxCameraStereo& pCameraStereo )
{
    int lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_STEREO_VERSION);
/*
    //read and set Stereo Camera properties
    int lStereotype = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYCAMERA_STEREO_STEREO);
    switch(lStereotype){
        case 0:
            pCameraStereo.Stereo.Set(FbxCameraStereo::eNone);
            break;
        case 1:
            pCameraStereo.Stereo.Set(FbxCameraStereo::eConverged);
            break;
        case 2:
            pCameraStereo.Stereo.Set(FbxCameraStereo::eOffAxis);
            break;
        case 3:
            pCameraStereo.Stereo.Set(FbxCameraStereo::eParallel);
            break;
        default:
            pCameraStereo.Stereo.Set(FbxCameraStereo::eOffAxis);
            break;
    }
    pCameraStereo.InteraxialSeparation.Set(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_STEREO_INTERAXIAL_SEP));
    pCameraStereo.ZeroParallax.Set(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_STEREO_ZERO_PARALLAX));
    pCameraStereo.ToeInAdjust.Set(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_STEREO_TOE_IN_ADJUST));
    pCameraStereo.FilmOffsetRightCam.Set(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_STEREO_FILM_OFFSET_RIGHT_CAM));
    pCameraStereo.FilmOffsetLeftCam.Set(mFileObject->FieldReadD(FIELD_KFBXGEOMETRYCAMERA_STEREO_FILM_OFFSET_LEFT_CAM));

    FbxCamera* lLeftCamera = pCameraStereo.GetLeftCamera();
    if(lLeftCamera)
        ReadCamera(*lLeftCamera);
    FbxCamera* lRightCamera = pCameraStereo.GetRightCamera();
    if(lRightCamera)
        ReadCamera(*lRightCamera);
*/
    return true;
}

//
// Read fbx camera stereo precomp file
//
bool FbxReaderFbx6::ReadCameraStereoPrecomp(FbxCameraStereo& pCameraStereo)
{
    //Extract the precomp file
    FbxString lFileName = pCameraStereo.PrecompFileName.Get();
    FbxString lRelativeFileName = pCameraStereo.RelativePrecompFileName.Get();

    if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true) == false)
        return true;

    if(lFileName.GetLen() > 0 && lRelativeFileName.GetLen() > 0
        && mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYCAMERA_STEREO_PRECOMP_FILE_CONTENT))
    {
        FbxString lDefaultPath = "";
        FbxString lUserDefinePath = IOS_REF.GetStringProp(IMP_EXTRACT_FOLDER, lDefaultPath);
        const char* lUserDefinePathBuffer = (lUserDefinePath == lDefaultPath) ? NULL : lUserDefinePath.Buffer();
        FbxString lPrecompFolder = mFileObject->GetMediaDirectory(true, lUserDefinePathBuffer);
        bool lStatus = mFileObject->FieldReadEmbeddedFile (lFileName, lRelativeFileName, lPrecompFolder);
        mFileObject->FieldReadEnd();
    }
    return true;
}

//
// Read fbx light
//
bool FbxReaderFbx6::ReadLight(FbxLight& pLight)
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


//
// Read fbx mesh smoothness
//
bool FbxReaderFbx6::ReadMeshSmoothness(FbxMesh& pMesh)
{
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_SMOOTHNESS))
    {
        //read smoothness
        int lSmoothnessValue = 0;
        lSmoothnessValue = mFileObject->FieldReadI();
        pMesh.SetMeshSmoothness((FbxMesh::ESmoothness)lSmoothnessValue);
        mFileObject->FieldReadEnd ();

        //read preview division level
        if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_PREVIEW_DIVSION_LEVELS))
        {
            int lPreviewDivisionLevels = 0;
            lPreviewDivisionLevels = mFileObject->FieldReadI();
            pMesh.SetMeshPreviewDivisionLevels(lPreviewDivisionLevels);
            mFileObject->FieldReadEnd ();
        }

        //read render division level 
        if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_RENDER_DIVSION_LEVELS))
        {
            int lRenderDivisionLevels = 0;
            lRenderDivisionLevels = mFileObject->FieldReadI();
            pMesh.SetMeshRenderDivisionLevels(lRenderDivisionLevels);
            mFileObject->FieldReadEnd ();
        }

        //read display subdivisions
        if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_DISPLAY_SUBDIVISIONS))
        {
            bool lDisplaySubdivisions = false;
            lDisplaySubdivisions = mFileObject->FieldReadB();
            pMesh.SetDisplaySubdivisions(lDisplaySubdivisions);
            mFileObject->FieldReadEnd ();
        }

        //read BoundaryRule
        if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_BOUNDARY_RULE))
        {
            int lBoundaryRuleValue = 0;
            lBoundaryRuleValue = mFileObject->FieldReadI();
            pMesh.SetBoundaryRule((FbxMesh::EBoundaryRule)lBoundaryRuleValue);
            mFileObject->FieldReadEnd ();
        }

        //read preserve borders
        if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_PRESERVE_BORDERS))
        {
            bool lPreserveBorders = false;
            lPreserveBorders = mFileObject->FieldReadB();
            pMesh.SetPreserveBorders(lPreserveBorders);
            mFileObject->FieldReadEnd ();
        }

        //read preserve hardEdges
        if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_PRESERVE_HARD_EDGES))
        {
            bool lPreserveHardEdges = false;
            lPreserveHardEdges = mFileObject->FieldReadB();
            pMesh.SetPreserveHardEdges(lPreserveHardEdges);
            mFileObject->FieldReadEnd ();
        }

        //read propagate EdgeHardness
        if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_PROPAGATE_EDGE_HARDNESS))
        {
            bool lPropagateEdgeHardness = false;
            lPropagateEdgeHardness = mFileObject->FieldReadB();
            pMesh.SetPropagateEdgeHardness(lPropagateEdgeHardness);
            mFileObject->FieldReadEnd ();
        }

    }

    return true;
}

//
// Read fbx mesh vertices
//
bool FbxReaderFbx6::ReadMeshVertices(FbxMesh& pMesh)
{
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_VERTICES))
    {
        int lCount, lTotalCount = (mFileObject->FieldReadGetCount () / 3);

        pMesh.SetControlPointCount(lTotalCount);

        for (lCount = 0; lCount < lTotalCount; lCount ++)
        {
            FbxVector4& lVector = pMesh.GetControlPoints()[lCount];
            mFileObject->FieldRead3D (lVector.mData);
        }

        mFileObject->FieldReadEnd ();
    }

    return true;
}

//
// Read polygon index for mesh
//
bool FbxReaderFbx6::ReadMeshPolygonIndex(FbxMesh& pMesh)
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

//
// Read edge data for mesh
//
bool FbxReaderFbx6::ReadMeshEdges(FbxMesh& pMesh)
{
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_EDGES))
    {
        int lIndexCount = mFileObject->FieldReadGetCount ();

        if (lIndexCount > 0)
        {
            pMesh.SetMeshEdgeCount( lIndexCount );

            for( int i = 0; i < lIndexCount; ++i )
            {
                pMesh.SetMeshEdge(i, mFileObject->FieldReadI() );
            }
        }

        mFileObject->FieldReadEnd();
    }

    return true;
}

//
// Read fbx mesh
//
bool FbxReaderFbx6::ReadMesh (FbxMesh& pMesh)
{
    int    lVersion;

    lVersion = mFileObject->FieldReadI (FIELD_KFBXGEOMETRYMESH_GEOMETRY_VERSION, 0);

    ReadMeshSmoothness(pMesh);
    ReadMeshVertices(pMesh);
    ReadMeshPolygonIndex(pMesh);
    ReadMeshEdges(pMesh);

    ReadLayerElements(pMesh);
    ReadGeometryLinks(pMesh);
    ReadGeometryShapes(pMesh);

    return true;
}


//
// Read fbx subdivion
//
//bool FbxReaderFbx6::ReadSubdiv( FbxSubDiv& pSubdiv, FbxString& pObjectName, FbxObject* pReferencedObject)
bool FbxReaderFbx6::ReadSubdiv( FbxSubDiv& pSubdiv)
{
    int lVersion;

    // read in the version number
    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYSUBDIV_GEOMETRY_VERSION);

    //read the levelCount
    int lLevelCount = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYSUBDIV_LEVEL_COUNT);
    //set LevelCount
    pSubdiv.InitSubdivLevel(lLevelCount);

    //read currentLevel
    int lCurrentLevel = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYSUBDIV_CURRENT_LEVEL);
    //set currentLevel
    pSubdiv.SetCurrentLevel(lCurrentLevel);

	//read display smoothness
	int lSmoothness = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYSUBDIV_DISPLAY_SMOOTHNESS);
	//set display smoothness
	pSubdiv.SetDisplaySmoothness(FbxSubDiv::EDisplaySmoothness(lSmoothness));

    return true;
}

//
// Read fbx boundary
//
bool FbxReaderFbx6::ReadBoundary( FbxBoundary& pBoundary )
{
    int lVersion;

    // read in the version number
    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYBOUNDARY_VERSION);

    ReadPropertiesAndFlags(&pBoundary,mFileObject);

    return true;
}

//
// Read trim NurbsSurface
//
bool FbxReaderFbx6::ReadTrimNurbsSurface( FbxTrimNurbsSurface& pNurbs )
{
    int lVersion;
    bool lFlipNormals;
    bool lReturn = true;

    // read in the version number
    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYTRIM_NURBS_SURFACE_VERSION);

    // read in the trim properties
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYTRIM_NURBS_SURFACE_FLIP_NORMALS) )
    {
        lFlipNormals = mFileObject->FieldReadB ();
        mFileObject->FieldReadEnd ();

        pNurbs.SetFlipNormals( lFlipNormals );
    }
    else
    {
        lReturn = false;
    }

    ReadLayerElements(pNurbs);
  
    return lReturn;
}

//
// Read NurbsCurve
//
bool FbxReaderFbx6::ReadNurbsCurve( FbxNurbsCurve& pNurbs )
{
    int lVersion, lOrder, lDimension, lCount;
    bool lIsRational, lReturn = true;
    double* lKnotVector;
    const char* lLine;
    FbxNurbsCurve::EType lType = (FbxNurbsCurve::EType)-1;

    // version number
    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYNURBS_CURVE_VERSION);

    // order
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_CURVE_ORDER))
    {
        lOrder = mFileObject->FieldReadI ();
        mFileObject->FieldReadEnd ();

        pNurbs.SetOrder (lOrder);
    }

    // type/form
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_CURVE_FORM))
    {
        lLine = mFileObject->FieldReadC ();

        if(FBXSDK_stricmp(lLine, "Periodic") == 0)
        {
            lType = FbxNurbsCurve::ePeriodic;
        }
        else if(FBXSDK_stricmp(lLine, "Closed") == 0)
        {
            lType = FbxNurbsCurve::eClosed;
        }
        else if(FBXSDK_stricmp(lLine, "Open") == 0)
        {
            lType = FbxNurbsCurve::eOpen;
        }
        else
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Type of nurbs curve unknown (invalid data)");
            FBX_ASSERT_NOW ("Type of nurbs curve unknown (invalid data).");
            lReturn = false;
        }

        mFileObject->FieldReadEnd ();
    }

    // dimension
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_CURVE_DIMENSION))
    {
        lDimension = mFileObject->FieldReadI ();

        mFileObject->FieldReadEnd ();

        pNurbs.SetDimension( (FbxNurbsCurve::EDimension) lDimension );
    }

    // rational-ness
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_CURVE_RATIONAL) )
    {
        lIsRational = mFileObject->FieldReadB ();
        mFileObject->FieldReadEnd ();
        pNurbs.mIsRational=lIsRational;
    }

    // control points
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_CURVE_POINTS) && lReturn)
    {
        lCount = mFileObject->FieldReadGetCount ();

        if( lCount % 4 != 0 )
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Control point definition error (wrong number of data)");
            FBX_ASSERT_NOW("Control point definition error (wrong number of data).");
            lReturn = false;
        }
        else
        {
            lCount /= 4;
            pNurbs.InitControlPoints(lCount, lType);

            for (int i=0; i<lCount; i++)
            {
                FbxVector4& lVector = pNurbs.GetControlPoints()[i];

                lVector[0] = mFileObject->FieldReadD();
                lVector[1] = mFileObject->FieldReadD();
                lVector[2] = mFileObject->FieldReadD();
                lVector[3] = mFileObject->FieldReadD();;

                if (lVector[3] <= 0.00001)
                {
                    GetStatus().SetCode(FbxStatus::eFailure, "Weight must be greater than 0 (invalid data)");
                    FBX_ASSERT_NOW("Weight must be greater than 0 (invalid data).");
                    lReturn = false;
                }
            }
        }

        mFileObject->FieldReadEnd ();
    }

    // knots
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_CURVE_KNOTVECTOR) && lReturn)
    {
        lCount = mFileObject->FieldReadGetCount ();

        if( lCount != pNurbs.GetKnotCount() )
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Knot vector definition error (wrong number of data)");
            FBX_ASSERT_NOW("Knot vector definition error (wrong number of data).");
            lReturn = false;
        }
        else
        {
            lKnotVector = pNurbs.GetKnotVector();
            for (int i=0; i<lCount; i++)
            {
                *(lKnotVector+i) = mFileObject->FieldReadD();
            }
        }

        mFileObject->FieldReadEnd ();
    }

    ReadPropertiesAndFlags( &pNurbs, mFileObject, true );

    return lReturn;
}

//
// Read FbxNurbs
//
bool FbxReaderFbx6::ReadNurb(FbxNurbs& pNurbs)
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

    const char* Line;
    FbxNurbs::EType lTypeU = FbxNurbs::ePeriodic, lTypeV = FbxNurbs::ePeriodic;

    //
    // FORM
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
            //NURBS surface from MB, though marked as closed in FBX file, 
            //but MB will add two extra zero to the end of Knot vector list, caused decreasing knot vectors
            //to ensure no decreasing knot vectors, give up the two extra zero knot vectors
            //preserve it to eClosed (not interpret closed as periodic)
            lTypeU = FbxNurbs::eClosed;
        }
        else if(FBXSDK_stricmp(Line, "Open") == 0)
        {
            lTypeU = FbxNurbs::eOpen;
        }
        else
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Type of nurb unknown (invalid data)");
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
            //NURBS surface from MB, though marked as closed in FBX file, 
            //but MB will add two extra zero to the end of Knot vector list, caused decreasing knot vectors
            //to ensure no decreasing knot vectors, give up the two extra zero knot vectors
            //preserve it to eClosed (not interpret closed as periodic)
            lTypeV = FbxNurbs::eClosed;
        }
        else if(FBXSDK_stricmp(Line, "Open") == 0)
        {
            lTypeV = FbxNurbs::eOpen;
        }
        else
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Type of nurb unknown (invalid data)");
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
                    GetStatus().SetCode(FbxStatus::eFailure, "Weight must be greater than 0 (invalid data).");
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
        //NURBS surface from MB, though marked as closed in FBX file, 
        //but MB will add two extra zero to the end of Knot vector list, caused decreasing knot vectors
        //to ensure no decreasing knot vectors, give up the two extra zero knot vectors
        //preserve it to eClosed (not interpret closed as periodic)
        if (mFileObject->FieldReadGetCount () < TotalCount)
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
        //NURBS surface from MB, though marked as closed in FBX file, 
        //but MB will add two extra zero to the end of Knot vector list, caused decreasing knot vectors
        //to ensure no decreasing knot vectors, give up the two extra zero knot vectors
        //preserve it to eClosed (not interpret closed as periodic)
        if (mFileObject->FieldReadGetCount () < TotalCount)
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

    ReadLayerElements(pNurbs);
    ReadGeometryLinks(pNurbs);
    ReadGeometryShapes(pNurbs);

    return Return;
}

//
// Read NurbsSurface
//
bool FbxReaderFbx6::ReadNurbsSurface(FbxNurbsSurface& pNurbs)
{
    int            lVersion,U,V;
    bool            Return = true;
    int            Count;
    int            TotalCount;
    double*        Points;

    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYNURBS_SURFACE_NURB_VERSION);

    //
    // Type of the surface
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_SURFACE_NURB_ORDER))
    {
        U = mFileObject->FieldReadI ();
        V = mFileObject->FieldReadI ();
        mFileObject->FieldReadEnd();
        pNurbs.SetOrder (U, V);
    }

    const char* Line;
    FbxNurbsSurface::EType lTypeU = FbxNurbsSurface::ePeriodic, lTypeV = FbxNurbsSurface::ePeriodic;

    //
    // FORM...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_SURFACE_FORM))
    {
        Line = mFileObject->FieldReadC ();

        if(FBXSDK_stricmp(Line, "Periodic") == 0)
        {
            lTypeU = FbxNurbsSurface::ePeriodic;
        }
        else if(FBXSDK_stricmp(Line, "Closed") == 0)
        {
            lTypeU = FbxNurbsSurface::eClosed;
        }
        else if(FBXSDK_stricmp(Line, "Open") == 0)
        {
            lTypeU = FbxNurbsSurface::eOpen;
        }
        else
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Type of nurb unknown (invalid data)");
            FBX_ASSERT_NOW ("Type of nurb unknown (invalid data).");
            Return = false;
        }

        Line = mFileObject->FieldReadC ();

        if(FBXSDK_stricmp(Line, "Periodic") == 0)
        {
            lTypeV = FbxNurbsSurface::ePeriodic;
        }
        else if(FBXSDK_stricmp(Line, "Closed") == 0)
        {
            lTypeV = FbxNurbsSurface::eClosed;
        }
        else if(FBXSDK_stricmp(Line, "Open") == 0)
        {
            lTypeV = FbxNurbsSurface::eOpen;
        }
        else
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Type of nurb unknown (invalid data)");
            FBX_ASSERT_NOW("Type of nurb unknown (invalid data).");
            Return = false;
        }

        mFileObject->FieldReadEnd();
    }

    //
    // SURFACE DISPLAY...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_SURFACE_SURFACE_DISPLAY))
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
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_SURFACE_STEP))
    {
        U = mFileObject->FieldReadI ();
        V = mFileObject->FieldReadI ();
        mFileObject->FieldReadEnd ();
        pNurbs.SetStep (U,V);
    }

    //
    // Surface information
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_SURFACE_DIMENSION))
    {
        U = mFileObject->FieldReadI ();
        V = mFileObject->FieldReadI ();
        mFileObject->FieldReadEnd ();
        pNurbs.InitControlPoints (U, lTypeU, V, lTypeV);
    }

    //
    // Control points
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_SURFACE_POINTS))
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
    // U Knot Vector
    //
    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYNURBS_SURFACE_KNOTVECTOR_U))
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
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURBS_SURFACE_KNOTVECTOR_V))
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

    //
    // Flip normals flag
    //
    int lFlipNormals = mFileObject->FieldReadI( FIELD_KFBXGEOMETRYNURBS_SURFACE_FLIP_NORMALS );
    pNurbs.SetFlipNormals( lFlipNormals != 0 );

    ReadLayerElements(pNurbs);
    ReadGeometryLinks(pNurbs);
    ReadGeometryShapes(pNurbs);

    ReadPropertiesAndFlags( &pNurbs, mFileObject, true );

    return Return;
}

//
// Read Fbx path
//
bool FbxReaderFbx6::ReadPatch(FbxPatch& pPatch)
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

    ReadLayerElements(pPatch);
    ReadGeometryLinks(pPatch);
    ReadGeometryShapes(pPatch);

    return true;
}

//
// Read patch type
//
int FbxReaderFbx6::ReadPatchType(FbxPatch& pPatch)
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

//
// Read shape from geometry
//
bool FbxReaderFbx6::ReadShape(FbxShape& pShape, FbxGeometry& pGeometry)
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
                lIndices.Add(mFileObject->FieldReadI());
            }

            mFileObject->FieldReadEnd ();
        }

        //
        // Read the control points.
        //
        if ( mFileObject->FieldReadBegin ( FIELD_KFBXSHAPE_VERTICES ) )
        {           
            pShape.mControlPoints = pGeometry.mControlPoints;

            int lTotalCount = mFileObject->FieldReadGetCount () / 3;

            for (i = 0; i < lTotalCount; i ++)
            {
                FbxVector4& lVector = pShape.GetControlPoints()[lIndices[i]];

                lVector[0] += mFileObject->FieldReadD ();
                lVector[1] += mFileObject->FieldReadD ();
                lVector[2] += mFileObject->FieldReadD ();
            }

            mFileObject->FieldReadEnd ();
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
            if (!lLayerElementNormal)
            {
                lLayerElementNormal = FbxLayerElementNormal::Create(&pShape, "");
                lLayer->SetNormals(lLayerElementNormal);
            }

            lLayerElementNormal->SetMappingMode(FbxLayerElement::eByControlPoint);
            lLayerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);
            FbxLayerElementArrayTemplate<FbxVector4>& lNormals = lLayerElementNormal->GetDirectArray();

            FbxMesh* lMesh = (FbxMesh*) &pGeometry;
			if(lMesh && lMesh->GetLayer(0) && lMesh->GetLayer(0)->GetNormals())
			{
				lNormals = lMesh->GetLayer(0)->GetNormals()->GetDirectArray();
			}

            //
            // Read the NORMALS...
            //
            if (lLayerElementNormal->GetDirectArray().GetCount())
            {
                if ( mFileObject->FieldReadBegin ( FIELD_KFBXSHAPE_NORMALS ) )
                {
                    int lTotalCount = mFileObject->FieldReadGetCount () / 3;

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

//
// Read texture
//
bool FbxReaderFbx6::ReadFileTexture(FbxFileTexture& pTexture)
{
    //
    // Read the name
    //
    if ( mFileObject->FieldReadBegin( FIELD_KFBXTEXTURE_TEXTURE_NAME ) )
    {
        FbxString lString = FbxObject::StripPrefix(mFileObject->FieldReadC());
        pTexture.SetName(lString.Buffer());
        mFileObject->FieldReadEnd();
    }
    if ( mFileObject->FieldReadBegin( FIELD_KFBXTEXTURE_FILENAME ) )
    {
        pTexture.SetFileName(mFileObject->FieldReadC());
        mFileObject->FieldReadEnd();
    }
    if ( mFileObject->FieldReadBegin( FIELD_KFBXTEXTURE_RELATIVE_FILENAME ) )
    {
        pTexture.SetRelativeFileName(mFileObject->FieldReadC());
        mFileObject->FieldReadEnd();
    }

    if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true))
    {
        // Check if the "absolute" path of the texture exist
        // If the "absolute" path of the texture is not found BUT the "relative" path is found
        // replace the "absolute" path of the texture, then if we later write this scene in a file, the "absolute" path exist.
        // This can occur when a FBX file and "relative" texture are moved.
        if( FbxFileUtils::Exist( pTexture.GetFileName() ) == false)
        {
            FbxString lNewAbsolutePath = mFileObject->GetFullFilePath( pTexture.GetRelativeFileName() );
            lNewAbsolutePath = FbxPathUtils::Clean( lNewAbsolutePath );
            if( FbxFileUtils::Exist( lNewAbsolutePath ) )
            {
			    // Set with a valid "absolute" path...only if lNewAbsolutePath is not a folder
				const char* pFile = lNewAbsolutePath.Buffer();
				if (!FbxPathUtils::Exist( pFile ))
					pTexture.SetFileName( pFile );
			}
        }
    }
    
    //
    // Read the Media Name...
    //
    if ( mFileObject->FieldReadBegin( FIELD_KFBXTEXTURE_MEDIA ) )
    {
        FbxString lString = FbxObject::StripPrefix(mFileObject->FieldReadC());
        pTexture.SetMediaName(lString.Buffer());
        mFileObject->FieldReadEnd();
    }

    ReadPropertiesAndFlags(&pTexture,mFileObject);

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
    if ( mFileObject->FieldReadBegin ( FIELD_KFBXTEXTURE_ALPHA_SRC ) )
    {
        const char* lTexAlphaSource = mFileObject->FieldReadC();

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
        mFileObject->FieldReadEnd ();
    }

    //
    // Read CROPPING...
    //
    if ( mFileObject->FieldReadBegin ( FIELD_KFBXTEXTURE_CROPPING ) )
    {
        pTexture.SetCropping(mFileObject->FieldReadI (), mFileObject->FieldReadI (), mFileObject->FieldReadI (), mFileObject->FieldReadI ());
        mFileObject->FieldReadEnd ();
    }

    return true;
}

//
// Read thumbnail
//
bool FbxReaderFbx6::ReadThumbnail(FbxThumbnail& pThumbnail)
{
    bool lImageRead = false;

    if (mFileObject->FieldReadBegin(FIELD_THUMBNAIL_VERSION))
    {
        mFileObject->FieldReadI();
        mFileObject->FieldReadEnd();
    }

    if (mFileObject->FieldReadBegin(FIELD_THUMBNAIL_FORMAT))
    {
        pThumbnail.SetDataFormat((FbxThumbnail::EDataFormat)mFileObject->FieldReadI());
        mFileObject->FieldReadEnd();
    }

    if (mFileObject->FieldReadBegin(FIELD_THUMBNAIL_SIZE))
    {
        pThumbnail.SetSize((FbxThumbnail::EImageSize)mFileObject->FieldReadI());
        mFileObject->FieldReadEnd();
    }

    // For the moment, do nothing with the encoding; assume it's a raw image
    int lEncoding = 0;
    if (mFileObject->FieldReadBegin(FIELD_THUMBNAIL_ENCODING))
    {
        mFileObject->FieldReadI();
        mFileObject->FieldReadEnd();
    }

    if (lEncoding == 0)
    {
        if (pThumbnail.GetSize() != FbxThumbnail::eNotSet)
        {
            if (mFileObject->FieldReadBegin(FIELD_THUMBNAIL_IMAGE))
            {
                FbxUChar* lImagePtr   = pThumbnail.GetThumbnailImage();
                unsigned long lSize = pThumbnail.GetSizeInBytes();
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

    lImageRead &= ReadPropertiesAndFlags(&pThumbnail, mFileObject);

    return lImageRead;
}

//
// Read surface material
//
FbxSurfaceMaterial* FbxReaderFbx6::ReadSurfaceMaterial(
    const char*             pObjectName,
    const char*             pMaterialType,
    FbxSurfaceMaterial*    pReferencedMaterial
    )
{
    FbxSurfaceMaterial* lMaterial = NULL;

    int lVersion = mFileObject->FieldReadI(FIELD_KFBXMATERIAL_VERSION, 100);

    if (FBXSDK_stricmp(pMaterialType, "") == 0)
    {
        pMaterialType = mFileObject->FieldReadC(FIELD_KFBXMATERIAL_SHADING_MODEL, "Phong");
    }

    if (pReferencedMaterial)
    {
        lMaterial = FbxCast<FbxSurfaceMaterial>(pReferencedMaterial->Clone(FbxObject::eReferenceClone));
        lMaterial->SetName(pObjectName);
    }
    else
    {
        FbxClassId lClassId = mManager.FindClass(ADSK_TYPE_SURFACEMATERIAL);
        if( lClassId.IsValid())
        {
            lMaterial = FbxCast<FbxSurfaceMaterial>( mManager.CreateNewObjectFromClassId(lClassId, pObjectName) );        
        }
        else
        {
            //give blinn same options as phong to be able to have textures...
            if (FBXSDK_stricmp(pMaterialType, "Phong") == 0 || FBXSDK_stricmp(pMaterialType, "Blinn") == 0)
            {
                lMaterial = FbxSurfacePhong::Create(&mManager, pObjectName);
            }
            else if (FBXSDK_stricmp(pMaterialType, "Lambert") == 0)
            {
                lMaterial = FbxSurfaceLambert::Create(&mManager, pObjectName);
            }
            else
            {
                lMaterial = FbxSurfaceMaterial::Create(&mManager, pObjectName);
                lMaterial->ShadingModel.Set(FbxString(pMaterialType));
            }
        }        
    }

    lMaterial->MultiLayer.Set(mFileObject->FieldReadI(FIELD_KFBXMATERIAL_MULTI_LAYER) != 0);

    ReadPropertiesAndFlags(lMaterial,mFileObject);

    if (lVersion < 102)
    {
        /*
        We need to convert old properties into the new material
        properties. If version >= 102, the file already contains the
        new properties, and we assume their value is properly set.
        */
        if( lMaterial->Is<FbxSurfaceLambert>() )	//Here we can test only lambert, because phong inherit from lambert
        {
            FbxProperty lProp = lMaterial->FindProperty("Emissive");
            if (lProp.IsValid())
            {
                FbxDouble3 lColor = lProp.Get<FbxDouble3>();
                FbxSurfaceLambert* lLambert = static_cast<FbxSurfaceLambert*>(lMaterial);
                lLambert->Emissive.Set(lColor);
                lLambert->EmissiveFactor.Set(1.);
            }

            lProp = lMaterial->FindProperty("Ambient");
            if (lProp.IsValid())
            {
                FbxDouble3 lColor = lProp.Get<FbxDouble3>();
                FbxSurfaceLambert* lLambert = static_cast<FbxSurfaceLambert*>(lMaterial);
                lLambert->Ambient.Set(lColor);
                lLambert->AmbientFactor.Set(1.);
            }

            lProp = lMaterial->FindProperty("Diffuse");
            if (lProp.IsValid())
            {
                FbxDouble3 lColor = lProp.Get<FbxDouble3>();
                FbxSurfaceLambert* lLambert = static_cast<FbxSurfaceLambert*>(lMaterial);
                lLambert->Diffuse.Set(lColor);
                lLambert->DiffuseFactor.Set(1.);
            }

            lProp = lMaterial->FindProperty("Opacity");
            if (lProp.IsValid())
            {
                double lOpacity = lProp.Get<FbxDouble>();
                FbxSurfaceLambert* lLambert = static_cast<FbxSurfaceLambert*>(lMaterial);
                lLambert->TransparencyFactor.Set(1. - lOpacity);
            }

            if( lMaterial->Is<FbxSurfacePhong>() )
            {
                lProp = lMaterial->FindProperty("Specular");
                if (lProp.IsValid())
                {
                    FbxDouble3 lColor = lProp.Get<FbxDouble3>();
                    FbxSurfacePhong* lPhong = static_cast<FbxSurfacePhong*>(lMaterial);
                    lPhong->Specular.Set(lColor);
                    lPhong->SpecularFactor.Set(1.);
                }

                lProp = lMaterial->FindProperty("Shininess");
                if (lProp.IsValid())
                {
                    double lFactor = lProp.Get<FbxDouble>();
                    FbxSurfacePhong* lPhong = static_cast<FbxSurfacePhong*>(lMaterial);
                    lPhong->Shininess.Set(lFactor);
                }

                lProp = lMaterial->FindProperty("Reflectivity");
                if (lProp.IsValid())
                {
                    double lReflectivity = lProp.Get<FbxDouble>();
                    FbxSurfacePhong* lPhong = static_cast<FbxSurfacePhong*>(lMaterial);
                    lPhong->ReflectionFactor.Set(lReflectivity);
                }
            }
        }
    }

    return lMaterial;
}

//
// Read LayeredTexture
//
bool FbxReaderFbx6::ReadLayeredTexture( FbxLayeredTexture& pTex )
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXLAYEREDTEXTURE_VERSION, 100);

    mFileObject->FieldReadBegin(FIELD_KFBXLAYEREDTEXTURE_BLENDMODES);

    int lCount = mFileObject->FieldReadGetCount();
    pTex.mInputData.Resize( lCount );

    for( int i = 0; i < lCount; ++i )
    {
        int lBlendMode = mFileObject->FieldReadI();
        if( lBlendMode < FbxLayeredTexture::eTranslucent ||
            lBlendMode >= FbxLayeredTexture::eBlendModeCount )
        {
            lBlendMode = FbxLayeredTexture::eNormal;
        }

        pTex.mInputData[i].mBlendMode = static_cast<FbxLayeredTexture::EBlendMode>(lBlendMode);
        pTex.mInputData[i].mAlpha = 1.0;  // default
    }

    mFileObject->FieldReadEnd();

    return ReadPropertiesAndFlags(&pTex,mFileObject);
}

//
// Read FbxImplementation
//
bool FbxReaderFbx6::ReadImplementation( FbxImplementation& pImplementation )
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXIMPLEMENTATION_VERSION, 100);

    return ReadPropertiesAndFlags(&pImplementation,mFileObject);
}

//
// Read collection
//
bool FbxReaderFbx6::ReadCollection( FbxCollection& pCollection )
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXIMPLEMENTATION_VERSION, 100);

    return ReadPropertiesAndFlags(&pCollection, mFileObject);
}

//
// Read document
//
bool FbxReaderFbx6::ReadDocument(FbxDocument& pSubDocument)
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXDOCUMENT_VERSION, 100);

    return ReadPropertiesAndFlags(&pSubDocument, mFileObject);
}

//
// Read binding operator
//
bool FbxReaderFbx6::ReadBindingOperator( FbxBindingOperator& pOperator )
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXBINDINGOPERATOR_VERSION, 100);

    bool lSuccess = ReadPropertiesAndFlags(&pOperator,mFileObject);

    const int lEntryCount = mFileObject->FieldGetInstanceCount(FIELD_KFBXBINDINGOPERATOR_ENTRY);

    // Create the operator entries
    int i = 0;
    for( i = 0; i < lEntryCount; ++i )
    {
        mFileObject->FieldReadBegin( FIELD_KFBXBINDINGOPERATOR_ENTRY );

        FbxBindingTableEntry& lEntry = pOperator.AddNewEntry();

        lEntry.SetSource( mFileObject->FieldReadC() );
        lEntry.SetEntryType( mFileObject->FieldReadC(), true );
        lEntry.SetDestination( mFileObject->FieldReadC() );
        lEntry.SetEntryType( mFileObject->FieldReadC(), false );

        mFileObject->FieldReadEnd();
    }

    return lSuccess;
}

//
// Read binding table
//
bool FbxReaderFbx6::ReadBindingTable( FbxBindingTable& pBindingTable )
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXBINDINGTABLE_VERSION, 100);

    bool lSuccess = ReadPropertiesAndFlags(&pBindingTable,mFileObject);

    const int lEntryCount = mFileObject->FieldGetInstanceCount(FIELD_KFBXBINDINGTABLE_ENTRY);

    // Create the binding table entries
    int i = 0;
    for( i = 0; i < lEntryCount; ++i )
    {
        mFileObject->FieldReadBegin( FIELD_KFBXBINDINGTABLE_ENTRY );

        FbxBindingTableEntry& lEntry = pBindingTable.AddNewEntry();

        lEntry.SetSource( mFileObject->FieldReadC() );
        lEntry.SetEntryType( mFileObject->FieldReadC(), true );
        lEntry.SetDestination( mFileObject->FieldReadC() );
        lEntry.SetEntryType( mFileObject->FieldReadC(), false );

        mFileObject->FieldReadEnd();
    }


    // read embedded shader files.
    if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true))
    {
        FbxString lAbsFileNames = "";
        FbxString lRelFileNames = "";
        bool lFirstIteration = true;

        while( mFileObject->FieldReadBegin(FIELD_EMBEDDED_FILE) )
        {
            if( mFileObject->FieldReadBlockBegin() )
            {
                FbxString lFileName = mFileObject->FieldReadC(FIELD_MEDIA_FILENAME, "" );
                FbxString lRelativeFileName = mFileObject->FieldReadC( FIELD_MEDIA_RELATIVE_FILENAME, "" );

                if( mFileObject->FieldReadBegin(FIELD_MEDIA_CONTENT) )
                {
                    FbxString lDefaultPath = "";
                    FbxString lUserDefinePath = IOS_REF.GetStringProp(IMP_EXTRACT_FOLDER, lDefaultPath);
                    const char* lUserDefinePathBuffer = (lUserDefinePath == lDefaultPath) ? NULL : lUserDefinePath.Buffer();
                    bool lStatus = mFileObject->FieldReadEmbeddedFile (lFileName, lRelativeFileName, mFileObject->GetMediaDirectory(true, lUserDefinePathBuffer));

                    mFileObject->FieldReadEnd();

                    if( lStatus )
                    {
                        if( lFirstIteration )
                        {
                            lAbsFileNames += lFileName;
                            lRelFileNames += lRelativeFileName;
                            lFirstIteration = false;
                        }
                        else 
                        {
                            lAbsFileNames += "|";
                            lAbsFileNames += lFileName;

                            lRelFileNames += "|";
                            lRelFileNames += lRelativeFileName;
                        }
                    }
                }


                mFileObject->FieldReadBlockEnd();
            }
            mFileObject->FieldReadEnd();
        }

        if( !lFirstIteration ) 
        {
            // found at least one embedded file.
            pBindingTable.DescAbsoluteURL.Set( lAbsFileNames );
            pBindingTable.DescRelativeURL.Set( lRelFileNames );
        }
    }

    return lSuccess;
}

//
// Read cache
//
bool FbxReaderFbx6::ReadCache( FbxCache& pCache )
{
    mFileObject->FieldReadI(FIELD_KFBXCACHE_VERSION, 100);

    ReadPropertiesAndFlags(&pCache,mFileObject);

    // Update the absolute path is necessary
    FbxString lRelativeFileName, lAbsoluteFileName;
    pCache.GetCacheFileName(lRelativeFileName, lAbsoluteFileName);

    if (!FbxFileUtils::Exist(lAbsoluteFileName))
    {
        // Try to construct an absolute path from the relative path
        FbxString lFBXDirectory = mFileObject->GetFullPath("");

        if (lFBXDirectory.GetLen() == 0 || FbxPathUtils::IsRelative(lFBXDirectory))
        {
            lFBXDirectory = FbxPathUtils::GetFolderName(FbxPathUtils::Resolve(lFBXDirectory.Buffer()));
        }

        FbxString lTentativePath = lFBXDirectory + FbxString("/") + lRelativeFileName;
        lTentativePath = FbxPathUtils::Clean(lTentativePath.Buffer());

        if (FbxFileUtils::Exist(lTentativePath))
        {
            pCache.SetCacheFileName(lRelativeFileName, lTentativePath);
        }
    }

    return true;
}

//
// Read video
//
bool FbxReaderFbx6::ReadVideo(FbxVideo& pVideo)
{
    FbxVideo* lReferencedVideo = FbxCast<FbxVideo>(pVideo.GetReferenceTo());
    if (lReferencedVideo != NULL)
    {
        return ReadPropertiesAndFlags(&pVideo, mFileObject);
    }

    ReadPropertiesAndFlags(&pVideo, mFileObject);

    pVideo.ImageTextureSetMipMap(mFileObject->FieldReadB(FIELD_KFBXVIDEO_USEMIPMAP));


    if (mFileObject->FieldReadI(FIELD_MEDIA_VERSION) > 100)
    {
        pVideo.SetOriginalFormat(mFileObject->FieldReadB(FIELD_MEDIA_ORIGINAL_FORMAT));
        pVideo.SetOriginalFilename(mFileObject->FieldReadC(FIELD_MEDIA_ORIGINAL_FILENAME));
    }

    FbxString lFileName, lRelativeFileName;
    lFileName = mFileObject->FieldReadC(FIELD_MEDIA_FILENAME);
    lFileName = pVideo.GetFileName();
    lRelativeFileName = mFileObject->FieldReadC(FIELD_MEDIA_RELATIVE_FILENAME);

    // If this field exist, the media is embedded.
    bool lSkipValidation = true;
    if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true))
    {
        lSkipValidation = false;
        if (mFileObject->FieldReadBegin(FIELD_MEDIA_CONTENT))
        {
            FbxString lDefaultPath = "";
            FbxString lUserDefinePath = IOS_REF.GetStringProp(IMP_EXTRACT_FOLDER, lDefaultPath);
            const char* lUserDefinePathBuffer = (lUserDefinePath == lDefaultPath) ? NULL : lUserDefinePath.Buffer();
            bool lStatus = mFileObject->FieldReadEmbeddedFile(lFileName, lRelativeFileName, mFileObject->GetMediaDirectory(true, lUserDefinePathBuffer));
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
        if (FbxFileUtils::Exist(pVideo.GetFileName()) == false)
        {
            FbxString lNewAbsolutePath = mFileObject->GetFullFilePath(pVideo.GetRelativeFileName());
            lNewAbsolutePath = FbxPathUtils::Clean(lNewAbsolutePath);
            if (FbxFileUtils::Exist(lNewAbsolutePath))
            {
                // Set with a valid "absolute" path
                pVideo.SetFileName(lNewAbsolutePath.Buffer());
            }
        }
    }

    return !lFileName.IsEmpty();
}

//
// Read GemetryWeightMap
//
bool FbxReaderFbx6::ReadGeometryWeightedMap(FbxGeometryWeightedMap& pGeometryWeightedMap)
{
    //
    // Read version number
    //
    int Version = mFileObject->FieldReadI (FIELD_KFBXGEOMETRYPATCH_PATCH_VERSION, 100);

    //
    // Read source count...
    //
    int lSrcCount = 0;
    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRY_WEIGHTED_MAP_SRC_COUNT))
    {
        lSrcCount = mFileObject->FieldReadI();
        mFileObject->FieldReadEnd ();
    }

    //
    // Read destination count...
    //
    int lDstCount = 0;
    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRY_WEIGHTED_MAP_DST_COUNT))
    {
        lDstCount = mFileObject->FieldReadI();
        mFileObject->FieldReadEnd ();
    }

    if ((lSrcCount < 1) || (lDstCount < 1))
    {
        return false;
    }

    FbxWeightedMapping* lMapping = FbxNew< FbxWeightedMapping >(lSrcCount, lDstCount);

    for (int i = 0; i < lSrcCount; i++)
    {
        //
        // Read index mapping...
        //
        if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRY_WEIGHTED_MAP_INDEX_MAPPING))
        {
            //
            // Read source count...
            //
            int lSrcIndex = mFileObject->FieldReadI();

            //
            // Read relation count...
            //
            int lRelCount = mFileObject->FieldReadI();

            for (int j = 0; j < lRelCount; j++)
            {
                //
                // Read destination index...
                //
                int     lDstIndex  = mFileObject->FieldReadI();

                //
                // Read destination weight...
                //
                double  lDstWeight = mFileObject->FieldReadD();

                //
                // Add read index mapping into weighting table
                //
                lMapping->Add(lSrcIndex, lDstIndex, lDstWeight);
            }

            mFileObject->FieldReadEnd();
        }
    }

    pGeometryWeightedMap.SetValues(lMapping);
    return true;
}

//
// Read cluster link
//
bool FbxReaderFbx6::ReadLink(FbxCluster& pLink)
{
    const char*ModeStr;
    const char*lAssModel;
    int        PointCount;
    int        Count;
    FbxVector4 lRow;

    // The name of the link node is stored, and will be resolved in
    // function FbxReader::ResolveLinks()
    pLink.mBeforeVersion6LinkName = mFileObject->FieldReadC ();

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
            FbxString UserDataId = mFileObject->FieldReadC();
            FbxString UserData = mFileObject->FieldReadC();
            pLink.SetUserData(UserDataId, UserData);
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
                pLink.GetControlPointIndices()[Count] = mFileObject->FieldReadI ();
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
                pLink.GetControlPointWeights()[Count] = mFileObject->FieldReadD ();
            }

            mFileObject->FieldReadEnd ();
        }

        //
        // Read the TRANSFORM matrix...
        //
		{
			FbxMatrix lMatrix;
			mFileObject->FieldReadDn(FIELD_KFBXLINK_TRANSFORM, (double*)lMatrix.mData, NULL, 16);
			pLink.SetTransformMatrix(*(FbxAMatrix*)&lMatrix);
		}

        //
        // Read the TRANSFORM LINK matrix...
        //
        if ( mFileObject->FieldReadBegin ( FIELD_KFBXLINK_TRANSFORM_LINK ) )
        {
            FbxMatrix lMatrix;

            mFileObject->FieldReadDn((double*) lMatrix.mData, 16);

            pLink.SetTransformLinkMatrix(*(FbxAMatrix*)&lMatrix);

            mFileObject->FieldReadEnd ();
        }

        FbxAMatrix Transform, TransformLink;

        pLink.GetTransformMatrix(Transform);
        pLink.GetTransformLinkMatrix(TransformLink);

        Transform = TransformLink * Transform;

        pLink.SetTransformMatrix(Transform);

        //
        // Read the ASSOCIATE MODEL...
        //

        if ( mFileObject->FieldReadBegin ( FIELD_KFBXLINK_ASSOCIATE_MODEL ) )
        {
            lAssModel = mFileObject->FieldReadC ();

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

//
// Read skin
//
bool FbxReaderFbx6::ReadSkin(FbxSkin& pSkin)
{
    mFileObject->FieldReadI(FIELD_KFBXDEFORMER_VERSION, 100);
   
    ReadPropertiesAndFlags(&pSkin,mFileObject);

    if (mFileObject->FieldReadBegin(FIELD_KFBXSKIN_DEFORM_ACCURACY))
    {
        pSkin.SetDeformAccuracy(mFileObject->FieldReadD());
        mFileObject->FieldReadEnd();
    }

    return true;
}

//
// Read vertex cache deformer
//
bool FbxReaderFbx6::ReadVertexCacheDeformer(FbxVertexCacheDeformer& pDeformer)
{
    mFileObject->FieldReadI(FILED_KFBXVERTEXCACHEDEFORMER_VERSION, 100);    

    ReadPropertiesAndFlags(&pDeformer,mFileObject);

    return true;
}

//
// Read Cluster
//
bool FbxReaderFbx6::ReadCluster(FbxCluster& pCluster)
{
    mFileObject->FieldReadI(FIELD_KFBXDEFORMER_VERSION, 100);    

    ReadPropertiesAndFlags(&pCluster,mFileObject);

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
        FbxString UserData   = mFileObject->FieldReadC();

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
    if( mFileObject->FieldReadBegin(FIELD_KFBXLINK_TRANSFORM_LINK) )
    {
        mFileObject->FieldReadDn((double*)TransformLink.mData, 16);
        mFileObject->FieldReadEnd();
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

    if( mFileObject->FieldReadBegin(FIELD_KFBXDEFORMER_TRANSFORM_PARENT) )
    {
        FbxMatrix TransformParent;
        mFileObject->FieldReadDn((double*)TransformParent.mData, 16);
        pCluster.SetTransformParentMatrix(*(FbxAMatrix*)&TransformParent);
        mFileObject->FieldReadEnd();
    }

    return true;
}

//
// Read gloabl settings
//
bool FbxReaderFbx6::ReadGlobalSettings(FbxGlobalSettings& pGlobalSettings)
{
   mFileObject->FieldReadI(FIELD_GLOBAL_SETTINGS_VERSION, 1000);

    ReadPropertiesAndFlags(&pGlobalSettings,mFileObject);

    return true;
}

//
// Read constraint
//
bool FbxReaderFbx6::ReadConstraint(FbxConstraint& pConstraint)
{
    if (IOS_REF.GetBoolProp(IMP_FBX_CONSTRAINT, true))
    {
        ReadPropertiesAndFlags(&pConstraint,mFileObject);


        
        // Version 100: original version
        // Version 101:
        //      - Added the translation, rotation and scaling offset
        //        Note: Only the first source weight is supported
        if (pConstraint.GetConstraintType() == FbxConstraint::eParent)
        {
            if (mFileObject->FieldReadBegin(FIELD_CONSTRAINT_VERSION))
            {
                int lVersion = mFileObject->FieldReadI(FIELD_CONSTRAINT_VERSION);
                if (lVersion == 101)
                {
                    int i = 0;
                    FbxIterator<FbxProperty>  lFbxPropertyIter(&pConstraint);
                    FbxProperty                lFbxProperty;

                    FbxForEach(lFbxPropertyIter,lFbxProperty) {
                        FbxString lName = lFbxProperty.GetName();

                        int lIndexR = lName.Find(".Offset R");

                        if (lIndexR != -1 && lIndexR == lName.GetLen() - 9 )
                        {
                            mFileObject->FieldReadBegin(FIELD_CONSTRAINT_OFFSET);

                            //Set the rotation value
                            FbxVector4 lRotationVector;
                            lRotationVector[0] = mFileObject->FieldReadD();
                            lRotationVector[1] = mFileObject->FieldReadD();
                            lRotationVector[2] = mFileObject->FieldReadD();
                            FbxDouble3 lDouble3 (lRotationVector[0],lRotationVector[1], lRotationVector[2]);
                            lFbxProperty.Set(lDouble3);

                            //Get the translation property
                            FbxString lTranslationOffsetName(lName.Left(lIndexR));
                            lTranslationOffsetName += ".Offset T";

                            FbxProperty lPropertyTranslation = pConstraint.FindProperty(lTranslationOffsetName.Buffer());
                            if (lPropertyTranslation.IsValid()) {
                                FbxVector4 lTranslationVector;
                                lTranslationVector[0] = mFileObject->FieldReadD();
                                lTranslationVector[1] = mFileObject->FieldReadD();
                                lTranslationVector[2] = mFileObject->FieldReadD();
                                FbxDouble4 lDouble4 =FbxDouble4(lTranslationVector[0],lTranslationVector[1], lTranslationVector[2],lTranslationVector[3]);
                                lPropertyTranslation.Set(lDouble4);
                            }
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

//
// Remove duplicate textures
//
void FbxReaderFbx6::RemoveDuplicateTextures(FbxScene& pScene)
{
    int i, j, lGeometryCount;
    FbxArray<FbxTexture*> lTextureDuplicate;
    FbxArray<FbxTexture*> lTextureReplacement;

    // Scan texture array in entity for duplicate textures.

    int lTextureCount = pScene.GetTextureCount();

    for (i = 0; i < lTextureCount; i++)
    {
        FbxTexture* lTextureA = pScene.GetTexture(i);

        for (j = lTextureCount - 1; j > i; j--)
        {
            FbxTexture* lTextureB = pScene.GetTexture(j);

            if (*lTextureB == *lTextureA)
            {
                pScene.RemoveTexture(lTextureB);

                mObjectMap.Remove(mObjectMap.Find(lTextureB));
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
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureDiffuse);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureEmissive);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureEmissiveFactor);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureAmbient);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureAmbientFactor);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureDiffuseFactor);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureSpecular);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureSpecularFactor);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureShininess);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureBump);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureNormalMap);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureTransparency);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureTransparencyFactor);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureReflection);
            ReplaceTextures(lTextureDuplicate, lTextureReplacement, lGeometry, FbxLayerElement::eTextureDisplacement);
        }
    }
}

//
// Replace textures
//
void FbxReaderFbx6::ReplaceTextures(FbxArray<FbxTexture*> const& pTextureDuplicate,
                                     FbxArray<FbxTexture*> const& pTextureReplacement,
                                     FbxGeometry* pGeometry, FbxLayerElement::EType pTextureType)
{
    int lTextureLayerCount = pGeometry->GetLayerCount(pTextureType);
    int j, k;

    for (k=0; k<lTextureLayerCount; k++)
    {
        FbxLayerElementTexture* lTextureLayer = pGeometry->GetLayer(k, pTextureType)->GetTextures(pTextureType);

        if( lTextureLayer->GetReferenceMode() != FbxLayerElement::eIndex )
        {
            int lTextureCount = lTextureLayer->GetDirectArray().GetCount();

            for (j = 0; j < lTextureCount; j++)
            {
                int lReplacementIndex = pTextureDuplicate.Find(lTextureLayer->GetDirectArray().GetAt(j));

                if (lReplacementIndex != -1)
                {
                    lTextureLayer->GetDirectArray().SetAt(j, pTextureReplacement[lReplacementIndex]);
                }
            }
        }
    }
}


void FbxReaderFbx6::RemoveDuplicateMaterials(FbxScene& pScene)
{
    // This code seems out of date. Its not called from anywhere. There is only
    // one call in Read() that is commented out. Even that call is made before
    // the geometry's material direct array is built, so I don't think that this code
    // has been used in awhile. So I'm commenting out this code as part of bug 261354.
    // SY

    return;    
}


//
// Convert camera name
//
FbxString FbxReaderFbx6::ConvertCameraName(FbxString pCameraName)
{
    FbxString lKModel = "Model::";
    FbxString lKModelProducerPerspective  = lKModel + FBXSDK_CAMERA_PERSPECTIVE;
    FbxString lKModelProducerTop          = lKModel + FBXSDK_CAMERA_TOP;
    FbxString lKModelProducerBottom       = lKModel + FBXSDK_CAMERA_BOTTOM;
    FbxString lKModelProducerFront        = lKModel + FBXSDK_CAMERA_FRONT;
    FbxString lKModelProducerBack         = lKModel + FBXSDK_CAMERA_BACK;
    FbxString lKModelProducerRight        = lKModel + FBXSDK_CAMERA_RIGHT;
    FbxString lKModelProducerLeft         = lKModel + FBXSDK_CAMERA_LEFT;
    FbxString lKModelCameraSwitcher       = lKModel + FBXSDK_CAMERA_SWITCHER;

    if (pCameraName == lKModelProducerPerspective)
    {
        return FBXSDK_CAMERA_PERSPECTIVE;
    }
    else if (pCameraName == lKModelProducerTop)
    {
        return FBXSDK_CAMERA_TOP;
    }
    else if (pCameraName == lKModelProducerBottom)
    {
        return FBXSDK_CAMERA_BOTTOM;
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

//
// Read object properties
//
bool FbxReaderFbx6::ReadProperties(FbxObject *pFbxObject, FbxIO* pFbxFileObject, bool pReadNodeAttributeProperties)
{
    //  return pFbxFileObject->FieldReadPropertiesList(KArrayPropertyList& pPropertyList, HKObject pObject /*= NULL*/)

    bool lReadExtendedProperties = pFbxFileObject->FieldReadBegin("Properties70");

    if (lReadExtendedProperties || pFbxFileObject->FieldReadBegin("Properties60") ) {
        if (pFbxFileObject->FieldReadBlockBegin()) {
            int     FieldInstanceCount = pFbxFileObject->FieldGetInstanceCount("Property");
            FbxString lPropertyName, lTypeName, lDataTypeName, lFlags;

            pFbxObject->RootProperty.BeginCreateOrFindProperty();

            int index = 0;
            for (int c = 0; c<FieldInstanceCount; c++ ) 
            {
                bool lFieldIsBegin = false;

                lFieldIsBegin   = pFbxFileObject->FieldReadBegin("Property",c);

                lPropertyName   = pFbxFileObject->FieldReadS( );    // Name
                lTypeName       = pFbxFileObject->FieldReadS( );    // TypeName

                if( lReadExtendedProperties )
                {
                    lDataTypeName = pFbxFileObject->FieldReadS();
                }

                lFlags          = pFbxFileObject->FieldReadS( );    // Flags : see store for description

                bool                lIsAnimatable   = strchr(lFlags.Buffer(),'A')!=NULL;
                bool                lIsUser         = strchr(lFlags.Buffer(),'U')!=NULL;
                bool                lIsAnimated     = strchr(lFlags.Buffer(),'+')!=NULL;
                bool                lIsHidden       = strchr(lFlags.Buffer(),'H')!=NULL;

                // Node attribute Patch, we need to look for the flag that indicates
                // this property was a property on the node attribute
                bool                lCreateOnNodeAttribute = strchr(lFlags.Buffer(),'N')!=NULL;;

                if( lCreateOnNodeAttribute && !pReadNodeAttributeProperties )
                {
                    if( lFieldIsBegin )
                        pFbxFileObject->FieldReadEnd();

                    continue;
                }

                FbxDataType        lDataType;

                // interpretation of the data types
                if( lReadExtendedProperties && !lDataTypeName.IsEmpty() )
                {
                    /*
                    This handles 'derived types' when we want to keep the information
                    around.

                    For instance, an URL's base type is a FbxString.  But we still want
                    to know it's an URL, so it's store as

                    "KString", "Url"

                    If we encounter a new derived type that we don't know about, we
                    register it.

                    This is only available when reading 'enhanced' property formats.
                    */

                    lDataType = mManager.GetDataTypeFromName(lDataTypeName.Buffer());
                    if( !lDataType.Valid() ) lDataType = mManager.GetDataTypeFromName(lTypeName.Buffer());

                    if( !lDataType.Valid() )
                    {
                        FbxDataType lBaseType = mManager.GetDataTypeFromName(lDataTypeName.Buffer());

                        if( lBaseType.Valid() )
                        {
                            lDataType = mManager.CreateDataType(lDataTypeName.Buffer(), lBaseType.GetType());
                        }
                    }
                }
                else
                {
                    lDataType = mManager.GetDataTypeFromName(lTypeName.Buffer());
                    if (!lDataType.Valid()) lDataType=mManager.GetDataTypeFromName(lTypeName.Buffer());
                }

                if( !lDataType.Valid() )
                {
                    FBX_ASSERT_NOW("Unsupported type!");
                    continue;
                }

                // Check if the property already exist
                // and create a new one if not
                FbxProperty lUserProperty;
                FbxObject*  lPropertyContainer = pFbxObject;

                // check if property is flagged as belonging to the node attribute
                if (lCreateOnNodeAttribute && pFbxObject->Is<FbxNode>())
                {
                    FbxNode* lNode = FbxCast<FbxNode>(pFbxObject);
                    FbxNodeAttribute* lAttr = lNode ? lNode->GetNodeAttribute() : NULL;
                    if (lAttr)
                        lPropertyContainer =  lAttr;
                }

                // Find the property
                lUserProperty = lPropertyContainer->FindPropertyHierarchical(lPropertyName.Buffer());

                //Before creating it, check if we have the property on the node attribute
                // Note that if the lCreateNodeAttribute is true (meaning we have the 'N' flag)
                // and the property is invalid, it will be created on the nodeattribute no matter
                // what. All the properties that come from MB and do not have the 'N' flag but
                // belong indeed to the nodeattribute will get processed in the block here and,
                // if after looking for the existance of the property on the node attribute we
                // still do not find it, we have to create it on the node.
                if (!lUserProperty.IsValid())
                {                    
                    if (!lCreateOnNodeAttribute && pFbxObject->Is<FbxNode>())
                    {
                        FbxNode* lNode = FbxCast<FbxNode>(pFbxObject);
                        FbxNodeAttribute* lAttr = lNode ? lNode->GetNodeAttribute() : NULL;
                        if (lAttr)
                            lPropertyContainer =  lAttr;
                    
                        lUserProperty = lPropertyContainer->FindPropertyHierarchical(lPropertyName.Buffer());
                        if (!lUserProperty.IsValid())
                        {
                            // the property is not even on the nodeAttribute, 
                            // make sure we create it on the Node.
                            lPropertyContainer = pFbxObject;
                        }
                    }
                }

                if (!lUserProperty.IsValid())
                {
                    // get the parent first
                    int lIndex = lPropertyName.ReverseFind( *FbxProperty::sHierarchicalSeparator );

                    if( -1 != lIndex )
                    {
                        FbxProperty lParent = lPropertyContainer->FindPropertyHierarchical( lPropertyName.Mid(0, lIndex ) );
                        FBX_ASSERT( lParent.IsValid() );

                        if( lParent.IsValid() )
                            lUserProperty   = FbxProperty::Create(lParent, lDataType, lPropertyName.Mid( lIndex + 1 ), "", false);
                        else
                            //user property name may contains "|"
                            lUserProperty   = FbxProperty::Create(lPropertyContainer, lDataType, lPropertyName, "", false);
                    }
                    else
                    {
                        lUserProperty   = FbxProperty::Create(lPropertyContainer, lDataType, lPropertyName, "", false);
                    }

					//Properties created upon import are flagged as such
					if( lUserProperty.IsValid() )
					{
						lUserProperty.ModifyFlag(FbxPropertyFlags::eImported, true);
					}
                }
                FBX_ASSERT(lUserProperty.IsValid());

                // Make sure it has the correct flags
                if( lUserProperty.GetFlag(FbxPropertyFlags::eAnimatable) != lIsAnimatable )
                    lUserProperty.ModifyFlag(FbxPropertyFlags::eAnimatable,lIsAnimatable);

                if( lUserProperty.GetFlag(FbxPropertyFlags::eUserDefined) != lIsUser )
                    lUserProperty.ModifyFlag(FbxPropertyFlags::eUserDefined,lIsUser);

                if( lUserProperty.GetFlag(FbxPropertyFlags::eAnimated) != lIsAnimated )
                    lUserProperty.ModifyFlag(FbxPropertyFlags::eAnimated,lIsAnimated);

                if( lUserProperty.GetFlag(FbxPropertyFlags::eHidden) != lIsHidden )
                    lUserProperty.ModifyFlag(FbxPropertyFlags::eHidden,lIsHidden);

                int lTempEnumVal = 0;
                if ( !lDataType.Is(FbxEventDT) && !lDataType.Is(FbxActionDT) && !lDataType.Is(FbxCompoundDT)) 
                {
                    // Read values based on type
                    switch (lUserProperty.GetPropertyDataType().GetType()) 
                    {
                    case eFbxChar:        lUserProperty.Set( FbxChar(pFbxFileObject->FieldReadByte()) );     break;
                    case eFbxUChar:        lUserProperty.Set( FbxUChar(pFbxFileObject->FieldReadUByte()) );     break;
                    case eFbxShort:        lUserProperty.Set( FbxShort(pFbxFileObject->FieldReadShort()) );     break;
                    case eFbxUShort:        lUserProperty.Set( FbxUShort(pFbxFileObject->FieldReadUShort()) );     break;
                    case eFbxUInt:    lUserProperty.Set( FbxUInt(pFbxFileObject->FieldReadUI()) );     break;
                    case eFbxULongLong:   lUserProperty.Set( FbxULongLong(pFbxFileObject->FieldReadULL()) );     break;
                    case eFbxHalfFloat:    
                        {
                            FbxHalfFloat hf(pFbxFileObject->FieldReadF());
                            lUserProperty.Set( hf );    
                        }
                        break;

                    case eFbxBool:        lUserProperty.Set( FbxBool(pFbxFileObject->FieldReadB()) );     break;
                    case eFbxInt:        lUserProperty.Set( FbxInt(pFbxFileObject->FieldReadI()) ); break;
                    case eFbxLongLong:    lUserProperty.Set( FbxLongLong(pFbxFileObject->FieldReadLL()) );     break;
                    case eFbxFloat:        lUserProperty.Set( FbxFloat(pFbxFileObject->FieldReadF()) );    break;
                    case eFbxDouble:        lUserProperty.Set( FbxDouble(pFbxFileObject->FieldReadD()) );   break;
                    case eFbxDouble2: 
                        {
                            FbxDouble2 lValue;
                            pFbxFileObject->FieldReadDn((double *)&lValue, 2);
                            lUserProperty.Set( lValue );
                        } 
                        break;
                    case eFbxDouble3: 
                        {
                            FbxDouble3 lValue;
                            pFbxFileObject->FieldRead3D((double *)&lValue);
                            lUserProperty.Set( lValue );
                        } 
                        break;
                    case eFbxDouble4:
                        {
                            FbxDouble4 lValue;
                            pFbxFileObject->FieldRead4D((double *)&lValue);
                            lUserProperty.Set( lValue );
                        } 
                        break;
                    case eFbxDouble4x4:
                        {
                            FbxDouble4x4 lValue;
                            pFbxFileObject->FieldRead4D( (double *)&(lValue[0]) );
                            pFbxFileObject->FieldRead4D( (double *)&(lValue[1]) );
                            pFbxFileObject->FieldRead4D( (double *)&(lValue[2]) );
                            pFbxFileObject->FieldRead4D( (double *)&(lValue[3]) );
                            lUserProperty.Set( lValue );
                        } 
                        break;
					case eFbxEnumM:
                    case eFbxEnum:       lTempEnumVal = FbxEnum(pFbxFileObject->FieldReadI()); lUserProperty.Set(lTempEnumVal); break;
                    case eFbxString:     lUserProperty.Set( FbxString(pFbxFileObject->FieldReadS()) ); break;
                    case eFbxTime:       lUserProperty.Set( FbxTime(pFbxFileObject->FieldReadT()) ); break;
                    case eFbxReference:  break; // used as a port entry to reference object or properties
                    case eFbxBlob:
                        {
                            FBX_ASSERT( lReadExtendedProperties );    // Should only be part of Properties70
							int lBlobLength = pFbxFileObject->FieldReadI();
							bool lResult    = (lBlobLength > 0);
							if (!lResult) break;
                            
							FbxBlob lBlobValue( lBlobLength );
							char* lBlobOutput = reinterpret_cast<char*>(lBlobValue.Modify());

                            if (pFbxFileObject->FieldReadBlockBegin())
                            {
								// Block may be empty -- we don't expect more than 1        
								int lBlobChunkCount = pFbxFileObject->FieldGetInstanceCount("BinaryData");

								for( int i = 0; i < lBlobChunkCount && (lBlobLength > 0) && lResult; ++i )
								{
									if (pFbxFileObject->FieldReadBegin("BinaryData"))
									{
										for( int lValueCount = pFbxFileObject->FieldReadGetCount();
											(lValueCount > 0) && (lBlobLength > 0) && lResult; --lValueCount )
										{
											int lChunkSize;
											void* lChunkData = pFbxFileObject->FieldReadR(&lChunkSize);

											FBX_ASSERT(lChunkData && lChunkSize > 0);

											if( lChunkData && lChunkSize > 0)
											{
												FBX_ASSERT( lChunkSize <= lBlobLength );

												// We may not read everything if this occurs                    
												lChunkSize = FbxMin(lChunkSize, lBlobLength);

												memcpy(lBlobOutput, lChunkData, lChunkSize);
												lBlobOutput  += lChunkSize;
												lBlobLength  -= lChunkSize;
											} 
											else
											{
												// File is most likely invalid - error out? Note that this does not appear to stop the
												// decoding of the file.
												GetStatus().SetCode(FbxStatus::eFailure, "Error decoding binary data chunk. The file may be corrupted.");
												lResult = false;
											}
										}
										pFbxFileObject->FieldReadEnd();
									}
								}
								pFbxFileObject->FieldReadBlockEnd();
							}
							else
							{
								lResult = false;
							}

                            // Refuse to store incomplete/invalid data.
                            if( lResult )
                            {
                                lUserProperty.Set(lBlobValue);
                            }
                        }
                        break;
                    case eFbxDistance:  
                        {
                            float value = pFbxFileObject->FieldReadF();
                            FbxString unit = pFbxFileObject->FieldReadS();
                            FbxDistance lDistance(value, unit);
                            lUserProperty.Set(lDistance);
                        }
                        break;
                    case eFbxDateTime: 
                        {
                            FbxDateTime lDateTime;
                            FbxString lBuffer   = mFileObject->FieldReadC();
                            if( !lBuffer.IsEmpty() && lDateTime.fromString(lBuffer) )
                            {
                                lUserProperty.Set(lDateTime);
                            }
                        }
                        break;
                    default:
                        FBX_ASSERT_NOW("Unsupported type!");
                        break;
                    } // switch
                } // if

                // for user properties get the min and max
                if (lIsUser) 
                {
                    switch (lUserProperty.GetPropertyDataType().GetType()) 
                    {
		            default:
		            break;
                    case eFbxChar:
                        {
                        lUserProperty.SetMinLimit( pFbxFileObject->FieldReadByte() );
                        lUserProperty.SetMaxLimit( pFbxFileObject->FieldReadByte() );
                        } 
                        break;
                    case eFbxUChar:
                        {
                        lUserProperty.SetMinLimit( pFbxFileObject->FieldReadUByte() );
                        lUserProperty.SetMaxLimit( pFbxFileObject->FieldReadUByte() );
                        } 
                        break;
                    case eFbxShort:
                        {
                        lUserProperty.SetMinLimit( pFbxFileObject->FieldReadShort() );
                        lUserProperty.SetMaxLimit( pFbxFileObject->FieldReadShort() );
                        } 
                        break;
                    case eFbxUShort:
                        {
                        lUserProperty.SetMinLimit( pFbxFileObject->FieldReadUShort() );
                        lUserProperty.SetMaxLimit( pFbxFileObject->FieldReadUShort() );
                        } 
                        break;
                    case eFbxUInt:
                        {
                        lUserProperty.SetMinLimit( pFbxFileObject->FieldReadUI() );
                        lUserProperty.SetMaxLimit( pFbxFileObject->FieldReadUI() );
                        } 
                        break;
                    case eFbxULongLong:
                        {
                        lUserProperty.SetMinLimit( double(pFbxFileObject->FieldReadULL()) );
                        lUserProperty.SetMaxLimit( double(pFbxFileObject->FieldReadULL()) );
                        } 
                        break;
                    case eFbxHalfFloat:
                        {
                            lUserProperty.SetMinLimit( pFbxFileObject->FieldReadF());
                            lUserProperty.SetMaxLimit( pFbxFileObject->FieldReadF());
                        } 
                        break;
                    case eFbxBool:
                        {
                            lUserProperty.SetMinLimit( pFbxFileObject->FieldReadB());
                            lUserProperty.SetMaxLimit( pFbxFileObject->FieldReadB());
                        } 
                        break;
                    case eFbxInt:
                        {
                            lUserProperty.SetMinLimit( pFbxFileObject->FieldReadI());
                            lUserProperty.SetMaxLimit( pFbxFileObject->FieldReadI());
                        } 
                        break;
                    case eFbxLongLong:
                        {
                            lUserProperty.SetMinLimit( double(pFbxFileObject->FieldReadLL()));
                            lUserProperty.SetMaxLimit( double(pFbxFileObject->FieldReadLL()));
                        } 
                        break;
                    case eFbxFloat:
                        {
                            lUserProperty.SetMinLimit( pFbxFileObject->FieldReadF());
                            lUserProperty.SetMaxLimit( pFbxFileObject->FieldReadF());
                        } 
                        break;
                    case eFbxDouble:
                        {
                            lUserProperty.SetMinLimit( pFbxFileObject->FieldReadD());
                            lUserProperty.SetMaxLimit( pFbxFileObject->FieldReadD());
                        } 
                        break;
					case eFbxEnumM:
                    case eFbxEnum:
                        { // enum
                            FbxString EnumList = pFbxFileObject->FieldReadS();    // Values
                            int     EnumListLen = (int)EnumList.GetLen();
                            int     Index = 0;
                            if (EnumList!="") 
                            {
                                do
                                {
                                    FbxString lItem = "";
                                    while( Index < EnumListLen && EnumList[Index] != '~' )
                                    {
                                        lItem += EnumList[Index];
                                        Index++;
                                    }

                                    if(Index < EnumListLen)
                                    {
                                        Index++;
                                    }

                                    lUserProperty.AddEnumValue(lItem.Buffer());
                                } while( Index < EnumListLen );
                            }
                            lUserProperty.Set(lTempEnumVal);
                        } 
                        break;
                    } // switch 
                } // if

                if (lFieldIsBegin) pFbxFileObject->FieldReadEnd();
            } // for

            pFbxObject->RootProperty.EndCreateOrFindProperty();

            pFbxFileObject->FieldReadBlockEnd();
        }
        pFbxFileObject->FieldReadEnd();

        return true;
    }
    return false;
}

//
// Read properties and flags for fbx object from file object
//
bool FbxReaderFbx6::ReadPropertiesAndFlags(FbxObject *pFbxObject, FbxIO* pFbxFileObject, bool pReadNodeAttributeProperties)
{
    if( mProgress && !mProgressPause )
    {
        mProgress->Update(1.0f, pFbxObject->GetName());
    }
    return ReadProperties(pFbxObject, pFbxFileObject, pReadNodeAttributeProperties);
}

// ****************************************************************************************
// Connections
// ****************************************************************************************

//
// Convert connection between sources and destinations
//
void ConvertConnectionSrcDst(FbxObject* &pSrc, FbxObject* &pDst)
{
    // Fix for Node Attribute connections adjustments
    // Deformers and Node attributes should not be connected to
    // FbxNode they should be connected to the geometry
    // Also shadow color textures should be connected directly
    // to the light.
    if( pSrc && pDst && pDst->Is<FbxNode>() )
    {
        if( pSrc->Is<FbxDeformer>() || pSrc->Is<FbxGeometryWeightedMap>() || (pSrc->Is<FbxTexture>() && (static_cast<FbxNode*>(pDst)->GetLight() || static_cast<FbxNode*>(pDst)->GetCamera())) )
        {
            pDst = pDst->GetSrcObject<FbxNodeAttribute>();
        }
    }
}

//
// Read connection section
//
bool FbxReaderFbx6::ReadConnectionSection(FbxDocument* pDocument)
{
    if (mFileObject->FieldReadBegin("Connections"))
    {
        if (mFileObject->FieldReadBlockBegin())
        {
            while (mFileObject->FieldReadBegin("Connect"))
            {
                char Type[32];
                FbxProperty SrcP,DstP;
                FbxObject* Src = NULL;
                FbxObject* Dst = NULL;
                FbxObject* Object = NULL;

                FBXSDK_strncpy(Type, 32, mFileObject->FieldReadC(), 31);
                if (strcmp(Type,"OO")==0) {
                    Src = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
                    Dst = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
                    // Fix for connections to FbxNode vs Node Attributes
                    ConvertConnectionSrcDst(Src,Dst);
                } else if (strcmp(Type,"OD")==0) {
                    Src = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
                    Dst = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
                    // Fix for connections to FbxNode vs Node Attributes
                    ConvertConnectionSrcDst(Src,Dst);
                } else if (strcmp(Type,"PO")==0) {
                    Object = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
                    if (Object) {
                        FbxString PropertyName = mFileObject->FieldReadC();
                        SrcP = Object->FindPropertyHierarchical(PropertyName.Buffer());
                        if (SrcP.IsValid()) {
                            Src  = Object;
                        }
                    }
                    Dst = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
                } else if (strcmp(Type,"OP")==0) {

                    Src = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
                    Object = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
                    if (Object) {
                        FbxString PropertyName = mFileObject->FieldReadC();
                        if(strcmp(PropertyName.Buffer(), "Background Texture") == 0 || 
                            strcmp(PropertyName.Buffer(), "Foreground Texture") == 0)    //connect the image plane with camera
                        {
                            Dst  = Object;
                            ConvertConnectionSrcDst(Src,Dst);
                            DstP = Dst->FindPropertyHierarchical(PropertyName.Buffer());
                            FBX_ASSERT(DstP.IsValid());
                        }
                        else
                        {
                            DstP = Object->FindPropertyHierarchical(PropertyName.Buffer());
                            if (DstP.IsValid()) {
                                Dst  = Object;
                            }
                        }


                    }
                } else if (strcmp(Type,"PP")==0) {
                    //Source Property
                    Object = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
                    if( Object ) {
                        SrcP = Object->FindPropertyHierarchical(mFileObject->FieldReadC());
                        if (SrcP.IsValid()) {
                            Src  = Object;
                        }
                    }

                    //Destination Property
                    Object = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
                    if( Object ) {
                        DstP = Object->FindPropertyHierarchical(mFileObject->FieldReadC());
                        if (DstP.IsValid()) {
                            Dst  = Object;
                        }
                    }
                } else if( strcmp(Type, "EP") == 0 ) {
                    //Source Object (Entity)
                    FbxString lEntityName = mFileObject->FieldReadC();
                    Src = pDocument;

                    //Destination Property
                    Object = mObjectMap.Get(mObjectMap.Find(mFileObject->FieldReadC()));
                    if( Object ) {
                        DstP = Object->FindPropertyHierarchical(mFileObject->FieldReadC());
                        if (DstP.IsValid()) {
                            Dst  = Object;
                        }
                    }
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

// ****************************************************************************************
// Animation
// ****************************************************************************************

//
// Read animation from document
//
bool FbxReaderFbx6::ReadDocumentAnimation(FbxDocument* pDocument)
{
    FbxScene*  lScene = FbxCast<FbxScene>(pDocument);
    bool        lIsAScene = (lScene != NULL);

    if (!lScene)
    {
        return true;
    }
    
    int         i, lCount = mTakeInfo.GetCount();
    bool        lResult = true;

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
                        if (ReadTakeAnimation(*lScene, lTakeInfo))
                        {
                            lScene->SetTakeInfo(*lTakeInfo);
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

    FbxString lCurrentTakeName = IOS_REF.GetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString(""));
    if( lScene->GetTakeInfo(lCurrentTakeName.Buffer()) )
        lScene->ActiveAnimStackName = lCurrentTakeName;

    return lResult;
}

// collect the animation/defaults from a PrivateData/Layer
void CollectAnimFromCurveNode(void **lSrc, void* pFcn, unsigned int nbCrvs, FbxAnimCurveNode *cn, FbxMultiMap* pNickToAnimCurveTimeWarpsSet, FbxMultiMap& pNickToKFCurveNodeWarpSet)
{
    FbxAnimUtilities::CurveNodeIntfce *fcn = (FbxAnimUtilities::CurveNodeIntfce*)pFcn;

    //Here The Channels and the Curves are not synchronous.
    //So we have to choose the minimum count of them.
    unsigned int lMinNumCrvs =fcn->GetCount();
    if( nbCrvs == 1 || nbCrvs < lMinNumCrvs)
    {
        lMinNumCrvs = nbCrvs;
    }
    // Get the Curve(s)
    if( lMinNumCrvs == 1 )
    {
        lSrc[0] = fcn->GetCurveHandle();
    }
    else
    {
        FBX_ASSERT(fcn->GetCount() == nbCrvs);
        for( unsigned int i = 0; i < lMinNumCrvs; i++ )
        {
            lSrc[i] = fcn->GetCurveHandle(i);
        }
    }

	// Look if we have a timewarp curve affecting the node
	FbxAnimCurve* lTimeWarp = NULL;
	FbxAnimUtilities::CurveNodeIntfce lTW = fcn->GetTimeWarp();
	if (lTW.IsValid())
	{
		// find the NickNumber of the KFCurveNode
        bool invalidNickNumber = true;
		FbxHandle lNickNumber = 0;
		for (int i = 0; i < pNickToKFCurveNodeWarpSet.GetCount(); i++)
		{
			FbxAnimUtilities::CurveNodeIntfce lTmp((void*)pNickToKFCurveNodeWarpSet.GetFromIndex(i, &lNickNumber));
			if (lTmp == lTW)
            {
                invalidNickNumber = false;
				break;
            }
		}

		if (pNickToAnimCurveTimeWarpsSet && invalidNickNumber == false)
		{
			lTimeWarp = (FbxAnimCurve*)pNickToAnimCurveTimeWarpsSet->Get(lNickNumber);
			cn->ConnectSrcObject(lTimeWarp);
		}
	}	

    unsigned int c;
    for (c = 0; c < lMinNumCrvs; c++)
    {
        // It must exist!
        FBX_ASSERT(lSrc[c]);

        FbxAnimUtilities::CurveIntfce lCurve(lSrc[c]);
        // Set default value on channel
        cn->SetChannelValue<float>(c, lCurve.GetValue());

		// Get lAnimCurve on AnimCurveNode's channel c (create as needed)
        bool lGotCreated = false;
		FbxAnimCurve* lAnimCurve = cn->GetCurve(c, 0);
		if (lCurve.KeyGetCount() && lAnimCurve == NULL)
		{
			lAnimCurve = cn->CreateCurve(cn->GetName(), c);
            lGotCreated = true;
		}

		if (!lAnimCurve)
		{
			continue;
		}

		// If lAnimCurve is implemented as FbxAnimCurveKFCurve, set its KFCurve.
		FbxAnimUtilities::CurveIntfce lAnimCurveFCurve(lAnimCurve);
		if (lAnimCurveFCurve.IsValid()) 
		{
            // let lAnimCurvKFCurve become the owner of lSrc[c]
			lAnimCurveFCurve.SetCurveHandle(lSrc[c]);

            // and we need to reset the FCurve from the fcn/child/layer FCurveNode
            // systematically to avoid deletion of already deleted objects (anyway,
            // when we exit from this function we call the ResetKFCurveNode() so there
            // is no danger of memory leaks).
            if( lMinNumCrvs == 1 )
            {
                fcn->SetCurveHandle(NULL);
            }
            else
            {
                fcn->SetCurveHandle(NULL, c);
            }
    
			// Update AnimCurveNode's "owner" flag.
            cn->ReleaseOwnershipOfKFCurve(c);

            // Now, if the lAnimCurve was already existing, we need to visit all the
            // FbxAnimCurveNode it is connected to and update the "owner" flag there also.
            // If the AnimCurve got created here, then the only AnimCurveNode using it is cn 
            // and we already uptdated the ownership flag.
            if (lGotCreated == false)
            {
			    // Find all AnimCurveNodes connected to this curve.
			    int lNbDstProp = lAnimCurve->GetDstPropertyCount();
			    for (int lP = 0; lP < lNbDstProp; lP++)
			    {
				    FbxProperty lProp = lAnimCurve->GetDstProperty(lP);
				    if (lProp.IsValid())
				    {
					    FbxAnimCurveNode* lCurveNodeUpdate = FbxCast<FbxAnimCurveNode>(lProp.GetFbxObject());
					    if (lCurveNodeUpdate && lCurveNodeUpdate != cn)
					    {
						    // Go through all curve node's channels to find which ones are connected to lAnimCurve.
						    for (unsigned int c2 = 0; c2 < lCurveNodeUpdate->GetChannelsCount(); c2++)
						    {
							    FbxAnimCurve* lAnimCurve2 = lCurveNodeUpdate->GetCurve(c2, 0);
							    if (lAnimCurve2 == lAnimCurve)
							    {
								    lCurveNodeUpdate->ReleaseOwnershipOfKFCurve(c2);
							    }
						    }
					    }
				    }
			    }
		    }
        }

		// Set Pre- and Post- Extrapolation
    	lAnimCurve->SetPreExtrapolation((FbxAnimCurve::EExtrapolationType)lCurve.GetPreExtrapolation());
    	lAnimCurve->SetPreExtrapolationCount(lCurve.GetPreExtrapolationCount());
    	lAnimCurve->SetPostExtrapolation((FbxAnimCurve::EExtrapolationType)lCurve.GetPostExtrapolation());
		lAnimCurve->SetPostExtrapolationCount(lCurve.GetPostExtrapolationCount());
    }
}


void FbxReaderFbx6::ReadPropertyAnimation(FbxIO& pFileObject, FbxProperty* pProp, FbxAnimStack& pAnimStack)
{
    FBX_ASSERT(pProp != NULL);

    // Get the base layer on the stack. Because this reader created the stack,
    // we are guaranteed that the first FbxAnimLayer that is connected to it
    // is the base layer we are looking for.
    FbxAnimLayer* lAnimLayer = pAnimStack.GetMember<FbxAnimLayer>();
    FBX_ASSERT(lAnimLayer != NULL);
    FBX_ASSERT(mAnimLayer == lAnimLayer);

    FbxAnimCurveNode   *cn = pProp->GetCurveNode(lAnimLayer, true);
    void              **lSrc = NULL;

    if( cn && cn->GetChannelsCount() > 0)
    {
        unsigned int nbCrvs = cn->GetChannelsCount();
        lSrc = FbxNewArray<void*>(nbCrvs);
        FBX_ASSERT(lSrc != NULL);
        if (lSrc == NULL)
            // out of memory!
            return;

        FbxAnimUtilities::CurveNodeIntfce fcn = FbxAnimUtilities::GrabCurveNode(cn);
        if (!fcn.IsValid())
            return;
            
        for (unsigned int i = 0; i < nbCrvs; i++)
            lSrc[i] = NULL;

        FbxAnimUtilities::RestrieveCurveNode(fcn, &pFileObject);

        // first, collect anim on the base
        CollectAnimFromCurveNode(lSrc, &fcn, nbCrvs, cn, mNickToAnimCurveTimeWarpsSet, mNickToKFCurveNodeTimeWarpsSet);

        // and now, process extra layers (if any)
        int              layer = 1;
        FbxString       layerName("Layer");
        FbxAnimUtilities::CurveNodeIntfce lLayer = fcn.GetLayer(layer);
        while (lLayer.IsValid())
        {
            // Find the correct layer (or create a new one when required)
            lAnimLayer = pAnimStack.GetMember<FbxAnimLayer>(layer);
            if (!lAnimLayer)
            {
                // Get some required objects
                FbxObject* lObj = pProp->GetFbxObject(); FBX_ASSERT(lObj != NULL);
                FbxScene* lScene = lObj->GetScene();     FBX_ASSERT(lScene != NULL);
                FbxString lName = layerName+layer;
                lAnimLayer = FbxAnimLayer::Create(lScene, lName);
                FBX_ASSERT(lAnimLayer != NULL);
                pAnimStack.AddMember(lAnimLayer);
            }

            // TODO: Need to fill the layer blend/rotation/scale modes

            // create the CurveNode that will hold the lLayer data on this layer
            FbxAnimCurveNode* n = pProp->GetCurveNode(lAnimLayer, true);
            CollectAnimFromCurveNode(lSrc, &lLayer, nbCrvs, n, mNickToAnimCurveTimeWarpsSet, mNickToKFCurveNodeTimeWarpsSet);

            layer++;
            FbxAnimUtilities::CurveNodeIntfce lLayer1 = fcn.GetLayer(layer);
			lLayer = lLayer1;
        }

        FbxAnimUtilities::ReleaseCurveNode(cn);

        FbxDeleteArray(lSrc);
    } //if cn
}

//
// Read object animation from file object
//
void FbxReaderFbx6::ReadObjectAnimation(FbxIO& pFileObject, FbxObject* pNode, FbxAnimStack& pAnimStack, int pExceptionFlag)
{
    FBX_ASSERT(pNode != NULL);
    // at this point we have just read the block beginning of the given node

    while( pFileObject.FieldReadBegin("Channel") )
    {
        FbxString Name = pFileObject.FieldReadC();

        if( Name == FBXSDK_CURVENODE_TRANSFORM )
        {
            if (pFileObject.FieldReadBlockBegin())
			{
				ReadObjectAnimation(pFileObject, pNode, pAnimStack, 1);
				pFileObject.FieldReadBlockEnd();
			}
        }
        else
        {
	        FbxString propName = Name;
            if( pExceptionFlag == 1 )
            {
                     if( Name == FBXSDK_CURVENODE_TRANSLATION ) {propName = "Lcl Translation"; }
                else if( Name == FBXSDK_CURVENODE_ROTATION ) {propName = "Lcl Rotation"; }
                else if( Name == FBXSDK_CURVENODE_SCALING ) {propName = "Lcl Scaling"; }
            }

            // Find the corresponding property
            pNode->RootProperty.BeginCreateOrFindProperty();
            FbxProperty prop = pNode->FindProperty(propName);
            if( !prop.IsValid() )
            {
                // maybe the propName refer to a property in the NodeAttribute ?
                if( pNode->Is<FbxNode>() )
                {
                    FbxNode* n = FbxCast<FbxNode>(pNode);
                    FbxObject* na = n->GetNodeAttribute();
                    if( na ) prop = na->FindProperty(propName);
                }
            }
            pNode->RootProperty.EndCreateOrFindProperty();
            ReadPropertyAnimation(pFileObject, &prop, pAnimStack);
        }
        // end of field
        pFileObject.FieldReadEnd();
    } // while    
}

//
// Read take animation from fbx scene
//
bool FbxReaderFbx6::ReadTakeAnimation(FbxScene& pScene, FbxTakeInfo* pTakeInfo)
{
    bool lResult = false;

    // Hopefully, the reader has already created the AnimStack (and AnimLayers connected to it)
    FbxAnimStack* lAnimStack = pScene.FindSrcObject<FbxAnimStack>(pTakeInfo->mName);
    if (!lAnimStack)
    {
        // But in case it did not, let's create the FbxAnimStack from the pTakeInfo
        lAnimStack = FbxAnimStack::Create(&pScene, pTakeInfo->mName);
    }

    FBX_ASSERT(lAnimStack != NULL);
    if (!lAnimStack)
        // This is a fatal error!
        return false;

    // Intialize the anim stack with the information stored in the pTakeInfo
    lAnimStack->Reset(pTakeInfo);

    // Again, if the AnimStack was already created, it will have AnimLayers connected to it (they have been proceseed
    // during the ReadObject pass)
    mAnimLayer = lAnimStack->GetMember<FbxAnimLayer>(); // base layer

    if (!mAnimLayer)
    {
        // but in case no Layers are connected/defined, we create one
        // This layer must always exist since the rest of the reader 
        //will use it to store the animation nodes.
        mAnimLayer = FbxAnimLayer::Create(&pScene, "Layer0");
        lAnimStack->AddMember(mAnimLayer);
    }

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

        if (lTakeFbxObject.ProjectOpenDirect(lFullFileName.Buffer(), this))
        {
            lResult = ReadNodeAnimation(lTakeFbxObject, pScene, *lAnimStack, pTakeInfo);
            lTakeFbxObject.ProjectClose ();
        }
        else
        {
            if (mFileObject->IsEmbedded())
            {
				// We want to clear the error if it was triggered by the embedded Take_001.tak
				// file. In the ProjectOpenDirect(), this would be raised since this particular
				// take is a dummy object and does not exist on disk.
				FbxString falsePositive = GetStatus().GetErrorString();
				if (GetStatus().GetCode() == FbxStatus::eFailure && falsePositive.Find("Unable to open file")>=0)
				{
					if (falsePositive.Find(".tak") == falsePositive.GetLen()-4)
						GetStatus().Clear();
				}
                lResult = ReadNodeAnimation(*mFileObject, pScene, *lAnimStack, pTakeInfo);
            }
            else
            {
                lResult = false;
            }
        }
    }
    else
    {
        lResult = ReadNodeAnimation(*mFileObject, pScene, *lAnimStack, pTakeInfo);
    }

    if (lResult)
    {
        lResult = TimeShiftNodeAnimation(pScene, *lAnimStack, pTakeInfo->mImportOffsetType, pTakeInfo->mImportOffset);
    }

	// if we already found that the file is corrupted, return this error code
	if (lStatus.GetCode() == FbxStatus::eInvalidFile)
		GetStatus() = lStatus;

    return lResult;
}

//
// Read node animation
//
bool FbxReaderFbx6::ReadNodeAnimation(FbxIO& pFileObject, FbxScene& pScene, FbxAnimStack& pAnimStack, FbxTakeInfo* pTakeInfo)
{
    const char* lSupportedTypes[] = {"Model", "Shader", "Renderer", "Texture", "Material", "Constraint",
        "Video", "Device", "Instrument", "Set", "Story Take", "GenericObject", NULL};

    //constraint,texture, material and GenericNode are directly inherited from FbxObject
    FbxObject* lTakeNodeCter;

    //model is FbxNode
    FbxNode*    lNode;    
    FbxString      lName;    
    
    //keyword of searching texture or material or constraint or GenericNode animation
    const char* lConstraintTitle = FIELD_CONSTRAINT "::";
    const char* lGenericNodeTitle = FIELD_KFBXGENERICNODE_GENERICNODE "::";
    const char* lTextureTitle = FIELD_KFBXTEXTURE_TEXTURE "::";
    const char* lMaterialTitle = FIELD_KFBXMATERIAL_MATERIAL "::";

    ReadLayers(pFileObject, pTakeInfo);
    mNickToAnimCurveTimeWarpsSet = pScene.AddTakeTimeWarpSet(pTakeInfo->mImportName.Buffer());
	mNickToKFCurveNodeTimeWarpsSet.Clear(); 
    ReadTimeWarps(pFileObject, *mNickToAnimCurveTimeWarpsSet, pScene);
    FbxAnimUtilities::SetTimeWarpSet(&mNickToKFCurveNodeTimeWarpsSet);

    if (pScene.GetSceneInfo())
    {
        if (pScene.GetSceneInfo()->GetSceneThumbnail() == NULL)
        {
            FbxThumbnail* lThumbnail = ReadThumbnail();
            pScene.GetSceneInfo()->SetSceneThumbnail(lThumbnail);
         }
    }

    //Load all instance
    bool    lFound = true;
    int     lIter;

    while( lFound )
    {
        lIter = 0;
        lFound = false;

        //Find type from supported hardcoded list (old fbx compatibility)
        while( lSupportedTypes[lIter] )
        {
            if( pFileObject.FieldReadBegin(lSupportedTypes[lIter++]) )
            {
                lFound = true;
                break;
            }
        }

        //Check for additional types based on registered classes
        if( !lFound )
        {
            FbxClassId lClassId = mManager.GetNextFbxClass(FbxClassId());
            while( lClassId.IsValid() )
            {
                if( strlen(lClassId.GetFbxFileTypeName()) > 0 && pFileObject.FieldReadBegin(lClassId.GetFbxFileTypeName()) )
                {
                    lFound = true;
                    break;
                }
                lClassId = mManager.GetNextFbxClass(lClassId);
            }
        }

        if( !lFound ) break;    //Didn't find type? exit while loop

        //Read anim
        lTakeNodeCter = NULL;
        lNode = NULL;
        lName = pFileObject.FieldReadC();

        //check if there are texture or material or constraint or GenericNode animation
        //not find, return -1; find, return 0 or bigger        
        int lNumConstraintAnimation = lName.Find(lConstraintTitle, 0);
        int lNumGenericNodeAnimation = lName.Find(lGenericNodeTitle, 0);
        int lNumTextureAnimation = lName.Find(lTextureTitle, 0);
        int lNumMaterialAnimation = lName.Find(lMaterialTitle, 0);

		FbxString camSwitcherName = FbxString("Model::") + FBXSDK_CAMERA_SWITCHER;
        if( lName.Compare(FBXSDK_CAMERA_SWITCHER) == 0 || (lName == camSwitcherName) )
        {
            if( pScene.GlobalCameraSettings().GetCameraSwitcher() )
            {
                lNode = pScene.GlobalCameraSettings().GetCameraSwitcher()->GetNode();
            }
        }

        if( lNumConstraintAnimation >= 0 || lNumGenericNodeAnimation >= 0 || lNumTextureAnimation >= 0 || lNumMaterialAnimation >= 0 )
        {
            //constraint,texture, material and GenericNode are directly inherited from FbxObject
            lTakeNodeCter = mObjectMap.Get(mObjectMap.Find(lName.Buffer()));
        }    
        else
        {
            //model is FbxNode
            lNode = FbxCast<FbxNode>(mObjectMap.Get(mObjectMap.Find(lName.Buffer())));
        }

        if( lNode )
        {
            if( pFileObject.FieldReadBlockBegin() )
            {                
                ReadObjectAnimation(pFileObject, lNode, pAnimStack, 0);

				if(IOS_REF.GetBoolProp(IMP_FBX_MERGE_LAYER_AND_TIMEWARP, false))
                {
                    FbxAnimUtilities::MergeLayerAndTimeWarp(lNode, mAnimLayer);
                }
                pFileObject.FieldReadBlockEnd();
            }
        }

       
        //read animation of constraint, texture, material and GenericNode
        if( lTakeNodeCter )
        {
            if( pFileObject.FieldReadBlockBegin() )
            {
                ReadObjectAnimation(pFileObject, lTakeNodeCter, pAnimStack, 0);

				if(IOS_REF.GetBoolProp(IMP_FBX_MERGE_LAYER_AND_TIMEWARP, false))
                {
                    FbxAnimUtilities::MergeLayerAndTimeWarp(lTakeNodeCter, mAnimLayer);
                }
                pFileObject.FieldReadBlockEnd();
            }
        }
        pFileObject.FieldReadEnd();
    }
    FbxAnimUtilities::SetTimeWarpSet(NULL);

	// Destroy the KFCurveNode objects stored in the mTimeWarps. We already retrieved their
	// FCurves and put them in AnimCurves objects
	for (int i = 0; i < mNickToKFCurveNodeTimeWarpsSet.GetCount(); i++)
	{
        void* lData = (void*)mNickToKFCurveNodeTimeWarpsSet.GetFromIndex(i);
        FbxAnimUtilities::CurveNodeIntfce lCN(lData);
        FbxAnimUtilities::DestroyCurveNode(lCN);
	}
    return true;
}

//
// Read layers from files
//
void FbxReaderFbx6::ReadLayers(FbxIO& pFileObject, FbxTakeInfo* pTakeInfo)
{
    if( pFileObject.FieldReadBegin("LayerNames") )
    {
        if (pFileObject.FieldReadBlockBegin())
		{
			pFileObject.FieldReadI("Version");
			FbxTakeLayerInfo* lLayerInfo;
			while( pFileObject.FieldReadBegin("LN") )
			{
				lLayerInfo = FbxNew< FbxTakeLayerInfo >();
				lLayerInfo->mId = pFileObject.FieldReadI();
				lLayerInfo->mName = pFileObject.FieldReadS();
				pTakeInfo->mLayerInfoList.Add(lLayerInfo);
				pFileObject.FieldReadEnd();
			}
			pTakeInfo->mCurrentLayer = pFileObject.FieldReadI("CurrentLayer", -1);
			pFileObject.FieldReadBlockEnd();
		}
        pFileObject.FieldReadEnd ();
    }
}

//
// Read timewarps
//
// do not make it static since it is declared as friend in the FbxAnimCurveKFCurve class
void TransferTimeWarp(int lNickNumber, FbxAnimUtilities::CurveNodeIntfce& lTimeWarp, FbxMultiMap& mNickToKFCurveNodeTimeWarpsSet, 
							 FbxMultiMap& pTimeWarpSet, FbxScene& pScene)
{
	mNickToKFCurveNodeTimeWarpsSet.Add(lNickNumber, (FbxHandle) lTimeWarp.GetHandle());
	
	// Now, let's simply move the FCurve to an AnimCurve
	FbxAnimCurve* lTimeWarpFCurve = FbxAnimCurve::Create(&pScene, lTimeWarp.GetTimeWarpName());
	pTimeWarpSet.Add(lNickNumber, (FbxHandle)lTimeWarpFCurve);

    FbxAnimUtilities::CurveIntfce lTWCurve(lTimeWarpFCurve);
    if (lTWCurve.IsValid())
    {
	    lTWCurve.SetCurveHandle(lTimeWarp.GetCurveHandle());
	    lTimeWarp.SetCurveHandle(NULL);
    }
}

void FbxReaderFbx6::ReadTimeWarps(FbxIO& pFileObject, FbxMultiMap& pTimeWarpSet, FbxScene& pScene)
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
						TransferTimeWarp(lNickNumber, lTimeWarp, mNickToKFCurveNodeTimeWarpsSet, pTimeWarpSet, pScene);
                    }

                    pFileObject.FieldReadBlockEnd();
                }
                pFileObject.FieldReadEnd();
            }
        }
		pFileObject.FieldReadBlockEnd();
        pFileObject.FieldReadEnd();
    }
}


//
// Set time shift for node animation from take information
//
bool FbxReaderFbx6::TimeShiftNodeAnimation(FbxScene& pScene, FbxAnimStack& pAnimStack, int pTimeOffsetType, FbxTime pTimeOffset)
{
    FbxTime lTimeShift;
    int i;
    int lCount = pScene.GetMemberCount<FbxNode>();

    if ((FbxTakeInfo::EImportOffsetType)pTimeOffsetType == FbxTakeInfo::eRelative)
    {
        lTimeShift = pTimeOffset;
    }
    else
    {
        FbxTimeSpan lAnimationInterval(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
        for (i = 0; i < lCount; i++)
            pScene.GetMember<FbxNode>(i)->GetAnimationInterval(lAnimationInterval, &pAnimStack);

        lTimeShift = pTimeOffset - lAnimationInterval.GetStart();
    }

    if (lTimeShift != FBXSDK_TIME_ZERO)
    {
        FbxAnimCurveFilterTSS lOffsetFilter;
        FbxTime lStart(FBXSDK_TIME_MINUS_INFINITE);
        FbxTime lStop(FBXSDK_TIME_INFINITE);

        lOffsetFilter.SetStartTime(lStart);
        lOffsetFilter.SetStopTime(lStop);
        lOffsetFilter.SetShift(lTimeShift);

        FbxArray<FbxAnimCurve*> lCurves;
        for (i = 0; i < lCount; i++)
        {
            FbxNode* lNode = pScene.GetMember<FbxNode>(i);

            GetAllAnimCurves(lNode, &pAnimStack, lCurves);
            if (lCurves.GetCount() > 0)
            {
                FbxAnimCurve** curves = lCurves.GetArray();
                lOffsetFilter.Apply(curves, lCurves.GetCount());
                lCurves.Clear();
            }
        }

        FbxTimeSpan lTimeSpan = pAnimStack.GetLocalTimeSpan();
        lTimeSpan.SetStart(lTimeSpan.GetStart()+lTimeShift);
        lTimeSpan.SetStop(lTimeSpan.GetStop()+lTimeShift);
        pAnimStack.SetLocalTimeSpan(lTimeSpan);
    }
    return true;
}


//
// Rebuild trim regions
//
void FbxReaderFbx6::RebuildTrimRegions(FbxScene& pScene) const
{
    int i, lCount = pScene.GetSrcObjectCount<FbxTrimNurbsSurface>();
    FbxTrimNurbsSurface* lSurf = NULL;
    for( i = 0; i < lCount; ++i )
    {
        lSurf = pScene.GetSrcObject<FbxTrimNurbsSurface>(i);
        if( lSurf )
            lSurf->RebuildRegions();
    }
}

//
// Rebuild subdivision
//
void FbxReaderFbx6::SetSubdivision(FbxScene& pScene) const
{
    int i, lCount = pScene.GetSrcObjectCount<FbxSubDiv>();
    FbxSubDiv* lSubdiv = NULL;
    for( i = 0; i < lCount; ++i )
    {
        lSubdiv = pScene.GetSrcObject<FbxSubDiv>(i);
        if( lSubdiv )
        {
            //set all mesh level for subdivision
            //TODO:: only set base and finest meshes now, need to set all level meshes later
            FbxNode* lSubdivNode = lSubdiv->GetNode();
            lSubdiv->SetBaseMesh((FbxMesh*)lSubdivNode->GetNodeAttributeByIndex(1));

			//we only export base/finest mesh for now
			if(lSubdivNode->GetNodeAttributeCount() >= 3)
				lSubdiv->SetFinestMesh((FbxMesh*)lSubdivNode->GetNodeAttributeByIndex(lSubdivNode->GetNodeAttributeCount()-1));
        }
    }
}

//
// Convert the shape deform property to the DeformPercent property of FbxBlendShapeChannel.
//
void FbxReaderFbx6::ConvertShapeDeformProperty(FbxScene& pScene) const
{
	int lGeometryCount = pScene.GetSrcObjectCount<FbxGeometry>();
	for(int i = 0; i<lGeometryCount;++i)
	{
		FbxGeometry* lGeometry = pScene.GetSrcObject<FbxGeometry>(i);

		int lBlendShapeDeformerCount = lGeometry->GetDeformerCount(FbxDeformer::eBlendShape);
		for(int lBlendShapeIndex = 0; lBlendShapeIndex<lBlendShapeDeformerCount; ++lBlendShapeIndex)
		{
			FbxBlendShape* lBlendShape = (FbxBlendShape*)lGeometry->GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);

			int lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();
			for(int lChannelIndex = 0; lChannelIndex<lBlendShapeChannelCount; ++lChannelIndex)
			{
				FbxBlendShapeChannel* lBlendShapeChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);

				//For FBX file before 7200, there is one shape on each blend shape channel at most.
				FbxShape* lShape = lBlendShapeChannel->GetTargetShape(0);
				if( lShape )
				{
						FbxString lShapeName = lShape->GetName();
						FbxProperty lProp = lGeometry->FindProperty(lShapeName);
						if(!lProp.IsValid())
						{
							// For some 6.0 version FBX file, the shape property might be on the node itself.
							// So if we can not get it from node attribute, try to get it from the node again.
							FbxNode* lNode = lGeometry->GetNode();
							if(lNode)
							{
								lProp = lNode->FindProperty(lShapeName); 
							}
						}
						if (lProp.IsValid())
						{
							lBlendShapeChannel->DeformPercent.CopyValue(lProp);

                            // loop through all the stacks & layers so we copy everything.
                            int nbStacks = pScene.GetMemberCount<FbxAnimStack>();
                            for (int j = 0; j < nbStacks; j++)
                            {
							    FbxAnimStack* lAnimStack = pScene.GetMember<FbxAnimStack>(j);
                                if (lAnimStack)
                                {
                                    int nbLayers = lAnimStack->GetMemberCount<FbxAnimLayer>();
                                    for (int l = 0; l < nbLayers; l++)
                                    {
                                        FbxAnimLayer* lAnimLayer = lAnimStack->GetMember<FbxAnimLayer>(l);
                                        if(lAnimLayer)
                                        {
											FbxAnimCurveNode* lShapeCNNew = NULL;
                                            FbxAnimCurveNode* lShapeCN = lProp.GetCurveNode(lAnimLayer);
                                            if (lShapeCN)
                                            {
                                                lShapeCNNew = lBlendShapeChannel->DeformPercent.GetCurveNode(lAnimLayer, true);
                                                if (lShapeCNNew)
                                                {
                                                    FBX_ASSERT(lShapeCN->GetChannelsCount() == lShapeCNNew->GetChannelsCount());
                                                    unsigned int nbc = (unsigned int)lShapeCN->GetChannelsCount();
                                                    for (unsigned int c = 0; c < nbc; c++)
													{
                                                        lShapeCNNew->SetChannelValue<float>(c, lShapeCN->GetChannelValue<float>(c, 0.0));
														while (lShapeCN->GetCurveCount(c))
														{
															FbxAnimCurve* lShapeCurve = lShapeCN->GetCurve(c);
															if (lShapeCurve)
															{
																lShapeCN->DisconnectFromChannel(lShapeCurve, c);
																lShapeCNNew->ConnectToChannel(lShapeCurve,c);
															}
														}
													}
													lShapeCN->Destroy();
                                                }
                                            }
                                        }// if animLayer
                                    }
							    }// if animStack
                            }
							// destroy the property. It is not required in memory (and if we keep it when we save to disk, we can break the import
							// into MoBu
							lProp.Destroy();
						}// If shape deform property exist
				}// If lShape is valid
			}//For each blend shape channel
		}//For each blend shape deformer
	}//For each geometry
}

//
// Rebuild layered texture alphas from sub texture connections
//
void FbxReaderFbx6::RebuildLayeredTextureAlphas(FbxScene& pScene) const
{
    FbxIteratorSrc<FbxLayeredTexture> lLayeredTexIter(&pScene);
    FbxLayeredTexture* lLayeredTex;
    FbxForEach (lLayeredTexIter, lLayeredTex)
    {
		for (int i = 0; i < lLayeredTex->GetSrcObjectCount<FbxTexture>(); i++)
        {
            FbxTexture* lSubTex = lLayeredTex->GetSrcObject<FbxTexture>(i);
            lLayeredTex->SetTextureAlpha(i, lSubTex->Alpha.Get());
        }  // for each sub texture
    }  // for each layered texture
}

#include <fbxsdk/fbxsdk_nsend.h>

