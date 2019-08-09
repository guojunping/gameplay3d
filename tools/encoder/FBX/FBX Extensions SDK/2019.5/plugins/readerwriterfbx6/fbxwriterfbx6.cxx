/****************************************************************************************
 
   Copyright (C) 2016 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/scene/fbxgroupname.h>
#include <fbxsdk/scene/animation/fbxanimutilities.h>
#include <fbxsdk/fileio/fbxglobalsettings.h>
#include <fbxsdk/fileio/fbxprogress.h>
#include <fbxsdk/fileio/fbx/fbxwriterfbx6.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

static const char* sENHANCED_PROPERITIES = "Properties70";
static const char* sLEGACY_PROPERTIES    = "Properties60";

static FbxArray<FbxLight*> gAreaLights;
//--------------------------------------
class Fbx6TypeDefinitionInfo
{
public:
    // Construction
    Fbx6TypeDefinitionInfo():mSupportsGenericWrite(true){}

    // ---- NOTE --------------------------------------------------
    // To support backward compatibility, some object types have to
    // be written with the old saving mechanism.   So this flag
    // is set to true by default and must be set to false
    // if the object type CAN'T be saved with the latest generic writer
    inline bool IsGenericWriteEnable()const {return mSupportsGenericWrite;}
    inline void SetGenericWriteEnable(bool pEnable){mSupportsGenericWrite = pEnable;}

	FbxString	mName;
	FbxClassId mClassId;
    int			mCount;    
    bool		mStore;
private:
    bool		mSupportsGenericWrite;
    // ------------------------------------------------------------
};

//--------------------------------------
class Fbx6TypeDefinition
{
public:
    Fbx6TypeDefinition();
    virtual ~Fbx6TypeDefinition();

    bool	AddObject(FbxObject* pObject);
    bool	AddObject(const char* pTypeName, FbxClassId pClassId, bool pStore);    
    int		GetDefinitionCount()const;
    void	MoveDefinition(const char* pDefName, int pNewIndex);
    int		GetObjectCount()const;

	Fbx6TypeDefinitionInfo* GetDefinitionFromName(const char* pTypeName) const;
    Fbx6TypeDefinitionInfo* GetDefinition(int pIndex)const;

private:
    FbxArray<Fbx6TypeDefinitionInfo*> mList;
    int		mObjectCount;
};

Fbx6TypeDefinition::Fbx6TypeDefinition() :
        mObjectCount(0)
{
}

Fbx6TypeDefinition::~Fbx6TypeDefinition()
{
    FbxArrayDelete(mList);
    mObjectCount = 0;
}

bool Fbx6TypeDefinition::AddObject(FbxObject* pObject)
{
    FbxClassId lClassId = pObject->GetRuntimeClassId();
    const char*	lTypeName = lClassId.GetFbxFileTypeName(true);

    return AddObject(lTypeName, lClassId, pObject->IsRuntimePlug());
}

bool Fbx6TypeDefinition::AddObject(const char* pTypeName, FbxClassId pClassId, bool pStore)
{
    if( strlen(pTypeName) <= 0 ) return false;

    Fbx6TypeDefinitionInfo* lDef = GetDefinitionFromName(pTypeName);
    if( lDef )
	{
		lDef->mCount++;
	}
    else
    {
        lDef = FbxNew< Fbx6TypeDefinitionInfo >();
        lDef->mName = pTypeName;
        lDef->mCount = 1;
        lDef->mClassId = pClassId;
        lDef->mStore = pStore;
        mList.Add(lDef);
    }

    mObjectCount++;
    return true;
}

Fbx6TypeDefinitionInfo* Fbx6TypeDefinition::GetDefinition(int pIndex) const
{
    return mList[pIndex];
}

int Fbx6TypeDefinition::GetDefinitionCount()const
{
    return mList.GetCount();
}

void Fbx6TypeDefinition::MoveDefinition(const char* pDefName, int pNewIndex)
{
    int lIter, lCount = mList.GetCount();
    for( lIter = 0; lIter < lCount; lIter++ )
    {
        if( mList[lIter]->mName == pDefName )
        {
            Fbx6TypeDefinitionInfo* lDef = mList[lIter];
            mList.RemoveAt(lIter);
            if( pNewIndex >= 0 )
            {
                mList.InsertAt(pNewIndex, lDef);
                return;
            }
            else
            {
                mList.InsertAt(mList.GetCount() - pNewIndex, lDef);
                return;
            }
        }
    }
}

int Fbx6TypeDefinition::GetObjectCount() const
{
    return mObjectCount;
}

Fbx6TypeDefinitionInfo* Fbx6TypeDefinition::GetDefinitionFromName(const char* pTypeName) const
{
    int lIter, lCount = mList.GetCount();
    for( lIter = 0; lIter < lCount; lIter++ )
    {
        if( mList[lIter]->mName == pTypeName )
        {
            return mList[lIter];
        }
    }
    return NULL;
}

//
// Structure used to store temporary object reference information
//
struct KTypeWriteReferenceInfo
{
    FbxObject*	mRefObj;
    FbxString	mRefName;
};

//
// Structure used to manage internal & external (in terms of Document) object reference
//
class Fbx6TypeWriteReferences
{
public:
    Fbx6TypeWriteReferences();
    virtual ~Fbx6TypeWriteReferences();
    int		AddReference(FbxObject* pRefObj, char* pRefName);
    int		GetCount() const { return mRefInfo.GetCount(); }
    bool	GetReferenceName(FbxObject* pRefObj, FbxString& pRefName) const;
private:
    FbxArray<KTypeWriteReferenceInfo*>    mRefInfo;
};

Fbx6TypeWriteReferences::Fbx6TypeWriteReferences()
{
}

Fbx6TypeWriteReferences::~Fbx6TypeWriteReferences()
{
    FbxArrayDelete(mRefInfo);
}

int Fbx6TypeWriteReferences::AddReference(FbxObject* pRefObj, char* pRefName)
{
    KTypeWriteReferenceInfo* lRefInfo = FbxNew< KTypeWriteReferenceInfo >();
    lRefInfo->mRefObj = pRefObj;
    lRefInfo->mRefName = pRefName;
    return mRefInfo.Add(lRefInfo);
}

bool Fbx6TypeWriteReferences::GetReferenceName(FbxObject* pRefObj, FbxString& pRefName) const
{
    int i, lCount = mRefInfo.GetCount();
    for (i=0; i < lCount; i++)
    {
        const FbxObject* lInfoObj = mRefInfo[i]->mRefObj;
        if (lInfoObj == pRefObj)
        {
            pRefName = mRefInfo[i]->mRefName;
            return true;
        }
    }
    return false;
}

//
// Structure used to store temporary object information
// Locally used to store the hierarchy of embedded documents
// in a flat manner
//
struct KTypeObjectHierarchyInfo
{
    FbxObject*		mObject;
    FbxDocument*   mDocument;
    FbxString		mOriginalName;
    FbxString		mRootPathName;
};

//
// Internal structure used to store object hierarchy information
// Locally used to store the hierarchy of embedded documents
// in a flat manner
//
class Fbx6TypeObjectHierarchy
{
public:
    Fbx6TypeObjectHierarchy();
    virtual ~Fbx6TypeObjectHierarchy();

    void			AddObject(FbxObject* pObj, FbxDocument* pObjDoc, char* pObjOriginalName, char* pObjRootPathName);
    int				GetCount(void);
    FbxObject*		GetObject(int pIndex);
    FbxDocument*	GetObjectDocument(int pIndex);
    const char*		GetObjectOriginalName(int pIndex);
    const char*		GetObjectRootPathName(int pIndex);
    int				AppendHierarchy(Fbx6TypeObjectHierarchy& pHierToAppend);
    void			Clear(void);

private:
    FbxArray<KTypeObjectHierarchyInfo*> mObjectInfo;
};

Fbx6TypeObjectHierarchy::Fbx6TypeObjectHierarchy()
{
}

Fbx6TypeObjectHierarchy::~Fbx6TypeObjectHierarchy()
{
    FbxArrayDelete(mObjectInfo);
}

void Fbx6TypeObjectHierarchy::Clear(void)
{
    mObjectInfo.Clear();
}

void Fbx6TypeObjectHierarchy::AddObject(FbxObject* pObj, FbxDocument* pObjDoc, char* pObjOriginalName, char* pObjRootPathName)
{
    KTypeObjectHierarchyInfo* lInfo = FbxNew< KTypeObjectHierarchyInfo >();

    lInfo->mObject = pObj;
    lInfo->mDocument = pObjDoc;
    lInfo->mOriginalName = pObjOriginalName;
    lInfo->mRootPathName = pObjRootPathName;

    mObjectInfo.Add(lInfo);
}

int Fbx6TypeObjectHierarchy::GetCount(void)
{
    return mObjectInfo.GetCount();
}

FbxObject* Fbx6TypeObjectHierarchy::GetObject(int pIndex)
{
    return mObjectInfo[pIndex]->mObject;
}

FbxDocument* Fbx6TypeObjectHierarchy::GetObjectDocument(int pIndex)
{
    return mObjectInfo[pIndex]->mDocument;
}

const char* Fbx6TypeObjectHierarchy::GetObjectOriginalName(int pIndex)
{
    return mObjectInfo[pIndex]->mOriginalName;
}

const char* Fbx6TypeObjectHierarchy::GetObjectRootPathName(int pIndex)
{
    return mObjectInfo[pIndex]->mRootPathName;
}

int Fbx6TypeObjectHierarchy::AppendHierarchy(Fbx6TypeObjectHierarchy& pHierToAppend)
{
    mObjectInfo.AddArray(pHierToAppend.mObjectInfo);
    pHierToAppend.Clear();
    return mObjectInfo.GetCount();
}

//
// Locally used to sort objects according their referencing depth.
// The depth is the number of chained references
//
template< typename T > struct KTypeObjectReferenceDepth
{
    T*  mObject;
    int mRefDepth;
};

template< typename T > static int CompareKTypeObjectReferenceDepth(const void* E1, const void* E2)
{
    if( ((KTypeObjectReferenceDepth< T >*)E1)->mRefDepth < ((KTypeObjectReferenceDepth< T >*)E2)->mRefDepth) 
	{
        return -1;
    } 
	else if( ((KTypeObjectReferenceDepth< T >*)E1)->mRefDepth > ((KTypeObjectReferenceDepth< T >*)E2)->mRefDepth) 
	{
        return 1;
    }
    return 0;
}

// Locally used utility functions
// (does not need to be member functions, thus reducing
//  the class size)

static const char*    GetMappingModeToken(FbxLayerElement::EMappingMode);
static const char*    GetReferenceModeToken(FbxLayerElement::EReferenceMode);
static const char*    GetBlendModeToken(FbxLayerElementTexture::EBlendMode);
static void     WriteStreamProperty(FbxProperty& pStreamProperty, FbxIO* pFbxObject);

static int		ComputeReferenceDepth(FbxObject* pObject);

// *******************************************************************************
//  Constructor and file management
// *******************************************************************************
FbxWriterFbx6::FbxWriterFbx6(FbxManager& pManager, FbxExporter& pExporter, int pID, FbxStatus& pStatus) : 
    FbxWriter(pManager, pID, pStatus),
    mFileObject(NULL),
    mExporter(pExporter),
    mDocumentHierarchy(NULL),
    mDocumentReferences(NULL),
    mWriteNonDefaultPropertiesOnly(false),
    mWriteEnhancedProperties(false),
    mExportMode(eBINARY),
    mCurrentNode(NULL),
    mProgress(NULL),
    mProgressPause(true)
{
	SetIOSettings(pExporter.GetIOSettings());
}


FbxWriterFbx6::~FbxWriterFbx6()
{
    if (mFileObject)
    {
        FileClose();
    }
    
    FBX_SAFE_DELETE(mDocumentHierarchy);
    FBX_SAFE_DELETE(mDocumentReferences);
}


bool FbxWriterFbx6::FileCreate(char* pFileName)
{
    if (!mFileObject)
    {
        mFileObject = FbxIO::Create(FbxIO::BinaryNormal, GetStatus());
        mFileObject->CacheSize(IOS_REF.GetIntProp(EXP_CACHE_SIZE, 8));
    }

    FbxIOFileHeaderInfo* lFileHeaderInfo = mExporter.GetFileHeaderInfo();

    lFileHeaderInfo->mCreationTimeStampPresent = false;
    lFileHeaderInfo->mFileVersion = FBX_FILE_VERSION_6100;

    if (!mFileObject->ProjectCreate(FbxPathUtils::Bind(FbxGetCurrentWorkPath(), pFileName), this,
									mExportMode == eBINARY || mExportMode == eENCRYPTED, mExportMode == eENCRYPTED, lFileHeaderInfo))
    {
        return false;
    }
    return true;
}

bool FbxWriterFbx6::FileCreate(FbxStream* pStream, void* pStreamData)
{
    if (!mFileObject)
    {
        mFileObject = FbxIO::Create(FbxIO::BinaryNormal, GetStatus());
        mFileObject->CacheSize(IOS_REF.GetIntProp(EXP_CACHE_SIZE, 8));
    }

    FbxIOFileHeaderInfo* lFileHeaderInfo = mExporter.GetFileHeaderInfo();

    lFileHeaderInfo->mCreationTimeStampPresent = false;
    lFileHeaderInfo->mFileVersion = FBX_FILE_VERSION_6100;

    if (!mFileObject->ProjectCreate (pStream, pStreamData, this, 
                                     mExportMode == eBINARY || mExportMode == eENCRYPTED,
                                     mExportMode == eENCRYPTED, lFileHeaderInfo))
    {
        return false;
    }

    return true;
}

bool FbxWriterFbx6::FileClose()
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


bool FbxWriterFbx6::IsFileOpen()
{
    return mFileObject != NULL;
}


void FbxWriterFbx6::SetExportMode(EExportMode pMode)
{
    mExportMode = pMode;
}


void FbxWriterFbx6::GetWriteOptions()
{
   // GetWriteOptions(NULL);
}


// *******************************************************************************
//  Writing General functions
// *******************************************************************************
bool FbxWriterFbx6::Write(FbxDocument* pDocument)
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
	char* lCurrent_Locale_LCNUMERIC = setlocale(LC_NUMERIC, "C");  // set locale using period as decimal separator

    mScene = FbxCast<FbxScene>(pDocument);
    
    bool lIsAScene = (mScene != NULL);

    // keep full properties listings
    if(mFileVersion.IsEmpty())
    {
        mFileVersion = FbxString(FBX_2010_00_COMPATIBLE);
    }

    mWriteNonDefaultPropertiesOnly = (mFileVersion.Compare(FBX_2009_00_V7_COMPATIBLE) == 0);
    mWriteEnhancedProperties       =  mWriteNonDefaultPropertiesOnly;

    // Note: This should go into PreprocessScene,
    // but it really needs to be updated to PreprocessDocument
    // since we could be saving a library
#ifndef FBXSDK_ENV_WINSTORE
    FbxEventPreExport lPreEvent( pDocument );
    pDocument->GetFbxManager()->EmitPluginsEvent(lPreEvent);
#endif

    if (lIsAScene)
    {
        PreprocessScene(*mScene);
#ifndef FBXSDK_ENV_WINSTORE
        PluginsWriteBegin(*mScene);
#endif
    }

	bool lResult = Write(pDocument, NULL);

    if (lIsAScene)
    {
#ifndef FBXSDK_ENV_WINSTORE
        PluginsWriteEnd(*mScene);
#endif
        PostprocessScene(*mScene);
    }

#ifndef FBXSDK_ENV_WINSTORE
    // See comment for the pre export event above.
    FbxEventPostExport lPostEvent( pDocument );
    pDocument->GetFbxManager()->EmitPluginsEvent(lPostEvent);
#endif

	// set numeric locale back
	setlocale(LC_NUMERIC, lPrevious_Locale_LCNUMERIC);

    return lResult;
}

void FbxWriterFbx6::ReplaceUnsupportedProperties(FbxScene* pScene, bool pPreprocessPass, int pFormatV)
{    
	if( pPreprocessPass )
	{
		FBX_ASSERT(mModifiedProperties.GetCount() == 0);

		FbxObject* lObject = NULL;
		for( int i = 0, c = pScene->GetSrcObjectCount(); i < c; ++i )
		{
			lObject = pScene->GetSrcObject(i);

			//Check for unsupported property types
			FbxProperty lProperty = lObject->GetFirstProperty();
			while( lProperty != 0 && lProperty.IsValid() ) 
			{
				bool lDoNotSaveProperty = false;

				//Compound properties not supported before FBX 2010
				if( lProperty.GetPropertyDataType() == FbxCompoundDT )
				{
					lDoNotSaveProperty = (pFormatV < 200900);
				}

				//We added new unsupported basic types in FBX 2009 and FBX 2010
				switch( lProperty.GetPropertyDataType().GetType() )
				{
				    default:
					    break;
					case eFbxDouble2:
					case eFbxBlob:
					case eFbxDistance:
					case eFbxDateTime:
						lDoNotSaveProperty = (pFormatV < 200900);
						break;

					case eFbxChar:
					case eFbxUChar:
					case eFbxShort:
					case eFbxUShort:
					case eFbxUInt:
					case eFbxLongLong:
					case eFbxULongLong:
					case eFbxHalfFloat:
						lDoNotSaveProperty = (pFormatV < 201000);
						break;
				}

				//If property is unsupported, store it in the list and set it to not savable
				if( lDoNotSaveProperty )
				{
					StoreUnsupportedProperty(lObject, lProperty);
				}
				lProperty = lObject->GetNextProperty(lProperty);
			}

			//Visibility Inheritance was added for FBX 2012 and above
			if( lObject->Is<FbxNode>() )
			{
				FbxNode* lNode = FbxCast<FbxNode>(lObject);
				StoreUnsupportedProperty(lObject, lNode->VisibilityInheritance);
			}

			//Image sequence were added for FBX 2012 and above
			if( lObject->Is<FbxVideo>() )
			{
				FbxVideo* lVideo = FbxCast<FbxVideo>(lObject);
				StoreUnsupportedProperty(lObject, lVideo->ImageSequence);
				StoreUnsupportedProperty(lObject, lVideo->ImageSequenceOffset);
			}

			//Barn Doors were added for FBX 2012 and above
			if( lObject->Is<FbxLight>() )
			{
				FbxLight* lLight = FbxCast<FbxLight>(lObject);
				StoreUnsupportedProperty(lObject, lLight->InnerAngle);
				StoreUnsupportedProperty(lObject, lLight->OuterAngle);
				StoreUnsupportedProperty(lObject, lLight->AreaLightShape);
				StoreUnsupportedProperty(lObject, lLight->LeftBarnDoor);
				StoreUnsupportedProperty(lObject, lLight->RightBarnDoor);
				StoreUnsupportedProperty(lObject, lLight->TopBarnDoor);
				StoreUnsupportedProperty(lObject, lLight->BottomBarnDoor);
				StoreUnsupportedProperty(lObject, lLight->EnableBarnDoor);
			}
		}

        // TimeMarkers cannot be saved in the GlobalSettings block, they'll go back in the Version5
        // section. We mark the properties as notSavable...
        FbxGlobalSettings& lGSettings = pScene->GetGlobalSettings();
        lObject = &lGSettings;
        lGSettings.mCurrentTimeMarker.ModifyFlag(FbxPropertyFlags::eNotSavable, true);
        lGSettings.mTimeMarkers.ModifyFlag(FbxPropertyFlags::eNotSavable, true);
        FbxProperty prop = lGSettings.mTimeMarkers.GetChild();
        while (prop.IsValid())
        {
            prop.ModifyFlag(FbxPropertyFlags::eNotSavable, true);
            prop = prop.GetSibling();
        }
	}
	else	//Restore unsupported properties to their original state
	{
        // Restore the TimeMarker properties (we cannot really use the StoreUnsupportedProperty() stuff
        // because the children of mTimeMarkers would not be restored :-(
        FbxGlobalSettings& lGSettings = pScene->GetGlobalSettings();
        lGSettings.mCurrentTimeMarker.ModifyFlag(FbxPropertyFlags::eNotSavable, false);
        lGSettings.mTimeMarkers.ModifyFlag(FbxPropertyFlags::eNotSavable, false);
        FbxProperty prop = lGSettings.mTimeMarkers.GetChild();
        while (prop.IsValid())
        {
            prop.ModifyFlag(FbxPropertyFlags::eNotSavable, false);
            prop = prop.GetSibling();
        }

		for( int i = 0, c = mModifiedProperties.GetCount(); i < c; ++i )
		{
			ModifiedPropertyInfo* lPropInfo = mModifiedProperties.GetAt(i);
			FbxProperty lProperty = lPropInfo->mObj->FindProperty(lPropInfo->mPropName.Buffer());
			if( lProperty.IsValid() )
			{
				lProperty.ModifyFlag(FbxPropertyFlags::eNotSavable, false);
			}
			FbxDelete(lPropInfo);
		}
		mModifiedProperties.Clear();
	}
}

void FbxWriterFbx6::StoreUnsupportedProperty(FbxObject* pObject, FbxProperty& pProperty)
{
	//Store only properties that don't have the FbxPropertyFlags::eNotSavable flag
	if( !pProperty.GetFlag(FbxPropertyFlags::eNotSavable) )
	{
		ModifiedPropertyInfo* mpi = FbxNew<ModifiedPropertyInfo>();
		mpi->mObj = pObject;
		mpi->mPropName = pProperty.GetName();
		pProperty.ModifyFlag(FbxPropertyFlags::eNotSavable, true);
		mModifiedProperties.Add(mpi);
	}
}

bool FbxWriterFbx6::IsLeafRoll(const FbxString& pNameWithoutNameSpacePrefix)
{
    return
        pNameWithoutNameSpacePrefix.Find(":LeafLeftUpLegRoll1") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftLegRoll1") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightUpLegRoll1") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightLegRoll1") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftArmRoll1") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftForeArmRoll1") != -1  ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightArmRoll1") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightForeArmRoll1") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftUpLegRoll2") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftLegRoll2") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightUpLegRoll2") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightLegRoll2") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftArmRoll2") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftForeArmRoll2") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightArmRoll2") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightForeArmRoll2") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftUpLegRoll3") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftLegRoll3") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightUpLegRoll3") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightLegRoll3") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftArmRoll3") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftForeArmRoll3") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightArmRoll3") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightForeArmRoll3") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftUpLegRoll4") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftLegRoll4") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightUpLegRoll4") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightLegRoll4") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftArmRoll4") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftForeArmRoll4") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightArmRoll4") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightForeArmRoll4") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftUpLegRoll5") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftLegRoll5") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightUpLegRoll5") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightLegRoll5") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftArmRoll5") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafLeftForeArmRoll5") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightArmRoll5") != -1 ||
        pNameWithoutNameSpacePrefix.Find(":LeafRightForeArmRoll5") != -1 ;
}

static FbxArray<FbxUInt> gAnimObjectsFlags; 
bool FbxWriterFbx6::PreprocessScene(FbxScene &pScene)
{   
    // Make geometry names the same as the geometry
	ConvertShapePropertyToOldStyle(pScene);

	// Mark all the new added objects since 7200 file as system objects 
	// so they do not get saved to the file
	for( int i = 0, c = pScene.GetSrcObjectCount<FbxBlendShape>(); i < c; ++i )
	{
		pScene.GetSrcObject<FbxBlendShape>(i)->SetObjectFlags(FbxObject::eSavable, false);
	}

	for( int i = 0, c = pScene.GetSrcObjectCount<FbxBlendShapeChannel>(); i < c; ++i )
	{
		pScene.GetSrcObject<FbxBlendShapeChannel>(i)->SetObjectFlags(FbxObject::eSavable, false);
	}

	for( int i = 0, c = pScene.GetSrcObjectCount<FbxShape>(); i < c; ++i )
	{
		pScene.GetSrcObject<FbxShape>(i)->SetObjectFlags(FbxObject::eSavable, false);
	}

	for( int i = 0, c = pScene.GetSrcObjectCount<FbxLine>(); i < c; ++i )
	{
		pScene.GetSrcObject<FbxLine>(i)->SetObjectFlags(FbxObject::eSavable, false);
	}

	for( int i = 0, c = pScene.GetSrcObjectCount<FbxProceduralTexture>(); i < c; ++i )
	{
		pScene.GetSrcObject<FbxProceduralTexture>(i)->SetObjectFlags(FbxObject::eSavable, false);
	}

	// Do not write new HIK 2016.5.0 leaf roll FK nodes
    for( int i = 0, c = pScene.GetSrcObjectCount<FbxMarker>(); i < c; ++i )
    {
        FbxMarker* lMarker = pScene.GetSrcObject<FbxMarker>(i);
        if( !lMarker || lMarker->GetType() != FbxMarker::eEffectorFK )
            continue;

        // Don't write marker's nodes if they are new leaf roll nodes.
        int lLeafRollNodes = 0;
        for( int n = 0; n < lMarker->GetNodeCount(); ++n )
        {
            FbxNode* lNode = lMarker->GetNode(n);
            if( lNode && IsLeafRoll(lNode->GetNameWithoutNameSpacePrefix()) )
            {
                lNode->SetObjectFlags(FbxObject::eSavable, false);
                lLeafRollNodes++;
            }
        }

        // Also don't write the marker if all the nodes sharing it are new leaf roll nodes.
        if( lLeafRollNodes == lMarker->GetNodeCount() )
        {
            lMarker->SetObjectFlags(FbxObject::eSavable, false);
        }
    }

    // cached effects are not supported in FBX 6 files, we temporarily mark them as system 
    // objects so they do not get saved in the file.
    for( int i = 0, c = pScene.GetSrcObjectCount<FbxCachedEffect>(); i < c; ++i )
	{
        FbxCachedEffect* lAttribute = pScene.GetSrcObject<FbxCachedEffect>(i);
        lAttribute->SetObjectFlags(FbxObject::eSavable, false);
    }

    // idem for the FbxCache objects that are only connected to FbxCachedEffect.
    for( int i = 0, c = pScene.GetSrcObjectCount<FbxCache>(); i < c; ++i) 
	{
        FbxCache* lCache = pScene.GetSrcObject<FbxCache>(i);
        int lNbDst = lCache->GetDstObjectCount();
        int lNbCachedEffs = lCache->GetDstObjectCount<FbxCachedEffect>();
        if( lNbDst == (lNbCachedEffs+1) ) // one connection of the cache is to the document!
		{
            lCache->SetObjectFlags(FbxObject::eSavable, false);
		}
    }

    for( int i = 0, c = pScene.GetSrcObjectCount<FbxNodeAttribute>(); i < c; ++i )
	{
        FbxNodeAttribute* lNodeAttribute = pScene.GetSrcObject<FbxNodeAttribute>(i);
        FbxNode*          lNode          = lNodeAttribute ? lNodeAttribute->GetDstObject<FbxNode>() : 0;

        if (lNode && lNodeAttribute->GetName()[0]==0) {
            lNodeAttribute->SetName( lNode->GetName() );
        }
        else if( lNodeAttribute->GetName()[0]==0 ) {
            lNodeAttribute->SetName( FbxString("NodeAttribute ") + i );
        }
    }
    
    // Make deformer names unique
   
    for( int i = 0, c = pScene.GetSrcObjectCount<FbxDeformer>(); i < c; ++i )
    {
        FbxDeformer*       lDeformer = pScene.GetSrcObject<FbxDeformer>(i);
        FbxNodeAttribute*  lNA       = lDeformer ? lDeformer->GetDstObject<FbxNodeAttribute>() : 0;
        FbxNode*           lNode     = lNA ? lNA->GetDstObject<FbxNode>() : 0;

        if (lNode && lDeformer->GetName()[0]==0) 
		{
            FbxString lName = " ";
            if( lDeformer->GetDeformerType() == FbxDeformer::eSkin ) 
			{
				lName = "Skin ";
			}
            else if( lDeformer->GetDeformerType() == FbxDeformer::eVertexCache ) 
			{
				lName = "VertexCache ";            
            }
			lDeformer->SetName((lName + FbxString(lNode->GetName())));
		}
    }
   
    // Make subdeformer names unique
    
	for( int i = 0, c = pScene.GetSrcObjectCount<FbxSubDeformer>(); i < c; ++i )
    {
        FbxSubDeformer* lSubDeformer = pScene.GetSrcObject<FbxSubDeformer>(i);
		FbxNode*        lNode        = lSubDeformer ? lSubDeformer->GetSrcObject<FbxNode>() : NULL;

        if( lNode && lSubDeformer->GetName()[0] == 0 )
        {
            lSubDeformer->SetName( (FbxString("Cluster ")+FbxString(lNode->GetName())));
        }
    }

	for( int i = 0, c = pScene.GetSrcObjectCount<FbxLight>(); i < c; ++i )
	{
		FbxLight* lLight = pScene.GetSrcObject<FbxLight>(i);
		if( lLight )
		{
			//Area lights not supported in FBX 6.x, exported as point lights
			if( lLight->LightType.Get() == FbxLight::eArea )
			{
				lLight->LightType.Set(FbxLight::ePoint);
				gAreaLights.Add(lLight);
			}

			//In file version 7.3, HotSpot became InnerAngle
			FbxProperty lOldHotSpot = FbxProperty::Create(lLight, FbxDoubleDT, "HotSpot");
			if( lOldHotSpot.IsValid() )
			{
				lOldHotSpot.ModifyFlag(FbxPropertyFlags::eAnimatable, true);
				lOldHotSpot.CopyValue(lLight->InnerAngle);
                FbxAnimUtilities::ShareAnimCurves(lOldHotSpot, lLight->InnerAngle, &pScene);
			}

			//In file version 7.3, ConeAngle became OuterAngle
			FbxProperty lOldConeAngle = FbxProperty::Create(lLight, FbxDoubleDT, "Cone angle");
			if( lOldConeAngle.IsValid() )
			{
				lOldConeAngle.ModifyFlag(FbxPropertyFlags::eAnimatable, true);
				lOldConeAngle.CopyValue(lLight->OuterAngle);
                FbxAnimUtilities::ShareAnimCurves(lOldHotSpot, lLight->InnerAngle, &pScene);
			}
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

	//Now iterate through all gathered properties and switch Src to Dst connections
	//In FBX 2015 or above, we won't have to do this; we can keep them connected to sources.
	for( int i = 0, c = lProperties.Size(); i < c; ++i )
	{
		FbxPropertyT<FbxReference> lRefProperty = lProperties[i];
		FBX_ASSERT_MSG(lRefProperty.GetSrcObjectCount() <= 1, "There's more than one source object, so we can't be sure we're picking the right one!");
		FbxObject* lRefObj = lRefProperty.GetSrcObject();
		if( lRefObj )
		{
			lRefProperty.DisconnectSrcObject(lRefObj);
			lRefProperty.DisconnectAllDstObject();
			lRefProperty.ConnectDstObject(lRefObj);
		}
	}


    if( mFileVersion.Compare(FBX_2005_08_COMPATIBLE) == 0 ||
        mFileVersion.Compare(FBX_60_COMPATIBLE)		== 0 ||
        mFileVersion.Compare(FBX_2006_02_COMPATIBLE) == 0 ||
        mFileVersion.Compare(FBX_2006_08_COMPATIBLE) == 0 ||
        mFileVersion.Compare(FBX_2006_11_COMPATIBLE) == 0 ||
        mFileVersion.Compare(FBX_2009_00_COMPATIBLE) == 0 ||
        mFileVersion.Compare(FBX_2010_00_COMPATIBLE) == 0)
    {
        // avoid writing properties of datatypes not supported by the older versions
        ReplaceUnsupportedProperties(&pScene, true, FbxFileVersionStrToInt(mFileVersion));

        // make sure we reconnect the textures to the model instead of the material.
        FbxMaterialConverter lConv( *pScene.GetFbxManager() );
        lConv.AssignTexturesToLayerElements(pScene);
    }

	for( int i = 0, c = pScene.GetSrcObjectCount<FbxCharacter>(); i < c; ++i )
	{
		FbxCharacter* lCharacter = pScene.GetSrcObject<FbxCharacter>(i);
		if ( lCharacter )
		{
			#define StoreProperty(propName, linktoo){\
				FbxProperty lProp = lCharacter->FindProperty(propName);\
				if( lProp.IsValid() ) StoreUnsupportedProperty(lCharacter, lProp);\
				if( linktoo )\
				{\
					FbxString linkName = FbxString(propName) + "Link";\
					FbxProperty lLinkProp = lCharacter->FindProperty(linkName.Buffer());\
					if( lLinkProp.IsValid() ) StoreUnsupportedProperty(lCharacter, lLinkProp);\
				}}

			StoreProperty("LeftUpLegRollEx", true);		        StoreProperty("LeftUpLegRollExMode",false);
			StoreProperty("LeftLegRollEx", true);		        StoreProperty("LeftLegRollExMode", false);
			StoreProperty("RightUpLegRollEx", true);	        StoreProperty("RightUpLegRollExMode", false);
			StoreProperty("RightLegRollEx", true);		        StoreProperty("RightLegRollExMode", false);
			StoreProperty("LeftArmRollEx", true);		        StoreProperty("LeftArmRollExMode", false);
			StoreProperty("LeftForeArmRollEx", true);	        StoreProperty("LeftForeArmRollExMode", false);
			StoreProperty("RightArmRollEx", true);		        StoreProperty("RightArmRollExMode", false);
			StoreProperty("RightForeArmRollEx", true); 	        StoreProperty("RightForeArmRollExMode", false);

            StoreProperty("RealisticShoulder", false);          StoreProperty("CollarStiffnessX", false);
            StoreProperty("CollarStiffnessY", false);           StoreProperty("CollarStiffnessZ", false); 
            StoreProperty("ExtraCollarRatio", false);           StoreProperty("LeftLegMaxExtensionAngle", false);   
            StoreProperty("RightLegMaxExtensionAngle", false);  StoreProperty("LeftArmMaxExtensionAngle", false);   
            StoreProperty("RightArmMaxExtensionAngle", false);  StoreProperty("StretchStartArmsAndLegs", false);    
            StoreProperty("StretchStopArmsAndLegs", false);     StoreProperty("SnSScaleArmsAndLegs", false);        
            StoreProperty("SnSReachLeftWrist", false);          StoreProperty("SnSReachRightWrist", false);         
            StoreProperty("SnSReachLeftAnkle", false);          StoreProperty("SnSReachRightAnkle", false);         
            StoreProperty("SnSScaleSpine", false);              StoreProperty("SnSScaleSpineChildren", false);      
            StoreProperty("SnSSpineFreedom", false);            StoreProperty("SnSReachChestEnd", false);           
            StoreProperty("SnSScaleNeck", false);               StoreProperty("SnSNeckFreedom", false);             
            StoreProperty("SnSReachHead", false);

            // HIK 2016.5.0 roll properties
            // Leaf roll properties
            StoreProperty("LeafLeftUpLegRoll1", true);		    StoreProperty("LeafLeftUpLegRoll1Mode",false);
            StoreProperty("LeafLeftLegRoll1", true);		    StoreProperty("LeafLeftLegRoll1Mode", false);
            StoreProperty("LeafRightUpLegRoll1", true);		    StoreProperty("LeafRightUpLegRoll1Mode", false);
            StoreProperty("LeafRightLegRoll1", true);		    StoreProperty("LeafRightLegRoll1Mode", false);
            StoreProperty("LeafLeftArmRoll1", true);		    StoreProperty("LeafLeftArmRoll1Mode", false);
            StoreProperty("LeafLeftForeArmRoll1", true);	    StoreProperty("LeafLeftForeArmRoll1Mode", false);
            StoreProperty("LeafRightArmRoll1", true);		    StoreProperty("LeafRightArmRoll1Mode", false);
            StoreProperty("LeafRightForeArmRoll1", true); 	    StoreProperty("LeafRightForeArmRoll1Mode", false);
            StoreProperty("LeafLeftUpLegRoll2", true);		    StoreProperty("LeafLeftUpLegRoll2Mode",false);
            StoreProperty("LeafLeftLegRoll2", true);		    StoreProperty("LeafLeftLegRoll2Mode", false);
            StoreProperty("LeafRightUpLegRoll2", true);		    StoreProperty("LeafRightUpLegRoll2Mode", false);
            StoreProperty("LeafRightLegRoll2", true);		    StoreProperty("LeafRightLegRoll2Mode", false);
            StoreProperty("LeafLeftArmRoll2", true);		    StoreProperty("LeafLeftArmRoll2Mode", false);
            StoreProperty("LeafLeftForeArmRoll2", true);	    StoreProperty("LeafLeftForeArmRoll2Mode", false);
            StoreProperty("LeafRightArmRoll2", true);		    StoreProperty("LeafRightArmRoll2Mode", false);
            StoreProperty("LeafRightForeArmRoll2", true); 	    StoreProperty("LeafRightForeArmRoll2Mode", false);
            StoreProperty("LeafLeftUpLegRoll3", true);		    StoreProperty("LeafLeftUpLegRoll3Mode",false);
            StoreProperty("LeafLeftLegRoll3", true);		    StoreProperty("LeafLeftLegRoll3Mode", false);
            StoreProperty("LeafRightUpLegRoll3", true);		    StoreProperty("LeafRightUpLegRoll3Mode", false);
            StoreProperty("LeafRightLegRoll3", true);		    StoreProperty("LeafRightLegRoll3Mode", false);
            StoreProperty("LeafLeftArmRoll3", true);		    StoreProperty("LeafLeftArmRoll3Mode", false);
            StoreProperty("LeafLeftForeArmRoll3", true);	    StoreProperty("LeafLeftForeArmRoll3Mode", false);
            StoreProperty("LeafRightArmRoll3", true);		    StoreProperty("LeafRightArmRoll3Mode", false);
            StoreProperty("LeafRightForeArmRoll3", true); 	    StoreProperty("LeafRightForeArmRoll3Mode", false);
            StoreProperty("LeafLeftUpLegRoll4", true);		    StoreProperty("LeafLeftUpLegRoll4Mode",false);
            StoreProperty("LeafLeftLegRoll4", true);		    StoreProperty("LeafLeftLegRoll4Mode", false);
            StoreProperty("LeafRightUpLegRoll4", true);		    StoreProperty("LeafRightUpLegRoll4Mode", false);
            StoreProperty("LeafRightLegRoll4", true);		    StoreProperty("LeafRightLegRoll4Mode", false);
            StoreProperty("LeafLeftArmRoll4", true);		    StoreProperty("LeafLeftArmRoll4Mode", false);
            StoreProperty("LeafLeftForeArmRoll4", true);	    StoreProperty("LeafLeftForeArmRoll4Mode", false);
            StoreProperty("LeafRightArmRoll4", true);		    StoreProperty("LeafRightArmRoll4Mode", false);
            StoreProperty("LeafRightForeArmRoll4", true); 	    StoreProperty("LeafRightForeArmRoll4Mode", false);
            StoreProperty("LeafLeftUpLegRoll5", true);		    StoreProperty("LeafLeftUpLegRoll5Mode",false);
            StoreProperty("LeafLeftLegRoll5", true);		    StoreProperty("LeafLeftLegRoll5Mode", false);
            StoreProperty("LeafRightUpLegRoll5", true);		    StoreProperty("LeafRightUpLegRoll5Mode", false);
            StoreProperty("LeafRightLegRoll5", true);		    StoreProperty("LeafRightLegRoll5Mode", false);
            StoreProperty("LeafLeftArmRoll5", true);		    StoreProperty("LeafLeftArmRoll5Mode", false);
            StoreProperty("LeafLeftForeArmRoll5", true);	    StoreProperty("LeafLeftForeArmRoll5Mode", false);
            StoreProperty("LeafRightArmRoll5", true);		    StoreProperty("LeafRightArmRoll5Mode", false);
            StoreProperty("LeafRightForeArmRoll5", true); 	    StoreProperty("LeafRightForeArmRoll5Mode", false);
            // Full limb roll extraction
            StoreProperty("LeftLegFullRollExtraction", false);
            StoreProperty("RightLegFullRollExtraction", false);
            StoreProperty("LeftArmFullRollExtraction", false);
            StoreProperty("RightArmFullRollExtraction", false);

			lCharacter->SetValuesForLegacySave(0);
		}
	}

	// Do not write ControlSetPlug's new HIK 2016.5.0 leaf roll properties
    for( int i = 0, c = pScene.GetSrcObjectCount<FbxControlSetPlug>(); i < c; ++i )
    {
        FbxControlSetPlug* lControlSetPlug = pScene.GetSrcObject<FbxControlSetPlug>(i);
        if ( lControlSetPlug )
        {
            #define StoreProperty2(propName){\
				FbxProperty lProp = lControlSetPlug->FindProperty(propName);\
				if( lProp.IsValid() ) StoreUnsupportedProperty(lControlSetPlug, lProp);\
            }

            StoreProperty2("LeafLeftUpLegRoll1");
            StoreProperty2("LeafLeftLegRoll1");
            StoreProperty2("LeafRightUpLegRoll1");
            StoreProperty2("LeafRightLegRoll1");
            StoreProperty2("LeafLeftArmRoll1");
            StoreProperty2("LeafLeftForeArmRoll1");
            StoreProperty2("LeafRightArmRoll1");
            StoreProperty2("LeafRightForeArmRoll1");
            StoreProperty2("LeafLeftUpLegRoll2");
            StoreProperty2("LeafLeftLegRoll2");
            StoreProperty2("LeafRightUpLegRoll2");
            StoreProperty2("LeafRightLegRoll2");
            StoreProperty2("LeafLeftArmRoll2");
            StoreProperty2("LeafLeftForeArmRoll2");
            StoreProperty2("LeafRightArmRoll2");
            StoreProperty2("LeafRightForeArmRoll2");
            StoreProperty2("LeafLeftUpLegRoll3");
            StoreProperty2("LeafLeftLegRoll3");
            StoreProperty2("LeafRightUpLegRoll3");
            StoreProperty2("LeafRightLegRoll3");
            StoreProperty2("LeafLeftArmRoll3");
            StoreProperty2("LeafLeftForeArmRoll3");
            StoreProperty2("LeafRightArmRoll3");
            StoreProperty2("LeafRightForeArmRoll3");
            StoreProperty2("LeafLeftUpLegRoll4");
            StoreProperty2("LeafLeftLegRoll4");
            StoreProperty2("LeafRightUpLegRoll4");
            StoreProperty2("LeafRightLegRoll4");
            StoreProperty2("LeafLeftArmRoll4");
            StoreProperty2("LeafLeftForeArmRoll4");
            StoreProperty2("LeafRightArmRoll4");
            StoreProperty2("LeafRightForeArmRoll4");
            StoreProperty2("LeafLeftUpLegRoll5");
            StoreProperty2("LeafLeftLegRoll5");
            StoreProperty2("LeafRightUpLegRoll5");
            StoreProperty2("LeafRightLegRoll5");
            StoreProperty2("LeafLeftArmRoll5");
            StoreProperty2("LeafLeftForeArmRoll5");
            StoreProperty2("LeafRightArmRoll5");
            StoreProperty2("LeafRightForeArmRoll5");
        }
    }

    // and now we need to mark all the new animation objects as system objects 
    // so they do not get saved to the file
    gAnimObjectsFlags.Clear(); 

    // be careful with the ordering of these loops, the sgAnimObjectFlags is filled
    // in this order AND visited in the same order in the PostprocessScene to reset
    // the flags to the previous state
    for( int i = 0, c = pScene.GetSrcObjectCount<FbxAnimStack>(); i < c; ++i )
    {
        gAnimObjectsFlags.Add(pScene.GetSrcObject<FbxAnimStack>(i)->GetAllObjectFlags());
        pScene.GetSrcObject<FbxAnimStack>(i)->SetObjectFlags(FbxObject::eSavable, false);
    }

    for( int i = 0, c = pScene.GetSrcObjectCount<FbxAnimLayer>(); i < c; ++i )
    {
        gAnimObjectsFlags.Add(pScene.GetSrcObject<FbxAnimLayer>(i)->GetAllObjectFlags());
        pScene.GetSrcObject<FbxAnimLayer>(i)->SetObjectFlags(FbxObject::eSavable, false);
    }

    for( int i = 0, c = pScene.GetSrcObjectCount<FbxAnimCurveNode>(); i < c; ++i )
    {
        gAnimObjectsFlags.Add(pScene.GetSrcObject<FbxAnimCurveNode>(i)->GetAllObjectFlags());
        pScene.GetSrcObject<FbxAnimCurveNode>(i)->SetObjectFlags(FbxObject::eSavable, false);
    }

    for( int i = 0, c = pScene.GetSrcObjectCount<FbxAnimCurve>(); i < c; ++i )
    {
        gAnimObjectsFlags.Add(pScene.GetSrcObject<FbxAnimCurve>(i)->GetAllObjectFlags());
        pScene.GetSrcObject<FbxAnimCurve>(i)->SetObjectFlags(FbxObject::eSavable, false);
    }

    for( int i = 0, c = pScene.GetSrcObjectCount<FbxAudioLayer>(); i < c; ++i )
    {
        gAnimObjectsFlags.Add(pScene.GetSrcObject<FbxAudioLayer>(i)->GetAllObjectFlags());
        pScene.GetSrcObject<FbxAudioLayer>(i)->SetObjectFlags(FbxObject::eSavable, false);
    }

    for( int i = 0, c = pScene.GetSrcObjectCount<FbxAudio>(); i < c; ++i )
    {
        gAnimObjectsFlags.Add(pScene.GetSrcObject<FbxAudio>(i)->GetAllObjectFlags());
        pScene.GetSrcObject<FbxAudio>(i)->SetObjectFlags(FbxObject::eSavable, false);
    }

    FbxRenamingStrategyFbx6 lRenaming;
    lRenaming.EncodeScene(&pScene);
    
    return true;

}

bool FbxWriterFbx6::PostprocessScene(FbxScene& pScene)
{
    int i, lIndex = 0, lCount;

	// Clear the System flag we temporarily set on newly added objects to avoid having them
	// saved to the file.
	for (i = 0; i < pScene.GetSrcObjectCount<FbxBlendShape>(); i++) 
	{
		pScene.GetSrcObject<FbxBlendShape>(i)->SetObjectFlags(FbxObject::eSavable, true);
	}

	for (i = 0; i < pScene.GetSrcObjectCount<FbxBlendShapeChannel>(); i++) 
	{
		pScene.GetSrcObject<FbxBlendShapeChannel>(i)->SetObjectFlags(FbxObject::eSavable, true);
	}

	for(i = 0; i < pScene.GetSrcObjectCount<FbxShape>(); i++ )
	{
		pScene.GetSrcObject<FbxShape>(i)->SetObjectFlags(FbxObject::eSavable, true);
	}

	for(i = 0; i < pScene.GetSrcObjectCount<FbxLine>(); i++ )
	{
		pScene.GetSrcObject<FbxLine>(i)->SetObjectFlags(FbxObject::eSavable, true);
	}

	for(i = 0; i < pScene.GetSrcObjectCount<FbxProceduralTexture>(); i++ )
	{
		pScene.GetSrcObject<FbxProceduralTexture>(i)->SetObjectFlags(FbxObject::eSavable, true);
	}

    for( int i = 0, c = pScene.GetSrcObjectCount<FbxMarker>(); i < c; ++i )
    {
        FbxMarker* lMarker = pScene.GetSrcObject<FbxMarker>(i);
        if( !lMarker || lMarker->GetType() != FbxMarker::eEffectorFK )
            continue;

        for( int n = 0; n < lMarker->GetNodeCount(); ++n )
        {
            FbxNode* lNode = lMarker->GetNode(n);
            if( lNode && IsLeafRoll(lNode->GetNameWithoutNameSpacePrefix()) )
            {
                lNode->SetObjectFlags(FbxObject::eSavable, true);
            }
        }

        lMarker->SetObjectFlags(FbxObject::eSavable, true);
    }

    for (i = 0; i < pScene.GetSrcObjectCount<FbxCachedEffect>(); i++) 
	{
        pScene.GetSrcObject<FbxCachedEffect>(i)->SetObjectFlags(FbxObject::eSavable, true);
	}

    for (i = 0; i < pScene.GetSrcObjectCount<FbxCache>(); i++) 
	{
        pScene.GetSrcObject<FbxCache>(i)->SetObjectFlags(FbxObject::eSavable, true);
	}

    for( i = 0; i < pScene.GetSrcObjectCount<FbxAnimStack>(); i++ )
	{
        pScene.GetSrcObject<FbxAnimStack>(i)->SetAllObjectFlags(gAnimObjectsFlags[lIndex++]);
	}

    for( i = 0; i < pScene.GetSrcObjectCount<FbxAnimLayer>(); i++ )
	{
        pScene.GetSrcObject<FbxAnimLayer>(i)->SetAllObjectFlags(gAnimObjectsFlags[lIndex++]);
	}

    for( i = 0; i < pScene.GetSrcObjectCount<FbxAnimCurveNode>(); i++ )
	{
        pScene.GetSrcObject<FbxAnimCurveNode>(i)->SetAllObjectFlags(gAnimObjectsFlags[lIndex++]);
	}

    for( i = 0; i < pScene.GetSrcObjectCount<FbxAnimCurve>(); i++ )
	{
        pScene.GetSrcObject<FbxAnimCurve>(i)->SetAllObjectFlags(gAnimObjectsFlags[lIndex++]);
	}

    for( i = 0; i < pScene.GetSrcObjectCount<FbxAudioLayer>(); i++ )
	{
        pScene.GetSrcObject<FbxAudioLayer>(i)->SetAllObjectFlags(gAnimObjectsFlags[lIndex++]);
	}

    for( i = 0; i < pScene.GetSrcObjectCount<FbxAudio>(); i++ )
	{
        pScene.GetSrcObject<FbxAudio>(i)->SetAllObjectFlags(gAnimObjectsFlags[lIndex++]);
	}

	gAnimObjectsFlags.Clear(); 

	//Area lights not supported in FBX 6.x, exported as point lights, so revert them to area lights
	for( i = 0, lCount = gAreaLights.GetCount(); i < lCount; i++ )
	{
		FbxLight* lLight = gAreaLights[i];
		lLight->LightType.Set(FbxLight::eArea);
	}
	gAreaLights.Clear();

	for( i = 0, lCount = pScene.GetSrcObjectCount<FbxLight>(); i < lCount; i++ )
	{
		FbxLight* lLight = pScene.GetSrcObject<FbxLight>(i);
		if( lLight )
		{
			//In file version 7.3, HotSpot became InnerAngle
			FbxProperty lHotSpot = lLight->FindProperty("HotSpot");
			if( lHotSpot.IsValid() ) lHotSpot.Destroy();

			//In file version 7.3, ConeAngle became OuterAngle
			FbxProperty lConeAngle = lLight->FindProperty("Cone angle");
			if( lConeAngle.IsValid() ) lConeAngle.Destroy();
		}
	}    

	for (i = 0, lCount = pScene.GetSrcObjectCount<FbxCharacter>(); i < lCount; i++)
	{
		FbxCharacter* lCharacter = pScene.GetSrcObject<FbxCharacter>(i);
		if ( lCharacter )
		{
			lCharacter->RestoreValuesFromLegacySave();
		}
	}

    int lVersion = FbxFileVersionStrToInt(mFileVersion);
    if (lVersion == -1) lVersion = FBX_FILE_VERSION_6100;
	ReplaceUnsupportedProperties(&pScene, false, lVersion);

    if( lVersion <= 201000)
    {
        //When we convert the fbx7 to the older version, we will lose the connections between textures and materials.
        //So If the want to write the scene using fbx7 again, we need to reconnect them at first.
        FbxMaterialConverter lConv( *pScene.GetFbxManager() );
        lConv.ConnectTexturesToMaterials(pScene);
    }

	// Remove the blend shape property we added on base geometry to FBX 6 file.
	ConvertShapePropertyToNewStyle(pScene);
    if( lVersion < 200900)
        return true;

    return false;
}

void FbxWriterFbx6::ConvertShapePropertyToOldStyle(FbxScene& pScene)
{
	FbxAnimStack* lAnimStack = pScene.GetMember<FbxAnimStack>();

	FbxAnimLayer* lAnimLayer = NULL;
	if(lAnimStack)
	{
		lAnimLayer = lAnimStack->GetMember<FbxAnimLayer>();
	}

	// For FBX file earlier than 7.2, the shape deform property is created on the base geometry as a user property.
	// To imitate this, convert FbxBlendShapeChannel::DeformPercent to a user property on base geometry.
	// Besides, FBX file earlier than 7.2, only need to consider the first blend shape deformer and the first target shape on each blend shape channel.
	int lGeometryCount = pScene.GetSrcObjectCount<FbxShape>();
	for(int i = 0; i<lGeometryCount;++i)
	{
		FbxGeometry* lGeometry = pScene.GetSrcObject<FbxGeometry>(i);
        if (lGeometry == NULL)
            continue;
            
		int lBlendShapeDeformerCount = lGeometry->GetDeformerCount(FbxDeformer::eBlendShape);
		for(int lBlendShapeIndex = 0; lBlendShapeIndex<lBlendShapeDeformerCount; ++lBlendShapeIndex)
		{
			FbxBlendShape* lBlendShape = (FbxBlendShape*)lGeometry->GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);

			int lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();
			for(int lChannelIndex = 0; lChannelIndex<lBlendShapeChannelCount; ++lChannelIndex)
			{
				FbxBlendShapeChannel* lBlendShapeChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);

				if(lBlendShapeChannel)
				{	
					//For FBX file before 7200, there is only one shape on each blend shape channel.
					FbxShape* lShape = lBlendShapeChannel->GetTargetShape(0);
					if( lShape )
					{
						FbxString lShapeName = lShape->GetName();
						lGeometry->CreateShapeChannelProperties(lShapeName);

						FbxProperty lProp = lGeometry->FindProperty(lShapeName);
						if (lProp.IsValid())
						{
							// Copy the property value.
							lProp.CopyValue(lBlendShapeChannel->DeformPercent);

							// Copy the animation curve on the property if any.
							FbxAnimCurve* lShapeCurve = lBlendShapeChannel->DeformPercent.GetCurve(lAnimLayer, false);
							if(lShapeCurve)
							{
								FbxAnimCurve* lShapeCurveNew = lProp.GetCurve(lAnimLayer, true);
								if(lShapeCurveNew)
								{
									lShapeCurveNew->CopyFrom(*lShapeCurve);
								}
							}

						}// If shape deform property exist
					}// If lShape is valid
				}//If lBlendShapeChannel is valid.
			}//For each blend shape channel.
		}// If we have at least one blend shape deformer on this geometry.
	}//For each geometry.
}

// This should be used with ConvertShapePropertyToOldStyle in pair.
void FbxWriterFbx6::ConvertShapePropertyToNewStyle(FbxScene& pScene)
{
	FbxAnimStack* lAnimStack = pScene.GetMember<FbxAnimStack>();
	if(lAnimStack == NULL) return;

	FbxAnimLayer* lAnimLayer = lAnimStack->GetMember<FbxAnimLayer>();

	// For FBX file earlier than 7.2, the shape deform property is created on the base geometry as a user property.
	// To imitate this, we convert it use ConvertShapePropertyToOldStyle in PreprocessScene.
	// After the FBX file is written out, need to convert the shape property back to new style to 
	// make sure the scene does not changed and can still be written out as 7.2 style FBX file.
	int lGeometryCount = pScene.GetSrcObjectCount<FbxShape>();
	for(int i = 0; i<lGeometryCount;++i)
	{
		FbxGeometry* lGeometry = pScene.GetSrcObject<FbxGeometry>(i);
        if (lGeometry == NULL)
            continue;

		int lBlendShapeDeformerCount = lGeometry->GetDeformerCount(FbxDeformer::eBlendShape);
		for(int lBlendShapeIndex = 0; lBlendShapeIndex<lBlendShapeDeformerCount; ++lBlendShapeIndex)
		{
			FbxBlendShape* lBlendShape = (FbxBlendShape*)lGeometry->GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);

			int lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();
			for(int lChannelIndex = 0; lChannelIndex<lBlendShapeChannelCount; ++lChannelIndex)
			{
				FbxBlendShapeChannel* lBlendShapeChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);

				if(lBlendShapeChannel)
				{
					//For FBX file before 7200, there is only one shape on each blend shape channel.
					FbxShape* lShape = lBlendShapeChannel->GetTargetShape(0);
					if( lShape )
					{
						FbxString lShapeName = lShape->GetName();
						FbxProperty lProp = lGeometry->FindProperty(lShapeName);
						if (lProp.IsValid())
						{
							FbxAnimCurveNode* lShapeCurveNode = lProp.GetCurveNode(lAnimLayer, false);
							FbxAnimCurve* lShapeCurve = lProp.GetCurve(lAnimLayer, false);

							//Delete the shape property on the base geometry and its animcurve/animcurvenode if any.
							lProp.Destroy();
							if(lShapeCurve)
							{
								lShapeCurve->Destroy();
							}

							if(lShapeCurveNode)
							{
								lShapeCurveNode->Destroy();
							}
						}// If shape deform property exist.
					}// If lShape is valid.
				}//If lBlendShapeChannel is valid.
			}//For each blend shape channel.
		}// If we have at least one blend shape deformer on this geometry.
	}//For each geometry.
}

void FbxWriterFbx6::PluginWriteParameters(FbxObject& pParams)
{
    WriteObjectPropertiesAndFlags(&pParams);
}

bool FbxWriterFbx6::Write(FbxDocument* pDocument, FbxIO* pFbx)
{
    if (!pDocument)
    {
        GetStatus().SetCode(FbxStatus::eFailure, "Invalid document handle");
        return false;
    }

    mDocumentHierarchy = FbxNew< Fbx6TypeObjectHierarchy >();

    FlattenDocument(pDocument, *mDocumentHierarchy);

    FbxIO* lInternalFbx = NULL;
    int lMediaCount = 0;
    bool lResult = true;

    if (pFbx)
    {
        lInternalFbx = mFileObject;
        mFileObject = pFbx;
    }
    else if (!mFileObject)
    {
        GetStatus().SetCode(FbxStatus::eFailure, "File not created");
        lResult = false;
    }


    FbxScene*  lScene = FbxCast<FbxScene>(pDocument);
    bool        lIsAScene = (lScene != NULL);

    //
    // Pre-processing
    //
    if (lIsAScene)
    {
        lScene->ConnectMaterials();
        lScene->ConnectTextures();
    }

    pDocument->ConnectVideos();

    //
    // Definition section
    //
    if (lResult)
    {
        lResult = WriteDescriptionSection(pDocument);
    }

    mDocumentReferences = FbxNew< Fbx6TypeWriteReferences >();

    //
    // Reference section
    //
    if (lResult)
    {
        lResult = WriteReferenceSection(pDocument, *mDocumentReferences);
    }

    //
    // Object definition Section
    //
    Fbx6TypeDefinition lDefinitions;
    if (lResult)
    {
        BuildObjectDefinition(pDocument, lDefinitions);
        mProgress->SetTotal(static_cast<float>(lDefinitions.GetObjectCount()));
        SetObjectWriteSupport(lDefinitions);
        WriteObjectDefinition(pDocument, lDefinitions);
        if (GetStatus().Error())
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Out of disk space");
            lResult = false;
        }
    }

    //
    // Object properties
    //
    if (lResult)
    {
        WriteObjectProperties(pDocument, lDefinitions);

        if (GetStatus().Error())
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Out of disk space");
            lResult = false;
        }
    }

    // password
    if (lResult)
    {
        WritePassword();

        if (GetStatus().Error())
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Out of disk space");
            lResult = false;
        }
    }

    //
    // Object connections
    //
    if (lResult)
    {
        WriteObjectConnections(pDocument);

        if (GetStatus().Error())
        {
            GetStatus().SetCode(FbxStatus::eFailure, "Out of disk space");
            lResult = false;
        }
    }

    //
    // Takes and animation
    //
	if(IOS_REF.GetBoolProp(EXP_FBX_ANIMATION, true))
	{
		if (lResult)
		{
			WriteTakesAndAnimation(pDocument);

			if (GetStatus().Error())
            {
                GetStatus().SetCode(FbxStatus::eFailure, "Out of disk space");
                lResult = false;
            }
		}
	}

    if (lIsAScene && lResult)
    {
        if (IOS_REF.GetBoolProp(EXP_FBX_GLOBAL_SETTINGS,    true))
        {
            mFileObject->WriteComments ("Version 5 settings");
            mFileObject->WriteComments ("------------------------------------------------------------------");
            mFileObject->WriteComments ("");

            mFileObject->FieldWriteBegin("Version5");
            mFileObject->FieldWriteBlockBegin();
			WriteGlobalLightSettings(*lScene);
			WriteGlobalTimeSettings(*lScene);
			WriteGlobalCameraSettings(*lScene);

			if (GetStatus().Error())
            {
                GetStatus().SetCode(FbxStatus::eFailure, "Out of disk space");
                lResult = false;
            }
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();
        }
    }

    //
    // Post processing
    //
    if (pFbx)
    {
        mFileObject = lInternalFbx;
    }
    else
    {
        mFileObject->ProjectCloseSection();
        if (lIsAScene)
        {
            WriteExtensionSection(*lScene, lMediaCount);
        }
    }

    UnFlattenDocument(pDocument, *mDocumentHierarchy);

    return true;
}

void FbxWriterFbx6::SetProgressHandler(FbxProgress* pProgress)
{
    mProgress = pProgress;
}

void FbxWriterFbx6::FlattenDocument(FbxDocument* pDocument, Fbx6TypeObjectHierarchy& pDocHierarchy, bool pFirstCall)
{
    int i, lCount = pDocument->GetMemberCount<FbxDocument>();
    FbxDocument* lSubDoc;

    if (pFirstCall)
    {
        pDocHierarchy.Clear();
    }

    // First, recurse on sub-documents
    for (i = 0; i < lCount; i++)
    {
        Fbx6TypeObjectHierarchy lSubDocHierarchy;
        lSubDoc = pDocument->GetMember<FbxDocument>(i);
        FlattenDocument(lSubDoc, lSubDocHierarchy, false);
        pDocHierarchy.AppendHierarchy(lSubDocHierarchy);
    }

    FbxObject* lObj;

    if (pFirstCall)
    {
        lCount = pDocHierarchy.GetCount();

        for (i = 0; i < lCount; i++)
        {
            const char* lObjNewName = pDocHierarchy.GetObjectRootPathName(i);
            lObj = pDocHierarchy.GetObject(i);
            lObj->SetInitialName(lObjNewName);
            lObj->SetName(lObjNewName);

            pDocument->AddMember(lObj);
        }
        return;
    }

    FbxString lNewPathName = pDocument->GetPathToRootDocument();
    FbxString lSep("::");

    // Remove the name of the root document from the beginning
    lNewPathName = lNewPathName.Right(lNewPathName.GetLen() - lNewPathName.Find(lSep) - lSep.GetLen());

    // Then loop on regular objects
    lCount = pDocument->GetMemberCount();
    
    for (i = lCount; i--; )
    {
        lObj = pDocument->GetMember(i);

        FbxString lObjOriginalName(lObj->GetNameOnly());
        FbxString lObjNewName(lNewPathName);

        lObjNewName += lSep + lObjOriginalName;

        pDocument->RemoveMember(lObj);

        pDocHierarchy.AddObject(lObj, pDocument, lObjOriginalName.Buffer(), lObjNewName.Buffer());

    }
}

void FbxWriterFbx6::UnFlattenDocument(FbxDocument* pDocument, Fbx6TypeObjectHierarchy& pDocHierarchy)
{
    FbxObject* lObj;
    FbxDocument* lObjDoc;
    int i, lCount = pDocHierarchy.GetCount();

    for (i = 0; i < lCount; i++)
    {
        const char* lObjOriginalName = pDocHierarchy.GetObjectOriginalName(i);
        lObj = pDocHierarchy.GetObject(i);
        lObj->SetInitialName(lObjOriginalName);
        lObj->SetName(lObjOriginalName);

        lObjDoc = pDocHierarchy.GetObjectDocument(i);

        int lA = lObj->GetDstObjectCount<FbxDocument>();

        pDocument->RemoveMember(lObj);
        if (lObjDoc)
        {
            lObjDoc->AddMember(lObj);
        }

        int lB = lObj->GetDstObjectCount<FbxDocument>();

    }
}

bool FbxWriterFbx6::WriteObjectHeaderAndReferenceIfAny(FbxObject& pObj, const char* pObjectType) const
{
    if (pObjectType == NULL)
    {
        return false;
    }

    FbxObject* lReferencedObject = pObj.GetReferenceTo();
   
    mFileObject->FieldWriteBegin(pObjectType);
    mFileObject->FieldWriteC(pObj.GetNameWithNameSpacePrefix());
    mFileObject->FieldWriteC(pObj.GetTypeName());

    if (lReferencedObject != NULL)
    {
        FbxString lRefName;

        if ( mDocumentReferences && mDocumentReferences->GetReferenceName(lReferencedObject, lRefName))
        {
            mFileObject->FieldWriteC(FIELD_KFBXOBECT_REFERENCE_TO);
            mFileObject->FieldWriteC(lRefName);
        }
        else
        {
            return false;
        }
    }

    return true;
}


bool FbxWriterFbx6::WriteExtensionSection(FbxScene& pScene, int pMediaCount)
{
    int i, lCount;
    FbxArray<FbxString*> lNameArray;

    if (!mFileObject->ProjectCreateExtensionSection())
    {
        return false;
    }

    mFileObject->FieldWriteBegin(FIELD_SUMMARY);
    mFileObject->FieldWriteBlockBegin();

        // Version 100: original version
        // Version 101:
        //      - added the scene info
        //      - added block version number for FIELD_SUMMARY_TAKES
        mFileObject->FieldWriteI(FIELD_SUMMARY_VERSION, 101);


        IOS_REF.SetBoolProp(EXP_FBX_TEMPLATE, false);

        lCount = pScene.GetNodeCount();

        for (i = 0; i < lCount; i++)
        {
            FbxNode* lNode = pScene.GetNode(i);
            FbxString lNodeName = lNode->GetNameWithNameSpacePrefix();
            FbxString lNodeNameNoPrefix = lNodeName.Mid(lNodeName.ReverseFind(':') + 1);

            if (lNodeNameNoPrefix.Compare("~fbxexport~") == 0)
            {
                IOS_REF.SetBoolProp(EXP_FBX_TEMPLATE, true);
                break;
            }
        }

        mFileObject->FieldWriteB(FIELD_SUMMARY_TEMPLATE, IOS_REF.GetBoolProp(EXP_FBX_TEMPLATE, false) );
        mFileObject->FieldWriteB(FIELD_SUMMARY_PASSWORD_PROTECTION, IOS_REF.GetBoolProp(EXP_FBX_PASSWORD_ENABLE, true) && !IOS_REF.GetStringProp(EXP_FBX_PASSWORD, "").IsEmpty() );

        mFileObject->FieldWriteBegin(FIELD_SUMMARY_CONTENT_COUNT);
        mFileObject->FieldWriteBlockBegin();
		{
            mFileObject->FieldWriteS(FIELD_SUMMARY_VERSION, 100);
            mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_MODEL, pScene.GetRootNode()->GetChildCount(true));
            mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_DEVICE, 0);
            mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_CHARACTER, pScene.GetCharacterCount());
            mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_ACTOR, 0);
			mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_CONSTRAINT, pScene.GetSrcObjectCount<FbxConstraint>());
            mFileObject->FieldWriteI(FIELD_SUMMARY_CONTENT_COUNT_MEDIA, pMediaCount);
		}
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();

        // Scene info, added in version 101 of FIELD_SUMMARY_VERSION
        WriteSceneInfo(pScene.GetSceneInfo());

        mFileObject->FieldWriteBegin(FIELD_SUMMARY_TAKES);
        mFileObject->FieldWriteBlockBegin();
		{
            // Well, it's never to late to do correctly...
            // The FIELD_SUMMARY_TAKES_VERSION field did not exist in
            // the initial block. It has been added in v101 of FIELD_SUMMARY_VERSION.
            // v100 has been left to represent the "original" block.
            //
            // Version 101: added the take thumbnail
            pScene.FillAnimStackNameArray(lNameArray);
            lCount = lNameArray.GetCount();

            mFileObject->FieldWriteI(FIELD_SUMMARY_TAKES_VERSION, 101);
            mFileObject->FieldWriteC(FIELD_SUMMARY_TAKES_CURRENT, pScene.ActiveAnimStackName.Get());

            for (i = 0; i < lCount; i++)
            {
                // Avoid writing the default take
                if (lNameArray.GetAt(i)->Compare(FBXSDK_TAKENODE_DEFAULT_NAME) == 0)
                {
                    continue;
                }

                FbxTakeInfo* lTakeInfo = pScene.GetTakeInfo(*(lNameArray.GetAt(i)));

                if (lTakeInfo && lTakeInfo->mSelect)
                {
                    mFileObject->FieldWriteBegin(FIELD_SUMMARY_TAKES_TAKE);
                    mFileObject->FieldWriteC(lTakeInfo->mName);
                    mFileObject->FieldWriteBlockBegin();
		    {
                        if (!lTakeInfo->mDescription.IsEmpty())
                        {
                            mFileObject->FieldWriteC(FIELD_SUMMARY_TAKES_TAKE_COMMENT, lTakeInfo->mDescription);
                        }

                        mFileObject->FieldWriteTS(FIELD_SUMMARY_TAKES_TAKE_LOCAL_TIME, lTakeInfo->mLocalTimeSpan);
                        mFileObject->FieldWriteTS(FIELD_SUMMARY_TAKES_TAKE_REFERENCE_TIME, lTakeInfo->mReferenceTimeSpan);

		        if (pScene.GetSceneInfo() && pScene.GetSceneInfo()->GetSceneThumbnail())
		        {
		            WriteThumbnail(pScene.GetSceneInfo()->GetSceneThumbnail());
		        }
		    }
                    mFileObject->FieldWriteBlockEnd();
                    mFileObject->FieldWriteEnd();
                }
            }
		}
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->ProjectCloseSection();

    FbxArrayDelete(lNameArray);

    return true;
}


void FbxWriterFbx6::WritePassword()
{
    if( IOS_REF.GetBoolProp(EXP_FBX_PASSWORD_ENABLE, true) && !IOS_REF.GetStringProp(EXP_FBX_PASSWORD, "").IsEmpty() )
    {        
        mFileObject->WritePassword(IOS_REF.GetStringProp(EXP_FBX_PASSWORD, ""));
    }
}

void FbxWriterFbx6::WriteLayeredAnimation(FbxScene& pScene)
{
	//Write Animation Stacks
	FbxAnimStack* lStack = NULL;
	for( int i = 0, c = pScene.GetSrcObjectCount<FbxAnimStack>(); i < c; i++ )
	{
		lStack = pScene.GetSrcObject<FbxAnimStack>(i);
        WriteObjectHeaderAndReferenceIfAny(*lStack, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_STACK);
        mFileObject->FieldWriteBlockBegin();
        {
            WriteObjectPropertiesAndFlags(lStack);
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
	}

	//Write Animation Layers
	FbxAnimLayer* lLayer = NULL;
	for( int i = 0, c = pScene.GetSrcObjectCount<FbxAnimLayer>(); i < c; i++ )
	{
		lLayer = pScene.GetSrcObject<FbxAnimLayer>(i);
        WriteObjectHeaderAndReferenceIfAny(*lLayer, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_LAYER);
        mFileObject->FieldWriteBlockBegin();
        {
            WriteObjectPropertiesAndFlags(lLayer);
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
	}
}

// *******************************************************************************
//  Global Settings
// *******************************************************************************
void FbxWriterFbx6::WriteGlobalTimeSettings(FbxScene& pScene)
{
    FbxGlobalSettings& lGlobalTimeSettings = pScene.GetGlobalSettings();
    mFileObject->FieldWriteBegin(FIELD_KFBXGLOBALCAMERASETTINGS_SETTINGS);
    mFileObject->FieldWriteBlockBegin();
    //
    // write new variable that hold the framerate
    //
    mFileObject->FieldWriteC(FIELD_KFBXGLOBALTIMESETTINGS_FRAMERATE, FbxGetGlobalFrameRateString (pScene.GetGlobalSettings().GetTimeMode()) );

    mFileObject->FieldWriteI(FIELD_KFBXGLOBALTIMESETTINGS_TIME_PROTOCOL, lGlobalTimeSettings.GetTimeProtocol());
    mFileObject->FieldWriteI(FIELD_KFBXGLOBALTIMESETTINGS_SNAP_ON_FRAMES, lGlobalTimeSettings.GetSnapOnFrameMode());
    mFileObject->FieldWriteI(FIELD_KFBXGLOBALTIMESETTINGS_REFERENCE_TIME_INDEX, lGlobalTimeSettings.GetCurrentTimeMarker());

    //Time Markers
    FbxGlobalSettings::TimeMarker lTimeMarker;
    int lIter, lCount = lGlobalTimeSettings.GetTimeMarkerCount();
    for( lIter = 0; lIter < lCount; lIter++ )
    {
        lTimeMarker = lGlobalTimeSettings.GetTimeMarker(lIter);
        mFileObject->FieldWriteBegin("TimeMarker");
        mFileObject->FieldWriteC(lTimeMarker.mName);
        mFileObject->FieldWriteBlockBegin();
        {
            mFileObject->FieldWriteT("Time", lTimeMarker.mTime);
            mFileObject->FieldWriteI("Loop", lTimeMarker.mLoop);
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    FbxTimeSpan lTimeSpan;
    pScene.GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeSpan);
    mFileObject->FieldWriteLL(FIELD_KFBXGLOBALTIMESETTINGS_TIMELINE_START_TIME, lTimeSpan.GetStart().Get());
    mFileObject->FieldWriteLL(FIELD_KFBXGLOBALTIMESETTINGS_TIMELINE_STOP_TIME,  lTimeSpan.GetStop().Get());

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

void FbxWriterFbx6::WriteGlobalCameraSettings(FbxScene& pScene)
    {
    FbxGlobalCameraSettings& lGlobalCameraSettings = pScene.GlobalCameraSettings();

    mFileObject->FieldWriteBegin(FIELD_KFBXGLOBALCAMERASETTINGS_RENDERER_SETTINGS);
	mFileObject->FieldWriteBlockBegin();

	mFileObject->FieldWriteC(FIELD_KFBXGLOBALCAMERASETTINGS_DEFAULT_CAMERA, pScene.GetGlobalSettings().GetDefaultCamera());
	mFileObject->FieldWriteI(FIELD_KFBXGLOBALCAMERASETTINGS_DEFAULT_VIEWING_MODE, lGlobalCameraSettings.GetDefaultViewingMode());

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

void FbxWriterFbx6::WriteGlobalLightSettings(FbxScene& pScene)
{
    WriteAmbientColor(pScene);
    WriteFogOption(pScene);
    WriteShadowPlane(pScene);
}


void FbxWriterFbx6::WriteFogOption(FbxScene& pScene)
{
    mFileObject->FieldWriteBegin(FIELD_KFBXGLOBALLIGHTSETTINGS_FOGOPTIONS);
    mFileObject->FieldWriteBlockBegin ();
	{
        mFileObject->FieldWriteI(FIELD_KFBXGLOBALLIGHTSETTINGS_FOGENABLE, pScene.GlobalLightSettings().GetFogEnable());
        mFileObject->FieldWriteI(FIELD_KFBXGLOBALLIGHTSETTINGS_FOGMODE, pScene.GlobalLightSettings().GetFogMode());
        mFileObject->FieldWriteD(FIELD_KFBXGLOBALLIGHTSETTINGS_FOGDENSITY, pScene.GlobalLightSettings().GetFogDensity());
        mFileObject->FieldWriteD(FIELD_KFBXGLOBALLIGHTSETTINGS_FOGSTART, pScene.GlobalLightSettings().GetFogStart());
        mFileObject->FieldWriteD(FIELD_KFBXGLOBALLIGHTSETTINGS_FOGEND, pScene.GlobalLightSettings().GetFogEnd());

        mFileObject->FieldWriteBegin(FIELD_KFBXGLOBALLIGHTSETTINGS_FOGCOLOR);
		{
            FbxColor   lColor = pScene.GlobalLightSettings().GetFogColor ();

            mFileObject->FieldWriteD(lColor.mRed);
            mFileObject->FieldWriteD(lColor.mGreen);
            mFileObject->FieldWriteD(lColor.mBlue);
            mFileObject->FieldWriteD(1);
		}
        mFileObject->FieldWriteEnd ();
	}
    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();
}


void FbxWriterFbx6::WriteAmbientColor(FbxScene& pScene)
{
    mFileObject->FieldWriteBegin(FIELD_KFBXGLOBALLIGHTSETTINGS_AMBIENTRENDER);
    mFileObject->FieldWriteBlockBegin ();
	{
        mFileObject->FieldWriteI(FIELD_KFBXGLOBALLIGHTSETTINGS_VERSION, 101);

        mFileObject->FieldWriteBegin(FIELD_KFBXGLOBALLIGHTSETTINGS_AMBIENTLIGHTCOLOR);
		{
            FbxColor lColor = pScene.GetGlobalSettings().GetAmbientColor();

            mFileObject->FieldWriteD (lColor.mRed);
            mFileObject->FieldWriteD (lColor.mGreen);
            mFileObject->FieldWriteD (lColor.mBlue);
            mFileObject->FieldWriteD (lColor.mAlpha);
		}
        mFileObject->FieldWriteEnd ();
	}
    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();
}


void FbxWriterFbx6::WriteShadowPlane(FbxScene& pScene)
{
    if (pScene.GlobalLightSettings().GetShadowPlaneCount() > 0)
    {
        mFileObject->FieldWriteBegin(FIELD_KFBXGLOBALLIGHTSETTINGS_SHADOWPLANES);
        mFileObject->FieldWriteBlockBegin ();
		{
            WriteShadowPlaneSection (pScene);
		}
        mFileObject->FieldWriteBlockEnd ();
        mFileObject->FieldWriteEnd ();
    }
}


void FbxWriterFbx6::WriteShadowPlaneSection(FbxScene& pScene)
{
    int i, lPlaneCount;

    lPlaneCount = pScene.GlobalLightSettings().GetShadowPlaneCount();

    mFileObject->WriteComments ("Shadow Planes Section ");
    mFileObject->WriteComments ("----------------------------------------------------");

    mFileObject->FieldWriteI(FIELD_KFBXGLOBALLIGHTSETTINGS_VERSION, 108);
    mFileObject->FieldWriteI(FIELD_KFBXGLOBALLIGHTSETTINGS_COUNT,  lPlaneCount);

    for (i = 0; i < lPlaneCount; i ++)
    {
        FbxGlobalLightSettings::ShadowPlane* lShadowPlane = pScene.GlobalLightSettings().GetShadowPlane(i);

        mFileObject->FieldWriteBegin(FIELD_KFBXGLOBALLIGHTSETTINGS_PLANE);
		{
            mFileObject->FieldWriteD(lShadowPlane->mOrigin[0]);
            mFileObject->FieldWriteD(lShadowPlane->mOrigin[1]);
            mFileObject->FieldWriteD(lShadowPlane->mOrigin[2]);

            mFileObject->FieldWriteD(lShadowPlane->mNormal[0]);
            mFileObject->FieldWriteD(lShadowPlane->mNormal[1]);
            mFileObject->FieldWriteD(lShadowPlane->mNormal[2]);

            mFileObject->FieldWriteI (lShadowPlane->mEnable);
		}
        mFileObject->FieldWriteEnd();
    }

    mFileObject->FieldWriteI(FIELD_KFBXGLOBALLIGHTSETTINGS_USESHADOW,       pScene.GlobalLightSettings().GetShadowEnable());
    mFileObject->FieldWriteD(FIELD_KFBXGLOBALLIGHTSETTINGS_SHADOWINTENSITY, pScene.GlobalLightSettings().GetShadowIntensity());

}

bool FbxWriterFbx6::WriteTimeWarps(FbxDocument* pDocument, FbxAnimStack* pAnimStack)
{
	FbxAnimUtilities::SetTimeWarpSet(NULL);
	FbxScene* lScene = FbxCast<FbxScene>(pDocument);
    if (lScene == NULL || pAnimStack == NULL)
        return false;

	FbxMultiMap* lTWset = lScene->GetTakeTimeWarpSet((char*)pAnimStack->GetName());
	if (lTWset && lTWset->GetCount())
	{
		mFileObject->FieldWriteBegin("TimeWarps");
		mFileObject->FieldWriteBlockBegin();
		mFileObject->FieldWriteI(FIELD_SCENEINFO_VERSION, 100);

		for (int i = 0; i < lTWset->GetCount(); i++)
		{
			FbxHandle lTWID = 0;
			FbxAnimCurve* lAnimCurve = (FbxAnimCurve*)lTWset->GetFromIndex(i, &lTWID);            
            FbxString lTWName = FbxString("TimeWarp") + i;

            FbxAnimUtilities::CurveNodeIntfce lTW = FbxAnimUtilities::CreateTimeWarpNode(lAnimCurve, lTWName.Buffer());
            if (lTW.IsValid())
	        {
		        mTimeWarpsCurveNodes.Add(lTWID, lTW.GetHandle());
		
		        mFileObject->FieldWriteBegin("TW");
		        mFileObject->FieldWriteI((int)lTWID);
		        mFileObject->FieldWriteBlockBegin();						
		        {
                    FbxAnimUtilities::StoreCurveNode(lTW, mFileObject);
			    }
			    mFileObject->FieldWriteBlockEnd();
			    mFileObject->FieldWriteEnd();
	        }
		}

		mFileObject->FieldWriteBlockEnd();
		mFileObject->FieldWriteEnd();
		FbxAnimUtilities::SetTimeWarpSet(&mTimeWarpsCurveNodes);
	}

	return true;
}

bool FbxWriterFbx6::WriteThumbnail(FbxThumbnail* pThumbnail)
{
    if (pThumbnail->GetSize() != FbxThumbnail::eNotSet)
    {
        // This is a non-empty thumbnail
        FbxUChar* lImagePtr = pThumbnail->GetThumbnailImage();
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

        WriteObjectPropertiesAndFlags(pThumbnail);

        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    return true;
}

void FbxWriterFbx6::WriteSceneInfo(FbxDocumentInfo* pSceneInfo)
{
    if (!pSceneInfo) return;

    mFileObject->FieldWriteBegin(FIELD_SCENEINFO);
    mFileObject->FieldWriteC("SceneInfo::GlobalInfo");
    mFileObject->FieldWriteC("UserData");
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
                mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_TITLE,    pSceneInfo->mTitle);
                mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_SUBJECT,  pSceneInfo->mSubject);
                mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_AUTHOR,   pSceneInfo->mAuthor);
                mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_KEYWORDS, pSceneInfo->mKeywords);
                mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_REVISION, pSceneInfo->mRevision);
                mFileObject->FieldWriteS(FIELD_SCENEINFO_METADATA_COMMENT,  pSceneInfo->mComment);
            }
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            WriteObjectPropertiesAndFlags(pSceneInfo);
        }
        mFileObject->FieldWriteBlockEnd();
    }
    mFileObject->FieldWriteEnd();
}

void FbxWriterFbx6::WriteGlobalSettings(FbxGlobalSettings& pGlobalSettings)
{
    mFileObject->FieldWriteBegin(FIELD_GLOBAL_SETTINGS);
    mFileObject->FieldWriteBlockBegin();
    {
        mFileObject->FieldWriteI(FIELD_GLOBAL_SETTINGS_VERSION, 1000);

        FbxAxisSystem lSystem = pGlobalSettings.GetAxisSystem();

        WriteObjectPropertiesAndFlags(&pGlobalSettings);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

// *******************************************************************************
//  Utility functions
// *******************************************************************************
bool IsNodeAttribute(const FbxObject* pObject)
{
    return pObject ? pObject->Is<FbxNodeAttribute>() : false;
}

// Returns true if pObject is NOT one of the old node attributes
// that gets merged into the node.
bool IsStorableNodeAttribute(const FbxObject* pObject)
{
    if (pObject && IsNodeAttribute(pObject)) 
	{
        if ( pObject->Is<FbxGeometry>() ) 
		{
            if ( !pObject->Is<FbxMesh>() && !pObject->Is<FbxPatch>() && !pObject->Is<FbxNurbs>()) 
			{
                return true;
            }
        }
    }
    return false;
}

bool IsStorableObject(FbxObject* pObject)
{
    // The old format supports only geometry as part of
    // the fbxnode for Motion Builder's perspective
    if (IsNodeAttribute(pObject)) 
	{
        FbxNodeAttribute* lAttr = FbxCast<FbxNodeAttribute>(pObject);
        if( !lAttr->GetNode() ) return true;

        if (!IsStorableNodeAttribute(pObject)) return false;
    }

    return true;
}

bool IsNameUnique(FbxScene& pScene, FbxObject* pObject)
{
    FbxObject* lObject;
	int lIter, lCount = pScene.GetSrcObjectCount(FbxCriteria::ObjectType(pObject->GetClassId()));
    for( lIter = 0; lIter < lCount; lIter++ )
    {
		lObject = pScene.GetSrcObject(FbxCriteria::ObjectType(pObject->GetClassId()), lIter);
        if( lObject != pObject && !strcmp(lObject->GetName(), pObject->GetName()) )
        {
            return false;
        }
    }
    return true;
}

// *******************************************************************************
//  Definitions
// *******************************************************************************
void FbxWriterFbx6::BuildObjectDefinition(FbxDocument* pDocument, Fbx6TypeDefinition& pDefinitions)
{
    //Compute definition list
    int         lIter, lCount = pDocument->GetSrcObjectCount();
    FbxObject* lFbxObject = NULL;

    // We do not need to check for root nodes if the document is not a
    // scene.
    FbxObject* lRoot = NULL;
    FbxScene*  lScene = FbxCast<FbxScene>(pDocument);

    if (lScene != NULL)
    {
        lRoot = lScene->GetRootNode();
    }

    if (pDocument->GetDocumentInfo()) {
        pDefinitions.AddObject(pDocument->GetDocumentInfo());
    }

    for( lIter = 0; lIter < lCount; lIter++ )
    {
        lFbxObject = pDocument->GetSrcObject(lIter);
        if( lFbxObject != lRoot && lFbxObject->GetObjectFlags(FbxObject::eSavable) )
        {
            if (IsStorableObject(lFbxObject))
            {
                // We want to separate the characters from the other constraints :-(
                FbxConstraint* lConstraint = FbxCast<FbxConstraint>(lFbxObject);
                if (lConstraint && lConstraint->GetConstraintType() == FbxConstraint::eCharacter && IOS_REF.GetBoolProp(EXP_FBX_CHARACTER, true))
                {
                    FbxClassId lClassId = lFbxObject->GetRuntimeClassId();                                      
                    pDefinitions.AddObject(TOKEN_KFBXCONSTRAINT_CHARACTER, lClassId, lFbxObject->IsRuntimePlug());
                }
                else
				{
                    pDefinitions.AddObject(lFbxObject);
				}
            }
        }
    }


    //Reorder Definition
    pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MODEL, 0);
    pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MATERIAL, 1);
    pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TEXTURE, 2);
    pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_VIDEO, 3);

    //At the end
    pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONSTRAINT, -5);
    pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TIMELINE, -4);
    pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TIMELINE_TRACK, -3);
    pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CLIP, -2);
    pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_FOLDER, -1);
}

void FbxWriterFbx6::SetObjectWriteSupport(const Fbx6TypeDefinition& pDefinitions)
{
    int lDefinitionCount = pDefinitions.GetDefinitionCount();

    // Iterates through the definitions to see if they are
    // supported by the new save mechanism
    for (int i = 0; i < lDefinitionCount; i++)
    {
        Fbx6TypeDefinitionInfo* lDef = pDefinitions.GetDefinition(i);

        if (( lDef->mClassId.Is(FbxCollectionExclusive::ClassId) ))
        {
            lDef->SetGenericWriteEnable(true);
        }
        // Those class ids are not supported by the new writing mechanism
        else if (    ( lDef->mClassId.Is(FbxSurfaceMaterial::ClassId) )
             || ( lDef->mClassId.Is(FbxVideo::ClassId) )
             || ( lDef->mClassId.Is(FbxTexture::ClassId) )
             || ( lDef->mClassId.Is(FbxImplementation::ClassId) )
             || ( lDef->mClassId.Is(FbxBindingTable::ClassId) )
             || ( lDef->mClassId.Is(FbxLayeredTexture::ClassId) )
             || ( lDef->mClassId.Is(FbxCollection::ClassId) )
             || ( lDef->mClassId.Is(FbxThumbnail::ClassId) )
             || ( lDef->mClassId.Is(FbxCache::ClassId) )
             || ( lDef->mClassId.Is(FbxDocument::ClassId) )
             || ( lDef->mClassId.Is(FbxGeometry::ClassId) )
             || ( lDef->mClassId.Is(FbxMesh::ClassId) )
             || ( lDef->mClassId.Is(FbxNurbs::ClassId) )
             || ( lDef->mClassId.Is(FbxPatch::ClassId) )
             || ( lDef->mClassId.Is(FbxNurbsCurve::ClassId) )
             || ( lDef->mClassId.Is(FbxBoundary::ClassId) )
             || ( lDef->mClassId.Is(FbxNurbsSurface::ClassId) )
             || ( lDef->mClassId.Is(FbxGeometryWeightedMap::ClassId) )
             || ( lDef->mClassId.Is(FbxSkin::ClassId) )
             || ( lDef->mClassId.Is(FbxCluster::ClassId) )
             || ( lDef->mClassId.Is(FbxVertexCacheDeformer::ClassId) )
             || ( lDef->mClassId.Is(FbxConstraint::ClassId) )
             || ( lDef->mClassId.Is(FbxNode::ClassId) )
             || ( lDef->mClassId.Is(FbxCharacter::ClassId) )
             || ( lDef->mClassId.Is(FbxCharacterPose::ClassId) )
             || ( lDef->mClassId.Is(FbxControlSetPlug::ClassId) )
             || ( lDef->mClassId.Is(FbxPose::ClassId) )
             || ( lDef->mClassId.Is(FbxNodeAttribute::ClassId) )
             || ( lDef->mClassId.Is(FbxLayerContainer::ClassId) )
             || ( lDef->mClassId.Is(FbxGenericNode::ClassId) )
             || ( lDef->mClassId.Is(FbxLayerContainer::ClassId) )
             || ( lDef->mClassId.Is(FbxGlobalSettings::ClassId) )
             || ( lDef->mClassId.Is(FbxContainer::ClassId) )
             || ( lDef->mClassId.Is(FbxSelectionNode::ClassId) )
           )
        {
            lDef->SetGenericWriteEnable(false);
        }
    }
}

bool FbxWriterFbx6::WriteDescriptionSection(FbxDocument* pDocument)
{
    bool lResult = true;

    //Write Document description
    mFileObject->WriteComments("");
    mFileObject->WriteComments(" Document Description");
    mFileObject->WriteComments("------------------------------------------------------------------");
    mFileObject->WriteComments("");

    mFileObject->FieldWriteBegin("Document");
    mFileObject->FieldWriteBlockBegin();
    {    
        mFileObject->FieldWriteC("Name", pDocument->GetName());
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    if (GetStatus().Error())
    {
        GetStatus().SetCode(FbxStatus::eFailure, "Out of disk space");
        lResult = false;
    }
    return lResult;
}

bool FbxWriterFbx6::WriteReferenceSection(FbxDocument* pDocument, Fbx6TypeWriteReferences& pReferences)
{
    //Write Document references
    mFileObject->WriteComments("");
    mFileObject->WriteComments(" Document References");
    mFileObject->WriteComments("------------------------------------------------------------------");
    mFileObject->WriteComments("");

    mFileObject->FieldWriteBegin("References");
    mFileObject->FieldWriteBlockBegin();

    FbxArray<FbxDocument*> lReferencedDocuments;
    int lDocRefCount = pDocument->GetReferencedDocuments(lReferencedDocuments);

    if (lDocRefCount > 0)
    {
        FbxArray<FbxDocument*>   lDocumentPath;
        FbxArray<FbxObject*>     lReferencedObjects;

        FbxString lRefBaseName = "Reference_";
        int lRefCurNum = 1;

        FbxDocument* lDocRoot = pDocument->GetRootDocument();

        int i, j;
        for (i = 0; i < lDocRefCount; i++)
        {
            // Write the document file path so that it can be found again.
            // Used in OOP renderer, for example, to reload asset libraries.
            FbxDocument* lRefDoc      = lReferencedDocuments[i];
            FbxDocumentInfo* lDocInfo = lRefDoc->GetDocumentInfo();
            if (lDocInfo)
            {
                // Avoid writing the section if the file path is empty.
                // The reader won't be able to read the document anyway
                // if no file name are given...
                FbxString lRefDocUrl = lDocInfo->Url.Get();
                if( ! lRefDocUrl.IsEmpty() )
                {
                    // Convert the external document path to a document-relative
                    // path. To do this, first ensure that it is a full path.
                    lRefDocUrl = FbxPathUtils::Resolve( lRefDocUrl );
                    lRefDocUrl = mFileObject->GetRelativeFilePath( lRefDocUrl );
                    mFileObject->FieldWriteBegin("FilePathUrl");
                    mFileObject->FieldWriteC(lRefDocUrl);
                    mFileObject->FieldWriteBlockBegin();

                    mFileObject->FieldWriteBegin(FIELD_OBJECT_DEFINITION_OBJECT_TYPE);
                    {
                        FbxClassId lClassId = lRefDoc->GetRuntimeClassId();
                        const char* lTypeName = lClassId.GetFbxFileTypeName(true);
                        if( FbxLibrary::ClassId == lClassId )
                        {
                            // The FbxLibrary class is a real class but it's not given
                            // its own typename, not even a sub-type or something.
                            // So we detect it and force a different name so that we
                            // can distinguish between documents and libraries.
                            lTypeName = FIELD_OBJECT_DEFINITION_OBJECT_TYPE_LIBRARY;
                        }
                        mFileObject->FieldWriteC(lTypeName);
                    }
                    mFileObject->FieldWriteEnd();

                    lRefDoc->GetDocumentPathToRootDocument(lDocumentPath);
                    int k, lDocPathCount = lDocumentPath.GetCount();
                    for (k = 0; k < lDocPathCount; k++)
                    {                       
                        mFileObject->FieldWriteBegin("Document");
                        mFileObject->FieldWriteC(lDocumentPath[k]->GetNameOnly());
                        mFileObject->FieldWriteBlockBegin();
                    }

                    for (k = 0; k < lDocPathCount; k++)
                    {
                        mFileObject->FieldWriteBlockEnd();
                        mFileObject->FieldWriteEnd();
                    }

                    mFileObject->FieldWriteBlockEnd();
                    mFileObject->FieldWriteEnd();
                }
            }
        }

        for (i = 0; i < lDocRefCount; i++)
        {
            FbxDocument* lRefDoc       = lReferencedDocuments[i];

            int lObjRefCount = pDocument->GetReferencedObjects(lRefDoc, lReferencedObjects);

            // Now write objects references referencing that document.
            for (j = 0; j < lObjRefCount; j++)
            {
                FbxString	lRefName        = lRefBaseName + (lRefCurNum++);
                FbxObject*	lRefObj         = lReferencedObjects[j];
                FbxString   lRefObjName     = lRefObj->GetNameWithNameSpacePrefix();
                bool        lRefIsInternal  = false;
                
                lRefDoc->GetDocumentPathToRootDocument(lDocumentPath);
                int k = 0;
                const int lDocPathCount = lDocumentPath.GetCount();

                for( k = lDocPathCount - 1; k > -1; --k )
                {
                    // if pDoc is in the ref doc path, it means that
                    // the ref doc is contained within pDoc
                    if( pDocument == lDocumentPath[k] )
                    {
                        lRefIsInternal = true;
                        break;
                    }
                }

                pReferences.AddReference(lRefObj, lRefName.Buffer());
                mFileObject->FieldWriteBegin("Reference");
                mFileObject->FieldWriteC(lRefName);
                mFileObject->FieldWriteC(lRefIsInternal ? (char*)"Internal" : (char*)"External");
                mFileObject->FieldWriteBlockBegin();

                mFileObject->FieldWriteBegin("Object");
                mFileObject->FieldWriteC(lRefObjName);

                mFileObject->FieldWriteBlockBegin();
                mFileObject->FieldWriteBlockEnd();
                mFileObject->FieldWriteEnd();

                for (k = 0; k < lDocPathCount; k++)
                {                    
                    mFileObject->FieldWriteBegin("Document");
                    mFileObject->FieldWriteC(lDocumentPath[k]->GetNameOnly());
                    mFileObject->FieldWriteBlockBegin();
                }

                for (k = 0; k < lDocPathCount; k++)
                {
                    mFileObject->FieldWriteBlockEnd();
                    mFileObject->FieldWriteEnd();
                }
                mFileObject->FieldWriteBlockEnd();
                mFileObject->FieldWriteEnd();
            }
        }
    }

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    if (GetStatus().Error())
    {
        GetStatus().SetCode(FbxStatus::eFailure, "Out of disk space");
        return false;
    }
    return true;
}

void FbxWriterFbx6::WriteObjectDefinition(FbxDocument* pDocument, Fbx6TypeDefinition& pDefinitions)
{
    bool lVisitedNodeClass = false;

    //Write definition list
    mFileObject->WriteComments("");
    mFileObject->WriteComments(" Object definitions");
    mFileObject->WriteComments("------------------------------------------------------------------");
    mFileObject->WriteComments("");

    mFileObject->FieldWriteBegin(FIELD_OBJECT_DEFINITION);
    mFileObject->FieldWriteBlockBegin();
    {
	#ifndef FBXSDK_ENV_WINSTORE
        int WritablePluginCount = mManager.GetPluginCount();
        mFileObject->FieldWriteI(FIELD_OBJECT_DEFINITION_VERSION, 100);
        mFileObject->FieldWriteI(FIELD_OBJECT_DEFINITION_COUNT, pDefinitions.GetObjectCount() + (WritablePluginCount > 0 ? 1 : 0));
	#endif

        int lIter, lCount = pDefinitions.GetDefinitionCount();
        for( lIter = 0; lIter < lCount; lIter++ )
        {
            mFileObject->FieldWriteBegin(FIELD_OBJECT_DEFINITION_OBJECT_TYPE);
            {
                mFileObject->FieldWriteC(pDefinitions.GetDefinition(lIter)->mName);
                mFileObject->FieldWriteBlockBegin();
                {
                    mFileObject->FieldWriteI(FIELD_OBJECT_DEFINITION_COUNT, pDefinitions.GetDefinition(lIter)->mCount);
                    if (mWriteNonDefaultPropertiesOnly)
                        WritePropertyTemplate( pDefinitions.GetDefinition(lIter)->mClassId, pDocument, lVisitedNodeClass );
                }
                mFileObject->FieldWriteBlockEnd();
            }
            mFileObject->FieldWriteEnd();
        }

	#ifndef FBXSDK_ENV_WINSTORE
        //Write plugins definition
        if( WritablePluginCount > 0 )
        {
            mFileObject->FieldWriteBegin(FIELD_OBJECT_DEFINITION_OBJECT_TYPE);
            {
                mFileObject->FieldWriteC(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_PLUGIN_PARAMS);
                mFileObject->FieldWriteBlockBegin();
                mFileObject->FieldWriteI(FIELD_OBJECT_DEFINITION_COUNT, WritablePluginCount);
                mFileObject->FieldWriteBlockEnd();
            }
            mFileObject->FieldWriteEnd();
        }
	#endif
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

void FbxWriterFbx6::WritePropertyTemplate( FbxClassId pClassId, FbxDocument* pDocument, bool& pVisitedNodeClass )
{
    FbxPropertyHandle* lRootClassDefault = pClassId.GetRootClassDefaultPropertyHandle();
    FBX_ASSERT( lRootClassDefault );
    if( !lRootClassDefault || !lRootClassDefault->Valid() )
        return;

    FbxProperty lClassRoot(*lRootClassDefault);

    // minor optimization: many classes don't have any properties
    // in their class roots so, let's avoid writing empty PropertyTemplate
    // blocks.
    FbxProperty lFirstDescendent = lClassRoot.GetFirstDescendent();
    if( !lFirstDescendent.IsValid() )
        return;

    // if the FirstDescendent is valid but notSavable we skip writing the empty template
    if (lFirstDescendent.GetFlag(FbxPropertyFlags::eNotSavable))
        return;

    mFileObject->FieldWriteBegin(FIELD_OBJECT_PROPERTY_TEMPLATE);

    // note: const correct writer.
    FbxAutoFreePtr<char> lStr( FbxStrDup(pClassId.GetName()) );
    mFileObject->FieldWriteC( lStr.Get() );

    mFileObject->FieldWriteBlockBegin();

    mFileObject->FieldWriteBegin(mWriteEnhancedProperties ? sENHANCED_PROPERITIES : sLEGACY_PROPERTIES);
    mFileObject->FieldWriteBlockBegin();
    {
        FbxProperty lProperty = lClassRoot.GetFirstDescendent();

        while( lProperty.IsValid() )
        {
            WriteProperty(lProperty, pClassId.Is( FbxNodeAttribute::ClassId ) );
            lProperty = lClassRoot.GetNextDescendent(lProperty);
        }
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    // If we are writing nodes we also have to check that
    // we are writing the "nonstorable" node attribute's templates.
    //
    if( pClassId.Is(FbxNode::ClassId) && !pVisitedNodeClass )
    {
        pVisitedNodeClass = true;
        FbxSet<FbxClassIdInfo*> lVisitedAttrClasses;

        int i = 0, lCount = pDocument->GetSrcObjectCount<FbxNodeAttribute>();
        for( i = 0; i < lCount; ++i )
        {
            FbxNodeAttribute* lAttr = pDocument->GetSrcObject<FbxNodeAttribute>(i);
            if( !IsStorableObject( lAttr ) && !lVisitedAttrClasses.Find( lAttr->GetClassId().GetClassIdInfo() ) )
            {
                lVisitedAttrClasses.Insert( lAttr->GetClassId().GetClassIdInfo() );
                WritePropertyTemplate( lAttr->GetClassId(), pDocument, pVisitedNodeClass );
            }
        }
    }
}

// *******************************************************************************
//  Objects section
// *******************************************************************************
bool FbxWriterFbx6::WriteNodes(FbxScene& pScene, bool pIncludeRoot)
{
    int i, lNodeCount = pScene.GetNodeCount();
    bool lSuccess = true;
    for( i = 0; i < lNodeCount; ++i )
    {
        FbxNode* lNode = pScene.GetNode(i);
        if( !pIncludeRoot && lNode == pScene.GetRootNode() )
        {
            continue;
        }
        lSuccess &= WriteNode(*pScene.GetNode(i));
    }
    return lSuccess;
}

bool FbxWriterFbx6::WriteContainers(FbxScene& pScene)
{
    int i, lCount = pScene.GetMemberCount<FbxContainer>();
    FbxContainer* lContainer;

    bool lStatus = true;
    bool pEmbeddedMedia;
    pEmbeddedMedia = IOS_REF.GetBoolProp(EXP_FBX_EMBEDDED, false) && mFileObject->IsBinary();

    for (i=0; i<lCount; i++)
    {
        lContainer = pScene.GetMember<FbxContainer>(i);

        WriteObjectHeaderAndReferenceIfAny(*lContainer, FIELD_KFBXCONTAINER_CONTAINER);
        mFileObject->FieldWriteBlockBegin ();
        {
            mFileObject->FieldWriteI(FIELD_KFBXCONTAINER_VERSION, 100);

            WriteObjectPropertiesAndFlags(lContainer);
             
            if (pEmbeddedMedia)
            {
                const char* lFileName = lContainer->TemplatePath.Get();
                mFileObject->FieldWriteBegin(FIELD_MEDIA_CONTENT);
                lStatus = (mFileObject->FieldWriteEmbeddedFile(lFileName, lFileName) ? true : false);
                mFileObject->FieldWriteEnd();

                for( int j = 0; j < int(lContainer->mContainerTemplate->GetExtendTemplateCount()); j++ )
                {
                    lFileName = lContainer->mContainerTemplate->GetExtendTemplatePathAt(j);
                    mFileObject->FieldWriteBegin(FIELD_MEDIA_CONTENT);
                    lStatus = (mFileObject->FieldWriteEmbeddedFile(lFileName, lFileName) ? true : false);
                    mFileObject->FieldWriteEnd();
                }
            }
        }
        mFileObject->FieldWriteBlockEnd ();
        mFileObject->FieldWriteEnd ();
    }

    return lStatus;
}

bool FbxWriterFbx6::WriteGenericNodes(FbxScene& pScene)
{
    int i, lCount = pScene.GetGenericNodeCount();
    FbxGenericNode* lNode;

    for (i=0; i<lCount; i++)
    {
        lNode = pScene.GetGenericNode(i);

        WriteObjectHeaderAndReferenceIfAny(*lNode, FIELD_KFBXGENERICNODE_GENERICNODE);
        mFileObject->FieldWriteBlockBegin ();
        {
            mFileObject->FieldWriteI(FIELD_KFBXGENERICNODE_VERSION, 100);

            WriteObjectPropertiesAndFlags(lNode);
        }
        mFileObject->FieldWriteBlockEnd ();
        mFileObject->FieldWriteEnd ();
    }

    return true;
}

void FbxWriterFbx6::WriteControlSetPlug(FbxScene& pScene)
{
    int i, lCount = pScene.GetControlSetPlugCount();

    FbxControlSetPlug* lPlug;

    for (i=0; i < lCount; i++)
    {
        lPlug = pScene.GetControlSetPlug(i);

        WriteObjectHeaderAndReferenceIfAny(*lPlug, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONTROLSET_PLUG);
       
        mFileObject->FieldWriteBlockBegin ();
        {
            mFileObject->FieldWriteC("Type",lPlug->GetTypeName());
            mFileObject->FieldWriteI("MultiLayer",0);

            WriteObjectPropertiesAndFlags(lPlug);
        }
        mFileObject->FieldWriteBlockEnd ();
        mFileObject->FieldWriteEnd ();
    }
}

void FbxWriterFbx6::WriteCharacterLinkGroup(FbxCharacter& pCharacter, int pCharacterGroupId, FbxScene& pScene)
{
    int i, lCount = FbxCharacter::GetCharacterGroupCount((FbxCharacter::EGroupId) pCharacterGroupId);

    for (i = 0; i < lCount; i++)
    {
        FbxCharacter::ENodeId lCharacterNodeId = FbxCharacter::GetCharacterGroupElementByIndex((FbxCharacter::EGroupId) pCharacterGroupId, i);
        FbxCharacterLink lCharacterLink;

        bool lFound = pCharacter.GetCharacterLink(lCharacterNodeId, &lCharacterLink);

        if (lFound)
        {
            lFound = (lCharacterLink.mNode && lCharacterLink.mNode->GetScene() == &pScene) || !lCharacterLink.mTemplateName.IsEmpty();
        }

        // Don't save links for new HIK 2016.5.0 leaf roll nodes
        bool lNodeBackwardCompatible = FbxCharacter::GetCharacterGroupVersionByIndex((FbxCharacter::EGroupId) pCharacterGroupId, i) < 2016500;

        if(lFound && lNodeBackwardCompatible)
        {
            mFileObject->FieldWriteBegin("LINK");
            mFileObject->FieldWriteC(FbxCharacter::GetCharacterGroupNameByIndex((FbxCharacter::EGroupId) pCharacterGroupId, i));
            mFileObject->FieldWriteBlockBegin();

            WriteCharacterLink(pCharacter, lCharacterNodeId, pScene);

            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();
        }
    }
}

void FbxWriterFbx6::WriteCharacterLink(FbxCharacter& pCharacter, int pCharacterNodeId, FbxScene& pScene)
{
    FbxVector4 lT;
    FbxVector4 lR;
    FbxVector4 lS;
    bool        lFound = false;

    FbxCharacterLink* lCharacterLink = pCharacter.GetCharacterLinkPtr((FbxCharacter::ENodeId)pCharacterNodeId);
    if (lCharacterLink)
    {
        lFound = (lCharacterLink->mNode && lCharacterLink->mNode->GetScene() == &pScene) || !lCharacterLink->mTemplateName.IsEmpty();
    }

    // Don't save node if it's not part of the current scene.
    if (lFound)
    {
        if (!lCharacterLink->mTemplateName.IsEmpty())
        {
            mFileObject->FieldWriteC("NAME", lCharacterLink->mTemplateName);
        }

        if( lCharacterLink->mPropertyOffsetT.IsValid() ) lCharacterLink->mOffsetT = lCharacterLink->mPropertyOffsetT.Get<FbxDouble3>();
        if( lCharacterLink->mPropertyOffsetR.IsValid() ) lCharacterLink->mOffsetR = lCharacterLink->mPropertyOffsetR.Get<FbxDouble3>();
        if( lCharacterLink->mPropertyOffsetS.IsValid() ) lCharacterLink->mOffsetS = lCharacterLink->mPropertyOffsetS.Get<FbxDouble3>();
        if( lCharacterLink->mPropertyParentOffsetR.IsValid() ) lCharacterLink->mParentROffset = lCharacterLink->mPropertyParentOffsetR.Get<FbxDouble3>();
        lT = lCharacterLink->mOffsetT;
        lR = lCharacterLink->mOffsetR;
        lS = lCharacterLink->mOffsetS;
    }
    else
    {
        lT.Set(0.0, 0.0, 0.0);
        lR.Set(0.0, 0.0, 0.0);
        lS.Set(1.0, 1.0, 1.0);
    }

    if(lFound)
    {
        mFileObject->FieldWriteD("TOFFSETX", lT.mData[0]);
        mFileObject->FieldWriteD("TOFFSETY", lT.mData[1]);
        mFileObject->FieldWriteD("TOFFSETZ", lT.mData[2]);
        mFileObject->FieldWriteD("ROFFSETX", lR.mData[0]);
        mFileObject->FieldWriteD("ROFFSETY", lR.mData[1]);
        mFileObject->FieldWriteD("ROFFSETZ", lR.mData[2]);
        mFileObject->FieldWriteD("SOFFSETX", lS.mData[0]);
        mFileObject->FieldWriteD("SOFFSETY", lS.mData[1]);
        mFileObject->FieldWriteD("SOFFSETZ", lS.mData[2]);
        mFileObject->FieldWriteD("PARENTROFFSETX", lCharacterLink->mParentROffset.mData[0]);
        mFileObject->FieldWriteD("PARENTROFFSETY", lCharacterLink->mParentROffset.mData[1]);
        mFileObject->FieldWriteD("PARENTROFFSETZ", lCharacterLink->mParentROffset.mData[2]);

        if(lCharacterLink->mHasRotSpace)
        {
            WriteCharacterLinkRotationSpace(*lCharacterLink);
        }
    }
}
void FbxWriterFbx6::WriteCharacterLinkRotationSpace(FbxCharacterLink& pCharacterLink)
{
    mFileObject->FieldWriteBegin("ROTATIONSPACE");
    mFileObject->FieldWriteBlockBegin();

    mFileObject->FieldWrite3D("PRE", pCharacterLink.mPreRotation);
    mFileObject->FieldWrite3D("POST", pCharacterLink.mPostRotation);

    mFileObject->FieldWriteD("AXISLEN", pCharacterLink.mAxisLen);
    mFileObject->FieldWriteI("ORDER", pCharacterLink.mRotOrder);

	mFileObject->FieldWriteI("XMINENABLE", pCharacterLink.mRLimits.GetMinXActive());
    mFileObject->FieldWriteI("YMINENABLE", pCharacterLink.mRLimits.GetMinYActive());
    mFileObject->FieldWriteI("ZMINENABLE", pCharacterLink.mRLimits.GetMinZActive());

    mFileObject->FieldWriteI("XMAXENABLE", pCharacterLink.mRLimits.GetMaxXActive());
    mFileObject->FieldWriteI("YMAXENABLE", pCharacterLink.mRLimits.GetMaxYActive());
    mFileObject->FieldWriteI("ZMAXENABLE", pCharacterLink.mRLimits.GetMaxZActive());

	mFileObject->FieldWrite3D("MIN", pCharacterLink.mRLimits.GetMin().mData);
	mFileObject->FieldWrite3D("MAX", pCharacterLink.mRLimits.GetMax().mData);

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

int FbxWriterFbx6::WriteCharacterPose(FbxScene& pScene)
{
    int i, lCharacterPoseCount = pScene.GetCharacterPoseCount();

    if (lCharacterPoseCount == 0)
    {
        return 0;
    }

    for (i = 0; i < lCharacterPoseCount; i++)
    {
        FbxCharacterPose*  CharacterPose = pScene.GetCharacterPose(i);

        WriteObjectHeaderAndReferenceIfAny(*CharacterPose, FIELD_KFBXPOSE_POSE);
        mFileObject->FieldWriteBlockBegin();
		{
            mFileObject->FieldWriteBegin("PoseScene");
            mFileObject->FieldWriteBlockBegin();
			{
                WriteCharacterPose(*CharacterPose);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();
		}
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    return 1;
}


void FbxWriterFbx6::WriteCharacterPose(FbxCharacterPose& pCharacterPose)
{
    FbxExporter* lExporter = FbxExporter::Create(&mManager,"");
	lExporter->SetIOSettings( GetIOSettings() );
   
    // Store original values
    bool bModel     = IOS_REF.GetBoolProp(EXP_FBX_MODEL,			false);
    bool bMaterial  = IOS_REF.GetBoolProp(EXP_FBX_MATERIAL,			false);
    bool bTexture   = IOS_REF.GetBoolProp(EXP_FBX_TEXTURE,			false);
    bool bShape     = IOS_REF.GetBoolProp(EXP_FBX_SHAPE,			false);
    bool bGobo      = IOS_REF.GetBoolProp(EXP_FBX_GOBO,				false);
    bool bPivot     = IOS_REF.GetBoolProp(EXP_FBX_PIVOT,			false);
    bool bAnimation = IOS_REF.GetBoolProp(EXP_FBX_ANIMATION,		false);
    bool bSettings  = IOS_REF.GetBoolProp(EXP_FBX_GLOBAL_SETTINGS,	false);
    bool bEmbedded  = IOS_REF.GetBoolProp(EXP_FBX_EMBEDDED,			false);

    IOS_REF.SetBoolProp(EXP_FBX_MODEL,				false);
    IOS_REF.SetBoolProp(EXP_FBX_MATERIAL,			false);
    IOS_REF.SetBoolProp(EXP_FBX_TEXTURE,			false);
    IOS_REF.SetBoolProp(EXP_FBX_SHAPE,				false);
    IOS_REF.SetBoolProp(EXP_FBX_GOBO,				false);
    IOS_REF.SetBoolProp(EXP_FBX_PIVOT,				false);
    IOS_REF.SetBoolProp(EXP_FBX_ANIMATION,			false);
    IOS_REF.SetBoolProp(EXP_FBX_GLOBAL_SETTINGS,	false);
    IOS_REF.SetBoolProp(EXP_FBX_EMBEDDED,			false);

    bool lIsBeforeV6 = mFileObject->IsBeforeVersion6();
    mFileObject->SetIsBeforeVersion6(true);
    lExporter->Export(pCharacterPose.GetPoseScene(), mFileObject);
    mFileObject->SetIsBeforeVersion6(lIsBeforeV6);

    lExporter->Destroy();

    // Keep original values
    IOS_REF.SetBoolProp(EXP_FBX_MODEL,				bModel     );
    IOS_REF.SetBoolProp(EXP_FBX_MATERIAL,			bMaterial  );
    IOS_REF.SetBoolProp(EXP_FBX_TEXTURE,			bTexture   );
    IOS_REF.SetBoolProp(EXP_FBX_SHAPE,				bShape     );
    IOS_REF.SetBoolProp(EXP_FBX_GOBO,				bGobo      );
    IOS_REF.SetBoolProp(EXP_FBX_PIVOT,				bPivot	   );
    IOS_REF.SetBoolProp(EXP_FBX_ANIMATION,			bAnimation );
    IOS_REF.SetBoolProp(EXP_FBX_GLOBAL_SETTINGS,	bSettings  );
    IOS_REF.SetBoolProp(EXP_FBX_EMBEDDED,			bEmbedded  );
}

bool FbxWriterFbx6::WritePose(FbxScene& pScene)
{
    int i, lPoseCount = pScene.GetPoseCount();

    for (i = 0; i < lPoseCount; i++)
    {
        FbxPose* lPose = pScene.GetPose(i);
        char* lTypeStr = (char*)(lPose->IsBindPose() ? FIELD_KFBXPOSE_BIND_POSE : FIELD_KFBXPOSE_REST_POSE);

        WriteObjectHeaderAndReferenceIfAny(*lPose, FIELD_KFBXPOSE_POSE);
        mFileObject->FieldWriteBlockBegin();
        {
            mFileObject->FieldWriteS(FIELD_KFBXPOSE_TYPE, lTypeStr);
            mFileObject->FieldWriteI(FIELD_KFBXPOSE_VERSION, 100);

            // Properties
            WriteObjectPropertiesAndFlags(lPose);

            WritePose(*lPose);
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    return true;
}


void FbxWriterFbx6::WritePose(FbxPose& pPose)
{
    int count = pPose.GetCount();
    mFileObject->FieldWriteI("NbPoseNodes", count);

    for (int i = 0; i < count; i++)
    {
        mFileObject->FieldWriteBegin("PoseNode");
        mFileObject->FieldWriteBlockBegin();

        FbxNameHandler nameStruct = pPose.GetNodeName(i);
        const char* name = nameStruct.GetCurrentName();
        mFileObject->FieldWriteS("Node", FbxManager::PrefixName(MODEL_PREFIX, name));
		{
            mFileObject->FieldWriteDn("Matrix", (double*)&pPose.GetMatrix(i).mData[0][0], 16);
            if (!pPose.IsBindPose()) // for bindPoses the matrix is always global
                mFileObject->FieldWriteB("Local", pPose.IsLocalMatrix(i));
		}
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }
}
bool FbxWriterFbx6::WriteSelectionNode(FbxScene& pScene)
{
    int lSelectionNodeCount = pScene.GetMemberCount<FbxSelectionNode>();

    for (int i = 0; i < lSelectionNodeCount; i++)
    {
        FbxSelectionNode* lSelectionNode = pScene.GetMember<FbxSelectionNode>(i);

        WriteObjectHeaderAndReferenceIfAny(*lSelectionNode, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_SELECTIONNODE);

        mFileObject->FieldWriteBlockBegin();
        {
            mFileObject->FieldWriteI(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_SELECTIONNODE, 100);

            // Properties
            WriteObjectPropertiesAndFlags(lSelectionNode);

            WriteSelectionNode(*lSelectionNode);
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    return true;
}

void FbxWriterFbx6::WriteSelectionNode(FbxSelectionNode& pSelectionNode)
{
    const char* lName = pSelectionNode.GetName();
    if(pSelectionNode.GetSrcObject())
    {
        lName = pSelectionNode.GetSrcObject()->GetName();
    }
    mFileObject->FieldWriteS("Node", FbxManager::PrefixName(MODEL_PREFIX, lName));

    mFileObject->FieldWriteB("IsTheNodeInSet", pSelectionNode.mIsTheNodeInSet);

    //Write out vertex index array.
    int lVIndex, lVertexIndexCount = pSelectionNode.mVertexIndexArray.GetCount();
    if( lVertexIndexCount>0 )
    {
        mFileObject->FieldWriteBegin(FIELD_KFBXSELECTIONSET_VERTICE_INDEXARRAY);
        {
            for ( int vCount = 0; vCount < lVertexIndexCount; vCount++ )
            {
                lVIndex = pSelectionNode.mVertexIndexArray[vCount];
                mFileObject->FieldWriteI(lVIndex);
            }
        }
        mFileObject->FieldWriteEnd();
    }

    //Write out edge index array.
    int lEIndex, lEdgeIndexCount = pSelectionNode.mEdgeIndexArray.GetCount();
    if( lEdgeIndexCount>0 )
    {
        mFileObject->FieldWriteBegin(FIELD_KFBXSELECTIONSET_EDGE_INDEXARRAY);
        {
            for ( int eCount = 0; eCount < lEdgeIndexCount; eCount++ )
            {
                lEIndex = pSelectionNode.mEdgeIndexArray[eCount];
                mFileObject->FieldWriteI(lEIndex);
            }
        }
        mFileObject->FieldWriteEnd();
    }

    //Write out face index array.
    int lFIndex, lPolygonVertexIndexCount = pSelectionNode.mPolygonIndexArray.GetCount();
    if( lPolygonVertexIndexCount>0 )
    {
        mFileObject->FieldWriteBegin(FIELD_KFBXSELECTIONSET_POLYGONVERTICES_INDEXARRAY);
        {
            for ( int fCount = 0; fCount < lPolygonVertexIndexCount; fCount++ )
            {
                lFIndex = pSelectionNode.mPolygonIndexArray[fCount];
                mFileObject->FieldWriteI(lFIndex);
            }
        }
        mFileObject->FieldWriteEnd();
    }
}

bool FbxWriterFbx6::WriteSelectionSet(FbxScene& pScene)
{
    int i, lSelectionSetCount = pScene.GetMemberCount<FbxSelectionSet>();

    for (i = 0; i < lSelectionSetCount; i++)
    {
        FbxSelectionSet* lSelectionSet = (FbxSelectionSet*)pScene.GetMember<FbxSelectionSet>(i);

        WriteObjectHeaderAndReferenceIfAny(*lSelectionSet, FIELD_KFBXCOLLECTION_COLLECTION);
        
        mFileObject->FieldWriteBlockBegin();
        {
            mFileObject->FieldWriteI(FIELD_KFBXCOLLECTION_VERSION, 100);

            // Properties
            WriteObjectPropertiesAndFlags(lSelectionSet);

            WriteSelectionSet(*lSelectionSet);
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    return true;
}

void FbxWriterFbx6::WriteSelectionSet(FbxSelectionSet& pSelectionSet)
{
    int lCount = pSelectionSet.GetMemberCount();
    mFileObject->FieldWriteI("NbMembers", lCount);

    FbxArray<FbxSelectionNode*> lSelectionNodeList;
    FbxArray<FbxObject*> lDirectObjectList;
    pSelectionSet.GetSelectionNodesAndDirectObjects(lSelectionNodeList, lDirectObjectList);

    lCount = lSelectionNodeList.GetCount();
    for (int i = 0; i < lCount; i++)
    {
        mFileObject->FieldWriteS("Member", FbxManager::PrefixName(SELECTION_SET_NODE_PREFIX, lSelectionNodeList[i]->GetName()));
    }

    lCount = lDirectObjectList.GetCount();
    for (int i = 0; i < lCount; i++)
    {
        mFileObject->FieldWriteS("Member", FbxManager::PrefixName(MODEL_PREFIX, lDirectObjectList[i]->GetName()));
    }
}

int FbxWriterFbx6::FindString(FbxString pString, FbxArray<FbxString*>& pStringArray)
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

static bool IsEnhancedProperty(FbxProperty& pFbxProperty)
{
    // These are the known property types from previous versions; it's safer to
    // list the known types from before rather than to add the new ones here -- add a new
    // one, and it's automatically flagged as 'enhanced'
    FBX_ASSERT( pFbxProperty.IsValid() );

    switch( pFbxProperty.GetPropertyDataType().GetType() )
    {
        default:
            break;
        case eFbxUndefined :
        case eFbxBool :
        case eFbxInt :
        case eFbxFloat :
        case eFbxDouble :
        case eFbxDouble3 :
        case eFbxDouble4 :
        case eFbxDouble4x4 :
        case eFbxEnum :
        case eFbxString :
        case eFbxTime :
        case eFbxReference :
            return false;
    }

    return true;
}

void FbxWriterFbx6::WriteProperty(FbxProperty& lFbxProperty, bool lSetNodeAttributeFlag)
{
	FBX_ASSERT(lFbxProperty.IsValid());

	if( !lFbxProperty.IsValid() && (!mWriteEnhancedProperties && IsEnhancedProperty(lFbxProperty)) )
	{
	  return;
	}

	if( lFbxProperty.GetFlag(FbxPropertyFlags::eNotSavable) )
	{
	  // This property is flagged as ... not savable.  So, don't save it.
	  // Note: Need to skip over the children, too, and what about connections to this
	  // object?  I suppose they should be skipped as well.
	  return;
	}

	char  flags[6] = {0,0,0,0,0,0};
	char* pflags   = flags;

	mFileObject->FieldWriteBegin("Property");
	{	
        mFileObject->FieldWriteS( lFbxProperty.GetHierarchicalName() );

        const char* lType;

        if( lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) )
        {
                *pflags++ = 'A';
                // DataType
                lType = lFbxProperty.GetPropertyDataType().GetName();
                if (lFbxProperty.GetFlag(FbxPropertyFlags::eAnimated)) *pflags++ = '+';
        }
        else
        {
                // Not animatable types
                lType = FbxGetDataTypeNameForIO(lFbxProperty.GetPropertyDataType());
        }

        mFileObject->FieldWriteS( lType );

        if( mWriteEnhancedProperties )
        {
            // Space saving -- if it's the same name, don't bother writing the subtype,
            // but we still need to reserve the value field.
            // Do a case insensitive compare, so we don't have "float", "Float".
            const char* lSubType = lFbxProperty.GetPropertyDataType().GetName();
            mFileObject->FieldWriteS( FBXSDK_stricmp(lSubType, lType) ? lSubType : "" );
        }

        if (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)) *pflags++ = 'U';
        if (lFbxProperty.GetFlag(FbxPropertyFlags::eHidden)) 
            *pflags++ = 'H';

        // Node attributes: This flag indicates the property
        // is on the node attribute and not the node.
        if( lSetNodeAttributeFlag )
            *pflags++ = 'N';

        mFileObject->FieldWriteS( flags );

        // Store Value
        // ====================================

        switch(lFbxProperty.GetPropertyDataType().GetType()) {
            default:
            break;
            case eFbxChar:
                mFileObject->FieldWriteByte( lFbxProperty.Get<FbxChar>() );
                if(lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
                {
                    if(   (!lFbxProperty.HasMinLimit() || lFbxProperty.GetMinLimit() == -HUGE_VAL)
                        && (!lFbxProperty.HasMaxLimit() || lFbxProperty.GetMaxLimit() == HUGE_VAL) )
                    {
                        lFbxProperty.SetMinLimit(lFbxProperty.Get<FbxChar>());
                        lFbxProperty.SetMaxLimit(lFbxProperty.Get<FbxChar>());
                    }
                    mFileObject->FieldWriteByte((FbxChar) lFbxProperty.GetMinLimit());
                    mFileObject->FieldWriteByte((FbxChar) lFbxProperty.GetMaxLimit());
                }
            break;
            case eFbxUChar:
                mFileObject->FieldWriteUByte( lFbxProperty.Get<FbxUChar>() );
                if(lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
                {
                    if(   (!lFbxProperty.HasMinLimit() || lFbxProperty.GetMinLimit() == -HUGE_VAL)
                        && (!lFbxProperty.HasMaxLimit() || lFbxProperty.GetMaxLimit() == HUGE_VAL) )
                    {
                        lFbxProperty.SetMinLimit(lFbxProperty.Get<FbxUChar>());
                        lFbxProperty.SetMaxLimit(lFbxProperty.Get<FbxUChar>());
                    }
                    mFileObject->FieldWriteUByte((FbxUChar) lFbxProperty.GetMinLimit());
                    mFileObject->FieldWriteUByte((FbxUChar) lFbxProperty.GetMaxLimit());
                }
            break;
            case eFbxShort:
                mFileObject->FieldWriteShort( lFbxProperty.Get<FbxShort>() );
                if(lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
                {
                    if(   (!lFbxProperty.HasMinLimit() || lFbxProperty.GetMinLimit() == -HUGE_VAL)
                        && (!lFbxProperty.HasMaxLimit() || lFbxProperty.GetMaxLimit() == HUGE_VAL) )
                    {
                        lFbxProperty.SetMinLimit(lFbxProperty.Get<FbxShort>());
                        lFbxProperty.SetMaxLimit(lFbxProperty.Get<FbxShort>());
                    }
                    mFileObject->FieldWriteShort((FbxShort) lFbxProperty.GetMinLimit());
                    mFileObject->FieldWriteShort((FbxShort) lFbxProperty.GetMaxLimit());
                }
            break;
            case eFbxUShort:
                mFileObject->FieldWriteUShort( lFbxProperty.Get<FbxUShort>() );
                if(lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
                {
                    if(   (!lFbxProperty.HasMinLimit() || lFbxProperty.GetMinLimit() == -HUGE_VAL)
                        && (!lFbxProperty.HasMaxLimit() || lFbxProperty.GetMaxLimit() == HUGE_VAL) )
                    {
                        lFbxProperty.SetMinLimit(lFbxProperty.Get<FbxUShort>());
                        lFbxProperty.SetMaxLimit(lFbxProperty.Get<FbxUShort>());
                    }
                    mFileObject->FieldWriteUShort((FbxUShort) lFbxProperty.GetMinLimit());
                    mFileObject->FieldWriteUShort((FbxUShort) lFbxProperty.GetMaxLimit());
                }
            break;
            case eFbxUInt:
                mFileObject->FieldWriteUI( lFbxProperty.Get<FbxUInt>() );
                if(lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
                {
                    if(   (!lFbxProperty.HasMinLimit() || lFbxProperty.GetMinLimit() == -HUGE_VAL)
                        && (!lFbxProperty.HasMaxLimit() || lFbxProperty.GetMaxLimit() == HUGE_VAL) )
                    {
                        lFbxProperty.SetMinLimit(lFbxProperty.Get<FbxUInt>());
                        lFbxProperty.SetMaxLimit(lFbxProperty.Get<FbxUInt>());
                    }
                    mFileObject->FieldWriteUI((FbxUInt) lFbxProperty.GetMinLimit());
                    mFileObject->FieldWriteUI((FbxUInt) lFbxProperty.GetMaxLimit());
                }
            break;
            case eFbxULongLong:
                mFileObject->FieldWriteULL( lFbxProperty.Get<FbxULongLong>() );
                if(lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
                {
                    if(   (!lFbxProperty.HasMinLimit() || lFbxProperty.GetMinLimit() == -HUGE_VAL)
                        && (!lFbxProperty.HasMaxLimit() || lFbxProperty.GetMaxLimit() == HUGE_VAL) )
                    {
                        lFbxProperty.SetMinLimit(double(lFbxProperty.Get<FbxULongLong>()));
                        lFbxProperty.SetMaxLimit(double(lFbxProperty.Get<FbxULongLong>()));
                    }
                    mFileObject->FieldWriteULL((FbxULongLong) lFbxProperty.GetMinLimit());
                    mFileObject->FieldWriteULL((FbxULongLong) lFbxProperty.GetMaxLimit());
                }
            break;
            case eFbxHalfFloat:
                mFileObject->FieldWriteF( lFbxProperty.Get<FbxHalfFloat>().value() );
                if(lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
                {
                    if(   (!lFbxProperty.HasMinLimit() || lFbxProperty.GetMinLimit() == -HUGE_VAL)
                        && (!lFbxProperty.HasMaxLimit() || lFbxProperty.GetMaxLimit() == HUGE_VAL) )
                    {
                        lFbxProperty.SetMinLimit(lFbxProperty.Get<FbxHalfFloat>().value());
                        lFbxProperty.SetMaxLimit(lFbxProperty.Get<FbxHalfFloat>().value());
                    }
                    mFileObject->FieldWriteD((double) lFbxProperty.GetMinLimit());
                    mFileObject->FieldWriteD((double) lFbxProperty.GetMaxLimit());
                }
            break;

            case eFbxBool:
                mFileObject->FieldWriteB( lFbxProperty.Get<FbxBool>() );
            break;
            case eFbxInt:
                mFileObject->FieldWriteI( lFbxProperty.Get<FbxInt>() );
                if(lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
                {
                    if(   (!lFbxProperty.HasMinLimit() || lFbxProperty.GetMinLimit() == -HUGE_VAL)
                        && (!lFbxProperty.HasMaxLimit() || lFbxProperty.GetMaxLimit() == HUGE_VAL) )
                    {
                        lFbxProperty.SetMinLimit(lFbxProperty.Get<FbxInt>());
                        lFbxProperty.SetMaxLimit(lFbxProperty.Get<FbxInt>());
                    }
                    mFileObject->FieldWriteI((int) lFbxProperty.GetMinLimit());
                    mFileObject->FieldWriteI((int) lFbxProperty.GetMaxLimit());
                }
            break;
            case eFbxLongLong:
                mFileObject->FieldWriteLL( lFbxProperty.Get<FbxLongLong>() );
                if(lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
                {
                    if(   (!lFbxProperty.HasMinLimit() || lFbxProperty.GetMinLimit() == -HUGE_VAL)
                        && (!lFbxProperty.HasMaxLimit() || lFbxProperty.GetMaxLimit() == HUGE_VAL) )
                    {
                        lFbxProperty.SetMinLimit(double(lFbxProperty.Get<FbxLongLong>()));
                        lFbxProperty.SetMaxLimit(double(lFbxProperty.Get<FbxLongLong>()));
                    }
                    mFileObject->FieldWriteLL((FbxLongLong) lFbxProperty.GetMinLimit());
                    mFileObject->FieldWriteLL((FbxLongLong) lFbxProperty.GetMaxLimit());
                }
            break;
            case eFbxFloat:
                mFileObject->FieldWriteF( lFbxProperty.Get<FbxFloat>() );
            break;
            case eFbxDouble:
                mFileObject->FieldWriteD( lFbxProperty.Get<FbxDouble>() );
                if(lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
                {
                    if(   (!lFbxProperty.HasMinLimit() || lFbxProperty.GetMinLimit() == -HUGE_VAL)
                        && (!lFbxProperty.HasMaxLimit() || lFbxProperty.GetMaxLimit() == HUGE_VAL) )
                    {
                        lFbxProperty.SetMinLimit(lFbxProperty.Get<FbxDouble>());
                        lFbxProperty.SetMaxLimit(lFbxProperty.Get<FbxDouble>());
                    }
                    mFileObject->FieldWriteD(lFbxProperty.GetMinLimit());
                    mFileObject->FieldWriteD(lFbxProperty.GetMaxLimit());
                }

            break;
            case eFbxDouble2: {
				FbxDouble2 lDouble2 = lFbxProperty.Get<FbxDouble2>();
                mFileObject->FieldWriteDn((double*)&lDouble2, 2 );
            } break;
            case eFbxDouble3: {
				FbxDouble3 lDouble3 = lFbxProperty.Get<FbxDouble3>();
                mFileObject->FieldWrite3D((double*)&lDouble3 );
            } break;
            case eFbxDouble4: {
				FbxDouble4 lDouble4 = lFbxProperty.Get<FbxDouble4>();
                mFileObject->FieldWrite4D((double*)&lDouble4 );
            } break;
            case eFbxDouble4x4: {
				FbxDouble4x4 lDouble44 = lFbxProperty.Get<FbxDouble4x4>();
                mFileObject->FieldWrite4D((double*)&lDouble44[0] );
                mFileObject->FieldWrite4D((double*)&lDouble44[1] );
                mFileObject->FieldWrite4D((double*)&lDouble44[2] );
                mFileObject->FieldWrite4D((double*)&lDouble44[3] );
            } break;
            case eFbxEnumM:
            case eFbxEnum:
                mFileObject->FieldWriteI( lFbxProperty.Get<FbxEnum>() );
            break;
            case eFbxString:
                mFileObject->FieldWriteS( lFbxProperty.Get<FbxString>() );
            break;
            case eFbxTime:
                mFileObject->FieldWriteT( lFbxProperty.Get<FbxTime>() );
            break;
            case eFbxReference:  // used as a port entry to reference object or properties
            break;
            case eFbxBlob:
            {
                FBX_ASSERT( mWriteEnhancedProperties );

                // Because of limitations to the FBX6 ASCII parser, we have to
                // break this into chunks.  It's largely useless for Binary
                // formats, but it keeps the logic similar without adding too much
                // overhead to the binary file.
                //
                // But to help everyone involved in parsing this back, we'll write the
                // total blob size as the field value.  NOTE THAT THIS SIZE WILL BE
                // SMALLER THAN THE PAYLOAD IN THE ASCII VERSION BECAUSE OF THE ENCODING.
                //
                // (And in Binary the size will be written twice -- once in this field,
                // and another time with the 'blob' itself (FieldWriteR writes it as
                // a prefix).  Can't please everyone, and I thought it wouldn't make
                // much sense to write the size on everyline of the ASCII version.
                FbxBlob lBlob = lFbxProperty.Get<FbxBlob>();

                mFileObject->FieldWriteI(lBlob.Size());
                mFileObject->FieldWriteBlockBegin();
				{
                    const char* lOutputBuffer = reinterpret_cast<const char*>(lBlob.Access());
                    int lOutputSize           = lBlob.Size();

                    const int kMaxChunkSize = mFileObject->GetFieldRMaxChunkSize();
                    FBX_ASSERT(kMaxChunkSize > 0);

                    if( lOutputSize > 0 )
                    {
                        mFileObject->FieldWriteBegin("BinaryData");

                        while( lOutputSize > 0 )
                        {
                            int lChunkSize = (lOutputSize >= kMaxChunkSize) ? kMaxChunkSize : lOutputSize;

                            mFileObject->FieldWriteR(lOutputBuffer, lChunkSize);

                            lOutputBuffer += lChunkSize;
                            lOutputSize   -= lChunkSize;
                        }

                        mFileObject->FieldWriteEnd();
                    }
				}
                mFileObject->FieldWriteBlockEnd();

                break;
            }
            case eFbxDistance: {
                FbxDistance lDistance = lFbxProperty.Get<FbxDistance>();
                mFileObject->FieldWriteF(lDistance.value());
                mFileObject->FieldWriteS(lDistance.unitName());
            } break;
            case eFbxDateTime:
            {
                FbxDateTime lDateTime = lFbxProperty.Get<FbxDateTime>();
                mFileObject->FieldWriteC(lDateTime.toString());

                break;
            }

        }

        if ((lFbxProperty.GetPropertyDataType().GetType()==eFbxEnum || lFbxProperty.GetPropertyDataType().GetType()==eFbxEnumM) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
        {
            // Enum list
            // ===========
            FbxString EnumString;
            for (int i=0; i<lFbxProperty.GetEnumCount(); i++) {
                EnumString += lFbxProperty.GetEnumValue(i);
                if (i<lFbxProperty.GetEnumCount()-1) EnumString += "~";
            }
            mFileObject->FieldWriteS( EnumString );
        }
	}
	mFileObject->FieldWriteEnd();
}

bool FbxWriterFbx6::WriteObjectProperties(FbxObject* pObject)
{
    FbxArray<FbxProperty> lStreamProperties;
    FbxObject* lReferencedObject = pObject->GetReferenceTo();

    const FbxObject* lPropertyObject = pObject;
    FbxProperty lFbxProperty = lPropertyObject->GetFirstProperty();
    FbxProperty lFbxNextProperty;
    const FbxNodeAttribute* const lNodeAttribute = pObject->Is<FbxNode>() && pObject->GetSrcObjectCount<FbxNodeAttribute>() > 0 ? pObject->GetSrcObject<FbxNodeAttribute>() : NULL;

    // reset the iterators
    lPropertyObject  = pObject;
    lFbxProperty     = lPropertyObject->GetFirstProperty();
    lFbxNextProperty = FbxProperty();

    // Patch for node attribute;
    mFileObject->FieldWriteBegin(mWriteEnhancedProperties ? sENHANCED_PROPERITIES : sLEGACY_PROPERTIES);
    mFileObject->FieldWriteBlockBegin();

    bool lSetNodeAttributeFlag = false;
    bool lStartUsingNodeAttributeFlag = false;
    while (lFbxProperty!=0) 
	{
        lFbxNextProperty = lPropertyObject->GetNextProperty(lFbxProperty);

        // Node attributes
        
        if (!lFbxNextProperty.IsValid() && lNodeAttribute && lNodeAttribute!=lPropertyObject && !IsStorableNodeAttribute(lNodeAttribute) ) 
		{
            lStartUsingNodeAttributeFlag = true;
            lPropertyObject = lNodeAttribute;
            lFbxNextProperty= lPropertyObject->GetFirstProperty();
        }

        // NOTE: We should also check if the node attribute is referenced and skip those ones too.
        // Skipping properties that equals the ones in the referenced object if any
        if (lReferencedObject)
        {
            bool            lDoContinue = false;

            FbxIterator<FbxProperty>  lFbxReferencePropertyIter(lReferencedObject);
            FbxProperty                lFbxReferenceProperty;

            FbxForEach(lFbxReferencePropertyIter,lFbxReferenceProperty) 
			{
                if (lFbxProperty.GetName() == lFbxReferenceProperty.GetName() ) 
				{
                    if (lFbxProperty.CompareValue(lFbxReferenceProperty))
                    {
                        lDoContinue = true;
                        break;
                    }
                }
            }
            if (lDoContinue) 
			{
                lFbxProperty = lFbxNextProperty;
                continue;
            }
        }

        // Skipping a default value property.
        if (mWriteNonDefaultPropertiesOnly && FbxProperty::HasDefaultValue(lFbxProperty))
        {
            lFbxProperty = lFbxNextProperty;
            continue;   //Do not write property
        }

        WriteProperty(lFbxProperty, lSetNodeAttributeFlag);
        if (lStartUsingNodeAttributeFlag)
        {
            lSetNodeAttributeFlag = true;
            lStartUsingNodeAttributeFlag = false;
        }
        lFbxProperty = lFbxNextProperty;
    }

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    return true;
}

bool FbxWriterFbx6::WriteObjectPropertiesAndFlags(FbxObject* pObject)
{
    FBX_ASSERT( pObject );

    if (mProgress && !mProgressPause)
    {
        mProgress->Update(1.0f, pObject->GetName());
    }

    return WriteObjectProperties(pObject);
}

bool FbxWriterFbx6::WriteNode(FbxNode& pNode)
{
    if (pNode.GetObjectFlags(FbxObject::eSavable))
    {
        WriteNodeBegin(pNode);

        WriteNodeParameters(pNode);

        WriteNodeEnd(pNode);
    }

    return (true);
}

bool FbxWriterFbx6::WriteNodeBegin(FbxNode& pNode)
{
    WriteObjectHeaderAndReferenceIfAny(pNode, FIELD_KFBXNODE_MODEL);
    mFileObject->FieldWriteBlockBegin ();

    return true;
}

bool FbxWriterFbx6::WriteNodeEnd(FbxNode& pNode)
{
    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();

    return true;
}

bool FbxWriterFbx6::WriteNodeParameters(FbxNode& pNode)
{
    WriteNodeVersion(pNode);

    WriteNodeProperties(pNode);

    WriteNodeAnimationSettings(pNode);

    WriteNodeShading(pNode);

    WriteNodeCullingType(pNode);    

    if (!IsStorableNodeAttribute(pNode.GetNodeAttribute ())) 
	{
        // Note: We need to know for which node we are writing
        // the node attribute for, since the attribute can now
        // be attached to multiple nodes.
        mCurrentNode = &pNode;

        WriteNodeAttribute(pNode.GetNodeAttribute());

        // Clear this variable as soon as we're done with it.
        mCurrentNode = NULL;
    }

    return true;
}


bool FbxWriterFbx6::WriteNodeVersion(FbxNode& pNode)
{
    // Changed from version 231 to 232
    // - release of MOTIONBUILDER 7.0 with patch for InheritType
    // Changed from version 230 to 231
    // - release of MOTIONBUILDER 6.0 with new layering (texture/material/normal/etc)
    // Changed from version 195 to 230
    // - release of MOTIONBUILDER 5.0
    //
    // Changed from version 194 to 195
    // - Support for Negative Shape (this was never release in the FBX SDK)
    //
    // Changed from version 193 to 194:
    // - Add a version number for the pivot + enable state
    //   of each source/destination pivots
    //
    // Changed from version 192 to 193 to introduce new field
    // "Animated". Translation, rotation and scaling are always
    // animated but all others are optional.
    //
    mFileObject->FieldWriteI(FIELD_KFBXNODE_VERSION, 232);

    return (true);
}


bool FbxWriterFbx6::WriteNodeShading(FbxNode& pNode)
{
    switch(pNode.GetShadingMode())
    {
        case FbxNode::eHardShading:
            // This is the default value, don't write it.
            // mFileObject->FieldWriteCH(FIELD_KFBXNODE_SHADING, 'L');
            break;

        case FbxNode::eWireFrame:
            mFileObject->FieldWriteCH(FIELD_KFBXNODE_SHADING, 'W');
            break;

        case FbxNode::eFlatShading:
            mFileObject->FieldWriteCH(FIELD_KFBXNODE_SHADING, 'F');
            break;

        case FbxNode::eLightShading:
            mFileObject->FieldWriteCH(FIELD_KFBXNODE_SHADING, 'Y');
            break;

        case FbxNode::eTextureShading:
            mFileObject->FieldWriteCH(FIELD_KFBXNODE_SHADING, 'T');
            break;

        case FbxNode::eFullShading:
            mFileObject->FieldWriteCH(FIELD_KFBXNODE_SHADING, 'U');
            break;

        default:
            FBX_ASSERT_NOW("Invalid shading mode.");
            break;
    }

    return (true);
}


bool FbxWriterFbx6::WriteNodeAnimationSettings(FbxNode& pNode)
{
    mFileObject->FieldWriteB("MultiLayer", false);
    mFileObject->FieldWriteI("MultiTake", 0);

    return true;
}


bool FbxWriterFbx6::WriteNodeCullingType(FbxNode& pNode)
{
    switch(pNode.mCullingType)
    {
		case FbxNode::eCullingOff:
			mFileObject->FieldWriteC(FIELD_KFBXNODE_CULLING_TYPE, TOKEN_KFBXNODE_CULLING_OFF);
			break;

		case FbxNode::eCullingOnCCW:
			mFileObject->FieldWriteC(FIELD_KFBXNODE_CULLING_TYPE, TOKEN_KFBXNODE_CULLING_ON_CCW);
			break;

		case FbxNode::eCullingOnCW:
			mFileObject->FieldWriteC(FIELD_KFBXNODE_CULLING_TYPE, TOKEN_KFBXNODE_CULLING_ON_CW);
			break;

		default:
			mFileObject->FieldWriteC(FIELD_KFBXNODE_CULLING_TYPE, TOKEN_KFBXNODE_CULLING_OFF);
			break;
		}

		return true;
}


bool FbxWriterFbx6::WriteNodeAttribute(FbxNodeAttribute* pNodeAttribute)
{
    //
    // Store Geometry if applicable.
    //

    if (pNodeAttribute != NULL)
    {
        // Storing only savable flagged objects
        if( !pNodeAttribute->GetObjectFlags(FbxObject::eSavable) )
            return true;

        if (!pNodeAttribute->ContentIsLoaded())
            pNodeAttribute->ContentLoad();

        bool lWriteNodeAttributeName = true;

        if (IOS_REF.GetBoolProp(EXP_FBX_MODEL, true))
        {
            switch (pNodeAttribute->GetAttributeType())
            {
                case FbxNodeAttribute::eLODGroup:
                    // nothing extra to do. This attribute only contains properties
                    // who gets saved automatically!
					break;

                case FbxNodeAttribute::eSubDiv:
					WriteSubdiv( *(FbxSubDiv*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eNull:
					WriteNull( (FbxNull*)pNodeAttribute);
					break;

                case FbxNodeAttribute::eMarker:
                    if( pNodeAttribute->GetNode() )
					{
                        WriteMarker( *pNodeAttribute->GetNode());
					}
					break;

                case FbxNodeAttribute::eCamera:
					WriteCamera(*(FbxCamera*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eCameraStereo:
					WriteCameraStereo(*(FbxCameraStereo*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eCameraSwitcher:
					WriteCameraSwitcher(*(FbxCameraSwitcher*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eLight:
					WriteLight(*(FbxLight*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eSkeleton:
					WriteSkeleton(*(FbxSkeleton*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eMesh:
					WriteMesh(*(FbxMesh*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eNurbs:
					WriteNurb(*(FbxNurbs*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eNurbsSurface:
					WriteNurbsSurface(*(FbxNurbsSurface*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eTrimNurbsSurface:
					WriteTrimNurbsSurface(*(FbxTrimNurbsSurface*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::ePatch:
					WritePatch(*(FbxPatch*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eNurbsCurve:
					WriteNurbsCurve( *(FbxNurbsCurve*)(pNodeAttribute) );
					break;

                case FbxNodeAttribute::eBoundary:
					WriteBoundary( *(FbxBoundary*)(pNodeAttribute) );
					break;

                case FbxNodeAttribute::eShape:
                    break;

                default:
					FBX_ASSERT_NOW("Unknown node attribute type!");
                    lWriteNodeAttributeName = false;
                    break;
            }

        }
        else
        {
            // We don't export the geometry. But we still whant the animation
            // and properites on Lights and cameras and LodGroups
            switch (pNodeAttribute->GetAttributeType ())
            {
                case FbxNodeAttribute::eLODGroup:
                    // nothing extra to do. This attribute only contains properties
                    // who gets saved automatically!
					break;

                case FbxNodeAttribute::eNull:
                    WriteNull( (FbxNull*)pNodeAttribute );
					break;

                case FbxNodeAttribute::eMarker:
                    if( pNodeAttribute->GetNode() )
					{
                        WriteMarker( *pNodeAttribute->GetNode() );
					}
					break;

                case FbxNodeAttribute::eCamera:
					WriteCamera(*(FbxCamera*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eLight:
					WriteLight(*(FbxLight*)(pNodeAttribute));
					break;

                case FbxNodeAttribute::eSkeleton:
					WriteSkeleton(*(FbxSkeleton*)(pNodeAttribute));
					break;                

                default:
					WriteNull(NULL);
					lWriteNodeAttributeName = false;
					break;
            }
        }

        if( lWriteNodeAttributeName )
        {
            if(pNodeAttribute->GetNodeCount() > 1 && (pNodeAttribute->GetNode(0) && pNodeAttribute->GetNode(0)->GetGeometry() != NULL))
            {
                FBX_ASSERT_NOW("Geometry Instancing is not supported in fbx 6 file format!!!");
            }
            mFileObject->FieldWriteS( FIELD_NODE_ATTRIBUTE_NAME, pNodeAttribute->GetNameWithNameSpacePrefix() );

            FbxObject* lReferenceTo = pNodeAttribute->GetReferenceTo();

            if( lReferenceTo )
            {
                FbxString lRefName;

                if ( mDocumentReferences && mDocumentReferences->GetReferenceName(lReferenceTo, lRefName) )
                {
                    mFileObject->FieldWriteS( FIELD_NODE_ATTRIBUTE_REFTO, lRefName );
                }
            }
        }

        //as we write shape during writing mesh, Don't unload shape here. 
        if(pNodeAttribute->GetAttributeType () != FbxNodeAttribute::eShape)
            pNodeAttribute->ContentUnload();
    }
    else
    {
        WriteNull (NULL);
    }

    return (true);
}


bool FbxWriterFbx6::WriteNodeProperties(FbxNode& pNode)
{
    pNode.UpdatePropertiesFromPivotsAndLimits();
    WriteObjectPropertiesAndFlags(&pNode);
    return true;
}


bool FbxWriterFbx6::WriteGeometry(FbxGeometry& pGeometry)
{
    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYMESH_GEOMETRY_VERSION, 124);

    FbxMultiMap lLayerIndexSet;

    if (pGeometry.GetAttributeType() == FbxNodeAttribute::eMesh)
    {
        WriteFbxLayerElementNormals(pGeometry, lLayerIndexSet);
        WriteFbxLayerElementBinormals(pGeometry, lLayerIndexSet);
        WriteFbxLayerElementTangents(pGeometry, lLayerIndexSet);
        WriteFbxLayerElementPolygonGroups(pGeometry, lLayerIndexSet);
        WriteFbxLayerElementVertexColors(pGeometry, lLayerIndexSet);
        WriteFbxLayerElementUVs(pGeometry, lLayerIndexSet);
        WriteFbxLayerElementSmoothing(pGeometry, lLayerIndexSet);
        WriteFbxLayerElementUserData(pGeometry, lLayerIndexSet);
        WriteFbxLayerElementVisibility(pGeometry, lLayerIndexSet);
        WriteFbxLayerElementEdgeCrease(pGeometry, lLayerIndexSet);
        WriteFbxLayerElementVertexCrease(pGeometry, lLayerIndexSet);
        WriteFbxLayerElementHole(pGeometry, lLayerIndexSet);
    }

    WriteFbxLayerElementMaterials(pGeometry, lLayerIndexSet);
    WriteFbxLayerElementTextures(pGeometry, lLayerIndexSet);
    WriteLayers(pGeometry, lLayerIndexSet);

    // the shapes on a trim nurb will be written out with the untrimmed surface
    // don't write them out here.
    if( pGeometry.GetAttributeType() != FbxNodeAttribute::eTrimNurbsSurface )
    {
		// Only write out the first shape of each blend shape channel.
		// Because we did not support In-between blend shape for version 6 FBX file. 
		int lBlendShapeDeformerCount = pGeometry.GetDeformerCount(FbxDeformer::eBlendShape);
		for(int lBlendShapeIndex = 0; lBlendShapeIndex<lBlendShapeDeformerCount; ++lBlendShapeIndex)
		{
			FbxBlendShape* lBlendShape = (FbxBlendShape*)pGeometry.GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);
			int lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();
			for(int lChannelIndex = 0; lChannelIndex<lBlendShapeChannelCount; ++lChannelIndex)
			{
				FbxBlendShapeChannel* lChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);
				FbxShape* lShape = lChannel->GetTargetShape(0);
				if(lShape)
				{
					WriteShape(*lShape, lShape->GetName(), pGeometry);
				}
			}
		}
	}

    return true;
}

bool FbxWriterFbx6::WriteNodeType(FbxNode& pNode)
{
    mFileObject->FieldWriteBegin(FIELD_KFBXNODE_TYPE_FLAGS);
    {       
        for (int i=0; i<pNode.GetTypeFlags().GetCount(); i++)
        {
            mFileObject->FieldWriteC(pNode.GetTypeFlags()[i]);
        }
    }
    mFileObject->FieldWriteEnd();

    return true;
}

bool FbxWriterFbx6::WriteSubdiv(FbxSubDiv& pSubdiv)
{
    //
    // Store what is needed for a subdiv
    //
    //TO DO: write more subdiv property to fbx

    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYSUBDIV_GEOMETRY_VERSION, 100);

    int lLevelCount = pSubdiv.GetLevelCount();
    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYSUBDIV_LEVEL_COUNT, lLevelCount);

    int lCurrentLevel = pSubdiv.GetCurrentLevel();
    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYSUBDIV_CURRENT_LEVEL, lCurrentLevel);

	int lDisplaySmoothness = int(pSubdiv.GetDisplaySmoothness());
	mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYSUBDIV_DISPLAY_SMOOTHNESS, lDisplaySmoothness);

    return true;

}

bool FbxWriterFbx6::WriteNull(FbxNull* pNull)
{
    double lSize = 100.0; 
    FbxStringList lTypeFlags;
    
    if (pNull)
    {
        lSize      = pNull->Size.Get();        
        lTypeFlags = pNull->GetTypeFlags();
    }
    else
    {
        lTypeFlags.Add("Null");
    }

    mFileObject->FieldWriteBegin(FIELD_KFBXNODE_TYPE_FLAGS);
    {
        for (int i=0; i<lTypeFlags.GetCount(); i++)
        {
            mFileObject->FieldWriteC(lTypeFlags[i]);
        }
    }
    mFileObject->FieldWriteEnd();

    return true;
}

bool FbxWriterFbx6::WriteMarker(FbxNode& pNode)
{
    return WriteNodeType(pNode);
}

bool FbxWriterFbx6::WriteCamera(FbxCamera& pCamera)
{
    if (!pCamera.GetNode()) return false;

    WriteNodeType(*pCamera.GetNode());

    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYCAMERA_GEOMETRY_VERSION, 124);

    // Camera Position and Orientation

    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_POSITION);
        FbxVector4 lVector = pCamera.Position.Get();
        mFileObject->FieldWriteD(lVector [0]);
        mFileObject->FieldWriteD(lVector [1]);
        mFileObject->FieldWriteD(lVector [2]);
    mFileObject->FieldWriteEnd ();

    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYCAMERA_CAMERA_UP_VECTOR);
        lVector = pCamera.UpVector.Get();
        mFileObject->FieldWriteD(lVector [0]);
        mFileObject->FieldWriteD(lVector [1]);
        mFileObject->FieldWriteD(lVector [2]);
    mFileObject->FieldWriteEnd ();

    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYCAMERA_DEFAULT_CAMERA_INTEREST_POSITION);
        lVector = pCamera.InterestPosition.Get();
        mFileObject->FieldWriteD(lVector [0]);
        mFileObject->FieldWriteD(lVector [1]);
        mFileObject->FieldWriteD(lVector [2]);
    mFileObject->FieldWriteEnd ();

    // Camera View Options

    mFileObject->FieldWriteB(FIELD_KFBXGEOMETRYCAMERA_SHOW_INFO_ON_MOVING, pCamera.ShowInfoOnMoving.Get());
    mFileObject->FieldWriteB(FIELD_KFBXGEOMETRYCAMERA_SHOW_AUDIO, pCamera.ShowAudio.Get());

    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYCAMERA_AUDIO_COLOR);
        FbxVector4 lColor = pCamera.AudioColor.Get();
        mFileObject->FieldWriteD(lColor[0]);
        mFileObject->FieldWriteD(lColor[1]);
        mFileObject->FieldWriteD(lColor[2]);
    mFileObject->FieldWriteEnd ();

    mFileObject->FieldWriteD(FIELD_KFBXGEOMETRYCAMERA_ORTHO_ZOOM, pCamera.OrthoZoom.Get());

    return true;
}

bool FbxWriterFbx6::WriteCameraStereo(FbxCameraStereo& pCameraStereo)
{
    if(!pCameraStereo.GetNode()) return false;

    WriteNodeType(*pCameraStereo.GetNode());

    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYCAMERA_STEREO_VERSION, 100);

    bool lStatus = true;
    bool lEmbeddedMedia;
    lEmbeddedMedia = IOS_REF.GetBoolProp(EXP_FBX_EMBEDDED, false) && mFileObject->IsBinary();
   
    if (lEmbeddedMedia)
    {       
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYCAMERA_STEREO_PRECOMP_FILE_CONTENT);
        lStatus = (mFileObject->FieldWriteEmbeddedFile(pCameraStereo.PrecompFileName.Get(), pCameraStereo.RelativePrecompFileName.Get()) ? true : false);
        mFileObject->FieldWriteEnd();
    }

    FbxCamera* lLeftCam = pCameraStereo.GetLeftCamera();
    FbxCamera* lRightCam = pCameraStereo.GetRightCamera();
    if(lLeftCam)
        WriteCamera(*lLeftCam);
    if(lRightCam)
        WriteCamera(*lRightCam);

    return true;
}

bool FbxWriterFbx6::WriteCameraSwitcher(FbxCameraSwitcher& pCameraSwitcher)
{
    mFileObject->FieldWriteI (FIELD_KFBXNODE_VERSION, 101);
    mFileObject->FieldWriteC (FIELD_KFBXGEOMETRYCAMERASWITCHER_NAME, "Model::Camera Switcher");
    mFileObject->FieldWriteI (FIELD_KFBXGEOMETRYCAMERASWITCHER_CAMERA_ID, pCameraSwitcher.GetDefaultCameraIndex());
    mFileObject->FieldWriteI (FIELD_KFBXGEOMETRYCAMERASWITCHER_CAMERA_NAME, 100);

    // Additional field useful to merge scenes in FiLMBOX.
    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYCAMERASWITCHER_CAMERA_INDEX_NAME);

    for( int i = 0; i < int(pCameraSwitcher.GetCameraNameCount()); i++ )
    {
        mFileObject->FieldWriteS(pCameraSwitcher.GetCameraName(i));
    }
    mFileObject->FieldWriteEnd();
    return true;
}


bool FbxWriterFbx6::WriteLight(FbxLight& pLight)
{
    mFileObject->FieldWriteC(FIELD_KFBXNODE_TYPE_FLAGS, "Light");
    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYLIGHT_GEOMETRY_VERSION, 124);
    return true;
}

bool FbxWriterFbx6::WriteMeshSmoothness(FbxMesh& pMesh)
{
    FbxMesh::ESmoothness lSmoothness = FbxMesh::eHull;
    lSmoothness = pMesh.GetMeshSmoothness();

    if(lSmoothness > FbxMesh::eRough)
    {
        int lPreviewDivisionLevels = 0;
        int lRenderDivisionLevels = 0;
        double lContinuity = 1.0;
        bool lDisplaySubdivisions = false;
        bool lPreserveBorders = false;
        bool lPreserveHardEdges = false;
        bool lPropagateEdgeHardness = false;
        FbxMesh::EBoundaryRule lBoundaryRule = FbxMesh::eLegacy;

        lPreviewDivisionLevels = pMesh.GetMeshPreviewDivisionLevels();
        lRenderDivisionLevels = pMesh.GetMeshRenderDivisionLevels();
        lDisplaySubdivisions = pMesh.GetDisplaySubdivisions();
        lBoundaryRule = pMesh.GetBoundaryRule();
        lPreserveBorders = pMesh.GetPreserveBorders();
        lPreserveHardEdges = pMesh.GetPreserveHardEdges();
        lPropagateEdgeHardness = pMesh.GetPropagateEdgeHardness();

        //write smoothness
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_SMOOTHNESS);
        {
            mFileObject->FieldWriteI((int)lSmoothness);
        }
        mFileObject->FieldWriteEnd ();

        //write preview division level
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_PREVIEW_DIVSION_LEVELS);
        {
            mFileObject->FieldWriteI(lPreviewDivisionLevels);
        }
        mFileObject->FieldWriteEnd ();
        
        //write render division level
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_RENDER_DIVSION_LEVELS);
        {
            mFileObject->FieldWriteI(lRenderDivisionLevels);
        }
        mFileObject->FieldWriteEnd ();

        //write display subdivisions
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_DISPLAY_SUBDIVISIONS);
        {
            mFileObject->FieldWriteB(lDisplaySubdivisions);
        }
        mFileObject->FieldWriteEnd ();

        //write BoundaryRule
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_BOUNDARY_RULE);
        {
            mFileObject->FieldWriteI((int)lBoundaryRule);
        }
        mFileObject->FieldWriteEnd ();

        //write preserve borders
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_PRESERVE_BORDERS);
        {
            mFileObject->FieldWriteB(lPreserveBorders);
        }
        mFileObject->FieldWriteEnd ();

        //write preserve hardEdges
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_PRESERVE_HARD_EDGES);
        {
            mFileObject->FieldWriteB(lPreserveHardEdges);
        }
        mFileObject->FieldWriteEnd ();

        //write propagate EdgeHardness
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_PROPAGATE_EDGE_HARDNESS);
        {
            mFileObject->FieldWriteB(lPropagateEdgeHardness);
        }
        mFileObject->FieldWriteEnd ();

    }

    return true;

}


bool FbxWriterFbx6::WriteMeshVertices(FbxMesh& pMesh)
{   
    FbxAMatrix    lPivot;
    FbxVector4    lSrcPoint;
    FbxVector4    lDestPoint;

    pMesh.GetPivot(lPivot);

    //
    // Vertices
    //
    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_VERTICES);
    {
        for( int lCount = 0; lCount < pMesh.GetControlPointsCount(); lCount++ )
        {
            lSrcPoint = pMesh.GetControlPoints()[lCount];
            lDestPoint = lPivot.MultT(lSrcPoint);

            mFileObject->FieldWriteD(lDestPoint.mData [0]);
            mFileObject->FieldWriteD(lDestPoint.mData [1]);
            mFileObject->FieldWriteD(lDestPoint.mData [2]);
        }
    }
    mFileObject->FieldWriteEnd (); 

    return true;
}

bool FbxWriterFbx6::WriteMeshPolyVertexIndex(FbxMesh& pMesh)
{
    int NextPolygonIndex = 1;
    int Index;
    int Count;

    if( pMesh.GetPolygonCount() )
    {
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_POLYGON_INDEX);
        {
            for (Count = 0; Count < pMesh.mPolygonVertices.GetCount(); Count ++)
            {
                Index = pMesh.mPolygonVertices[Count];

                // Set a polygon vertex to it's complement to mark the end of a polygon.

                // All polygons except the last one.
                if (NextPolygonIndex < pMesh.GetPolygonCount())
                {
                    // Last polygon vertex in the current polygon
                    if (Count == (pMesh.GetPolygonVertexIndex(NextPolygonIndex) - 1))
                    {
                        Index = -Index - 1;
                        NextPolygonIndex ++;
                    }
                }
                // Last polygon vertex of the last polygon.
                else if (Count == pMesh.mPolygonVertices.GetCount() - 1)
                {
                     Index = -Index-1;
                }

                mFileObject->FieldWriteI(Index);
            }
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx6::WriteFbxLayerElementNormals(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eNormal);

    for (i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementNormal* lLayerElementNormal = pLayerContainer.GetLayer(i, FbxLayerElement::eNormal)->GetNormals();

        pLayerIndexSet.Add((FbxHandle)lLayerElementNormal, i);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_NORMAL);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElementNormal->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElementNormal->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElementNormal->GetReferenceMode()));

                mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_NORMALS);
                {
                    unsigned int j, lNormalCount = lLayerElementNormal->GetDirectArray().GetCount();
                    FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementNormal->GetDirectArray();

                    for (j=0; j < lNormalCount; j++)
                    {
                        FbxVector4 v = lDirectArray.GetAt(j);
                        mFileObject->FieldWriteD( v.mData[0] );
                        mFileObject->FieldWriteD( v.mData[1] );
                        mFileObject->FieldWriteD( v.mData[2] );
                    }
                }
                mFileObject->FieldWriteEnd();

                // Write the index if the mapping type is index to direct
                if (lLayerElementNormal->GetReferenceMode() != FbxLayerElement::eDirect)
                {
                    unsigned int lCount = lLayerElementNormal->GetIndexArray().GetCount();
                    if (lCount > 0)
                    {
                        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_NORMALS_INDEX);

                        unsigned int j;
                        FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementNormal->GetIndexArray();

                        for (j=0; j<lCount; j++)
                        {
                            mFileObject->FieldWriteI(lIndexArray.GetAt(j));
                        }
                        mFileObject->FieldWriteEnd();
                    }
                }

            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx6::WriteFbxLayerElementBinormals(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eBiNormal);

    for (i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementBinormal* lLayerElementBinormal = pLayerContainer.GetLayer(i, FbxLayerElement::eBiNormal)->GetBinormals();

        pLayerIndexSet.Add((FbxHandle)lLayerElementBinormal, i);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_BINORMAL);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElementBinormal->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElementBinormal->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElementBinormal->GetReferenceMode()));

                mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_BINORMALS);
                {
                    unsigned int j, lBinormalCount = lLayerElementBinormal->GetDirectArray().GetCount();
                    FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementBinormal->GetDirectArray();

                    for (j=0; j < lBinormalCount; j++)
                    {
                        FbxVector4 v = lDirectArray.GetAt(j);
                        mFileObject->FieldWriteD( v.mData[0] );
                        mFileObject->FieldWriteD( v.mData[1] );
                        mFileObject->FieldWriteD( v.mData[2] );
                    }
                }
                mFileObject->FieldWriteEnd();

                // Write the index if the mapping type is index to direct
                if (lLayerElementBinormal->GetReferenceMode() != FbxLayerElement::eDirect)
                {
                    unsigned int lCount = lLayerElementBinormal->GetIndexArray().GetCount();
                    if (lCount > 0)
                    {
                        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_BINORMALS_INDEX);

                        unsigned int j;
                        FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementBinormal->GetIndexArray();

                        for (j=0; j<lCount; j++)
                        {
                            mFileObject->FieldWriteI(lIndexArray.GetAt(j));
                        }
                        mFileObject->FieldWriteEnd();
                    }
                }

            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}
bool FbxWriterFbx6::WriteFbxLayerElementTangents(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eTangent);

    for (i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementTangent* lLayerElementTangent = pLayerContainer.GetLayer(i, FbxLayerElement::eTangent)->GetTangents();

        pLayerIndexSet.Add((FbxHandle)lLayerElementTangent, i);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_TANGENT);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElementTangent->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElementTangent->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElementTangent->GetReferenceMode()));

                mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_TANGENTS);
                {
                    unsigned int j, lTangentCount = lLayerElementTangent->GetDirectArray().GetCount();
                    FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementTangent->GetDirectArray();

                    for (j=0; j < lTangentCount; j++)
                    {
                        FbxVector4 v = lDirectArray.GetAt(j);
                        mFileObject->FieldWriteD( v.mData[0] );
                        mFileObject->FieldWriteD( v.mData[1] );
                        mFileObject->FieldWriteD( v.mData[2] );
                    }
                }
                mFileObject->FieldWriteEnd();

                // Write the index if the mapping type is index to direct
                if (lLayerElementTangent->GetReferenceMode() != FbxLayerElement::eDirect)
                {
                    unsigned int lCount = lLayerElementTangent->GetIndexArray().GetCount();
                    if (lCount > 0)
                    {
                        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_TANGENTS_INDEX);

                        unsigned int j;
                        FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementTangent->GetIndexArray();

                        for (j=0; j<lCount; j++)
                        {
                            mFileObject->FieldWriteI(lIndexArray.GetAt(j));
                        }
                        mFileObject->FieldWriteEnd();
                    }
                }

            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx6::WriteFbxLayerElementMaterials(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eMaterial);

    // We must be attached to a node in order to write out the materials,
    // since they get connected to the parent node.
    if( !mCurrentNode && !pLayerContainer.GetNode() )
        return false;

    for (i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementMaterial*        lLayerElementMaterial = pLayerContainer.GetLayer(i, FbxLayerElement::eMaterial)->GetMaterials();
        FbxLayerElement::EReferenceMode lReferenceMode = lLayerElementMaterial->GetReferenceMode();

        if (lReferenceMode == FbxLayerElement::eDirect) continue;

        pLayerIndexSet.Add((FbxHandle)lLayerElementMaterial, i);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_MATERIAL);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElementMaterial->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElementMaterial->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElementMaterial->GetReferenceMode()));

                mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_MATERIALS_ID);
                {
                    int lMaterialCount = lLayerElementMaterial->GetMappingMode() == FbxLayerElement::eAllSame ? 1 : lLayerElementMaterial->GetIndexArray().GetCount();
                    const FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementMaterial->GetIndexArray();

                    for( int j = 0; j < lMaterialCount; j++ )
                    {
                        int lConnectionIndex = lIndexArray.GetAt(j);
                        FBX_ASSERT( lConnectionIndex >= -1 );
                        
                        mFileObject->FieldWriteI(lConnectionIndex);
                    }
                }
                mFileObject->FieldWriteEnd();
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx6::WriteFbxLayerElementTextures(FbxLayerContainer& pLayerContainer,
                                                   FbxMultiMap& pLayerIndexSet)
{
    int lLayerIndex;
    FBXSDK_FOR_EACH_TEXTURE(lLayerIndex)
    {
        WriteFbxLayerElementTexturesChannel(pLayerContainer, FBXSDK_TEXTURE_TYPE(lLayerIndex), pLayerIndexSet);
    }
    return true;
}

bool FbxWriterFbx6::WriteFbxLayerElementTexturesChannel( FbxLayerContainer& pLayerContainer, FbxLayerElement::EType pTextureType, FbxMultiMap& pLayerIndexSet )
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(pTextureType);

    // We must be attached to a node in order to write out the textures,
    // since they get connected to the parent node.
    if( !mCurrentNode && !pLayerContainer.GetNode())
        return false;

    for (i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementTexture* lLayerElementTexture = pLayerContainer.GetLayer(i, pTextureType)->GetTextures(pTextureType);
        FbxLayerElement::EReferenceMode lReferenceMode = lLayerElementTexture->GetReferenceMode();

        if (lReferenceMode == FbxLayerElement::eDirect) continue;


        pLayerIndexSet.Add((FbxHandle)lLayerElementTexture, i);

        mFileObject->FieldWriteBegin(FbxLayerElement::sTextureNames[FBXSDK_TEXTURE_INDEX(pTextureType)]);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElementTexture->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElementTexture->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElementTexture->GetReferenceMode()));
                mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_BLEND_MODE,                GetBlendModeToken(lLayerElementTexture->GetBlendMode()));
                mFileObject->FieldWriteD(FIELD_KFBXTEXTURE_ALPHA,                     lLayerElementTexture->GetAlpha());

                mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_TEXTURE_ID);
                {
                    unsigned int j, lTextureCount     = lLayerElementTexture->GetIndexArray().GetCount();
                    FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementTexture->GetIndexArray();

                    for (j=0; j<lTextureCount; j++)
                    {
                        int lConnectionIndex;

                        if (lReferenceMode == FbxLayerElement::eIndexToDirect)
                        {
                            lConnectionIndex = lIndexArray.GetAt(j);
                            if (lIndexArray.GetAt(j) != -1)
                                lConnectionIndex = MapLayeredTextureIndexToConnectionIndex( mCurrentNode ? mCurrentNode : pLayerContainer.GetNode(), lLayerElementTexture, j);
                        }
                        else
                        {
                            // eIndex
                            lConnectionIndex = lIndexArray.GetAt(j);
                        }
                        mFileObject->FieldWriteI(lConnectionIndex);
                    }
                }
                mFileObject->FieldWriteEnd();
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx6::WriteFbxLayerElementPolygonGroups(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::ePolygonGroup);

    for (i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementPolygonGroup* lLayerElement = pLayerContainer.GetLayer(i, FbxLayerElement::ePolygonGroup)->GetPolygonGroups();

        if (lLayerElement->GetReferenceMode() == FbxLayerElement::eDirect) continue;

        pLayerIndexSet.Add((FbxHandle)lLayerElement, i);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_POLYGON_GROUP);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_POLYGON_GROUP);
                {
                    unsigned int j, lCount            = lLayerElement->GetIndexArray().GetCount();
                    FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElement->GetIndexArray();

                    for (j=0; j<lCount; j++)
                    {
                        mFileObject->FieldWriteI(lIndexArray.GetAt(j));
                    }
                }
                mFileObject->FieldWriteEnd();
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx6::WriteFbxLayerElementVertexColors(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eVertexColor);

    for (i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementVertexColor* lLayerElement = pLayerContainer.GetLayer(i, FbxLayerElement::eVertexColor)->GetVertexColors();

        if (lLayerElement->GetReferenceMode() == FbxLayerElement::eIndex) continue;

        pLayerIndexSet.Add((FbxHandle)lLayerElement, i);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_COLOR);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                // Write the color values
                mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_VALUES);
                {
                    int lElementIndex;
                    int lElementCount                       = lLayerElement->GetDirectArray().GetCount();
                    FbxLayerElementArrayTemplate<FbxColor>& lDirectArray = lLayerElement->GetDirectArray();

                    for (lElementIndex = 0; lElementIndex < lElementCount; lElementIndex++)
                    {
                        mFileObject->FieldWriteD(lDirectArray.GetAt(lElementIndex).mRed);
                        mFileObject->FieldWriteD(lDirectArray.GetAt(lElementIndex).mGreen);
                        mFileObject->FieldWriteD(lDirectArray.GetAt(lElementIndex).mBlue);
                        mFileObject->FieldWriteD(lDirectArray.GetAt(lElementIndex).mAlpha);
                    }
                }
                mFileObject->FieldWriteEnd();

                // Write the index if the mapping type is index to direct
                if (lLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                {
                    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_INDEX);
                    {
                        unsigned int j, lCount            = lLayerElement->GetIndexArray().GetCount();
                        FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElement->GetIndexArray();

                        for (j=0; j<lCount; j++)
                        {
                            mFileObject->FieldWriteI(lIndexArray.GetAt(j));
                        }
                    }
                    mFileObject->FieldWriteEnd();
                }
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}


bool FbxWriterFbx6::WriteFbxLayerElementUVsChannel( FbxLayerContainer& pLayerContainer, FbxLayerElement::EType pTextureType, FbxMultiMap& pLayerIndexSet )
{
    int i, lLayerCount = pLayerContainer.GetLayerCount();

    //Note : We should write for the number of UVs
    //and not for the number of textures, as we do have non-associated
    //UVs
   
    int lRelIndex = 0;
    for (i=0; i<lLayerCount; i++)
    {
        FbxLayer* lLayer = pLayerContainer.GetLayer(i);
        if (lLayer==NULL)
            continue;
        FbxLayerElementUV* lLayerElement= lLayer->GetUVs(pTextureType);

        if(lLayerElement==NULL)
            continue;

        if (lLayerElement->GetReferenceMode() == FbxLayerElement::eIndex) continue;

        pLayerIndexSet.Add((FbxHandle)lLayerElement, lRelIndex);

        mFileObject->FieldWriteBegin(FbxLayerElement::sTextureUVNames[FBXSDK_TEXTURE_INDEX(pTextureType)]);
        {
            // Layer Element index
            mFileObject->FieldWriteI(lRelIndex++);

            mFileObject->FieldWriteBlockBegin();
            {
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                // Write the UV values
                if (lLayerElement->GetReferenceMode() == FbxLayerElement::eDirect || lLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                {
                    mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_UV);
                    {
                        int lElementIndex;
                        int lElementCount                         = lLayerElement->GetDirectArray().GetCount();
                        FbxLayerElementArrayTemplate<FbxVector2>& lDirectArray = lLayerElement->GetDirectArray();

                        for (lElementIndex = 0; lElementIndex < lElementCount; lElementIndex++)
                        {
                            FbxVector2 v = lDirectArray.GetAt(lElementIndex);
                            mFileObject->FieldWriteD(v.mData[0]);
                            mFileObject->FieldWriteD(v.mData[1]);
                        }
                    }
                    mFileObject->FieldWriteEnd();
                }

                // Write the index if the mapping type is index to direct
                if (lLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                {
                    mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_UV_INDEX);
                    {
                        unsigned int j, lCount            = lLayerElement->GetIndexArray().GetCount();
                        FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElement->GetIndexArray();

                        for (j=0; j<lCount; j++)
                        {
                            mFileObject->FieldWriteI( lIndexArray.GetAt(j) );
                        }
                    }
                    mFileObject->FieldWriteEnd();
                }
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }
    return true;
}

bool FbxWriterFbx6::WriteFbxLayerElementUVs(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int lLayerIndex;
    FBXSDK_FOR_EACH_TEXTURE(lLayerIndex)
    {
        WriteFbxLayerElementUVsChannel(pLayerContainer, FBXSDK_TEXTURE_TYPE(lLayerIndex), pLayerIndexSet);
    }
    return true;
}

bool FbxWriterFbx6::WriteFbxLayerElementSmoothing(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eSmoothing);

    for (i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementSmoothing* lLayerElement = pLayerContainer.GetLayer(i, FbxLayerElement::eSmoothing)->GetSmoothing();

        // by edge is the only valid mapping mode for smoothing supported so far
        if( lLayerElement->GetMappingMode() != FbxLayerElement::eByEdge && lLayerElement->GetMappingMode() != FbxLayerElement::eByPolygon )
            continue;

        // makes no sense to have indexed bools.
        if( lLayerElement->GetReferenceMode() != FbxLayerElement::eDirect )
            continue;

        pLayerIndexSet.Add((FbxHandle)lLayerElement, i);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_SMOOTHING);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {
                //Version 101: bool
                //Version 102: int
                bool lWriteAsInt=false;
                if(
                    mFileVersion.Compare(FBX_60_COMPATIBLE)==0 ||
                    mFileVersion.Compare(FBX_2005_08_COMPATIBLE)==0 ||
                    mFileVersion.Compare(FBX_2006_02_COMPATIBLE)==0 ||
                    mFileVersion.Compare(FBX_2006_08_COMPATIBLE)==0 ||
                    mFileVersion.Compare(FBX_2006_11_COMPATIBLE)==0 
                    )
                {
                    mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                    //in these versions, we only support BOOL and BY_EDGE
                    if(lLayerElement->GetMappingMode() == FbxLayerElement::eByPolygon)
                    {
                        FbxMesh* lMesh = FbxCast <FbxMesh> (&pLayerContainer);
                        if(lMesh)
                        {
                            FbxGeometryConverter lConverter(mScene->GetFbxManager());
                            lConverter.ComputeEdgeSmoothingFromPolygonSmoothing(lMesh,i);
                        }
                    }
                }
                else
                {
                    lWriteAsInt=true;
                    mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 102);
                }
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                // Write the smoothing values
                mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_SMOOTHING);
                {                   
                    int lElementCount = lLayerElement->GetDirectArray().GetCount();
                    FbxLayerElementArrayTemplate<int>& lDirectArray = lLayerElement->GetDirectArray();

                    for (int lElementIndex = 0; lElementIndex < lElementCount; lElementIndex++)
                    {
                        if(lWriteAsInt == true)
                        {
                            mFileObject->FieldWriteI(lDirectArray.GetAt(lElementIndex));
                        }
                        else
                        {
                            mFileObject->FieldWriteB(lDirectArray.GetAt(lElementIndex) != 0);
                        }
                    }
                }
                mFileObject->FieldWriteEnd();
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx6::WriteFbxLayerElementUserData(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eUserData);

    for (i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementUserData* lLayerElement = pLayerContainer.GetLayer(i, FbxLayerElement::eUserData )->GetUserData();

        pLayerIndexSet.Add((FbxHandle)lLayerElement, i);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_USER_DATA);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {
                // Write out version and name
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                // Write out the data ID
                mFileObject->FieldWriteI( FIELD_KFBXGEOMETRYMESH_USER_DATA_ID, lLayerElement->GetId() );

                for( int a = 0; a < lLayerElement->GetDirectArrayCount(); ++a )
                {
                    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_USER_DATA_ARRAY);
                    mFileObject->FieldWriteBlockBegin();
                    {
                        // Write out the data type                     
                        mFileObject->FieldWriteC( FIELD_KFBXGEOMETRYMESH_USER_DATA_TYPE, lLayerElement->GetDataType(a).GetName());

                        // Write out the data name                     
                        mFileObject->FieldWriteC( FIELD_KFBXGEOMETRYMESH_USER_DATA_NAME, lLayerElement->GetDataName(a));

                        // Write the user data values
                        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_USER_DATA);

                        for( int j = 0; j < lLayerElement->GetArrayCount(a); ++j)
                        {
                            switch( lLayerElement->GetDataType(a).GetType() )
                            {
                                case eFbxBool:    mFileObject->FieldWriteB( FbxGetDirectArray<bool>(lLayerElement,a).GetAt(j) ); break;
                                case eFbxInt: mFileObject->FieldWriteI( FbxGetDirectArray<int>(lLayerElement,a).GetAt(j) ); break;
                                case eFbxFloat:   mFileObject->FieldWriteF( FbxGetDirectArray<float>(lLayerElement,a).GetAt(j) ); break;
                                case eFbxDouble:  mFileObject->FieldWriteD( FbxGetDirectArray<double>(lLayerElement,a).GetAt(j) ); break;
                                default:
                                    mFileObject->FieldWriteI( -1 );
                                    FBX_ASSERT_NOW("Unsupported User Data type.");
                                    break;
                            }
                        }
                        mFileObject->FieldWriteEnd();
                    }
                    mFileObject->FieldWriteBlockEnd();
                    mFileObject->FieldWriteEnd();
                }

                // Write the index if the mapping type is index to direct
                if (lLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                {
                    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_USER_DATA_INDEX);
                    {
                        unsigned int j, lCount            = lLayerElement->GetIndexArray().GetCount();
                        FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElement->GetIndexArray();

                        for (j=0; j<lCount; j++)
                        {
                            mFileObject->FieldWriteI(lIndexArray.GetAt(j));
                        }
                    }
                    mFileObject->FieldWriteEnd();
                }
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;

}

bool FbxWriterFbx6::WriteFbxLayerElementVisibility(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eVisibility);

    for (i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementVisibility* lLayerElement = pLayerContainer.GetLayer(i, FbxLayerElement::eVisibility )->GetVisibility();

        pLayerIndexSet.Add((FbxHandle)lLayerElement, i);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_VISIBILITY);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {
                // Write out version and name
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                // Write the smoothing values
                mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_VISIBILITY);
                {                    
                    int lElementCount = lLayerElement->GetDirectArray().GetCount();
                    FbxLayerElementArrayTemplate<bool>& lDirectArray = lLayerElement->GetDirectArray();

                    for (int lElementIndex = 0; lElementIndex < lElementCount; lElementIndex++)
                    {
                        mFileObject->FieldWriteB(lDirectArray.GetAt(lElementIndex));
                    }
                }
                mFileObject->FieldWriteEnd();
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;

}

bool FbxWriterFbx6::WriteFbxLayerElementEdgeCrease(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
	int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eEdgeCrease);

	for (i=0; i<lLayerElementCount; i++)
	{
		FbxLayerElementCrease* lLayerElement = pLayerContainer.GetLayer(i, FbxLayerElement::eEdgeCrease )->GetEdgeCrease();

		pLayerIndexSet.Add((FbxHandle)lLayerElement, i);

		mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_EDGE_CREASE);
		{
			// Layer Element index
			mFileObject->FieldWriteI(i);

			mFileObject->FieldWriteBlockBegin();
			{
				// Write out version and name
				mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 100);
				mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

				mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
				mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

				// Write the smoothing values
				mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_EDGE_CREASE);
				{					
					int lElementCount = lLayerElement->GetDirectArray().GetCount();
					FbxLayerElementArrayTemplate<double>& lDirectArray = lLayerElement->GetDirectArray();

					for (int lElementIndex = 0; lElementIndex < lElementCount; lElementIndex++)
					{
						mFileObject->FieldWriteD(lDirectArray.GetAt(lElementIndex));
					}
				}
				mFileObject->FieldWriteEnd();
			}
			mFileObject->FieldWriteBlockEnd();
		}
		mFileObject->FieldWriteEnd();
	}

	return true;

}

bool FbxWriterFbx6::WriteFbxLayerElementVertexCrease(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
	int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eVertexCrease);

	for (i=0; i<lLayerElementCount; i++)
	{
		FbxLayerElementCrease* lLayerElement = pLayerContainer.GetLayer(i, FbxLayerElement::eVertexCrease )->GetVertexCrease();

		pLayerIndexSet.Add((FbxHandle)lLayerElement, i);

		mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_VERTEX_CREASE);
		{
			// Layer Element index
			mFileObject->FieldWriteI(i);

			mFileObject->FieldWriteBlockBegin();
			{
				// Write out version and name
				mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 100);
				mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

				mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
				mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

				// Write the smoothing values
				mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_VERTEX_CREASE);
				{					
					int lElementCount = lLayerElement->GetDirectArray().GetCount();
					FbxLayerElementArrayTemplate<double>& lDirectArray = lLayerElement->GetDirectArray();

					for (int lElementIndex = 0; lElementIndex < lElementCount; lElementIndex++)
					{
						mFileObject->FieldWriteD(lDirectArray.GetAt(lElementIndex));
					}
				}
				mFileObject->FieldWriteEnd();
			}
			mFileObject->FieldWriteBlockEnd();
		}
		mFileObject->FieldWriteEnd();
	}

	return true;

}

bool FbxWriterFbx6::WriteFbxLayerElementHole(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eHole);

    for (i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementHole* lLayerElement = pLayerContainer.GetLayer(i, FbxLayerElement::eHole )->GetHole();

        pLayerIndexSet.Add((FbxHandle)lLayerElement, i);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_HOLE);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {
                // Write out version and name
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 100);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                // Write the smoothing values
                mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_HOLE);
                {                    
                    int lElementCount = lLayerElement->GetDirectArray().GetCount();
                    FbxLayerElementArrayTemplate<bool>& lDirectArray = lLayerElement->GetDirectArray();

                    for (int lElementIndex = 0; lElementIndex < lElementCount; lElementIndex++)
                    {
                        mFileObject->FieldWriteB(lDirectArray.GetAt(lElementIndex));
                    }
                }
                mFileObject->FieldWriteEnd();
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx6::WriteLayers(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lSavedLayerCount = 0, lLayerCount = pLayerContainer.GetLayerCount();

    for (i=0; i<lLayerCount; i++)
    {
        FbxLayer* lLayer = pLayerContainer.GetLayer(i);
        bool lResult = true;
        int lLayerIndex;
        for(lLayerIndex = FbxLayerElement::eUnknown + 1; lLayerIndex < FbxLayerElement::eTypeCount; lLayerIndex++)
        {
            lResult &= (lLayer->GetLayerElementOfType((FbxLayerElement::EType)lLayerIndex) == NULL);
            if(lResult == false)
                break;
        }

        if(lResult)
        {
            FBXSDK_FOR_EACH_TEXTURE(lLayerIndex)
            {
                lResult &= (lLayer->GetLayerElementOfType(FBXSDK_TEXTURE_TYPE(lLayerIndex), true) == NULL);
                if(lResult == false)
                    break;
            }
        }

        if(lResult)
            continue;

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER);

        mFileObject->FieldWriteI(lSavedLayerCount);

        mFileObject->FieldWriteBlockBegin();
        {
            mFileObject->FieldWriteI(FIELD_KFBXLAYER_VERSION, 100);

            FBXSDK_FOR_EACH_NON_TEXTURE(lLayerIndex)
            {
                if (FBXSDK_NON_TEXTURE_TYPE(lLayerIndex)!= FbxLayerElement::eUV &&
                    lLayer->GetLayerElementOfType(FBXSDK_NON_TEXTURE_TYPE(lLayerIndex)))
                {
                    mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT);
                    mFileObject->FieldWriteBlockBegin();
                    {
                        mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_TYPE, FbxLayerElement::sNonTextureNames[lLayerIndex]);
                        mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_TYPED_INDEX, (int)pLayerIndexSet.Get((FbxHandle)lLayer->GetLayerElementOfType(FBXSDK_NON_TEXTURE_TYPE(lLayerIndex))));
                    }
                    mFileObject->FieldWriteBlockEnd();
                    mFileObject->FieldWriteEnd();
                }
            }
            

            FBXSDK_FOR_EACH_TEXTURE(lLayerIndex)
            {
                FbxLayerElement::EType lTextureType = FBXSDK_TEXTURE_TYPE(lLayerIndex);
                if (lLayer->GetTextures(lTextureType))
                {
                    mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT);
                    mFileObject->FieldWriteBlockBegin();
                    {
                        mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_TYPE, FbxLayerElement::sTextureNames[lLayerIndex]);
                        mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_TYPED_INDEX, (int)pLayerIndexSet.Get((FbxHandle)lLayer->GetTextures(lTextureType)));
                    }
                    mFileObject->FieldWriteBlockEnd();
                    mFileObject->FieldWriteEnd();
                }

                if (lLayer->GetUVs(lTextureType))
                {
                    mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT);
                    mFileObject->FieldWriteBlockBegin();
                    {
                        mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_TYPE, FbxLayerElement::sTextureUVNames[lLayerIndex]);
                        mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_TYPED_INDEX, (int)pLayerIndexSet.Get((FbxHandle)lLayer->GetUVs(lTextureType)));
                    }
                    mFileObject->FieldWriteBlockEnd();
                    mFileObject->FieldWriteEnd();
                }
            }
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();

        lSavedLayerCount++;
    }
    return true;
}

int  FbxWriterFbx6::MapLayeredTextureIndexToConnectionIndex(FbxNode* pNode, void* pLET, int pIndex)
{
    FbxLayerElementTexture* lLET   = static_cast<FbxLayerElementTexture*>(pLET);
    int           lLayeredIndex     = lLET->GetIndexArray().GetAt(pIndex);
    FbxTexture*  lTexture          = lLET->GetDirectArray().GetAt(lLayeredIndex);

	for( int i=0; i < pNode->GetSrcObjectCount<FbxTexture>(); i++ )
	{
        if( lTexture == pNode->GetSrcObject<FbxTexture>(i) )
		{
            return i;
        }
    }
    return -1;
}

bool FbxWriterFbx6::WriteMeshEdges(FbxMesh& pMesh)
{
    //
    // Only write them out if we have some data
    //
    if( pMesh.GetMeshEdgeCount() )
    {
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_EDGES);
        {
            for (int lCount=0; lCount < pMesh.GetMeshEdgeCount(); lCount++)
            {
                mFileObject->FieldWriteI( pMesh.mEdgeArray[lCount] );
            }
        }
        mFileObject->FieldWriteEnd ();
    }

    return true;
}

bool FbxWriterFbx6::WriteMesh(FbxMesh& pMesh)
{
    //
    // Store what is needed for a mesh
    //
    if (pMesh.GetControlPointsCount ())
    {
        WriteMeshSmoothness(pMesh);
        WriteMeshVertices(pMesh);
        WriteMeshPolyVertexIndex(pMesh);
        WriteMeshEdges(pMesh);
        WriteGeometry(pMesh);
    }

    return true;
}

bool FbxWriterFbx6::WriteTrimNurbsSurface(FbxTrimNurbsSurface& pNurbs)
{
    // we need a untrimmed surface and at least 1 boundary, with at least 1 curve to be valid.
    if( pNurbs.GetNurbsSurface() == NULL || pNurbs.GetBoundaryCount() < 1 || pNurbs.GetBoundary(0)->GetCurveCount() < 1 )
        return false;

    // write out the version number
    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYTRIM_NURBS_SURFACE_VERSION, 100);

    // write out the type
    mFileObject->FieldWriteC(FIELD_KFBXNODE_TYPE, FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_TRIM_NURB_SURFACE );

    // output the trim surface properties
    mFileObject->FieldWriteB(FIELD_KFBXGEOMETRYTRIM_NURBS_SURFACE_FLIP_NORMALS, pNurbs.GetFlipNormals() );

    // write out all the good geometry stuff
    WriteGeometry( pNurbs );

    return true;
}

bool FbxWriterFbx6::WriteBoundary( FbxBoundary& pBoundary )
{
    // boundary has to have at least one curve to be valid.
    if( pBoundary.GetCurveCount() < 1 )
        return false;

    WriteGeometry( pBoundary );

    mFileObject->FieldWriteC(FIELD_KFBXNODE_TYPE, FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_BOUNDARY );

    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYBOUNDARY_VERSION, 100);

    // Properties
    WriteObjectPropertiesAndFlags(&pBoundary);

    return true;
}

bool FbxWriterFbx6::WriteNurbsCurve(FbxNurbsCurve& pNurbs)
{
    int lCount = 0;
    int lTotalCount = 0;

    FbxVector4 lSrcPoint;
    FbxVector4 lDestPoint;

    FbxAMatrix lPivot;
    pNurbs.GetPivot( lPivot );

    // only write if we have some control points to writeout
    if( pNurbs.GetControlPointsCount() )
    {
        // write out all the good geometry stuff
        WriteGeometry( pNurbs );

        mFileObject->FieldWriteC(FIELD_KFBXNODE_TYPE, "NurbsCurve");

        mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYNURBS_CURVE_VERSION, 100);

        // order
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_CURVE_ORDER);
		{
            mFileObject->FieldWriteI(pNurbs.GetOrder());
		}
        mFileObject->FieldWriteEnd ();

        // dimension
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_CURVE_DIMENSION);
		{
            mFileObject->FieldWriteI( (int)pNurbs.GetDimension() );
		}
        mFileObject->FieldWriteEnd ();

        // form
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_CURVE_FORM);

        FbxNurbsCurve::EType lType = pNurbs.GetType();
        if(lType == FbxNurbsCurve::ePeriodic)
        {
            mFileObject->FieldWriteC("Periodic");
        }
        else if(lType == FbxNurbsCurve::eClosed)
        {
            mFileObject->FieldWriteC("Closed");
        }
        else // must be open
        {
            mFileObject->FieldWriteC("Open");
        }
        mFileObject->FieldWriteEnd ();

        // rational
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_CURVE_RATIONAL);
        mFileObject->FieldWriteB( pNurbs.IsRational() );
        mFileObject->FieldWriteEnd();

        // Note :we dont really need to save this, we can compute it from the degree and number of control points

        // control points

        mFileObject->FieldWriteBegin( FIELD_KFBXGEOMETRYNURBS_CURVE_POINTS);

        lTotalCount = pNurbs.GetControlPointsCount();
        for (lCount = 0; lCount < lTotalCount; lCount++)
        {
            lSrcPoint = pNurbs.GetControlPoints()[lCount];
            lDestPoint = lPivot.MultT(lSrcPoint);

            mFileObject->FieldWriteD(lDestPoint.mData [0]);   // X
            mFileObject->FieldWriteD(lDestPoint.mData [1]);   // Y
            mFileObject->FieldWriteD(lDestPoint.mData [2]);   // Z
            mFileObject->FieldWriteD( (pNurbs.GetControlPoints()[lCount]).mData[3] );       // Weight
        }


        mFileObject->FieldWriteEnd();

        // Knot Vector
        //
        mFileObject->FieldWriteBegin( FIELD_KFBXGEOMETRYNURBS_CURVE_KNOTVECTOR);

        lTotalCount = pNurbs.GetKnotCount();
        double* lKnotPtr = pNurbs.GetKnotVector();

        for (lCount = 0; lCount < lTotalCount; lCount ++)
        {            
            mFileObject->FieldWriteD(*(lKnotPtr) );
            ++lKnotPtr;
        }

        mFileObject->FieldWriteEnd ();

    }

    return true;
}

bool FbxWriterFbx6::WriteNurb(FbxNurbs& pNurbs)
{
    int Count, TotalCount;
    FbxAMatrix lPivot;

    FbxVector4 lSrcPoint;
    FbxVector4 lDestPoint;

    FbxNurbs* lNurbs;

    if (pNurbs.GetApplyFlip())
    {
        FbxGeometryConverter lConverter(&mManager);
        lNurbs = lConverter.FlipNurbs(&pNurbs, pNurbs.GetApplyFlipUV(), pNurbs.GetApplyFlipLinks());
    }
    else
    {
        lNurbs = &pNurbs;
    }

    lNurbs->GetPivot(lPivot);

    //
    // Store what is needed for a nurb
    //
    if(lNurbs->GetControlPointsCount ())
    {
        mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYNURB_NURB_VERSION, 200);

        //
        // SURFACE DISPLAY...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURB_SURFACE_DISPLAY);
		{
            mFileObject->FieldWriteI(lNurbs->GetSurfaceMode());
            mFileObject->FieldWriteI(lNurbs->GetUStep());
            mFileObject->FieldWriteI(lNurbs->GetVStep());
		}
        mFileObject->FieldWriteEnd ();

        //
        // NURB ORDER...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURB_NURB_ORDER);
		{
            mFileObject->FieldWriteI(lNurbs->GetUOrder());
			mFileObject->FieldWriteI(lNurbs->GetVOrder());
		}
        mFileObject->FieldWriteEnd ();

        //
        // Surface DIMENSION
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURB_DIMENSION);
		{
            mFileObject->FieldWriteI(lNurbs->GetUCount());
            mFileObject->FieldWriteI(lNurbs->GetVCount());
		}
        mFileObject->FieldWriteEnd ();

        //
        // STEP
        //
		mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURB_STEP);
		{
            mFileObject->FieldWriteI(lNurbs->GetUStep());
            mFileObject->FieldWriteI(lNurbs->GetVStep());
		}
        mFileObject->FieldWriteEnd ();

        //
        // FORM...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURB_FORM);

        //
        // NURB TYPE U...
        //
        if(lNurbs->GetNurbsUType() == FbxNurbs::ePeriodic)
        {
            mFileObject->FieldWriteC("Periodic");
        }
        else if(lNurbs->GetNurbsUType() == FbxNurbs::eClosed)
        {
            mFileObject->FieldWriteC("Closed");
        }
        else
        {
            mFileObject->FieldWriteC("Open");
        }

        //
        // NURB TYPE V...
        //
        if(lNurbs->GetNurbsVType() == FbxNurbs::ePeriodic)
        {
            mFileObject->FieldWriteC("Periodic");
        }
        else if(lNurbs->GetNurbsVType() == FbxNurbs::eClosed)
        {
            mFileObject->FieldWriteC("Closed");
        }
        else
        {
            mFileObject->FieldWriteC("Open");
        }

        mFileObject->FieldWriteEnd ();

        //
        // Control points
        //
        mFileObject->FieldWriteBegin( FIELD_KFBXGEOMETRYNURB_POINTS);

        for (Count = 0; Count < lNurbs->GetControlPointsCount (); Count ++)
        {
            lSrcPoint = lNurbs->GetControlPoints()[Count];
            lDestPoint = lPivot.MultT(lSrcPoint);

            mFileObject->FieldWriteD(lDestPoint.mData [0]);   // X
            mFileObject->FieldWriteD(lDestPoint.mData [1]);   // Y
            mFileObject->FieldWriteD(lDestPoint.mData [2]);   // Z
            mFileObject->FieldWriteD(lSrcPoint[3]);       // Weight
        }

        mFileObject->FieldWriteEnd();

        FBX_ASSERT_MSG
        (
            (lNurbs->GetUMultiplicityVector()!=NULL) && (lNurbs->GetVMultiplicityVector()!=NULL),
            "FbxWriterFbx6::WriteNurb : Null multiplicity vector."
        );

        //
        // MultiplicityU...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURB_MULTIPLICITY_U);
        for (Count = 0; Count < lNurbs->GetUCount(); Count ++)
        {
            mFileObject->FieldWriteI(lNurbs->GetUMultiplicityVector() [Count]);
        }
        mFileObject->FieldWriteEnd();

        //
        // MultiplicityV...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURB_MULTIPLICITY_V);
        for (Count = 0; Count < lNurbs->GetVCount(); Count ++)
        {
            mFileObject->FieldWriteI(lNurbs->GetVMultiplicityVector() [Count]);
        }
        mFileObject->FieldWriteEnd ();

        FBX_ASSERT_MSG
        (
            (lNurbs->GetUKnotVector()!=NULL) && (lNurbs->GetVKnotVector()!=NULL),
            "FbxWriterFbx6::WriteNurb : Null knot vector."
        );

        //
        // Knot Vector U
        //
        mFileObject->FieldWriteBegin( FIELD_KFBXGEOMETRYNURB_KNOTVECTOR_U);

        TotalCount = pNurbs.GetUKnotCount();

        for (Count = 0; Count < TotalCount; Count ++)
        {
            mFileObject->FieldWriteD(*(lNurbs->GetUKnotVector()+Count));
        }

        mFileObject->FieldWriteEnd ();

        //
        // Knot Vector V
        //
        mFileObject->FieldWriteBegin( FIELD_KFBXGEOMETRYNURB_KNOTVECTOR_V);

        TotalCount = pNurbs.GetVKnotCount();

        for (Count = 0; Count < TotalCount; Count ++)
        {
            mFileObject->FieldWriteD(*(lNurbs->GetVKnotVector()+Count));
        }

        mFileObject->FieldWriteEnd ();

        WriteGeometry(*lNurbs);

    }

    if (pNurbs.GetApplyFlip())
    {
        lNurbs->Destroy();
    }

    return true;
}


bool FbxWriterFbx6::WriteNurbsSurface(FbxNurbsSurface& pNurbs)
{
    int Count, TotalCount;
    FbxAMatrix lPivot;

    FbxVector4 lSrcPoint;
    FbxVector4 lDestPoint;

    FbxNurbsSurface* lNurbs;

    if (pNurbs.GetApplyFlip())
    {
        FbxGeometryConverter lConverter(&mManager);
        lNurbs = lConverter.FlipNurbsSurface(&pNurbs, pNurbs.GetApplyFlipUV(), pNurbs.GetApplyFlipLinks());
    }
    else
    {
        lNurbs = &pNurbs;
    }

    lNurbs->GetPivot(lPivot);

    //
    // Store what is needed for a nurb
    //
    if(lNurbs->GetControlPointsCount ())
    {
        mFileObject->FieldWriteC(FIELD_KFBXNODE_TYPE, "NurbsSurface");

        mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYNURBS_SURFACE_NURB_VERSION, 100);

        //
        // SURFACE DISPLAY...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_SURFACE_SURFACE_DISPLAY);
		{
            mFileObject->FieldWriteI(lNurbs->GetSurfaceMode());
            mFileObject->FieldWriteI(lNurbs->GetUStep());
            mFileObject->FieldWriteI(lNurbs->GetVStep());
		}
        mFileObject->FieldWriteEnd ();

        //
        // NURB ORDER...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_SURFACE_NURB_ORDER);
		{
            mFileObject->FieldWriteI(lNurbs->GetUOrder());
            mFileObject->FieldWriteI(lNurbs->GetVOrder());
		}
        mFileObject->FieldWriteEnd ();

        //
        // Surface DIMENSION
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_SURFACE_DIMENSION);
		{
            mFileObject->FieldWriteI(lNurbs->GetUCount());
            mFileObject->FieldWriteI(lNurbs->GetVCount());
		}
        mFileObject->FieldWriteEnd ();

        //
        // STEP
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_SURFACE_STEP);
            mFileObject->FieldWriteI(lNurbs->GetUStep());
            mFileObject->FieldWriteI(lNurbs->GetVStep());
        mFileObject->FieldWriteEnd ();

        //
        // FORM...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_SURFACE_FORM);

        //
        // NURB TYPE U...
        //
        if(lNurbs->GetNurbsUType() == FbxNurbsSurface::ePeriodic)
        {
            mFileObject->FieldWriteC("Periodic");
        }
        else if(lNurbs->GetNurbsUType() == FbxNurbsSurface::eClosed)
        {
            mFileObject->FieldWriteC("Closed");
        }
        else
        {
            mFileObject->FieldWriteC("Open");
        }

        //
        // NURB TYPE V...
        //
        if(lNurbs->GetNurbsVType() == FbxNurbsSurface::ePeriodic)
        {
            mFileObject->FieldWriteC("Periodic");
        }
        else if(lNurbs->GetNurbsVType() == FbxNurbsSurface::eClosed)
        {
            mFileObject->FieldWriteC("Closed");
        }
        else
        {
            mFileObject->FieldWriteC("Open");
        }

        mFileObject->FieldWriteEnd();

        //
        // Control points
        //
        mFileObject->FieldWriteBegin( FIELD_KFBXGEOMETRYNURBS_SURFACE_POINTS);

        for (Count = 0; Count < lNurbs->GetControlPointsCount (); Count ++)
        {
            lSrcPoint = lNurbs->GetControlPoints()[Count];
            lDestPoint = lPivot.MultT(lSrcPoint);

            mFileObject->FieldWriteD(lDestPoint.mData [0]);   // X
            mFileObject->FieldWriteD(lDestPoint.mData [1]);   // Y
            mFileObject->FieldWriteD(lDestPoint.mData [2]);   // Z
            mFileObject->FieldWriteD(lNurbs->GetControlPoints()[Count] [3] );       // Weight
        }

        mFileObject->FieldWriteEnd();      

        FBX_ASSERT_MSG
        (
            (lNurbs->GetUKnotVector()!=NULL) && (lNurbs->GetVKnotVector()!=NULL),
            "FbxWriterFbx6::WriteNurb : Null knot vector."
        );

        //
        // Knot Vector U
        //
        mFileObject->FieldWriteBegin( FIELD_KFBXGEOMETRYNURBS_SURFACE_KNOTVECTOR_U);

        TotalCount = lNurbs->GetUKnotCount();

        for (Count = 0; Count < TotalCount; Count ++)
        {
            mFileObject->FieldWriteD(*(lNurbs->GetUKnotVector()+Count));
        }

        mFileObject->FieldWriteEnd ();

        //
        // Knot Vector V
        //
        mFileObject->FieldWriteBegin( FIELD_KFBXGEOMETRYNURBS_SURFACE_KNOTVECTOR_V);

        TotalCount = lNurbs->GetVKnotCount();

        for (Count = 0; Count < TotalCount; Count ++)
        {
            mFileObject->FieldWriteD(*(lNurbs->GetVKnotVector()+Count));
        }

        mFileObject->FieldWriteEnd ();

        WriteGeometry(*lNurbs);

        //
        // FlipNormal flag
        //
        mFileObject->FieldWriteI( FIELD_KFBXGEOMETRYNURBS_SURFACE_FLIP_NORMALS, (int) lNurbs->GetFlipNormals() );

    }

    if (pNurbs.GetApplyFlip())
    {
        lNurbs->Destroy();
    }

    return true;
}


bool FbxWriterFbx6::WritePatch(FbxPatch& pPatch)
{
    int Count;
    FbxAMatrix lPivot;
    pPatch.GetPivot(lPivot);

    FbxVector4 lSrcPoint;
    FbxVector4 lDestPoint;

    //
    // Store the Patch.
    //
    if(pPatch.GetControlPointsCount ())
    {
        //
        // Store PATCHVERSION...
        //
        // Version 100: Original version
        mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYPATCH_PATCH_VERSION, 100);

        //
        // Store the SURFACEDISPLAY...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_SURFACE_DISPLAY);
		{
            mFileObject->FieldWriteI(pPatch.GetSurfaceMode());
            mFileObject->FieldWriteI(pPatch.GetUStep());
            mFileObject->FieldWriteI(pPatch.GetVStep());
		}
        mFileObject->FieldWriteEnd ();

        //
        // Store the PATCHTYPE...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_PATCH_TYPE);
		{
            WritePatchType(pPatch, pPatch.GetPatchUType());
            WritePatchType(pPatch, pPatch.GetPatchVType());
		}
        mFileObject->FieldWriteEnd();

        //
        // Store the DIMENSION...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_DIMENSIONS);
		{
            mFileObject->FieldWriteI(pPatch.GetUCount());
            mFileObject->FieldWriteI(pPatch.GetVCount());
		}
        mFileObject->FieldWriteEnd ();

        //
        // Store the STEP...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_STEP);
		{
            mFileObject->FieldWriteI(pPatch.GetUStep());
            mFileObject->FieldWriteI(pPatch.GetVStep());
		}
        mFileObject->FieldWriteEnd ();

        //
        // Store the CLOSED...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_CLOSED);
		{
            mFileObject->FieldWriteI(pPatch.GetUClosed());
            mFileObject->FieldWriteI(pPatch.GetVClosed());
		}
        mFileObject->FieldWriteEnd ();

        //
        // Store the UCAPPED...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_UCAPPED);
		{
            mFileObject->FieldWriteI(pPatch.GetUCappedBottom());
            mFileObject->FieldWriteI(pPatch.GetUCappedTop());
		}
        mFileObject->FieldWriteEnd ();

        //
        // Store the VCAPPED...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_VCAPPED);
		{
            mFileObject->FieldWriteI(pPatch.GetVCappedBottom());
            mFileObject->FieldWriteI(pPatch.GetVCappedTop());
		}
        mFileObject->FieldWriteEnd ();


        //
        // Store the Control points
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_POINTS);
		{
            for (Count = 0; Count < pPatch.GetControlPointsCount (); Count ++)
            {
                lSrcPoint = pPatch.GetControlPoints()[Count];
                lDestPoint = lPivot.MultT(lSrcPoint);

                mFileObject->FieldWriteD(lDestPoint.mData [0]);   // X
                mFileObject->FieldWriteD(lDestPoint.mData [1]);   // Y
                mFileObject->FieldWriteD(lDestPoint.mData [2]);   // Z
            }
		}
        mFileObject->FieldWriteEnd();

        WriteGeometry(pPatch);
    }

    return true;
}

void FbxWriterFbx6::WriteGeometryWeightedMap(FbxGeometryWeightedMap& pGeometryWeightedMap)
{
    FbxWeightedMapping* lMapping = pGeometryWeightedMap.GetValues();

    int lSrcCount = (lMapping) ? lMapping->GetElementCount(FbxWeightedMapping::eSource) : 0;
    int lDstCount = (lMapping) ? lMapping->GetElementCount(FbxWeightedMapping::eDestination) : 0;

    //
    // Store version...
    //
    // Version 100: Original version
    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRY_WEIGHTED_MAP_VERSION, 100);

    //
    // Store source count...
    //
    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRY_WEIGHTED_MAP_SRC_COUNT);
	{
        mFileObject->FieldWriteI(lSrcCount);
	}
    mFileObject->FieldWriteEnd ();

    //
    // Store destination count...
    //
    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRY_WEIGHTED_MAP_DST_COUNT);
	{
        mFileObject->FieldWriteI(lDstCount);
	}
    mFileObject->FieldWriteEnd ();


    FbxWeightedMapping::Element lElem;
    int i, j, lRelCount;

    for (i = 0; i < lSrcCount; i++)
    {
        lRelCount = lMapping->GetRelationCount(FbxWeightedMapping::eSource, i);

        if (lRelCount < 1)
        {
            continue;
        }

        //
        // Store index mapping...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRY_WEIGHTED_MAP_INDEX_MAPPING);
		{
            //
            // Store source Index
            //
            mFileObject->FieldWriteI(i);

            //
            // Store relation count
            //
            mFileObject->FieldWriteI(lRelCount);

            for (j = 0; j < lRelCount; j++)
            {
                lElem = lMapping->GetRelation(FbxWeightedMapping::eSource, i, j);

                //
                // Store destination index
                //
                mFileObject->FieldWriteI(lElem.mIndex);

                //
                // Store destination weight
                //
                mFileObject->FieldWriteD(lElem.mWeight);
            }
		}
        mFileObject->FieldWriteEnd();
    }

}

bool FbxWriterFbx6::WriteDeformers(FbxScene& pScene)
{
    int i, lDeformerCount, lSubDeformerCount;

    // Write Skin deformers
	lDeformerCount = pScene.GetSrcObjectCount<FbxSkin>();
    for (i=0; i<lDeformerCount; i++)
    {
        FbxSkin* lSkin = pScene.GetSrcObject<FbxSkin>(i);

        WriteObjectHeaderAndReferenceIfAny(*lSkin, FIELD_KFBXDEFORMER_DEFORMER);
        mFileObject->FieldWriteBlockBegin();
        {
            WriteSkin(*lSkin);
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    // Write Cluster deformers
	lSubDeformerCount = pScene.GetSrcObjectCount<FbxCluster>();
    for (i=0; i<lSubDeformerCount; i++)
    {
        FbxCluster* lCluster = pScene.GetSrcObject<FbxCluster>(i);

        WriteObjectHeaderAndReferenceIfAny(*lCluster, FIELD_KFBXDEFORMER_DEFORMER);
        mFileObject->FieldWriteBlockBegin();
        {
            WriteCluster(*lCluster);
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    // Write Vertex Cache Deformers
	lDeformerCount = pScene.GetSrcObjectCount<FbxVertexCacheDeformer>();
    for (i=0; i<lDeformerCount; ++i)
    {
        FbxVertexCacheDeformer* lCacheDeformer = pScene.GetSrcObject<FbxVertexCacheDeformer>(i);

        WriteObjectHeaderAndReferenceIfAny(*lCacheDeformer, FIELD_KFBXDEFORMER_DEFORMER);
        mFileObject->FieldWriteBlockBegin();
        {
            WriteVertexCacheDeformer(*lCacheDeformer);
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx6::WriteVertexCacheDeformer( FbxVertexCacheDeformer& pDeformer )
{
    // Version
    mFileObject->FieldWriteI(FILED_KFBXVERTEXCACHEDEFORMER_VERSION, 100);

    // Properties
    WriteObjectPropertiesAndFlags(&pDeformer);

    return true;
}

bool FbxWriterFbx6::WriteSkin(FbxSkin& pSkin)
{
    // Version
    mFileObject->FieldWriteI(FIELD_KFBXDEFORMER_VERSION, 100);

    // Properties
    WriteObjectPropertiesAndFlags(&pSkin);

    mFileObject->FieldWriteD(FIELD_KFBXSKIN_DEFORM_ACCURACY, pSkin.GetDeformAccuracy());

    return true;
}

bool FbxWriterFbx6::WriteCluster(FbxCluster& pCluster)
{
    int lIndex;

    // Version
    mFileObject->FieldWriteI(FIELD_KFBXDEFORMER_VERSION, 100);

    // Properties
    WriteObjectPropertiesAndFlags(&pCluster);

    //
    // Store the Field MODE...
    //
    switch( pCluster.GetLinkMode() )
    {
        case FbxCluster::eNormalize:
			break;

        case FbxCluster::eAdditive:
			mFileObject->FieldWriteC(FIELD_KFBXDEFORMER_MODE, TOKEN_KFBXDEFORMER_ADDITIVE);
			break;

        case FbxCluster::eTotalOne:
			mFileObject->FieldWriteC(FIELD_KFBXDEFORMER_MODE, TOKEN_KFBXDEFORMER_TOTAL1);
			break;

        default:
			FBX_ASSERT_NOW("Unexpected deformer mode.");
			break;
    }

    //
    // Store the Field USERDATA...
    //
    mFileObject->FieldWriteBegin(FIELD_KFBXDEFORMER_USERDATA);
    {
        mFileObject->FieldWriteC(pCluster.GetUserDataID());
        mFileObject->FieldWriteC(pCluster.GetUserData());
    }
    mFileObject->FieldWriteEnd();

    //
    // Store the Field INDICES...
    //
    mFileObject->FieldWriteBegin(FIELD_KFBXDEFORMER_INDEXES);
    {
        for (lIndex=0; lIndex<pCluster.GetControlPointIndicesCount(); lIndex++)
        {
            mFileObject->FieldWriteI(pCluster.GetControlPointIndices()[lIndex]);
        }
    }
    mFileObject->FieldWriteEnd ();

    //
    // Store the Field WEIGHTS...
    //
    mFileObject->FieldWriteBegin(FIELD_KFBXDEFORMER_WEIGHTS);
    {
        for (lIndex=0; lIndex<pCluster.GetControlPointIndicesCount(); lIndex++)
        {
            mFileObject->FieldWriteD(pCluster.GetControlPointWeights()[lIndex]);
        }
    }
    mFileObject->FieldWriteEnd();

    FbxAMatrix Transform;
    pCluster.GetTransformMatrix(Transform);
    FbxAMatrix TransformLink;
    pCluster.GetTransformLinkMatrix(TransformLink);
    Transform = TransformLink.Inverse() * Transform;

    //
    // Store the transformation matrix
    //
    mFileObject->FieldWriteDn(FIELD_KFBXDEFORMER_TRANSFORM, (double*)Transform.mData, 16);

    //
    // Store the transformation link matrix
    //
    mFileObject->FieldWriteDn(FIELD_KFBXDEFORMER_TRANSFORM_LINK, (double*)TransformLink.mData, 16);

    //
    // Associate Model
    //
    FbxProperty lProperty = pCluster.FindProperty("SrcModelReference");
    if (lProperty.IsValid())
    {
		FbxNode* lNode = lProperty.GetSrcObject<FbxNode>();
        if (lNode)
        {
            mFileObject->FieldWriteBegin(FIELD_KFBXDEFORMER_ASSOCIATE_MODEL);
            mFileObject->FieldWriteBlockBegin();
            {
                FbxAMatrix TransformAssociate;
                pCluster.GetTransformAssociateModelMatrix(TransformAssociate);
                TransformAssociate = TransformLink.Inverse() * TransformAssociate;

                //
                // Store the transformation associate matrix
                //
                mFileObject->FieldWriteDn(FIELD_KFBXDEFORMER_TRANSFORM, (double*)TransformAssociate.mData, 16);
            }
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();
        }
    }

    //
    // Parent Node
    //
    if(pCluster.IsTransformParentSet())
    {
        FbxAMatrix TransformParent;
        pCluster.GetTransformParentMatrix(TransformParent);
        mFileObject->FieldWriteDn(FIELD_KFBXDEFORMER_TRANSFORM_PARENT, (double*)TransformParent.mData, 16);
    }

    return true;
}

bool FbxWriterFbx6::WriteShape(FbxShape& pShape, FbxString pShapeName, FbxGeometry& pGeometry)
{   
    FbxAMatrix lPivot;
    pGeometry.GetPivot(lPivot);   
    FbxVector4  lSrcPoint;
    FbxVector4  lDestPoint;
    FbxVector4  lRefSrcPoint;
    FbxVector4  lRefDestPoint;
    int i, lCount;
    bool lControlPointsValid = true, lNormalsValid = true;
    FbxArray<int> lValidIndices;

    if (pGeometry.GetControlPointsCount() != pShape.GetControlPointsCount())
    {
        FBX_ASSERT_NOW("Control points in shape incompatible with control points in geometry.");
        lControlPointsValid = false;
        lValidIndices.Add(0);
    }
    else
    {
        FindShapeValidIndices(pGeometry.mControlPoints, pShape.mControlPoints, lValidIndices);

        if (lValidIndices.GetCount() == 0)
        {
            lControlPointsValid = false;
            lValidIndices.Add(0);
        }
    }

    lCount = lValidIndices.GetCount();

    mFileObject->FieldWriteBegin(FIELD_KFBXSHAPE_SHAPE);

    mFileObject->FieldWriteC(pShapeName);

    mFileObject->FieldWriteBlockBegin ();
	{
        //
        // Save the indices.
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXSHAPE_INDEXES);

        for (i = 0; i < lCount; i++)
        {
            mFileObject->FieldWriteI(lValidIndices[i]);
        }

        mFileObject->FieldWriteEnd();

        FbxArray<FbxVector4> lWorkControlPoints;

        if (lControlPointsValid)
        {
            FbxArray<FbxVector4>& lGeometryControlPoints = pGeometry.mControlPoints;
            lWorkControlPoints = pShape.mControlPoints;

            //
            // Convert control points to relative vectors.
            //
            for (i = 0; i < lCount; i++)
            {
                FbxVector4& lVector = lWorkControlPoints[lValidIndices[i]];
                FbxVector4& lReferenceVector = lGeometryControlPoints[lValidIndices[i]];

                lSrcPoint = lVector;
                lDestPoint = lPivot.MultT(lSrcPoint);
                lRefSrcPoint = lReferenceVector;
                lRefDestPoint = lPivot.MultT(lRefSrcPoint);

                lVector[0] = lDestPoint[0] - lRefDestPoint[0];
                lVector[1] = lDestPoint[1] - lRefDestPoint[1];
                lVector[2] = lDestPoint[2] - lRefDestPoint[2];
            }
        }

        //
        // Save the control points.
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXSHAPE_VERTICES);

        for (i = 0; i < lCount; i++)
        {
            if (lControlPointsValid)
            {
                FbxVector4& lVector = lWorkControlPoints[lValidIndices[i]];

                mFileObject->FieldWriteD(lVector[0]);
                mFileObject->FieldWriteD(lVector[1]);
                mFileObject->FieldWriteD(lVector[2]);
            }
            else
            {
                mFileObject->FieldWriteD(0.0);
                mFileObject->FieldWriteD(0.0);
                mFileObject->FieldWriteD(0.0);
            }
        }

        mFileObject->FieldWriteEnd ();

        if (pGeometry.GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            FbxMesh* lMesh = (FbxMesh*) &pGeometry;
            FbxVector4* lWorkNormals = NULL;
            int* lIndices = NULL;
            int lArrayCount = lCount;
            bool lFatalCondition = (lMesh->GetLayer(0, FbxLayerElement::eNormal) == NULL || lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals() == NULL);
            if (lFatalCondition ||  pShape.GetLayer(0, FbxLayerElement::eNormal) == NULL || pShape.GetLayer(0, FbxLayerElement::eNormal)->GetNormals() == NULL)
            {                
                // Note: The case of the mesh not having the Normals layer is very improbable, however, the case of a shape without normals can
                // be quite frequent especially when we export from Maya and the shapes are baked (no actual target geometries are in the scene)
                lNormalsValid = false;
            }
            else
            {
                FbxLayerElementArrayTemplate<FbxVector4>& lGeometryNormals = lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetDirectArray();
                FbxLayerElementArrayTemplate<FbxVector4>& lShapeNormals = pShape.GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetDirectArray();

                // since targets and base geometries should have the same topology, the criteria for valid normals must also check that the arrays 
                // have the same number of items. If this is not the case, the loop below can possibly overflow the array and write outside its 
                // boundaries corrupting the memory (and the program execution)
                lNormalsValid = (lGeometryNormals.GetCount() == lShapeNormals.GetCount()) &&
                                (lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetMappingMode() == pShape.GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetMappingMode() &&
                                 lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetReferenceMode() == pShape.GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetReferenceMode() &&
                                (lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetMappingMode() == FbxLayerElement::eByControlPoint ||
                                 lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetMappingMode() == FbxLayerElement::eByPolygonVertex));

                if (lNormalsValid == false)
                {
                    FBX_ASSERT_NOW("Normals data in shape and geometry do not match criteria.");
                }
                else
                {
                    lArrayCount = lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetDirectArray().GetCount();
                    if (lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetReferenceMode() != FbxLayerElement::eDirect)
                        lIndices = lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetIndexArray().GetLocked(lIndices);

                    lWorkNormals = lShapeNormals.GetLocked(lWorkNormals);
                    FBX_ASSERT(lWorkNormals != NULL);

                    if (lWorkNormals)
                    {
                        //
                        // Convert the normals to relative vectors.
                        //
                        for (i = 0; i < lArrayCount; i++)
                        {
                            FbxVector4& lVector = lWorkNormals[i];
                            const FbxVector4& lReferenceVector = lGeometryNormals.GetAt(i);

                            lSrcPoint = lVector;
                            lDestPoint = lPivot.MultT(lSrcPoint);
                            lRefSrcPoint = lReferenceVector;
                            lRefDestPoint = lPivot.MultT(lRefSrcPoint);

                            lVector[0] = lDestPoint[0] - lRefDestPoint[0];
                            lVector[1] = lDestPoint[1] - lRefDestPoint[1];
                            lVector[2] = lDestPoint[2] - lRefDestPoint[2];
                        }
                    }
                }
            }

            //
            // Save the normals.
            //
            if (lFatalCondition)
            {
                FBX_ASSERT_NOW("FATAL CONDITION: Mesh object does not have normals.");
            }
            else
            {
                mFileObject->FieldWriteBegin(FIELD_KFBXSHAPE_NORMALS);

                bool byPolyVertex = lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetMappingMode() == FbxLayerElement::eByPolygonVertex;
                for (i = 0; i < lCount; i++)
                {
                    if (lNormalsValid && lWorkNormals)
                    {
                        int index = lValidIndices[i];
                        if (lIndices)
                        {
                            if (byPolyVertex)
                            {
                                int j = 0;
                                while (j < lMesh->GetPolygonVertexCount() && lMesh->GetPolygonVertices()[j] != index) j++;
                                FBX_ASSERT(j != lMesh->GetPolygonVertexCount());
                                index = j;
                            }
                            index = lIndices[index];
                        }

                        FbxVector4& lVector = lWorkNormals[index];

                        mFileObject->FieldWriteD(lVector[0]);
                        mFileObject->FieldWriteD(lVector[1]);
                        mFileObject->FieldWriteD(lVector[2]);
                    }
                    else
                    {
                        mFileObject->FieldWriteD(0.0);
                        mFileObject->FieldWriteD(0.0);
                        mFileObject->FieldWriteD(0.0);
                    }
                }

                if (lWorkNormals)
                    pShape.GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetDirectArray().Release(&lWorkNormals, lWorkNormals);

                if (lIndices)
                    lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetIndexArray().Release(&lIndices, lIndices);

                mFileObject->FieldWriteEnd ();
            } // lFatalCondition
        } // attribute == mesh
	}
    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();

    return true;
}

void FbxWriterFbx6::FindShapeValidIndices(FbxArray<FbxVector4>& pGeometryControlPoints, FbxArray<FbxVector4>& pShapeControlPoints, FbxArray<int>& lValidIndices)
{
    int i, lCount = pGeometryControlPoints.GetCount();

    for (i = 0; i < lCount; i++)
    {
        FbxVector4& lGeometryPoint = pGeometryControlPoints[i];
        FbxVector4& lShapePoint = pShapeControlPoints[i];

        if (lGeometryPoint[0] != lShapePoint[0] || lGeometryPoint[1] != lShapePoint[1] || lGeometryPoint[2] != lShapePoint[2])
        {
            lValidIndices.Add(i);
        }
    }
}

bool FbxWriterFbx6::WritePatchType(FbxPatch& pPatch, int pType)
{
    switch ((FbxPatch::EType) pType)
    {
        case FbxPatch::eBezier:            
			mFileObject->FieldWriteC("Bezier");
			break;

        case FbxPatch::eBezierQuadric:   
			mFileObject->FieldWriteC("BezierQuadric");
			break;

        case FbxPatch::eCardinal:          
			mFileObject->FieldWriteC("Cardinal");
			break;

        case FbxPatch::eBSpline:           
			mFileObject->FieldWriteC("BSpline");
			break;

        case FbxPatch::eLinear:            
			mFileObject->FieldWriteC("Linear");
			break;
    }

    return true;
}


bool FbxWriterFbx6::WriteSkeleton(FbxSkeleton& pSkeleton)
{
    switch(pSkeleton.GetSkeletonType())
    {
		case FbxSkeleton::eRoot:
			WriteSkeletonRoot(pSkeleton);
			break;

		case FbxSkeleton::eLimb:
			WriteSkeletonLimb(pSkeleton);
			break;

		case FbxSkeleton::eLimbNode:
			WriteSkeletonLimbNode(pSkeleton);
			break;

		case FbxSkeleton::eEffector:
			WriteSkeletonEffector(pSkeleton);
			break;
    }

    return true;
}


bool FbxWriterFbx6::WriteSkeletonRoot(FbxSkeleton& pSkeleton)
{
    if (pSkeleton.GetNode())
    {
        return WriteNodeType(*pSkeleton.GetNode());
    }
    return false;
}


bool FbxWriterFbx6::WriteSkeletonLimb(FbxSkeleton& pSkeleton)
{
    if (pSkeleton.GetNode())
    {
        return WriteNodeType(*pSkeleton.GetNode());
    }
    return false;
}


bool FbxWriterFbx6::WriteSkeletonLimbNode(FbxSkeleton& pSkeleton)
{
    if (pSkeleton.GetNode())
    {
        return WriteNodeType(*pSkeleton.GetNode());
    }
    return false;
}


bool FbxWriterFbx6::WriteSkeletonEffector(FbxSkeleton& pSkeleton)
{
    return WriteSkeletonRoot(pSkeleton);
}

bool FbxWriterFbx6::WriteCaches(FbxDocument* pDocument)
{
    int i, lCount = pDocument->GetSrcObjectCount<FbxCache>();
    FbxCache* lCache = NULL;

    for (i=0; i<lCount; i++)
    {
        lCache = pDocument->GetSrcObject<FbxCache>(i);
        // if the Cache object has not the savable flag set, it means that this object is only
        // referenced by FbxCachedEffect attributes and they are not supported in this format, so
        // we skip this cache too.
        if (lCache->GetObjectFlags(FbxObject::eSavable))
            WriteCache(*lCache);
    }

    return true;
}

bool FbxWriterFbx6::WriteCache(FbxCache& pCache)
{
    WriteObjectHeaderAndReferenceIfAny(pCache, FIELD_KFBXCACHE_VERTEX_CACHE);
    mFileObject->FieldWriteBlockBegin ();
    {
        mFileObject->FieldWriteI(FIELD_KFBXCACHE_VERSION, 100);

		// Properties
		WriteObjectPropertiesAndFlags(&pCache);

		mFileObject->FieldWriteBlockEnd ();
    }
    mFileObject->FieldWriteEnd ();
    return true;
}

bool FbxWriterFbx6::WriteBindingTables(FbxDocument* pDocument)
{
    int i, lCount = pDocument->GetSrcObjectCount<FbxBindingTable>();
    FbxBindingTable* lTable = NULL;

    for (i=0; i<lCount; i++)
    {
        lTable = pDocument->GetSrcObject<FbxBindingTable>(i);
        WriteBindingTable(*lTable);
    }

    return true;
}

bool FbxWriterFbx6::WriteBindingTable(FbxBindingTable& pTable)
{
    WriteObjectHeaderAndReferenceIfAny(pTable, FIELD_KFBXBINDINGTABLE_BINDING_TABLE);

    bool lStatus = true;

    mFileObject->FieldWriteBlockBegin();
    {
        mFileObject->FieldWriteI(FIELD_KFBXBINDINGTABLE_VERSION, 100);

        //Get the relative url from absolute url
        FbxPropertyT<FbxString> lRelativeURL = pTable.DescRelativeURL;
        FbxPropertyT<FbxString> lAbsoluteURL = pTable.DescAbsoluteURL;
        if (lRelativeURL.IsValid() && lAbsoluteURL.IsValid())
        {
            FbxString lRelativePathStr = lRelativeURL.Get();
            FbxString llAbsoluteURLStr = lAbsoluteURL.Get();
            if(lRelativePathStr == "" && llAbsoluteURLStr != "")
            {
                lRelativePathStr = mFileObject->GetRelativeFilePath(llAbsoluteURLStr);
                pTable.DescRelativeURL = lRelativePathStr.Buffer();
            }
        }

        // Properties
        WriteObjectPropertiesAndFlags(&pTable);

        // Write out the binding table entries
        size_t i = 0, lCount = pTable.GetEntryCount();
        for( i = 0; i < lCount; ++i )
        {
            // entries: source, source type, destination, destination type.
            mFileObject->FieldWriteBegin(FIELD_KFBXBINDINGTABLE_ENTRY);

            const FbxBindingTableEntry& lEntry = pTable.GetEntry(i);
           
            mFileObject->FieldWriteC(lEntry.GetSource());
            mFileObject->FieldWriteC(lEntry.GetEntryType( true ));
            mFileObject->FieldWriteC(lEntry.GetDestination());
            mFileObject->FieldWriteC(lEntry.GetEntryType( false ));

            mFileObject->FieldWriteEnd();
        }


        // check if the user wants to embed 
        bool lEmbed = IOS_REF.GetBoolProp(EXP_FBX_EMBEDDED, false) && mFileObject->IsBinary();

        if( lEmbed )
        {           
            FbxProperty lExcludedProp = IOS_REF.GetProperty(EXP_FBX_EMBEDDED_PROPERTIES_SKIP).GetFirstDescendent();

            while( lExcludedProp.IsValid() )
            {
                FbxString lExcludedName = lExcludedProp.Get<FbxString>();
                if( lExcludedName == pTable.DescRelativeURL.GetHierarchicalName() || lExcludedName == pTable.DescAbsoluteURL.GetHierarchicalName() )
                {
                    lEmbed = false;
                    break;
                }

                lExcludedProp = IOS_REF.GetProperty(EXP_FBX_EMBEDDED_PROPERTIES_SKIP).GetNextDescendent( lExcludedProp );
            }
        }

        if( lEmbed )
        {
            FbxString lResolvedAbsPath;

            FbxProperty lTestProp = pTable.DescAbsoluteURL;
            int lUrlCount = mManager.GetXRefManager().GetUrlCount( lTestProp );

            if( lUrlCount == 0 )
            {                
                lTestProp = pTable.DescRelativeURL;
                lUrlCount = mManager.GetXRefManager().GetUrlCount( lTestProp );
            }

            mFileObject->FieldWriteI(FIELD_MEDIA_VERSION, 100);

            for(int ii = 0; ii < lUrlCount; ++ii )
            {

                mFileObject->FieldWriteBegin(FIELD_EMBEDDED_FILE);
                mFileObject->FieldWriteBlockBegin();

                if( mManager.GetXRefManager().GetResolvedUrl( lTestProp, ii, lResolvedAbsPath ) )
                {
                    mFileObject->FieldWriteC(FIELD_MEDIA_FILENAME, lResolvedAbsPath);
                    
                    mFileObject->FieldWriteC(FIELD_MEDIA_RELATIVE_FILENAME, mFileObject->GetRelativeFilePath(lResolvedAbsPath));
                   
                    mFileObject->FieldWriteBegin(FIELD_MEDIA_CONTENT);
                    
                    lStatus &= mFileObject->FieldWriteEmbeddedFile(lResolvedAbsPath, lResolvedAbsPath);

                    mFileObject->FieldWriteEnd();
                }

                mFileObject->FieldWriteBlockEnd();
                mFileObject->FieldWriteEnd();
            }


            FBX_ASSERT( lStatus );
        }
    }
    mFileObject->FieldWriteBlockEnd ();

    mFileObject->FieldWriteEnd ();
    return lStatus;
}

bool FbxWriterFbx6::WriteBindingOperators(FbxDocument* pDocument)
{
    int i, lCount = pDocument->GetSrcObjectCount<FbxBindingOperator>();
    FbxBindingOperator* lOp = NULL;

    for (i=0; i<lCount; i++)
    {
        lOp = pDocument->GetSrcObject<FbxBindingOperator>(i);
        WriteBindingOperator(*lOp);
    }

    return true;
}

bool FbxWriterFbx6::WriteBindingOperator(FbxBindingOperator& pOperator)
{
    WriteObjectHeaderAndReferenceIfAny(pOperator, FIELD_KFBXBINDINGOPERATOR_BINDING_OPERATOR);

    mFileObject->FieldWriteBlockBegin();
    {
        mFileObject->FieldWriteI(FIELD_KFBXBINDINGOPERATOR_VERSION, 100);

        // Properties
        WriteObjectPropertiesAndFlags(&pOperator);

        // Write out the binding table entries
        size_t i = 0, lCount = pOperator.GetEntryCount();
        for( i = 0; i < lCount; ++i )
        {
            // entries: source, source type, destination, destination type.
            mFileObject->FieldWriteBegin(FIELD_KFBXBINDINGOPERATOR_ENTRY);

            const FbxBindingTableEntry& lEntry = pOperator.GetEntry(i);

			mFileObject->FieldWriteC(lEntry.GetSource());
            mFileObject->FieldWriteC(lEntry.GetEntryType( true ));
            mFileObject->FieldWriteC(lEntry.GetDestination());
            mFileObject->FieldWriteC(lEntry.GetEntryType( false ));

            mFileObject->FieldWriteEnd();
        }


    }
    mFileObject->FieldWriteBlockEnd ();

    mFileObject->FieldWriteEnd ();
    return true;
}

bool FbxWriterFbx6::WriteImplementations(FbxDocument* pDocument)
{
	int i, lCount = pDocument ? pDocument->GetSrcObjectCount<FbxImplementation>() : 0;
    FbxImplementation* lImplementation = NULL;

    FbxArray< KTypeObjectReferenceDepth<FbxImplementation> > lDepthArray;

    // Construct an array of implementation and compute their reference depth
    for (i=0; i<lCount; i++)
    {
        lImplementation = pDocument->GetSrcObject<FbxImplementation>(i);

        KTypeObjectReferenceDepth<FbxImplementation> lObjRefDepth;

        lObjRefDepth.mObject = lImplementation;
        lObjRefDepth.mRefDepth = ComputeReferenceDepth(lImplementation);
        lDepthArray.Add(lObjRefDepth);
    }

    // Sort the array of implementation by their reference depth
    qsort( lDepthArray.GetArray(), lDepthArray.GetCount(), sizeof(KTypeObjectReferenceDepth<FbxImplementation>),
			CompareKTypeObjectReferenceDepth<FbxImplementation> );

    for (i=0; i<lCount; i++)
    {
        lImplementation = lDepthArray.GetAt(i).mObject;
        WriteImplementation(*lImplementation);
    }

    return true;
}

bool FbxWriterFbx6::WriteImplementation(FbxImplementation& pImplementation)
{
    WriteObjectHeaderAndReferenceIfAny(pImplementation, FIELD_KFBXIMPLEMENTATION_IMPLEMENTATION);

    mFileObject->FieldWriteBlockBegin ();
    {
        mFileObject->FieldWriteI(FIELD_KFBXIMPLEMENTATION_VERSION, 100);

        WriteObjectPropertiesAndFlags(&pImplementation);

        mFileObject->FieldWriteBlockEnd ();
    }

    mFileObject->FieldWriteEnd ();

    return true;
}

bool FbxWriterFbx6::WriteCollections(FbxDocument* pDocument)
{
	int i, lCount = pDocument ? pDocument->GetSrcObjectCount<FbxCollection>() : 0;
    FbxCollection* lCollection = NULL;

    for (i=0; i<lCount; i++)
    {
        lCollection = pDocument->GetSrcObject<FbxCollection>(i);

        // Following test is used to skip sub-classes like FbxDocument & FbxScene
        if (FbxCollection::ClassId == lCollection->GetRuntimeClassId())
        {
            WriteCollection(*lCollection);
        }
    }

    return true;
}

bool FbxWriterFbx6::WriteCollection(FbxCollection& pCollection)
{
    WriteObjectHeaderAndReferenceIfAny(pCollection, FIELD_KFBXCOLLECTION_COLLECTION);
    mFileObject->FieldWriteBlockBegin ();
    {
        mFileObject->FieldWriteI(FIELD_KFBXCOLLECTION_VERSION, 100);

        WriteObjectPropertiesAndFlags(&pCollection);

        mFileObject->FieldWriteBlockEnd ();
    }
    mFileObject->FieldWriteEnd ();

    return true;
}

bool FbxWriterFbx6::WriteDocuments(FbxDocument* pDocument)
{
	int i, lCount = pDocument ? pDocument->GetSrcObjectCount<FbxDocument>() : 0;
    FbxDocument* lDoc = NULL;

    for (i=0; i<lCount; i++)
    {
        lDoc = pDocument->GetSrcObject<FbxDocument>(i);
        WriteDocument(*lDoc);
    }

    return true;
}

bool FbxWriterFbx6::WriteDocument(FbxDocument& pSubDocument)
{
    WriteObjectHeaderAndReferenceIfAny(pSubDocument, FIELD_KFBXDOCUMENT_DOCUMENT);
    mFileObject->FieldWriteBlockBegin ();
    {
        mFileObject->FieldWriteI(FIELD_KFBXDOCUMENT_VERSION, 100);
        WriteObjectPropertiesAndFlags(&pSubDocument);
    }
    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();

    return true;
}

bool FbxWriterFbx6::WriteLayeredTextures(FbxDocument* pDocument)
{
	int i, lCount = pDocument ? pDocument->GetSrcObjectCount<FbxLayeredTexture>() : 0;
    FbxLayeredTexture* lTexture = NULL;

    for (i=0; i<lCount; i++)
    {
        lTexture = pDocument->GetSrcObject<FbxLayeredTexture>(i);
        WriteLayeredTexture(*lTexture);
    }

    return true;
}

bool FbxWriterFbx6::WriteLayeredTexture(FbxLayeredTexture& pTexture)
{
    WriteObjectHeaderAndReferenceIfAny(pTexture, FIELD_KFBXLAYEREDTEXTURE_LAYERED_TEXTURE);

    mFileObject->FieldWriteBlockBegin ();
    {
        mFileObject->FieldWriteI(FIELD_KFBXLAYEREDTEXTURE_LAYERED_TEXTURE, 100);

        // Properties
        WriteObjectPropertiesAndFlags(&pTexture);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYEREDTEXTURE_BLENDMODES);

        for( int i = 0; i < pTexture.mInputData.GetCount(); ++i )
        {
            mFileObject->FieldWriteI( pTexture.mInputData[i].mBlendMode );
        }

        mFileObject->FieldWriteEnd();


        mFileObject->FieldWriteBlockEnd ();
    }
    mFileObject->FieldWriteEnd ();

    return true;
}


bool FbxWriterFbx6::WriteTextures(FbxDocument* pDocument)
{
	int i, lCount = pDocument ? pDocument->GetSrcObjectCount<FbxTexture>() : 0;
    FbxTexture* lTexture;

    FbxArray< KTypeObjectReferenceDepth<FbxTexture> > lDepthArray;

    // Construct an array of texture and compute their reference depth
    for (i=0; i<lCount; i++)
    {
        lTexture = pDocument->GetSrcObject<FbxTexture>(i);

        KTypeObjectReferenceDepth<FbxTexture> lObjRefDepth;

        lObjRefDepth.mObject = lTexture;
        lObjRefDepth.mRefDepth = ComputeReferenceDepth(lTexture);
        lDepthArray.Add(lObjRefDepth);
    }

    // Sort the array of texture by their reference depth
    qsort(lDepthArray.GetArray(), lDepthArray.GetCount(), sizeof(KTypeObjectReferenceDepth<FbxTexture>), CompareKTypeObjectReferenceDepth<FbxTexture>);

    for (i=0; i<lCount; i++)
    {
        lTexture = lDepthArray.GetAt(i).mObject;

        // layered texture is a texture, that needs to
        // be written out slightly differently.
        if( lTexture->Is<FbxLayeredTexture>() )
        {
            WriteLayeredTexture( *(FbxCast<FbxLayeredTexture>(lTexture)));
        }
        else if (lTexture->Is<FbxFileTexture>() )
        {
            WriteTexture( *(FbxCast<FbxFileTexture>(lTexture)));
        }
		// Skip other texture types like FbxProceduralTexture.
    }

    return true;
}

bool FbxWriterFbx6::WriteTexture(FbxFileTexture& pTexture)
{    
    FbxVector4     lVector;

    WriteObjectHeaderAndReferenceIfAny(pTexture, FIELD_KFBXTEXTURE_TEXTURE);

    mFileObject->FieldWriteBlockBegin ();

    FbxFileTexture* lReferencedObject = FbxCast<FbxFileTexture>(pTexture.GetReferenceTo());
  
    FbxString lTextureType = pTexture.GetTextureType();

    bool lDoWrite = (lReferencedObject == NULL) ? true : (lTextureType != lReferencedObject->GetTextureType());
    if (lDoWrite)
    {
        mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_TYPE, lTextureType);
    }

    mFileObject->FieldWriteI(FIELD_KFBXTEXTURE_VERSION, 202);

    FbxString lTextureName = pTexture.GetNameWithNameSpacePrefix();
    lDoWrite = (lReferencedObject == NULL) ? true : (lTextureName != lReferencedObject->GetNameWithNameSpacePrefix());
    if (lDoWrite)
    {
        mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_TEXTURE_NAME, lTextureName);
    }

    // Write properties
    WriteObjectPropertiesAndFlags(&pTexture);

    FbxString lName;

    lName = pTexture.GetMediaName();
    lDoWrite = (lReferencedObject == NULL) ? true : (lName.Compare(lReferencedObject->GetMediaName()) != 0);
    if (lDoWrite)
    {
        FbxString lTmpName = FbxManager::PrefixName(VIDEO_PREFIX,  lName);
        mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_MEDIA, lTmpName.Buffer());
    }

    lName = pTexture.GetFileName();
    lDoWrite = (lReferencedObject == NULL) ? true : (lName.Compare(lReferencedObject->GetFileName()) != 0);
    if (lDoWrite)
    {
        mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_FILENAME, lName);
    }

    lName = pTexture.GetRelativeFileName();
    lDoWrite = (lReferencedObject == NULL) ? true : (lName.Compare(lReferencedObject->GetRelativeFileName()) != 0);
    if (lDoWrite)
    {
        mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_RELATIVE_FILENAME, lName);
    }

    lDoWrite = (lReferencedObject == NULL)? true : (pTexture.GetUVTranslation() != lReferencedObject->GetUVTranslation());
    if (lDoWrite)
    {
        mFileObject->FieldWriteBegin (FIELD_KFBXTEXTURE_UV_TRANSLATION);
        mFileObject->FieldWriteD(pTexture.GetUVTranslation()[FbxTexture::eU]);
        mFileObject->FieldWriteD(pTexture.GetUVTranslation()[FbxTexture::eV]);
        mFileObject->FieldWriteEnd ();
    }

    lDoWrite = (lReferencedObject == NULL) ? true : (pTexture.GetUVScaling() != lReferencedObject->GetUVScaling());
    if (lDoWrite)
    {
        mFileObject->FieldWriteBegin (FIELD_KFBXTEXTURE_UV_SCALING);
        mFileObject->FieldWriteD(pTexture.GetUVScaling()[FbxTexture::eU]);
        mFileObject->FieldWriteD(pTexture.GetUVScaling()[FbxTexture::eV]);
        mFileObject->FieldWriteEnd ();
    }

    lDoWrite = (lReferencedObject == NULL) ? true : (pTexture.GetAlphaSource() != lReferencedObject->GetAlphaSource());
    if (lDoWrite)
    {
        //
        // Texture alpha source
        //
        switch(pTexture.GetAlphaSource())
        {
			case  FbxTexture::eNone:
				mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_ALPHA_SRC, "None");
				break;

			case  FbxTexture::eRGBIntensity:
				mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_ALPHA_SRC, "RGB_Intensity");
				break;

			case  FbxTexture::eBlack:
				mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_ALPHA_SRC, "Alpha_Black");
				break;

			default:
				mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_ALPHA_SRC, "None");
				break;
        }
    }

    lDoWrite =	(lReferencedObject == NULL) ? true :(pTexture.GetCroppingLeft() != lReferencedObject->GetCroppingLeft()) ||
				(pTexture.GetCroppingRight() != lReferencedObject->GetCroppingRight()) || (pTexture.GetCroppingTop() != lReferencedObject->GetCroppingTop()) ||
				(pTexture.GetCroppingBottom() != lReferencedObject->GetCroppingBottom());
    if (lDoWrite)
    {
        mFileObject->FieldWriteBegin(FIELD_KFBXTEXTURE_CROPPING);
		{
            mFileObject->FieldWriteI(pTexture.GetCroppingLeft());
            mFileObject->FieldWriteI(pTexture.GetCroppingRight());
            mFileObject->FieldWriteI(pTexture.GetCroppingTop());
            mFileObject->FieldWriteI(pTexture.GetCroppingBottom());
		}
        mFileObject->FieldWriteEnd ();
    }      

    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();

    return true;
}

bool FbxWriterFbx6::WriteThumbnails(FbxDocument* pDocument)
{
	int i, lCount = pDocument ? pDocument->GetSrcObjectCount<FbxThumbnail>() : 0;
    FbxThumbnail* lThumbnail;

    FbxArray<KTypeObjectReferenceDepth<FbxThumbnail> > lDepthArray;

    // Construct an array of thumbnail and compute their reference depth
    for (i=0; i<lCount; i++)
    {
        lThumbnail = pDocument->GetSrcObject<FbxThumbnail>(i);

        KTypeObjectReferenceDepth<FbxThumbnail> lObjRefDepth;

        lObjRefDepth.mObject = lThumbnail;
        lObjRefDepth.mRefDepth = ComputeReferenceDepth(lThumbnail);
        lDepthArray.Add(lObjRefDepth);
    }

    // Sort the array of thumbnail by their reference depth
    qsort( lDepthArray.GetArray(), lDepthArray.GetCount(), sizeof(KTypeObjectReferenceDepth<FbxThumbnail>), CompareKTypeObjectReferenceDepth<FbxThumbnail> );

    for (i=0; i<lCount; i++)
    {
        lThumbnail = lDepthArray.GetAt(i).mObject;
        WriteThumbnail(*lThumbnail);
    }

    return true;
}

bool FbxWriterFbx6::WriteThumbnail(FbxThumbnail& pThumbnail)
{
    if (pThumbnail.GetSize() != FbxThumbnail::eNotSet)
    {
        // This is a non-empty thumbnail, so save it
        FbxUChar* lImagePtr = pThumbnail.GetThumbnailImage();
        unsigned long lSize = pThumbnail.GetSizeInBytes();
        unsigned long i;

        WriteObjectHeaderAndReferenceIfAny(pThumbnail, FIELD_THUMBNAIL);

        mFileObject->FieldWriteBlockBegin();

        bool lDoWrite = true;
        FbxThumbnail* lReferencedObject = FbxCast<FbxThumbnail>(pThumbnail.GetReferenceTo());

        if (lReferencedObject != NULL)
        {
            lDoWrite = (pThumbnail.GetDataFormat() != lReferencedObject->GetDataFormat());
            if (!lDoWrite)
            {
                lDoWrite = (pThumbnail.GetSize() != lReferencedObject->GetSize());
            }
            if (!lDoWrite) 
			{
                FbxUChar* lRefImagePtr = lReferencedObject->GetThumbnailImage();
                for (i=0; i<lSize; i++)
                {
                    if ((lRefImagePtr[i] != lImagePtr[i]))
                    {
                        lDoWrite = true;
                        break;
                    }
                }
            }
        }

        mFileObject->FieldWriteI(FIELD_THUMBNAIL_VERSION, 100);

        if (lDoWrite)
        {
            mFileObject->FieldWriteI(FIELD_THUMBNAIL_FORMAT, pThumbnail.GetDataFormat());

            mFileObject->FieldWriteI(FIELD_THUMBNAIL_SIZE,   pThumbnail.GetSize());

            // hard code an encoding of "0" for "raw data". In future version, encoding
            // will indicate if the file is stored with OpenEXR or RAW.
            mFileObject->FieldWriteI(FIELD_THUMBNAIL_ENCODING, 0);

            mFileObject->FieldWriteBegin(FIELD_THUMBNAIL_IMAGE);

            for (i=0; i<lSize; i++)
            {
                mFileObject->FieldWriteI(lImagePtr[i]);
            }
        }

        mFileObject->FieldWriteEnd();

        WriteObjectPropertiesAndFlags(&pThumbnail);

        mFileObject->FieldWriteBlockEnd ();
        mFileObject->FieldWriteEnd ();

        return true;
    }
    return false;
}

bool FbxWriterFbx6::WriteMaterials(FbxDocument* pDocument)
{
	int i, lCount = pDocument ? pDocument->GetSrcObjectCount<FbxSurfaceMaterial>() : 0;
    FbxSurfaceMaterial* lMaterial;

    FbxArray< KTypeObjectReferenceDepth<FbxSurfaceMaterial> > lDepthArray;

    // Construct an array of material and compute their reference depth
    for (i=0; i<lCount; i++)
    {
        lMaterial = pDocument->GetSrcObject<FbxSurfaceMaterial>(i);

        KTypeObjectReferenceDepth<FbxSurfaceMaterial> lObjRefDepth;

        lObjRefDepth.mObject = lMaterial;
        lObjRefDepth.mRefDepth = ComputeReferenceDepth(lMaterial);
        lDepthArray.Add(lObjRefDepth);
    }

    // Sort the array of material by their reference depth
    qsort( lDepthArray.GetArray(), lDepthArray.GetCount(), sizeof(KTypeObjectReferenceDepth<FbxSurfaceMaterial>), 
		   CompareKTypeObjectReferenceDepth<FbxSurfaceMaterial> );

    // Write material objects according to their reference depth
    for (i=0; i<lCount; i++)
    {
        lMaterial = lDepthArray.GetAt(i).mObject;

        WriteObjectHeaderAndReferenceIfAny(*lMaterial, FIELD_KFBXMATERIAL_MATERIAL);
        mFileObject->FieldWriteBlockBegin ();
        {
            WriteSurfaceMaterial(*lMaterial);
        }
        mFileObject->FieldWriteBlockEnd ();
        mFileObject->FieldWriteEnd ();
    }

    return true;
}


bool FbxWriterFbx6::WriteSurfaceMaterial(FbxSurfaceMaterial& pMaterial)
{
    FbxString lString;
    bool lBool;

    // version 100: original version
    // version 101: material is now an independent object base on KProperty
    /* version 102: extended materials Materials have been extended
       and can include arbitrary properties.  In particular, the Phong
       shader have replaced the old Opacity property by a
       TransparentColor and a TransparencyFactor. The old Reflectivity
       have been replaced by a Reflection and a ReflectionFactor.

       To keep the new files compatible with old versions of
       MotionBuilder, if we write a Phong or Lambert material, we add
       the old properties, converting from the new properties to the
       old ones. These properties should be ignored when we read back
       the version 102 material. When we read earlier versions, we
       should convert old properties to the new properties.
     */
    mFileObject->FieldWriteI(FIELD_KFBXMATERIAL_VERSION, 102);

    FbxSurfaceMaterial* lReferencedMaterial = FbxCast<FbxSurfaceMaterial>(pMaterial.GetReferenceTo());
    bool lDoWrite;

    lString = pMaterial.ShadingModel.Get();
    lDoWrite = (lReferencedMaterial == NULL) ? true : !(pMaterial.ShadingModel.CompareValue(lReferencedMaterial->ShadingModel));
    
	if (lDoWrite)
    {        
        mFileObject->FieldWriteC(FIELD_KFBXMATERIAL_SHADING_MODEL, lString.Lower());
    }

    lBool = pMaterial.MultiLayer.Get();
    lDoWrite = (lReferencedMaterial == NULL) ? true : !(pMaterial.MultiLayer.CompareValue(lReferencedMaterial->MultiLayer));
    
	if (lDoWrite)
    {
        mFileObject->FieldWriteI(FIELD_KFBXMATERIAL_MULTI_LAYER, (int)lBool);
    }

    FbxProperty lEmissiveProp;
    FbxProperty lAmbientProp;
    FbxProperty lDiffuseProp;
    FbxProperty lSpecularProp;
    FbxProperty lShininessProp;
    FbxProperty lOpacityProp;
    FbxProperty lReflProp;

    FbxDouble3	lColor;
    double		lFactor;

	//IMPORTANT NOTE:
	//Always check for the most complex class before the less one. In this case, Phong inherit from Lambert,
	//so if we would be testing for lambert classid before phong, we would never enter the phong case.
    if( pMaterial.Is<FbxSurfacePhong>() )
    {
        FbxSurfacePhong*   lPhong = FbxCast<FbxSurfacePhong>(&pMaterial);
        FbxSurfacePhong*   lReferencedPhong = FbxCast<FbxSurfacePhong>(lPhong->GetReferenceTo());
        bool                lDoCreateProp;

        //
        // lEmissiveProp
        //
        lColor  = lPhong->Emissive.Get();
        lFactor = lPhong->EmissiveFactor.Get();
        lDoCreateProp = true;
        if (lReferencedPhong)
        {
            lDoCreateProp = !((lReferencedPhong->Emissive.Get() == lColor) && (lReferencedPhong->EmissiveFactor.Get() == lFactor));
        }
        if (lDoCreateProp)
        {
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            lEmissiveProp = FbxProperty::Create(&pMaterial, FbxDouble3DT, "Emissive");
            lEmissiveProp.Set(lColor);
        }
        else
        {
            lEmissiveProp = lPhong->FindProperty("Emissive");
            if (lEmissiveProp.IsValid())
            {
                lEmissiveProp.Destroy();
            }
        }

        //
        // lAmbientProp
        //
        lColor  = lPhong->Ambient.Get();
        lFactor = lPhong->AmbientFactor.Get();
        lDoCreateProp = true;
        if (lReferencedPhong)
        {
            lDoCreateProp = !((lReferencedPhong->Ambient.Get() == lColor) && (lReferencedPhong->AmbientFactor.Get() == lFactor));
        }
        if (lDoCreateProp)
        {
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            lAmbientProp = FbxProperty::Create(&pMaterial, FbxDouble3DT, "Ambient");
            lAmbientProp.Set(lColor);
        }
        else
        {
            lAmbientProp = lPhong->FindProperty("Ambient");
            if (lAmbientProp.IsValid())
            {
                lAmbientProp.Destroy();
            }
        }

        //
        // lDiffuseProp
        //
        lColor = lPhong->Diffuse.Get();
        lFactor = lPhong->DiffuseFactor.Get();
        lDoCreateProp = true;
        if (lReferencedPhong)
        {
            lDoCreateProp = !((lReferencedPhong->Diffuse.Get() == lColor) &&(lReferencedPhong->DiffuseFactor.Get() == lFactor));
        }
        if (lDoCreateProp)
        {
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            lDiffuseProp = FbxProperty::Create(&pMaterial, FbxDouble3DT, "Diffuse");
            lDiffuseProp.Set(lColor);
        }
        else
        {
            lDiffuseProp = lPhong->FindProperty("Diffuse");
            if (lDiffuseProp.IsValid())
            {
                lDiffuseProp.Destroy();
            }
        }

        //
        // lSpecularProp
        //
        lColor = lPhong->Specular.Get();
        lFactor = lPhong->SpecularFactor.Get();
        lDoCreateProp = true;
        if (lReferencedPhong)
        {
            lDoCreateProp = !((lReferencedPhong->Specular.Get() == lColor) &&(lReferencedPhong->SpecularFactor.Get() == lFactor));
        }
        if (lDoCreateProp)
        {
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            lSpecularProp = FbxProperty::Create(&pMaterial, FbxDouble3DT, "Specular");
            lSpecularProp.Set(lColor);
        }
        else
        {
            lSpecularProp = lPhong->FindProperty("Specular");
            if (lSpecularProp.IsValid())
            {
                lSpecularProp.Destroy();
            }
        }

        //
        // lShininessProp
        //
        lFactor = lPhong->Shininess.Get();
        lDoCreateProp = true;
        if (lReferencedPhong)
        {
            lDoCreateProp = !((lReferencedPhong->Shininess.Get() == lFactor));
        }
        if (lDoCreateProp)
        {
            lShininessProp = FbxProperty::Create(&pMaterial, FbxDoubleDT, "Shininess");
            lShininessProp.Set(lFactor);
        }
        else
        {
            lShininessProp = lPhong->FindProperty("Shininess");
            if (lShininessProp.IsValid())
            {
                lShininessProp.Destroy();
            }
        }

        //
        // lOpacityProp
        //
        lColor = lPhong->TransparentColor.Get();
        lFactor = lPhong->TransparencyFactor.Get();
        lDoCreateProp = true;
        if (lReferencedPhong)
        {
            lDoCreateProp = !((lReferencedPhong->TransparentColor.Get() == lColor) && (lReferencedPhong->TransparencyFactor.Get() == lFactor));
        }
        if (lDoCreateProp)
        {
            double lOpacity = 1. - (lColor[0] + lColor[1] + lColor[2]) / 3. * lFactor;
            lOpacityProp = FbxProperty::Create(&pMaterial, FbxDoubleDT, "Opacity");
            lOpacityProp.Set(lOpacity);
        }
        else
        {
            lOpacityProp = lPhong->FindProperty("Opacity");
            if (lOpacityProp.IsValid())
            {
                lOpacityProp.Destroy();
            }
        }

        //
        // lReflProp
        //
        lColor = lPhong->Reflection.Get();
        lFactor = lPhong->ReflectionFactor.Get();
        lDoCreateProp = true;
        if (lReferencedPhong)
        {
            lDoCreateProp = !((lReferencedPhong->Reflection.Get() == lColor) && (lReferencedPhong->ReflectionFactor.Get() == lFactor));
        }
        if (lDoCreateProp)
        {
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            lReflProp = FbxProperty::Create(&pMaterial, FbxDoubleDT, "Reflectivity");
            double lReflectivity = (lColor[0] + lColor[1] + lColor[2]) / 3. * lFactor;
            lReflProp.Set(lReflectivity);
        }
        else
        {
            lReflProp = lPhong->FindProperty("Reflectivity");
            if (lReflProp.IsValid())
            {
                lReflProp.Destroy();
            }
        }
    }
    else if( pMaterial.Is<FbxSurfaceLambert>() )
    {
        FbxSurfaceLambert* lLambert = FbxCast<FbxSurfaceLambert>(&pMaterial);
        FbxSurfaceLambert* lReferencedLambert = FbxCast<FbxSurfaceLambert>(lLambert->GetReferenceTo());
        bool                lDoCreateProp;

        //
        // lEmissiveProp
        //
        lColor = lLambert->Emissive.Get();
        lFactor = lLambert->EmissiveFactor.Get();
        lDoCreateProp = true;
        if (lReferencedLambert)
        {
            lDoCreateProp = !((lReferencedLambert->Emissive.Get() == lColor) && (lReferencedLambert->EmissiveFactor.Get() == lFactor));
        }
        if (lDoCreateProp)
        {
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            lEmissiveProp = FbxProperty::Create(&pMaterial, FbxDouble3DT, "Emissive");
            lEmissiveProp.Set(lColor);
        }
        else
        {
            lEmissiveProp = lLambert->FindProperty("Emissive");
            if (lEmissiveProp.IsValid())
            {
                lEmissiveProp.Destroy();
            }
        }

        //
        // lAmbientProp
        //
        lColor = lLambert->Ambient.Get();
        lFactor = lLambert->AmbientFactor.Get();
        lDoCreateProp = true;
        if (lReferencedLambert)
        {
            lDoCreateProp = !((lReferencedLambert->Ambient.Get() == lColor) && (lReferencedLambert->AmbientFactor.Get() == lFactor));
        }
        if (lDoCreateProp)
        {
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            lAmbientProp = FbxProperty::Create(&pMaterial, FbxDouble3DT, "Ambient");
            lAmbientProp.Set(lColor);
        }
        else
        {
            lAmbientProp = lLambert->FindProperty("Ambient");
            if (lAmbientProp.IsValid())
            {
                lAmbientProp.Destroy();
            }
        }

        //
        // lDiffuseProp
        //
        lColor  = lLambert->Diffuse.Get();
        lFactor = lLambert->DiffuseFactor.Get();
        lDoCreateProp = true;
        if (lReferencedLambert)
        {
            lDoCreateProp = !((lReferencedLambert->Diffuse.Get() == lColor) && (lReferencedLambert->DiffuseFactor.Get() == lFactor));
        }
        if (lDoCreateProp)
        {
            lColor[0] *= lFactor;
            lColor[1] *= lFactor;
            lColor[2] *= lFactor;
            lDiffuseProp = FbxProperty::Create(&pMaterial, FbxDouble3DT, "Diffuse");
            lDiffuseProp.Set(lColor);
        }
        else
        {
            lDiffuseProp = lLambert->FindProperty("Diffuse");
            if (lDiffuseProp.IsValid())
            {
                lDiffuseProp.Destroy();
            }
        }

        //
        // lOpacityProp
        //
        lColor  = lLambert->TransparentColor.Get();
        lFactor = lLambert->TransparencyFactor.Get();
        lDoCreateProp = true;
        if (lReferencedLambert)
        {
            lDoCreateProp = !((lReferencedLambert->TransparentColor.Get() == lColor) && (lReferencedLambert->TransparencyFactor.Get() == lFactor));
        }
        if (lDoCreateProp)
        {
            double lOpacity = 1. - (lColor[0] + lColor[1] + lColor[2]) / 3. * lFactor;
            lOpacityProp = FbxProperty::Create(&pMaterial, FbxDoubleDT, "Opacity");
            lOpacityProp.Set(lOpacity);
        }
        else
        {
            lOpacityProp = lLambert->FindProperty("Opacity");
            if (lOpacityProp.IsValid())
            {
                lOpacityProp.Destroy();
            }
        }
    }

    // Properties
    WriteObjectPropertiesAndFlags(&pMaterial);

    if (lEmissiveProp.IsValid())
    {
        lEmissiveProp.Destroy();
    }
    if (lAmbientProp.IsValid())
    {
        lAmbientProp.Destroy();
    }
    if (lDiffuseProp.IsValid())
    {
        lDiffuseProp.Destroy();
    }
    if (lSpecularProp.IsValid())
    {
        lSpecularProp.Destroy();
    }
    if (lShininessProp.IsValid())
    {
        lShininessProp.Destroy();
    }
    if (lReflProp.IsValid())
    {
        lReflProp.Destroy();
    }
    if (lOpacityProp.IsValid())
    {
        lOpacityProp.Destroy();
    }

    return true;
}

bool FbxWriterFbx6::WriteVideos(FbxDocument* pDocument)
{
	int i, lCount = pDocument ? pDocument->GetSrcObjectCount<FbxVideo>() : 0;
    if (lCount == 0)
        return true;

    bool embeddedMedia = IOS_REF.GetBoolProp(EXP_FBX_EMBEDDED, false) && mFileObject->IsBinary();

    FbxVideo* lVideo;
    FbxArray<FbxString*> lFileNames;

    FbxArray< KTypeObjectReferenceDepth<FbxVideo> > lDepthArray;
    lDepthArray.Reserve(lCount);

    // Construct an array of video and compute their reference depth
    for (i=0; i<lCount; i++)
    {
        lVideo = pDocument->GetSrcObject<FbxVideo>(i);

        KTypeObjectReferenceDepth<FbxVideo> lObjRefDepth;

        lObjRefDepth.mObject = lVideo;
        lObjRefDepth.mRefDepth = ComputeReferenceDepth(lVideo);
        lDepthArray.Add(lObjRefDepth);
    }

    // Sort the array of video by their reference depth
    qsort( lDepthArray.GetArray(), lDepthArray.GetCount(), sizeof(KTypeObjectReferenceDepth<FbxVideo>), CompareKTypeObjectReferenceDepth<FbxVideo>);

    FbxString* lFileName;  

    for (i=0; i<lCount; i++)
    {
        lVideo = lDepthArray.GetAt(i).mObject;

        WriteObjectHeaderAndReferenceIfAny(*lVideo, FIELD_MEDIA_VIDEO);

        mFileObject->FieldWriteBlockBegin ();
        {
            //If the length of lFileName is zero, means fail to get out the filename, use relativeFileName
            //to restore FileName (that is absolute filename)
            lFileName = FbxNew< FbxString >(lVideo->GetFileName());
            FBX_ASSERT(lFileName != NULL);
            if(lFileName && lFileName->GetLen() == 0)
            {
                if (!lVideo->GetRelativeFileName() || strlen(lVideo->GetRelativeFileName()) == 0 )
                    FBX_ASSERT_NOW("should not be empty!!!");
                *lFileName = FbxString(mFileObject->GetFullFilePath(lVideo->GetRelativeFileName()));
            }
            
            lFileNames.Add(lFileName);

            // Don't set error code because it will break the rest of the writing since the object is
            // shared with FbxIO
            if(WriteVideo(*lVideo, *lFileNames[i], embeddedMedia) == false)
            {
                ; //GetStatus().SetCode(FbxStatus::eInsufficientMemory, "Out of memory resources for embedding textures");
            }
        }
        mFileObject->FieldWriteBlockEnd ();
        mFileObject->FieldWriteEnd ();
    }

    FbxArrayDelete(lFileNames);
    return true;
}

bool FbxWriterFbx6::WriteVideo(FbxVideo& pVideo, FbxString& pFileName, bool pEmbeddedMedia)
{
    FbxObject* lReferencedVideo = pVideo.GetReferenceTo();
    if( lReferencedVideo != NULL )
    {
        return WriteObjectPropertiesAndFlags(&pVideo);
    }
    
    mFileObject->FieldWriteC(FIELD_MEDIA_TYPE, pVideo.GetTypeName());

    FbxString lRelativeFileName = mFileObject->GetRelativeFilePath(pFileName);
    pVideo.SetFileName(pFileName);
    pVideo.SetRelativeFileName(lRelativeFileName);

    WriteObjectPropertiesAndFlags(&pVideo);

    mFileObject->FieldWriteI(FIELD_KFBXVIDEO_USEMIPMAP, pVideo.ImageTextureGetMipMap());
    if( pEmbeddedMedia )
    {
		//2009-07-23: Not converting anymore, so its always original format
        mFileObject->FieldWriteI(FIELD_MEDIA_VERSION, 101);
        mFileObject->FieldWriteI(FIELD_MEDIA_ORIGINAL_FORMAT, 1);
		mFileObject->FieldWriteC(FIELD_MEDIA_ORIGINAL_FILENAME, pFileName);
    }
    mFileObject->FieldWriteC(FIELD_MEDIA_FILENAME, pFileName);
    mFileObject->FieldWriteC(FIELD_MEDIA_RELATIVE_FILENAME, lRelativeFileName);

    // Write the raw binary content of the media file if it must be embedded.
    // This option only works if the FBX file is binary.
    if( pEmbeddedMedia )
    {
        if (!FbxFileUtils::Exist(pFileName))
        {
            if(!FbxFileUtils::Exist(lRelativeFileName))
            {
                FbxUserNotification* lUserNotification = mManager.GetUserNotification();
                if( lUserNotification )
                {
                    lUserNotification->AddDetail( FbxUserNotification::eEmbedMediaNotify, pFileName);
                }
                return false;
            }
        }

        mFileObject->FieldWriteBegin(FIELD_MEDIA_CONTENT);
        bool lStatus = mFileObject->FieldWriteEmbeddedFile(pFileName, lRelativeFileName);
        mFileObject->FieldWriteEnd();
        return lStatus;
    }
    return true;
}

void FbxWriterFbx6::WriteConstraints(FbxScene& pScene)
{
	int i, lCount = pScene.GetSrcObjectCount<FbxConstraint>();
    FbxConstraint* lConstraint;

    for (i=0; i<lCount; i++)
    {
        lConstraint = pScene.GetSrcObject<FbxConstraint>(i);
        if (lConstraint)
        {
            if ((lConstraint->GetConstraintType() == FbxConstraint::eCharacter && IOS_REF.GetBoolProp(EXP_FBX_CHARACTER, true)) ||
                (lConstraint->GetConstraintType() != FbxConstraint::eCharacter && IOS_REF.GetBoolProp(EXP_FBX_CONSTRAINT, true)))
            {
                WriteConstraint(*lConstraint, pScene);
            }
        }
    }
}

void FbxWriterFbx6::WriteConstraint(FbxConstraint& pConstraint, FbxScene& pScene)
{    
    FbxVector4 lVector;

    WriteObjectHeaderAndReferenceIfAny(pConstraint, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONSTRAINT);
    mFileObject->FieldWriteBlockBegin ();
    {        
        mFileObject->FieldWriteC("Type", pConstraint.GetTypeName());
        mFileObject->FieldWriteI("MultiLayer",0);

        WriteObjectPropertiesAndFlags(&pConstraint);

        if (pConstraint.GetConstraintType() == FbxConstraint::eCharacter)
        {
            FbxCharacter& lCharacter = (FbxCharacter&)pConstraint;

            mFileObject->FieldWriteB("CHARACTERIZE", lCharacter.Characterize.Get());
            mFileObject->FieldWriteB("LOCK_XFORM", lCharacter.LockXForm.Get());
            mFileObject->FieldWriteB("LOCK_PICK", lCharacter.LockPick.Get());

            mFileObject->FieldWriteBegin("REFERENCE");
            mFileObject->FieldWriteBlockBegin();
			{
				WriteCharacterLink(lCharacter, FbxCharacter::eReference, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("LEFT_FLOOR");
            mFileObject->FieldWriteBlockBegin();
			{
				WriteCharacterLink(lCharacter, FbxCharacter::eLeftFloor, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("RIGHT_FLOOR");
            mFileObject->FieldWriteBlockBegin();
			{
				WriteCharacterLink(lCharacter, FbxCharacter::eRightFloor, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("LEFT_HANDFLOOR");
            mFileObject->FieldWriteBlockBegin();
			{
				WriteCharacterLink(lCharacter, FbxCharacter::eLeftHandFloor, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("RIGHT_HANDFLOOR");
            mFileObject->FieldWriteBlockBegin();
			{
				WriteCharacterLink(lCharacter, FbxCharacter::eRightHandFloor, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("BASE");
            mFileObject->FieldWriteBlockBegin();
			{
				WriteCharacterLinkGroup(lCharacter, FbxCharacter::eGroupBase, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("AUXILIARY");
            mFileObject->FieldWriteBlockBegin();
			{
                WriteCharacterLinkGroup(lCharacter, FbxCharacter::eGroupAuxiliary, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("SPINE");
            mFileObject->FieldWriteBlockBegin();
			{
                WriteCharacterLinkGroup(lCharacter, FbxCharacter::eGroupSpine, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("NECK");
            mFileObject->FieldWriteBlockBegin();
			{
                WriteCharacterLinkGroup(lCharacter, FbxCharacter::eGroupNeck, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("ROLL");
            mFileObject->FieldWriteBlockBegin();
			{
                WriteCharacterLinkGroup(lCharacter, FbxCharacter::eGroupRoll, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("SPECIAL");
            mFileObject->FieldWriteBlockBegin();
			{
                WriteCharacterLinkGroup(lCharacter, FbxCharacter::eGroupSpecial, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("LEFTHAND");
            mFileObject->FieldWriteBlockBegin();
			{
                WriteCharacterLinkGroup(lCharacter, FbxCharacter::eGroupLeftHand, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("RIGHTHAND");
            mFileObject->FieldWriteBlockBegin();
			{
                WriteCharacterLinkGroup(lCharacter, FbxCharacter::eGroupRightHand, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("LEFTFOOT");
            mFileObject->FieldWriteBlockBegin();
			{
                WriteCharacterLinkGroup(lCharacter, FbxCharacter::eGroupLeftFoot, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("RIGHTFOOT");
            mFileObject->FieldWriteBlockBegin();
			{
                WriteCharacterLinkGroup(lCharacter, FbxCharacter::eGroupRightFoot, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();

            mFileObject->FieldWriteBegin("PROPS");
            mFileObject->FieldWriteBlockBegin();
			{
                WriteCharacterLinkGroup(lCharacter, FbxCharacter::eGroupProps, pScene);
			}
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();
        }

        // Duplicates the values of the Offset properties required for backward compatibility with MotionBuilder 6, 7.5, 7.5sp1
        // Version 100: original version
        // Version 101:
        //      - Added the translation, rotation and scaling offset
        //        Note: Only the first source weight is supported
        if (pConstraint.GetConstraintType() == FbxConstraint::eParent)
        {
            FbxConstraintParent* lConstraintParent = (FbxConstraintParent*)&pConstraint;
            FbxObject* lSource = lConstraintParent->GetConstraintSource(0);
            if (lSource)
            {
                FbxVector4 lRotation = lConstraintParent->GetRotationOffset(lSource);
                FbxVector4 lTranslation = lConstraintParent->GetTranslationOffset(lSource);
                FbxVector4 lScale(0.0, 0.0, 0.0);

                mFileObject->FieldWriteI(FIELD_CONSTRAINT_VERSION, 101);
                mFileObject->FieldWriteBegin(FIELD_CONSTRAINT_OFFSET);
                mFileObject->FieldWrite3D(lRotation);
                mFileObject->FieldWrite3D(lTranslation);
                mFileObject->FieldWrite3D(lScale);

                mFileObject->FieldWriteEnd ();
            }
        }
    }

    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();
}

void FbxWriterFbx6::WriteAllGeometries(FbxScene& pScene)
{
    int i, lCount;
    for (i = 0, lCount = pScene.GetSrcObjectCount<FbxGeometry>(); i < lCount; i++) 
	{
		FbxGeometry* lGeometry = pScene.GetSrcObject<FbxGeometry>(i);

        if (lGeometry && IsStorableNodeAttribute(lGeometry)) 
		{
            // Open the Geometry block
            WriteObjectHeaderAndReferenceIfAny(*lGeometry, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GEOMETRY);
            mFileObject->FieldWriteBlockBegin ();

            switch (lGeometry->GetAttributeType()) 
			{
                case FbxNodeAttribute::eMesh:
                    WriteMesh(*((FbxMesh*)lGeometry));
                    break;
                case FbxNodeAttribute::eNurbs:
                    WriteNurb(*((FbxNurbs*)lGeometry));
                    break;
                case FbxNodeAttribute::ePatch:
                    WritePatch(*((FbxPatch*)lGeometry));
                    break;
                case FbxNodeAttribute::eNurbsCurve:
                    WriteNurbsCurve(*((FbxNurbsCurve*)lGeometry));
                    break;
                case FbxNodeAttribute::eTrimNurbsSurface:
                    WriteTrimNurbsSurface(*((FbxTrimNurbsSurface*)lGeometry));
                    break;
                case FbxNodeAttribute::eBoundary:
                    WriteBoundary( *(FbxBoundary*)lGeometry );
                    break;
                case FbxNodeAttribute::eNurbsSurface:
                    WriteNurbsSurface( *(FbxNurbsSurface*)lGeometry );
                    break;
                default:                    
                    break;
            }

            // Close the Geometry block
            mFileObject->FieldWriteBlockEnd ();
            mFileObject->FieldWriteEnd ();
        }
    }
}

void FbxWriterFbx6::WriteAllGeometryWeightedMaps(FbxScene& pScene)
{
    int i, lCount;
	for (i = 0, lCount = pScene.GetSrcObjectCount<FbxGeometryWeightedMap>(); i < lCount; i++) 
	{
		FbxGeometryWeightedMap* lGeometryWeightedMap = pScene.GetSrcObject<FbxGeometryWeightedMap>(i);

        if (lGeometryWeightedMap) {
            // Open the Geometry Weighted Map block
            WriteObjectHeaderAndReferenceIfAny(*lGeometryWeightedMap, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GEOMETRY_WEIGHTED_MAP);
            mFileObject->FieldWriteBlockBegin ();
            WriteGeometryWeightedMap(*lGeometryWeightedMap);

            // Close the Geometry Weighted Map
            mFileObject->FieldWriteBlockEnd ();
            mFileObject->FieldWriteEnd ();
        }
    }
}


bool FbxWriterFbx6::WriteNodes( const FbxDocument& pDocument)
{
    int i, lNodeCount = pDocument.GetSrcObjectCount<FbxNode>();
    bool lSuccess = true;
    for( i = 0; i < lNodeCount; ++i )
    {
        FbxNode* lNode = pDocument.GetSrcObject<FbxNode>(i);
        lSuccess &= WriteNode(*lNode);
    }
    return lSuccess;
}

void FbxWriterFbx6::WriteNodeAttributes( const FbxDocument& pDocument)
{
    int i = 0;
    int lCount = pDocument.GetSrcObjectCount<FbxNodeAttribute>();

    for( i = 0 ; i < lCount; ++i )
    {
        FbxNodeAttribute* lAttr = pDocument.GetSrcObject<FbxNodeAttribute>(i);
        FbxNode*          lNode = lAttr->GetNode();

        // skip attributes that were already written out with their nodes.
        // Note: Storable means we can store it separately from the node
        if( lNode && !IsStorableNodeAttribute(lAttr))
        {
            if(lNode->GetNodeAttributeCount())
            {
                FbxNodeAttribute* lDefaultAttr = lNode->GetNodeAttribute();
                FBX_ASSERT(lDefaultAttr != NULL);
                if(!lDefaultAttr || !(lDefaultAttr->GetAttributeType() == FbxNodeAttribute::eSubDiv))
                    continue;
            }
            else
                continue;
        }

		// skip shapes, which are also already written out in geometry sections.
		if(lAttr->GetAttributeType() == FbxNodeAttribute::eShape)
		{
			continue;
		}

        if( WriteObjectHeaderAndReferenceIfAny( *lAttr,
            lAttr->Is<FbxGeometryBase>() ?
            FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GEOMETRY :
            FIELD_OBJECT_DEFINITION_OBJECT_TYPE_NODE_ATTRIBUTE ) )
        {
            mFileObject->FieldWriteBlockBegin();

            // write the node.
            WriteObjectPropertiesAndFlags(lAttr);
            WriteNodeAttribute(lAttr);

            mFileObject->FieldWriteBlockEnd ();
            mFileObject->FieldWriteEnd ();
        }
    }
}

void FbxWriterFbx6::WriteObjectProperties(FbxDocument* pDocument, Fbx6TypeDefinition& pDefinitions)
{
    FbxScene*  lScene = FbxCast<FbxScene>(pDocument);
    bool        lIsAScene = (lScene != NULL);

    mProgressPause = false;

    mFileObject->WriteComments ("");
    mFileObject->WriteComments (" Object properties");
    mFileObject->WriteComments ("------------------------------------------------------------------");
    mFileObject->WriteComments ("");

    mFileObject->FieldWriteBegin(FIELD_OBJECT_PROPERTIES);
    mFileObject->FieldWriteBlockBegin();

    // Export all sub-documents
    WriteDocuments(pDocument);

    if (lIsAScene)
    {
	#ifndef FBXSDK_ENV_WINSTORE
        //Export plugin settings
        PluginsWrite(*mFileObject, /*Don't write object ID in fbx6*/false);
	#endif

        // Export all nodes except the root node. (This includes the camera switcher)
        if (lScene->GetRootNode()) WriteNodes(*lScene, false);

        if(IOS_REF.GetBoolProp(EXP_FBX_MODEL, true))
        {
            WriteNodeAttributes( *lScene );

            // Export all geometry weighted maps
            WriteAllGeometryWeightedMaps(*lScene);
        }

        // Export all the generic nodes
        WriteGenericNodes(*lScene);

        // Export all the asset containers
        WriteContainers(*lScene);

        // Export the SceneInfo
        WriteSceneInfo(lScene->GetSceneInfo());

        // Export all the poses
        WritePose(*lScene);

        // Export all the selection sets
        WriteSelectionNode(*lScene);

        // Export all the selection sets
        WriteSelectionSet(*lScene);

        // Export all the materials
        WriteMaterials(pDocument);

        // Export all the deformer
        WriteDeformers(*lScene);

        // Export all the videos
        // (we export the videos before the textures because the FileName and
        //  relative filename may get changed and we want the Texture to reference
        // the correct file).
        WriteVideos(pDocument);

        // Export all the textures
        WriteTextures(pDocument);

        // Export all Caches
        WriteCaches(pDocument);

        // Export all shader implementations
        WriteImplementations(pDocument);

        WriteBindingTables(pDocument);
        WriteBindingOperators(pDocument);
        
        if (IOS_REF.GetBoolProp(EXP_FBX_CHARACTER, true))
        {
            WriteCharacterPose(*lScene);
            WriteControlSetPlug(*lScene);
        }      

        if( IOS_REF.GetBoolProp(EXP_FBX_CONSTRAINT, true) || IOS_REF.GetBoolProp(EXP_FBX_CHARACTER, true) )
        {
            WriteConstraints(*lScene);
        }

        WriteGlobalSettings(lScene->GetGlobalSettings());

		//WriteLayeredAnimation(*lScene);

    }
    else 
	{

        // Export the SceneInfo
        WriteSceneInfo(pDocument->GetDocumentInfo());

        // Export all the materials
        WriteMaterials(pDocument);

        // Export all the videos
        // (we export the videos before the textures because the FileName and
        //  relative filename may get changed and we want the Texture to reference
        // the correct file).
        WriteVideos(pDocument);

        // Export all the textures
        WriteTextures(pDocument);

        // Export all shader implementations
        WriteImplementations(pDocument);

        WriteBindingTables(pDocument);
        WriteBindingOperators(pDocument);

        // Export all layered textures
        WriteLayeredTextures(pDocument);

        // Export all collections
        WriteCollections(pDocument);

        // Export all thumbnails
        WriteThumbnails(pDocument);

        // Export all nodes
        WriteNodes(*pDocument);

        // Export all node attributes
        WriteNodeAttributes(*pDocument);
    }

    // Write objects supported by the generic writing mechanism
    FbxObject* lFbxObject = NULL;
    FbxClassId lClassId;
    const char* lFbxClassTypeName = "";    
    int lIter, lCount = pDocument->GetSrcObjectCount();
    for( lIter = 0; lIter < lCount; lIter++ )
    {
        // Retrieve object
        lFbxObject = pDocument->GetSrcObject(lIter);
        FBX_ASSERT_MSG(lFbxObject, "Unable to retreive object from current document!");

        // Storing only savable objects
        if( !lFbxObject->GetObjectFlags(FbxObject::eSavable) )
            continue;

        // Retrieve object information
        lClassId = lFbxObject->GetRuntimeClassId();
        lFbxClassTypeName = lClassId.GetFbxFileTypeName(true);


        Fbx6TypeDefinitionInfo* pTypeDefinition = pDefinitions.GetDefinitionFromName(lFbxClassTypeName);
        if (( pTypeDefinition != NULL ) && pTypeDefinition->IsGenericWriteEnable())
        {
            // Begin object write
            WriteObjectHeaderAndReferenceIfAny(*lFbxObject, lFbxClassTypeName);
            mFileObject->FieldWriteBlockBegin();
            WriteObjectPropertiesAndFlags(lFbxObject);
            mFileObject->FieldWriteBlockEnd();

            // End object write
            mFileObject->FieldWriteEnd();
        }
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mProgressPause = true;
}

void FbxWriterFbx6::WriteObjectConnections(FbxDocument* pDocument)
{
    mFileObject->WriteComments ("");
    mFileObject->WriteComments (" Object connections");
    mFileObject->WriteComments ("------------------------------------------------------------------");
    mFileObject->WriteComments ("");

    mFileObject->FieldWriteBegin(FIELD_OBJECT_CONNECTIONS);
    mFileObject->FieldWriteBlockBegin();

    FbxScene* lScene = FbxCast<FbxScene>(pDocument);
    FbxString lRootNodeName;
    if( lScene )
    {
        lRootNodeName = lScene->GetRootNode()->GetName();
        lScene->GetRootNode()->SetName("Scene");
    }

    FbxIteratorSrc<FbxObject> lFbxObjectIterator(pDocument);
    FbxObject* lFbxObject;
    FbxForEach(lFbxObjectIterator, lFbxObject)
    {
        if(lFbxObject && (lFbxObject->Is<FbxControlSetPlug>() || lFbxObject->Is<FbxConstraint>()))
            continue;

        WriteObjectConnections(pDocument, lFbxObject, false);
    }
    
    //Fix for character and HIK        
    //write ConstrolSetPlug connections first. 
    //This must be written before writing character connections
    if (lScene && IOS_REF.GetBoolProp(EXP_FBX_CHARACTER, true))
    {
        for(int lCtrlIter = 0; lCtrlIter < lScene->GetControlSetPlugCount(); lCtrlIter++)
        {
            FbxObject* lObject = FbxCast<FbxObject>(lScene->GetControlSetPlug(lCtrlIter));
            WriteObjectConnections(pDocument, lObject, false);
        }
    }

    //then write character connections
    if (IOS_REF.GetBoolProp(EXP_FBX_CHARACTER, true) || IOS_REF.GetBoolProp(EXP_FBX_CONSTRAINT, true))
    {        
		for(int lIter = 0; lIter < lScene->GetSrcObjectCount<FbxConstraint>(); lIter++)
        {
            FbxConstraint* lConstraint = lScene->GetSrcObject<FbxConstraint>(lIter);
            if(lConstraint != NULL)
			{
                if ((lConstraint->GetConstraintType() == FbxConstraint::eCharacter && IOS_REF.GetBoolProp(EXP_FBX_CHARACTER, true)) ||
                    (lConstraint->GetConstraintType() != FbxConstraint::eCharacter && IOS_REF.GetBoolProp(EXP_FBX_CONSTRAINT, true)))
                {
                    FbxObject* lObject = FbxCast<FbxObject>(lConstraint);
                    WriteObjectConnections(pDocument, lObject, false); 
                }
            }            
        }
    }    
    //~Fix

    if( lScene )
	{
        lScene->GetRootNode()->SetName(lRootNodeName.Buffer());
    }

    if (mDocumentHierarchy)
    {
        FbxDocument* lFbxObjectDocument;
        int i, lCount = mDocumentHierarchy->GetCount();

        for (i = 0; i < lCount; i++)
        {
            lFbxObject = mDocumentHierarchy->GetObject(i);
            lFbxObjectDocument = mDocumentHierarchy->GetObjectDocument(i);
            WriteFieldConnection(pDocument, lFbxObject, lFbxObjectDocument);
        }
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

// *****************************************************************************
//  Connections
// *****************************************************************************
FbxObject* FbxWriterFbx6::GetObjectIndirection(FbxObject* pObject)
{
    // Note: Geometry node attributes stored with the FbxNode
    if (pObject && IsNodeAttribute(pObject) && !IsStorableNodeAttribute(pObject)) 
	{
        return pObject->GetDstObject<FbxNode>();
    }
    return pObject;
}

bool FbxWriterFbx6::WriteFieldConnection(FbxDocument* pDocument, FbxObject* pSrcObject, FbxDocument* pDstDocument)
{
    if (pDocument == pDstDocument)
    {
        return true;
    }

    if ((pDocument == NULL) || (pSrcObject == NULL) || (pDstDocument == NULL))
    {
        return false;
    }

    mFileObject->FieldWriteBegin("Connect");
    mFileObject->FieldWriteC("OD");
    mFileObject->FieldWriteC(pSrcObject->GetNameWithNameSpacePrefix());
    mFileObject->FieldWriteC(pDstDocument->GetNameWithNameSpacePrefix());
    mFileObject->FieldWriteEnd();
    return true;
}

bool FbxWriterFbx6::WriteFieldConnection(FbxDocument* pDocument, FbxObject* pSrc,FbxObject* pDst)
{
    FbxObject* lSrcObject = pSrc;
    FbxObject* lDstObject = pDst;

    if (lSrcObject && lDstObject)
    {
        if (lSrcObject == lDstObject)
        {
            return false;
        }

        // Eleminate Connection between
        if (lSrcObject->Is<FbxNodeAttribute>() && lDstObject->Is<FbxNode>() && 
            FbxCast<FbxNodeAttribute>(lSrcObject)->GetNode() && !(FbxCast<FbxNodeAttribute>(lSrcObject)->GetNode()->GetSubdiv()))
        {
            if(!IOS_REF.GetBoolProp(EXP_FBX_MODEL, true)) 
			{
                return true;
            }

            // connections to old attribute types should not be stored
            if( !IsStorableNodeAttribute(lSrcObject) )
                return true;
        }

        // Note: if pObject is a NoteAttribute, we use its parent in the connection.
        if ( lDstObject->Is<FbxNodeAttribute>() )
        {
            if( !IsStorableObject(lDstObject) )
            {
                FbxNodeAttribute* na = FbxCast <FbxNodeAttribute> (lDstObject);
                lDstObject = (FbxObject*)na->GetNode();
            }
        }
       
        mFileObject->FieldWriteBegin("Connect");
        mFileObject->FieldWriteC("OO");
        mFileObject->FieldWriteC(lSrcObject->GetNameWithNameSpacePrefix());
        mFileObject->FieldWriteC(lDstObject->GetNameWithNameSpacePrefix());
        mFileObject->FieldWriteEnd();
        return true;
    }
    return false;
}

bool FbxWriterFbx6::WriteFieldConnection(FbxDocument* pDocument, FbxObject* pSrc,FbxProperty& pDst)
{
	FbxObject*		lSrcObject   = pSrc;
	FbxProperty*	lDstProperty = pDst.IsValid() ? &pDst : 0;

    if( lSrcObject && lDstProperty )
    {       
        const char* lConnectType = lSrcObject != pDocument ? "OP" : "EP";

        mFileObject->FieldWriteBegin("Connect");
        mFileObject->FieldWriteC(lConnectType);
        mFileObject->FieldWriteC(lSrcObject->GetNameWithNameSpacePrefix());
        mFileObject->FieldWriteC(lDstProperty->GetFbxObject()->GetNameWithNameSpacePrefix());
        mFileObject->FieldWriteC(lDstProperty->GetHierarchicalName());
        mFileObject->FieldWriteEnd();

        return true;
    }
    return false;
}

bool FbxWriterFbx6::WriteFieldConnection(FbxDocument *pDocument, FbxProperty& pSrc,FbxObject* pDst)
{
	FbxObject*       lDstObject   = pDst;
	FbxProperty*     lSrcProperty = pSrc.IsValid() ? &pSrc: 0;

    if (lSrcProperty && lDstObject) 
	{

        // Note: if pObject is a NoteAttribute, we use its parent in the connection.
        if ( lDstObject->Is<FbxNodeAttribute>())
        {
            FbxNodeAttribute* na = FbxCast<FbxNodeAttribute>(lDstObject);
            lDstObject = (FbxObject*)na->GetNode();
        }
       
        mFileObject->FieldWriteBegin("Connect");
        mFileObject->FieldWriteC("PO");
        mFileObject->FieldWriteC(lSrcProperty->GetFbxObject()->GetNameWithNameSpacePrefix());
        mFileObject->FieldWriteC(lSrcProperty->GetHierarchicalName());
        mFileObject->FieldWriteC(lDstObject->GetNameWithNameSpacePrefix());
        mFileObject->FieldWriteEnd();
        return true;
    }
    return false;
}

bool FbxWriterFbx6::WriteFieldConnection(FbxDocument* pDocument, FbxProperty& pSrc,FbxProperty& pDst)
{
	FbxProperty*	lSrcProperty = pSrc.IsValid() ? &pSrc: 0;
	FbxProperty*	lDstProperty = pDst.IsValid() ? &pDst: 0;

    if (lSrcProperty && lDstProperty) 
	{
  
        mFileObject->FieldWriteBegin("Connect");
        mFileObject->FieldWriteC("PP");
        mFileObject->FieldWriteC(lSrcProperty->GetFbxObject()->GetNameWithNameSpacePrefix());
        mFileObject->FieldWriteC(lSrcProperty->GetHierarchicalName());
        mFileObject->FieldWriteC(lDstProperty->GetFbxObject()->GetNameWithNameSpacePrefix());
        mFileObject->FieldWriteC(lDstProperty->GetHierarchicalName());
        mFileObject->FieldWriteEnd();
        return true;
    }
    return false;
}

void FbxWriterFbx6::WriteObjectConnections(FbxDocument* pDocument, FbxObject* pObject, bool pRecursive)
{
    //Skip objects that are not savable
    if( !pObject->GetObjectFlags(FbxObject::eSavable) )
        return;

    // Store all connections on the object
    // -------------------------------------------------
    FbxIteratorSrc<FbxObject> lFbxObjectIterator(pObject);
    FbxObject* lFbxObject;
    FbxForEach(lFbxObjectIterator,lFbxObject) {

		// skip objects that are not savable
		if (!lFbxObject->GetObjectFlags(FbxObject::eSavable))
			continue;

        if (pDocument == lFbxObject->GetDocument())
        {
            WriteFieldConnection(pDocument, lFbxObject, pObject);
        }
    }

    // Store all property connections
    FbxIterator<FbxProperty>  lFbxDstPropertyIter(pObject);
    FbxProperty                lFbxDstProperty;

    FbxForEach(lFbxDstPropertyIter,lFbxDstProperty) 
	{
		FbxProperty                lSrcFbxProperty;
		FbxObject*                 lSrcObject;
		int	i;

        if (lFbxDstProperty.GetFlag(FbxPropertyFlags::eNotSavable))
        {
            // the property is not savable, no point on continuing...
            continue;
        }

        // PP Connections
        for (i=0; i<lFbxDstProperty.GetSrcPropertyCount(); i++) 
		{
            lSrcFbxProperty= lFbxDstProperty.GetSrcProperty(i);
            if (lSrcFbxProperty.GetFlag(FbxPropertyFlags::eNotSavable))
            {
                // the property is not savable, no point on continuing...
                continue;
            }

			// skip objects that are not savable
			if (!lSrcFbxProperty.GetFbxObject()->GetObjectFlags(FbxObject::eSavable))
			{
				continue;
			}
            WriteFieldConnection(pDocument,lSrcFbxProperty, lFbxDstProperty);
        }

        // OP Connections
        for (i=0; i<lFbxDstProperty.GetSrcObjectCount(); i++) 
		{
            lSrcObject= lFbxDstProperty.GetSrcObject(i);

            // skip objects that are not savable
            if (!lSrcObject->GetObjectFlags(FbxObject::eSavable))
                continue;

            WriteFieldConnection(pDocument,lSrcObject, lFbxDstProperty);
        }
    }

    // PO Connections
    const int lSrcPropertyCount = pObject->GetSrcPropertyCount();
    for( int i = 0; i < lSrcPropertyCount; ++i )
    {
        FbxProperty lSrcProperty = pObject->GetSrcProperty(i);
        if (lSrcProperty.GetFlag(FbxPropertyFlags::eNotSavable))
        {
            // the property is not savable, no point on continuing...
            continue;
        }
        WriteFieldConnection( pDocument, lSrcProperty, pObject );
    }
}


// *******************************************************************************
//  Animation
// *******************************************************************************

static bool HasKeysOnFCurves(FbxAnimCurveNode* pFCNode)
{
    bool lRet = false;

    if (pFCNode == NULL) return false;
    for( unsigned int i = 0; lRet == false && i < pFCNode->GetChannelsCount(); i++ )
    {
        FbxAnimCurve* lCurve = pFCNode->GetCurve(i);
        if (lCurve != NULL) lRet = lCurve->KeyGetCount() > 0;
    }
    return lRet;
}

static bool HasSomeFCurves(FbxObject* pNode, FbxAnimLayer* pAnimLayer)
{
    // return true as soon as one property on the object (or it's attributes)
    // has an fcurve with one or more keys.
    if (!pNode) return false;

    bool hasKeys = false;
    pNode->RootProperty.BeginCreateOrFindProperty();
    FbxProperty prop = pNode->GetFirstProperty();
    while (prop.IsValid() && hasKeys == false)
    {
        FbxAnimCurveNode* fcn = prop.GetCurveNode(pAnimLayer, false);
        hasKeys = HasKeysOnFCurves(fcn);
        prop = pNode->GetNextProperty(prop);
    }
    pNode->RootProperty.EndCreateOrFindProperty();
    return hasKeys;
}


bool FbxWriterFbx6::WriteAnimation(FbxDocument* pDocument)
{
    if (pDocument == NULL)
    {
        return false;
    }

    FbxScene* lScene = FbxCast<FbxScene>(pDocument);
    if (lScene == NULL)
        return false;

    FbxArray<FbxString*> lNameArray;
    FbxTimeSpan lAnimationInterval(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
    int lNameIter, lObjectIter;
    int lNodeCount = (lScene == NULL) ? 0 : lScene->GetRootNode()->GetChildCount(true);
    int lMaterialCount = pDocument->GetSrcObjectCount<FbxSurfaceMaterial>();
    int lTextureCount = pDocument->GetSrcObjectCount<FbxTexture>();
    int lConstraintCount = pDocument->GetSrcObjectCount<FbxConstraint>();
    int lGenericNodeCount = pDocument->GetSrcObjectCount<FbxGenericNode>();
    int lAssetContainerCount = pDocument->GetSrcObjectCount<FbxContainer>();

    pDocument->FillAnimStackNameArray(lNameArray);
    int lCount = lNameArray.GetCount();

    mFileObject->WriteComments("Takes and animation section");
    mFileObject->WriteComments("----------------------------------------------------");
    mFileObject->WriteComments("");

    mFileObject->FieldWriteBegin("Takes");
    mFileObject->FieldWriteBlockBegin();
    mFileObject->FieldWriteC("Current", pDocument->ActiveAnimStackName.Get());

    // For all takes


    for( lNameIter = 0; lNameIter < lCount; lNameIter++ )
    {
        // Avoid writing the default take
        if (lNameArray.GetAt(lNameIter)->Compare(FBXSDK_TAKENODE_DEFAULT_NAME) == 0)
        {
            continue;
        }

        FbxTakeInfo* lTakeInfo = pDocument->GetTakeInfo(*(lNameArray.GetAt(lNameIter)));

        if (lTakeInfo)
        {
            if (lTakeInfo->mSelect == false)
            {
                continue;
            }
        }
        else
        {
            FbxTakeInfo lNewTakeInfo;
            lNewTakeInfo.mName = *(lNameArray.GetAt(lNameIter));
            pDocument->SetTakeInfo(lNewTakeInfo);
            lTakeInfo = pDocument->GetTakeInfo(lNewTakeInfo.mName);
        }

        FbxString lSubTitle;
        lSubTitle += "Storing take ";
        lSubTitle += lNameArray.GetAt(lNameIter)->Buffer();

        mFileObject->FieldWriteBegin("Take");
        mFileObject->FieldWriteC(lNameArray.GetAt(lNameIter)->Buffer());
        mFileObject->FieldWriteBlockBegin();

        FbxString lFileName(lNameArray.GetAt(lNameIter)->Buffer());
        lFileName += ".tak";
        while (lFileName.FindAndReplace(" ", "_")) {} // Make sure no whitespace is left in the file name
        while (lFileName.FindAndReplace("\t", "_")) {} // Make sure no tabulation is left in the file name
        mFileObject->FieldWriteC("FileName",lFileName);

        FbxAnimStack* lAnimStack = lScene->FindMember<FbxAnimStack>(lNameArray.GetAt(lNameIter)->Buffer());
        FBX_ASSERT(lAnimStack != NULL);
        if (lAnimStack == NULL)
            // this is a fatal error!
            return false;

        // The anim stack always contain the base layer. We get it now.
        FbxAnimLayer* lAnimLayer = lAnimStack->GetSrcObject<FbxAnimLayer>();
        FBX_ASSERT(lAnimLayer != NULL);
        if (lAnimLayer == NULL)
            // this is a fatal error!
            return false;
        
        // Find current take animation interval if necessary.
        if ((lTakeInfo->mLocalTimeSpan.GetStart() == FBXSDK_TIME_ZERO && lTakeInfo->mLocalTimeSpan.GetStop() == FBXSDK_TIME_ZERO) ||
            (lTakeInfo->mReferenceTimeSpan.GetStart() == FBXSDK_TIME_ZERO && lTakeInfo->mReferenceTimeSpan.GetStop() == FBXSDK_TIME_ZERO))
        {
            FbxTimeSpan lCameraSwitcherInterval(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
            if (lScene->GlobalCameraSettings().GetCameraSwitcher()) 
			{
                lScene->GlobalCameraSettings().GetCameraSwitcher()->GetNode()->GetAnimationInterval(lCameraSwitcherInterval, lAnimStack, 0);
            }
            lScene->GetRootNode()->GetAnimationInterval(lAnimationInterval, lAnimStack, 0);
            lAnimationInterval.UnionAssignment(lCameraSwitcherInterval);
        }
        
        if(lAnimationInterval.GetStart() > lAnimationInterval.GetStop() )
        {
            lAnimationInterval.SetStart(0) ;
            FbxTime lTmpTime;
            lTmpTime.SetTime(0,0,1);
            lAnimationInterval.SetStop(lTmpTime);
        }

        // Local time span is set to current take animation interval if it was
        // left to it's default value i.e. both start and stop time at 0.
        if (lTakeInfo->mLocalTimeSpan.GetStart() != FBXSDK_TIME_ZERO || lTakeInfo->mLocalTimeSpan.GetStop() != FBXSDK_TIME_ZERO)
        {
            mFileObject->FieldWriteTS("LocalTime", lTakeInfo->mLocalTimeSpan);
        }
        else
        {
            mFileObject->FieldWriteTS("LocalTime", lAnimationInterval);
            lTakeInfo->mLocalTimeSpan = lAnimationInterval;
        }

        // Reference time span is set to current take animation interval if it was
        // left to it's default value i.e. both start and stop time at 0.
        if (lTakeInfo->mReferenceTimeSpan.GetStart() != FBXSDK_TIME_ZERO || lTakeInfo->mReferenceTimeSpan.GetStop() != FBXSDK_TIME_ZERO)
        {
            mFileObject->FieldWriteTS("ReferenceTime", lTakeInfo->mReferenceTimeSpan);
        }
        else
        {
            mFileObject->FieldWriteTS("ReferenceTime", lAnimationInterval);
            lTakeInfo->mReferenceTimeSpan = lAnimationInterval;
        }

        if (lTakeInfo->mDescription.IsEmpty() == false)
        {
            mFileObject->FieldWriteC("Comments", lTakeInfo->mDescription);
        }

		WriteTimeWarps(pDocument, lAnimStack);

		// Use sceneInfo thumbnail in the takes (to maintain backward compatibility)
		if (lScene->GetSceneInfo() && lScene->GetSceneInfo()->GetSceneThumbnail())
		{
			WriteThumbnail(lScene->GetSceneInfo()->GetSceneThumbnail());
		}

        mFileObject->WriteComments("");
        mFileObject->WriteComments("Models animation");
        mFileObject->WriteComments("----------------------------------------------------");

        if (lScene != NULL)
        {
            if (lScene->GlobalCameraSettings().GetCameraSwitcher()) 
			{
                WriteFCurves(*(lScene->GlobalCameraSettings().GetCameraSwitcher()->GetNode()), lAnimLayer, FIELD_KFBXNODE_MODEL, false);
            }
        }

        WriteAnimation(pDocument, lAnimLayer);

        mFileObject->WriteComments("");
        mFileObject->WriteComments("Generic nodes animation");
        mFileObject->WriteComments("----------------------------------------------------");

        for( lObjectIter = 0; lObjectIter < lGenericNodeCount; lObjectIter++ )
        {
            FbxGenericNode* lGenericNode = pDocument->GetSrcObject<FbxGenericNode>(lObjectIter);
            FbxObject* lGenericNodeObj = reinterpret_cast<FbxObject*>(lGenericNode);
            if( HasSomeFCurves(lGenericNodeObj, lAnimLayer) )
            {
                // Let's open the block!
                mFileObject->FieldWriteBegin(FIELD_KFBXGENERICNODE_GENERICNODE);
                FbxString lObjName = lGenericNode->GetNameWithNameSpacePrefix();
                mFileObject->FieldWriteC(lObjName);
                mFileObject->FieldWriteBlockBegin();
                mFileObject->FieldWriteD("Version", 1.1);

                WriteFCurves(*lGenericNode, lAnimLayer, FIELD_KFBXGENERICNODE_GENERICNODE, false);

                // and now, close it!
                mFileObject->FieldWriteBlockEnd();
                mFileObject->FieldWriteEnd();
            }
        }       

        mFileObject->WriteComments("");
        mFileObject->WriteComments("Textures animation");
        mFileObject->WriteComments("----------------------------------------------------");

        for (lObjectIter = 0; lObjectIter < lTextureCount; lObjectIter++)
        {
            FbxTexture* lTexture = pDocument->GetSrcObject<FbxTexture>(lObjectIter);
            FbxObject* lTextureObj = reinterpret_cast<FbxObject*>(lTexture);
            if ( HasSomeFCurves(lTextureObj, lAnimLayer) )
            {
                // Let's open the block!
                mFileObject->FieldWriteBegin(FIELD_KFBXTEXTURE_TEXTURE);
                FbxString lObjName = lTexture->GetNameWithNameSpacePrefix();
                mFileObject->FieldWriteC(lObjName);
                mFileObject->FieldWriteBlockBegin ();
                mFileObject->FieldWriteD("Version", 1.1);

                WriteFCurves(*lTexture, lAnimLayer, FIELD_KFBXTEXTURE_TEXTURE, false);

                // and now, close it!
                mFileObject->FieldWriteBlockEnd ();
                mFileObject->FieldWriteEnd ();
            } //end if
           
        }

        mFileObject->WriteComments("");
        mFileObject->WriteComments("Materials animation");
        mFileObject->WriteComments("----------------------------------------------------");
        mFileObject->WriteComments("");

        for (lObjectIter = 0; lObjectIter < lMaterialCount; lObjectIter++)
        {
            FbxSurfaceMaterial* lMaterial = pDocument->GetSrcObject<FbxSurfaceMaterial>(lObjectIter);
            FbxObject* lMaterialObj = reinterpret_cast<FbxObject*>(lMaterial);
            if ( HasSomeFCurves(lMaterialObj, lAnimLayer) )
            {
                // Let's open the block!
                mFileObject->FieldWriteBegin(FIELD_KFBXMATERIAL_MATERIAL);
                FbxString lObjName = lMaterial->GetNameWithNameSpacePrefix();
                mFileObject->FieldWriteC(lObjName);
                mFileObject->FieldWriteBlockBegin ();
                mFileObject->FieldWriteD("Version", 1.1);

                WriteFCurves(*lMaterial, lAnimLayer, FIELD_KFBXMATERIAL_MATERIAL, false);

                // and now, close it!
                mFileObject->FieldWriteBlockEnd ();
                mFileObject->FieldWriteEnd ();
            } //end if
   
        }

        mFileObject->WriteComments("");
        mFileObject->WriteComments("Constraints animation");
        mFileObject->WriteComments("----------------------------------------------------");

        for (lObjectIter = 0; lObjectIter < lConstraintCount; lObjectIter++)
        {
            FbxConstraint* lConstraint = pDocument->GetSrcObject<FbxConstraint>(lObjectIter);
            FbxObject* lConstraintObj = reinterpret_cast<FbxObject*>(lConstraint);
            if ( HasSomeFCurves(lConstraintObj, lAnimLayer) )
            {
                // Let's open the block!
                mFileObject->FieldWriteBegin(FIELD_CONSTRAINT);                
                mFileObject->FieldWriteC(lConstraint->GetNameWithNameSpacePrefix());
                mFileObject->FieldWriteBlockBegin ();
                mFileObject->FieldWriteD("Version", 1.1);

                WriteFCurves(*lConstraint, lAnimLayer, FIELD_CONSTRAINT, false);

                // and now, close it!
                mFileObject->FieldWriteBlockEnd ();
                mFileObject->FieldWriteEnd ();
            } //end if

        }

        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    FbxArrayDelete(lNameArray);

    return true;
}


bool FbxWriterFbx6::WriteAnimation( FbxDocument* pDocument, FbxAnimLayer* pAnimLayer )
{
    if (pDocument == NULL)
    {
        return false;
    }

    const int lNodeCount = pDocument->GetSrcObjectCount<FbxNode>();
    bool lSuccess = true;
    for( int i = 0; i < lNodeCount; ++i )
    {
        FbxNode* node = pDocument->GetSrcObject<FbxNode>(i);
        FbxObject* nodeAttribute = node->GetNodeAttribute();
        if (HasSomeFCurves(node, pAnimLayer) || HasSomeFCurves(nodeAttribute, pAnimLayer))
        {
            // Let's open the block!
            mFileObject->FieldWriteBegin(FIELD_KFBXNODE_MODEL);            
            mFileObject->FieldWriteC(node->GetNameWithNameSpacePrefix());
            mFileObject->FieldWriteBlockBegin ();
            mFileObject->FieldWriteD("Version", 1.1);

            // first we write the properties of the Node leaving the block open if we also
            // have a node attribute associated.
            lSuccess &= WriteFCurves(*node, pAnimLayer, FIELD_KFBXNODE_MODEL, nodeAttribute!=NULL, false);
            if (nodeAttribute)
            {
                // since we have a node attribute associated to the node,
                // we now write its properties fcurves and we close the block
                lSuccess &= WriteFCurves(*nodeAttribute, pAnimLayer, NULL, false, false);
            }

            // and now, close it!
            mFileObject->FieldWriteBlockEnd ();
            mFileObject->FieldWriteEnd ();
        }
    }

    return lSuccess;
}


bool FbxWriterFbx6::WriteFCurves(FbxObject& pObject, FbxAnimLayer* pAnimLayer, const char* pBlockName, bool pKeepBlockOpen, bool pRescaleShininess /* = false */)
{
    //Note: In MB the fcurve for the shininess can have any value. However,
    // the interface does, in fact, limit the range between 0 and 100. I leave the code but
    // the pRescaleShininess parameter is now always false so we never rescale the fcurve.
    FBX_ASSERT(pRescaleShininess == false);

    // We have one special case to process so we group the T, R and S fcurves together
    // in a "Transform" block.
    FbxProperty propT = pObject.FindProperty("Lcl Translation");
    FbxProperty propR = pObject.FindProperty("Lcl Rotation");
    FbxProperty propS = pObject.FindProperty("Lcl Scaling");

    FbxAnimCurveNode* cnT = (propT.IsValid()) ? propT.GetCurveNode(pAnimLayer, false) : NULL;
    FbxAnimCurveNode* cnR = (propR.IsValid()) ? propR.GetCurveNode(pAnimLayer, false) : NULL;
    FbxAnimCurveNode* cnS = (propS.IsValid()) ? propS.GetCurveNode(pAnimLayer, false) : NULL;
    
    FbxAnimUtilities::CurveNodeIntfce fcnT = FbxAnimUtilities::GrabCurveNode(cnT);
    FbxAnimUtilities::CurveNodeIntfce fcnR = FbxAnimUtilities::GrabCurveNode(cnR);
    FbxAnimUtilities::CurveNodeIntfce fcnS = FbxAnimUtilities::GrabCurveNode(cnS);

    if (fcnT.IsValid() || fcnR.IsValid() || fcnS.IsValid())
    {
        mFileObject->FieldWriteBegin("Channel");
        mFileObject->FieldWriteC(FBXSDK_CURVENODE_TRANSFORM);
        mFileObject->FieldWriteBlockBegin ();

		FbxAnimUtilities::ConnectTimeWarp(cnT, fcnT, mTimeWarpsCurveNodes);
		FbxAnimUtilities::ConnectTimeWarp(cnR, fcnR, mTimeWarpsCurveNodes);
		FbxAnimUtilities::ConnectTimeWarp(cnS, fcnS, mTimeWarpsCurveNodes);
                                                                 
        FbxAnimUtilities::StoreCurveNode(fcnT, mFileObject);
        FbxAnimUtilities::StoreCurveNode(fcnR, mFileObject);
        FbxAnimUtilities::StoreCurveNode(fcnS, mFileObject);

        mFileObject->FieldWriteBlockEnd ();
        mFileObject->FieldWriteEnd ();
    }

    FbxAnimUtilities::ReleaseCurveNode(cnT);
    FbxAnimUtilities::ReleaseCurveNode(cnR);
    FbxAnimUtilities::ReleaseCurveNode(cnS);

    FbxProperty prop = pObject.GetFirstProperty();
    while (prop.IsValid())
    {
        // skip this property if it is propT, propR or propS
        if (prop != propT && prop != propR && prop != propS)
        {
            FbxAnimCurveNode* cn = prop.GetCurveNode(pAnimLayer, false);
            if (cn)
            {
                FbxAnimUtilities::CurveNodeIntfce fcn = FbxAnimUtilities::GrabCurveNode(cn);
				FbxAnimUtilities::ConnectTimeWarp(cn, fcn, mTimeWarpsCurveNodes);
                FbxAnimUtilities::StoreCurveNode(fcn, mFileObject);
                FbxAnimUtilities::ReleaseCurveNode(cn);
            }
        }

        prop = pObject.GetNextProperty(prop);
    }
   
    return true;
}

void FbxWriterFbx6::WriteTakesAndAnimation(FbxDocument* pDocument)
{
	WriteAnimation(pDocument);
}

// *******************************************************************************
//  Utility functions
// *******************************************************************************
const char* GetMappingModeToken(FbxLayerElement::EMappingMode pMappingMode) 
{
    const char* lRet = TOKEN_KFBXGEOMETRYMESH_NO_MAPPING_INFORMATION;

    switch (pMappingMode)
    {
        case FbxLayerElement::eNone:               lRet = TOKEN_KFBXGEOMETRYMESH_NO_MAPPING_INFORMATION;   break;
        case FbxLayerElement::eByControlPoint:   lRet = TOKEN_KFBXGEOMETRYMESH_BY_VERTICE;               break;
        case FbxLayerElement::eByPolygonVertex:  lRet = TOKEN_KFBXGEOMETRYMESH_BY_POLYGON_VERTEX;        break;
        case FbxLayerElement::eByPolygon:         lRet = TOKEN_KFBXGEOMETRYMESH_BY_POLYGON;               break;
        case FbxLayerElement::eAllSame:           lRet = TOKEN_KFBXGEOMETRYMESH_ALL_SAME;                 break;
        case FbxLayerElement::eByEdge:            lRet = TOKEN_KFBXGEOMETRYMESH_BY_EDGE;                  break;
    }

    return lRet;
}

const char* GetReferenceModeToken(FbxLayerElement::EReferenceMode pReferenceMode)
{
    const char* lRet = TOKEN_REFERENCE_DIRECT;

    switch (pReferenceMode)
    {
        case FbxLayerElement::eDirect:         lRet = TOKEN_REFERENCE_DIRECT;          break;
        case FbxLayerElement::eIndex:          lRet = TOKEN_REFERENCE_INDEX;           break;
        case FbxLayerElement::eIndexToDirect:lRet = TOKEN_REFERENCE_INDEX_TO_DIRECT; break;
    }

    return lRet;
}

const char* GetBlendModeToken(FbxLayerElementTexture::EBlendMode pBlendMode) 
{
    const char* lRet = TOKEN_KFBXTEXTURE_BLEND_NORMAL;

    switch (pBlendMode)
    {
        case FbxLayerElementTexture::eTranslucent: lRet = TOKEN_KFBXTEXTURE_BLEND_TRANSLUCENT; break;
        case FbxLayerElementTexture::eAdd:         lRet = TOKEN_KFBXTEXTURE_BLEND_ADD;         break;
        case FbxLayerElementTexture::eModulate:    lRet = TOKEN_KFBXTEXTURE_BLEND_MODULATE;    break;
        case FbxLayerElementTexture::eModulate2:   lRet = TOKEN_KFBXTEXTURE_BLEND_MODULATE2;   break;
        case FbxLayerElementTexture::eOver:        lRet = TOKEN_KFBXTEXTURE_BLEND_OVER;        break;  
        case FbxLayerElementTexture::eNormal:		lRet = TOKEN_KFBXTEXTURE_BLEND_NORMAL;      break;
        case FbxLayerElementTexture::eDissolve:    lRet = TOKEN_KFBXTEXTURE_BLEND_DISSOLVE;    break;
        case FbxLayerElementTexture::eDarken:		lRet = TOKEN_KFBXTEXTURE_BLEND_DARKEN;      break;
        case FbxLayerElementTexture::eColorBurn:   lRet = TOKEN_KFBXTEXTURE_BLEND_COLORBURN;   break;
        case FbxLayerElementTexture::eLinearBurn:  lRet = TOKEN_KFBXTEXTURE_BLEND_LINEARBURN;  break;
        case FbxLayerElementTexture::eDarkerColor: lRet = TOKEN_KFBXTEXTURE_BLEND_DARKERCOLOR; break;
        case FbxLayerElementTexture::eLighten:		lRet = TOKEN_KFBXTEXTURE_BLEND_LIGHTEN;     break;
        case FbxLayerElementTexture::eScreen:		lRet = TOKEN_KFBXTEXTURE_BLEND_SCREEN;      break;
        case FbxLayerElementTexture::eColorDodge:	lRet = TOKEN_KFBXTEXTURE_BLEND_COLORDODGE;  break;
        case FbxLayerElementTexture::eLinearDodge: lRet = TOKEN_KFBXTEXTURE_BLEND_LINEARDODGE; break;
        case FbxLayerElementTexture::eLighterColor:lRet = TOKEN_KFBXTEXTURE_BLEND_LIGHTERCOLOR;break;
        case FbxLayerElementTexture::eSoftLight:	lRet = TOKEN_KFBXTEXTURE_BLEND_SOFTLIGHT;   break;
        case FbxLayerElementTexture::eHardLight:	lRet = TOKEN_KFBXTEXTURE_BLEND_HARDLIGHT;   break;
        case FbxLayerElementTexture::eVividLight:  lRet = TOKEN_KFBXTEXTURE_BLEND_VIVIDLIGHT;  break;
        case FbxLayerElementTexture::eLinearLight: lRet = TOKEN_KFBXTEXTURE_BLEND_LINEARLIGHT; break;
        case FbxLayerElementTexture::ePinLight: 	lRet = TOKEN_KFBXTEXTURE_BLEND_PINLIGHT;    break;
        case FbxLayerElementTexture::eHardMix:		lRet = TOKEN_KFBXTEXTURE_BLEND_HARDMIX;     break;
        case FbxLayerElementTexture::eDifference:  lRet = TOKEN_KFBXTEXTURE_BLEND_DIFFERENCE;  break;
        case FbxLayerElementTexture::eExclusion: 	lRet = TOKEN_KFBXTEXTURE_BLEND_EXCLUSION;   break;
        case FbxLayerElementTexture::eSubtract:    lRet = TOKEN_KFBXTEXTURE_BLEND_SUBTRACT;    break;
        case FbxLayerElementTexture::eDivide:      lRet = TOKEN_KFBXTEXTURE_BLEND_DIVIDE;      break;
        case FbxLayerElementTexture::eHue: 		lRet = TOKEN_KFBXTEXTURE_BLEND_HUE;         break;
        case FbxLayerElementTexture::eSaturation:	lRet = TOKEN_KFBXTEXTURE_BLEND_SATURATION;  break;
        case FbxLayerElementTexture::eColor:		lRet = TOKEN_KFBXTEXTURE_BLEND_COLOR;       break;
        case FbxLayerElementTexture::eLuminosity:  lRet = TOKEN_KFBXTEXTURE_BLEND_LUMINOSITY;  break;
        case FbxLayerElementTexture::eOverlay:     lRet = TOKEN_KFBXTEXTURE_BLEND_OVERLAY;     break;
        case FbxLayerElementTexture::eBlendModeCount:    lRet = TOKEN_KFBXTEXTURE_BLEND_MAXBLEND;    break;
    }

    return lRet;
}

int ComputeReferenceDepth(FbxObject* pObject)
{
    if (pObject == NULL) 
	{
        return 0;
    }

    FbxObject* pReferencedObject = pObject->GetReferenceTo();

    if (pReferencedObject == NULL)
    {
        return 0;
    }

    return 1 + ComputeReferenceDepth(pReferencedObject);
}

#include <fbxsdk/fbxsdk_nsend.h>

