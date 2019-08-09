/****************************************************************************************
 
   Copyright (C) 2017 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/scene/fbxgroupname.h>
#include <fbxsdk/scene/animation/fbxanimutilities.h>
#include <fbxsdk/fileio/fbx/fbxwriterfbx7.h>
#include <fbxsdk/utils/fbxembeddedfilesaccumulator.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

#define IOS_REF (*GetIOSettings())

static FbxArray<FbxLight*> gAreaLights;

const char* kENHANCED_PROPERTIES = "Properties70";
const char* kENHANCED_PROPERTY_FIELD = "P";
const char* kENHANCED_CONNECT_FIELD = "C";

const FbxLongLong kInvalidObjectId  = -1;
const FbxLongLong kRootNodeObjectId = 0;

// *******************************************************************************
//  Utility functions
// *******************************************************************************
const char* GetMappingModeToken(FbxLayerElement::EMappingMode pMappingMode)
{
    const char* lRet = TOKEN_KFBXGEOMETRYMESH_NO_MAPPING_INFORMATION;

    switch(pMappingMode)
    {
		case FbxLayerElement::eNone:			lRet = TOKEN_KFBXGEOMETRYMESH_NO_MAPPING_INFORMATION;   break;
		case FbxLayerElement::eByControlPoint:	lRet = TOKEN_KFBXGEOMETRYMESH_BY_VERTICE;               break;
		case FbxLayerElement::eByPolygonVertex: lRet = TOKEN_KFBXGEOMETRYMESH_BY_POLYGON_VERTEX;        break;
		case FbxLayerElement::eByPolygon:		lRet = TOKEN_KFBXGEOMETRYMESH_BY_POLYGON;               break;
		case FbxLayerElement::eAllSame:			lRet = TOKEN_KFBXGEOMETRYMESH_ALL_SAME;                 break;
		case FbxLayerElement::eByEdge:			lRet = TOKEN_KFBXGEOMETRYMESH_BY_EDGE;                  break;
    }

    return lRet;
}

const char* GetReferenceModeToken(FbxLayerElement::EReferenceMode pReferenceMode)
{
    const char* lRet = TOKEN_REFERENCE_DIRECT;

    switch(pReferenceMode)
    {
    case FbxLayerElement::eDirect:			lRet = TOKEN_REFERENCE_DIRECT;          break;
    case FbxLayerElement::eIndex:			lRet = TOKEN_REFERENCE_INDEX;           break;
    case FbxLayerElement::eIndexToDirect:	lRet = TOKEN_REFERENCE_INDEX_TO_DIRECT; break;
    }

    return lRet;
}

const char* GetBlendModeToken(FbxLayerElementTexture::EBlendMode pBlendMode)
{
    const char* lRet = TOKEN_KFBXTEXTURE_BLEND_NORMAL;

    switch(pBlendMode)
    {
        case FbxLayerElementTexture::eTranslucent:	lRet = TOKEN_KFBXTEXTURE_BLEND_TRANSLUCENT; break;
        case FbxLayerElementTexture::eAdd:			lRet = TOKEN_KFBXTEXTURE_BLEND_ADD;         break;
        case FbxLayerElementTexture::eModulate:		lRet = TOKEN_KFBXTEXTURE_BLEND_MODULATE;    break;
        case FbxLayerElementTexture::eModulate2:	lRet = TOKEN_KFBXTEXTURE_BLEND_MODULATE2;   break;
        case FbxLayerElementTexture::eOver:			lRet = TOKEN_KFBXTEXTURE_BLEND_OVER;        break;  
        case FbxLayerElementTexture::eNormal:		lRet = TOKEN_KFBXTEXTURE_BLEND_NORMAL;      break;
        case FbxLayerElementTexture::eDissolve:		lRet = TOKEN_KFBXTEXTURE_BLEND_DISSOLVE;    break;
        case FbxLayerElementTexture::eDarken:		lRet = TOKEN_KFBXTEXTURE_BLEND_DARKEN;      break;
        case FbxLayerElementTexture::eColorBurn:	lRet = TOKEN_KFBXTEXTURE_BLEND_COLORBURN;   break;
        case FbxLayerElementTexture::eLinearBurn:	lRet = TOKEN_KFBXTEXTURE_BLEND_LINEARBURN;  break;
        case FbxLayerElementTexture::eDarkerColor:	lRet = TOKEN_KFBXTEXTURE_BLEND_DARKERCOLOR; break;
        case FbxLayerElementTexture::eLighten:		lRet = TOKEN_KFBXTEXTURE_BLEND_LIGHTEN;     break;
        case FbxLayerElementTexture::eScreen:		lRet = TOKEN_KFBXTEXTURE_BLEND_SCREEN;      break;
        case FbxLayerElementTexture::eColorDodge:	lRet = TOKEN_KFBXTEXTURE_BLEND_COLORDODGE;  break;
        case FbxLayerElementTexture::eLinearDodge:	lRet = TOKEN_KFBXTEXTURE_BLEND_LINEARDODGE; break;
        case FbxLayerElementTexture::eLighterColor:	lRet = TOKEN_KFBXTEXTURE_BLEND_LIGHTERCOLOR;break;
        case FbxLayerElementTexture::eSoftLight:	lRet = TOKEN_KFBXTEXTURE_BLEND_SOFTLIGHT;   break;
        case FbxLayerElementTexture::eHardLight:	lRet = TOKEN_KFBXTEXTURE_BLEND_HARDLIGHT;   break;
        case FbxLayerElementTexture::eVividLight:	lRet = TOKEN_KFBXTEXTURE_BLEND_VIVIDLIGHT;  break;
        case FbxLayerElementTexture::eLinearLight:	lRet = TOKEN_KFBXTEXTURE_BLEND_LINEARLIGHT; break;
        case FbxLayerElementTexture::ePinLight:		lRet = TOKEN_KFBXTEXTURE_BLEND_PINLIGHT;    break;
        case FbxLayerElementTexture::eHardMix:		lRet = TOKEN_KFBXTEXTURE_BLEND_HARDMIX;     break;
        case FbxLayerElementTexture::eDifference:	lRet = TOKEN_KFBXTEXTURE_BLEND_DIFFERENCE;  break;
        case FbxLayerElementTexture::eExclusion:	lRet = TOKEN_KFBXTEXTURE_BLEND_EXCLUSION;   break;
        case FbxLayerElementTexture::eSubtract:		lRet = TOKEN_KFBXTEXTURE_BLEND_SUBTRACT;    break;
        case FbxLayerElementTexture::eDivide:		lRet = TOKEN_KFBXTEXTURE_BLEND_DIVIDE;      break;
        case FbxLayerElementTexture::eHue:			lRet = TOKEN_KFBXTEXTURE_BLEND_HUE;         break;
        case FbxLayerElementTexture::eSaturation:	lRet = TOKEN_KFBXTEXTURE_BLEND_SATURATION;  break;
        case FbxLayerElementTexture::eColor:		lRet = TOKEN_KFBXTEXTURE_BLEND_COLOR;       break;
        case FbxLayerElementTexture::eLuminosity:	lRet = TOKEN_KFBXTEXTURE_BLEND_LUMINOSITY;  break;
        case FbxLayerElementTexture::eOverlay:		lRet = TOKEN_KFBXTEXTURE_BLEND_OVERLAY;     break;
        case FbxLayerElementTexture::eBlendModeCount:lRet = TOKEN_KFBXTEXTURE_BLEND_MAXBLEND;    break;
    }

    return lRet;
}

int ComputeReferenceDepth(FbxObject * pObject)
{
    if(pObject == NULL) 
    {
        return 0;
    }

    int lDepth = 0;

    for( FbxObject* pReferencedObject = pObject->GetReferenceTo(); pReferencedObject != NULL;
         pReferencedObject = pReferencedObject->GetReferenceTo() )
    {
        ++lDepth;
    }

    return lDepth;
}

char FbxGetCharFromFlags(int pFlags)
{
    // This function assumes that all flags can be stored in one byte (actually, in four bits).
    FBX_ASSERT_STATIC( FbxPropertyFlags::sLockedMembersMax == 4 );
    FBX_ASSERT_STATIC( FbxPropertyFlags::eLockedAll == ( 0xf << FbxPropertyFlags::sLockedMembersBitOffset ) );
    FBX_ASSERT_STATIC( FbxPropertyFlags::sMutedMembersMax == 4 );
    FBX_ASSERT_STATIC( FbxPropertyFlags::eMutedAll == ( 0xf << FbxPropertyFlags::sMutedMembersBitOffset ) );
	FBX_ASSERT(pFlags >= 0x0 && pFlags <= 0xf);

	if( pFlags >= 1 && pFlags <= 9 ) return '0' + pFlags;				//Number 1 to 9: Return one digit char
	else if( pFlags >= 10 && pFlags <= 14 ) return 'a' + (pFlags - 10);	//Number 10 to 14: Return one hexa digit char

	return '\0';	//Number 0 or 15 (or anything else), return no character
}

//--------------------------------------
class KTypeDefinitionInfo
{
public:
    FbxClassId mClassId;
    FbxString mName;
    bool mStore;

    int mCount;

    // Construction
    KTypeDefinitionInfo()
     : mSupportsGenericWrite(true), mWritePropertyTemplate(true) {}

    // ---- HACK --------------------------------------------------
    // To support backward compatibility, some object types have to
    // be written with the old saving mechanism.   So this flag
    // is set to true by default and must be set to false
    // if the object type CAN'T be saved with the latest generic writer
    inline bool IsGenericWriteEnabled() const {return mSupportsGenericWrite;}
    inline void SetGenericWriteEnabled(bool pEnable){mSupportsGenericWrite = pEnable;}

    inline void SetPropertyTemplateEnabled(bool pEnable) 
    {
        mWritePropertyTemplate = pEnable;
    }

    inline bool IsPropertyTemplateEnabled() const
    {
        return mWritePropertyTemplate;
    }

private:
    bool mSupportsGenericWrite;
    // ---- END HACK ------------------------------------------------

    // Some odd-balls classes should not be stored with property templates, such
    // as the document info.
    bool mWritePropertyTemplate;
};

//--------------------------------------
class KTypeDefinition
{
public:
    KTypeDefinition();
    virtual ~KTypeDefinition();

    bool                 AddObject(FbxObject* pObject);
    bool                 AddObject(const char* pTypeName, FbxClassId pClassId, bool pStore);
    KTypeDefinitionInfo* GetDefinitionFromName(const char* pTypeName) const;
    KTypeDefinitionInfo* GetDefinition(int pIndex) const;
    int                  GetDefinitionCount() const;
    int                  GetObjectCount()const;

private:
    FbxArray<KTypeDefinitionInfo*> mList;

    // Quick lookup by name
    struct CompareTypeInfoPtr
    {
        inline int operator()(KTypeDefinitionInfo* pInfo1, KTypeDefinitionInfo* pInfo2) const
        {
            return pInfo1->mName < pInfo2->mName ? -1 : (pInfo1->mName > pInfo2->mName) ? 1 : 0;
        }
    };

    typedef FbxSet<KTypeDefinitionInfo*, CompareTypeInfoPtr> KDefinitionSet;
    KDefinitionSet mDefinitionSet;

    int mObjectCount;
};

KTypeDefinition::KTypeDefinition() :
	mObjectCount(0)
{
}

KTypeDefinition::~KTypeDefinition()
{
    FbxArrayDelete(mList);
    mObjectCount = 0;
}

bool KTypeDefinition::AddObject(FbxObject* pObject)
{
    FbxClassId lClassId = pObject->GetRuntimeClassId();
    FbxString     lTypeName = lClassId.GetFbxFileTypeName(true);

    return AddObject(lTypeName, lClassId, pObject->IsRuntimePlug());
}

bool KTypeDefinition::AddObject(const char* pTypeName, FbxClassId pClassId, bool pStore)
{
    if( strlen(pTypeName) == 0 ) return false;

    KTypeDefinitionInfo* lDef = GetDefinitionFromName(pTypeName);
    if( lDef ) lDef->mCount++;
    else
    {
        lDef = FbxNew< KTypeDefinitionInfo >();
        lDef->mName = pTypeName;
        lDef->mCount = 1;
        lDef->mClassId = pClassId;
        lDef->mStore = pStore;
        mList.Add(lDef);
        mDefinitionSet.Insert(lDef);
    }

    mObjectCount++;
    return true;
}

KTypeDefinitionInfo* KTypeDefinition::GetDefinition(int pIndex)const
{
    return mList[pIndex];
}

int KTypeDefinition::GetDefinitionCount() const
{
    return mList.GetCount();
}

int KTypeDefinition::GetObjectCount() const
{
    return mObjectCount;
}

KTypeDefinitionInfo* KTypeDefinition::GetDefinitionFromName(const char* pTypeName) const
{
    KTypeDefinitionInfo lInfo;
    lInfo.mName = pTypeName;

	const KDefinitionSet::RecordType* lRecord = mDefinitionSet.Find(&lInfo);

    if( !lRecord ) return NULL;
	return lRecord->GetValue();
}

//
// Structure used to store temporary object reference information
//
struct KTypeWriteReferenceInfo
{
    FbxObject* mRefObj;
    FbxString     mRefName;
};

//
// Structure used to manage internal & external (in terms of Document) object reference
//
class KTypeWriteReferences
{
public:
    KTypeWriteReferences();
    virtual ~KTypeWriteReferences();
    int AddReference(FbxObject* pRefObj, const char* pRefName);
    int GetCount() const { return mRefInfo.GetCount();}
    bool GetReferenceName(FbxObject* pRefObj, FbxString & pRefName) const;
private:
    FbxArray<KTypeWriteReferenceInfo*>    mRefInfo;
};

KTypeWriteReferences::KTypeWriteReferences()
{
}

KTypeWriteReferences::~KTypeWriteReferences()
{
    FbxArrayDelete(mRefInfo);
}

int KTypeWriteReferences::AddReference(FbxObject* pRefObj, const char* pRefName)
{
    KTypeWriteReferenceInfo* lRefInfo = FbxNew< KTypeWriteReferenceInfo >();
    lRefInfo->mRefObj = pRefObj;
    lRefInfo->mRefName = pRefName;
    return mRefInfo.Add(lRefInfo);
}

bool KTypeWriteReferences::GetReferenceName(FbxObject* pRefObj, FbxString & pRefName) const
{
    int i, lCount = mRefInfo.GetCount();
    for(i=0; i < lCount; i++)
    {
        FbxObject const * lInfoObj = mRefInfo[i]->mRefObj;
        if(lInfoObj == pRefObj)
        {
            pRefName = mRefInfo[i]->mRefName;
            return true;
        }
    }
    return false;
}

//
// Locally used to sort objects according their referencing depth.
// The depth is the number of chained references
//

struct KReferenceDepth
{
    FbxObject* mObject;
    int mRefDepth;

    KReferenceDepth() 
    : mObject(NULL)
    , mRefDepth(0)
    {
    }
};

typedef FbxDynamicArray<KReferenceDepth> KReferenceDepthList;

// Pass-through: collect everything in CollectObjectsByDepth
struct CollectAll
{
    inline bool operator()(FbxObject* pObject)
    {
        return true;
    }
};

struct CollectExcept
{
    FbxObject* mException;

    CollectExcept(FbxObject* pException = NULL)
    : mException(pException)
    {
    }

    inline bool operator()(FbxObject* pObject)
    {
        return pObject != mException;
    }
};

// Generic: collect storable, non-system objects in CollectObjectsByDepth
struct CollectGenericObject
{
    KTypeDefinition* mTypeDefinitions;

    CollectGenericObject(KTypeDefinition* pTypeDefinitions = NULL)
    : mTypeDefinitions(pTypeDefinitions)
    {
    }

    inline bool operator()(FbxObject* pObject)
    {
        FBX_ASSERT_RETURN_VALUE( mTypeDefinitions, false );

        // Storing only savable objects
        if( !pObject->GetObjectFlags(FbxObject::eSavable) )
            return false;

        FbxClassId      lClassId = pObject->GetRuntimeClassId();
        FbxString lFbxClassTypeName = lClassId.GetFbxFileTypeName(true);

        KTypeDefinitionInfo* pTypeDefinition = mTypeDefinitions->GetDefinitionFromName(lFbxClassTypeName);

        return pTypeDefinition && pTypeDefinition->IsGenericWriteEnabled();
    }
};

template <typename T, typename Cond> void CollectObjectsByDepth(FbxDocument* pDocument, KReferenceDepthList& pObjects, const T* pType, Cond pCond = Cond())
{
    FBX_ASSERT_RETURN(pDocument);

    KReferenceDepth lDepth;

    for( int i = 0, n = pDocument->GetSrcObjectCount<T>(); i < n; ++i )
    {
        lDepth.mObject      = pDocument->GetSrcObject<T>(i);

        if( !pCond(lDepth.mObject) )
        {
            continue;
        }

        lDepth.mRefDepth    = ComputeReferenceDepth(lDepth.mObject);

		// Sort the object be ref depth (from depth 0 and up), 
		// while still need to keep the original object order.
		// Assuming that we'll be dealing with mostly references of depth 0..
		size_t iter = pObjects.Size() - 1;
		for( ; iter != -1; --iter )
		{
			if( lDepth.mRefDepth >= pObjects[iter].mRefDepth )
			{
				pObjects.Insert(iter+1, lDepth);
				break;
			}
		}

		if( iter == -1 )
		{
			pObjects.Insert(0, lDepth);
		}
    }

    // Next recurse into sub-docs.
    for( int i = 0, n = pDocument->GetSrcObjectCount<FbxDocument>(); i < n; ++i )
    {
        FbxDocument* lSubDoc = pDocument->GetSrcObject<FbxDocument>(i);

        CollectObjectsByDepth(lSubDoc, pObjects, pType, pCond);
    }
}

template <typename T>
void CollectObjectsByDepth(FbxDocument* pDocument, KReferenceDepthList& pObjects, const T* pType)
{
    CollectAll  lFilter;

    CollectObjectsByDepth(pDocument, pObjects, pType, lFilter);
}

class FbxBinaryDataReader
{
public:
    virtual ~FbxBinaryDataReader() {}

    virtual int  GetSize()  = 0;
    virtual bool GetNextChunk(const char*& pBuffer, int& pChunkLength, int pMaxChunkSize) = 0;
};

class FbxBinaryBlobReader : public FbxBinaryDataReader
{
public:
    FbxBinaryBlobReader(FbxProperty pProperty) 
    : mBlob(pProperty.Get<FbxBlob>())
    {
        mBuffer = reinterpret_cast<const char*>(mBlob.Access());
        mOffset = 0;
    }

    virtual int GetSize()
    {
        return mBlob.Size();
    }

    virtual bool GetNextChunk(const char*& pBuffer, int& pChunkLength, int pMaxChunkSize)
    {
        if( !pBuffer )              // Reset 'iterator' back to the beginning
        {   
            mOffset = 0;
        }

        if( mOffset >= GetSize() )  // Empty buffer?
        {
            return false;
        }

        pBuffer      = mBuffer + mOffset;
        pChunkLength = FbxClamp(GetSize() - mOffset, 0, pMaxChunkSize);
        mOffset     += pChunkLength;

        return pChunkLength > 0;
    }

private:
    FbxBlob     mBlob;
    const char* mBuffer;
    int         mOffset;
};

class FbxBinaryFileReader : public FbxBinaryDataReader
{
public:
	FbxBinaryFileReader(FbxFile& pFile) :
		mFile(pFile),
		mSize((int)pFile.GetSize())
	{
		Reset();
	}

    virtual int GetSize()
    {
        return mSize;
    }

    virtual bool GetNextChunk(const char*& pBuffer, int& pChunkLength, int pMaxChunkSize)
    {
        if( !pBuffer )              // Reset 'iterator' back to the beginning
        {   
            Reset();
        }

        if( mIteratorPosition >= GetSize() )  // Empty buffer?
        {
            return false;
        }

        if( EndOfBuffer() && !FillBuffer(pMaxChunkSize) )
        {
        }

        FBX_ASSERT( mIteratorPosition >= mBaseOffset );

        int lLocalIteratorPosition = (mIteratorPosition - mBaseOffset);

        FBX_ASSERT( lLocalIteratorPosition <= int(mBuffer.Size()) );
        FBX_ASSERT( lLocalIteratorPosition >= 0 );

		pBuffer             = mBuffer.GetArray() + lLocalIteratorPosition;
        pChunkLength        = FbxClamp((int)mBuffer.Size() - lLocalIteratorPosition, 0, pMaxChunkSize);
        mIteratorPosition  += pChunkLength;

        return pChunkLength > 0;
    }

private:
    FbxArray<char>   mBuffer;

    FbxFile&            mFile;
    int                 mSize;
    int                 mIteratorPosition;

    int                 mBaseOffset;

private:
    enum { kMaxBufferSize = 512 * 1024 };

    bool FillBuffer(int pChunkSize)
    {
        const int lBufferSize = (pChunkSize > kMaxBufferSize) ? kMaxBufferSize : (kMaxBufferSize / pChunkSize) * pChunkSize;
        FBX_ASSERT( lBufferSize > 0 );

        mBuffer.Resize(lBufferSize);
        mBaseOffset = (int)mFile.Tell();

		int lReadCount = (int)mFile.Read(mBuffer.GetArray(), mBuffer.Size());
        if( lReadCount >= 0 )
        {
            mBuffer.Resize(lReadCount);
        }
        else
        {
            mBuffer.Clear();
        }

		return mBuffer.Size() > 0;
    }

    void Reset()
    {
        mIteratorPosition = 0;
        mBaseOffset       = 0;

        mBuffer.Clear();
        mBuffer.Reserve(kMaxBufferSize);
    }

    bool EndOfBuffer() const
    {
        FBX_ASSERT( mIteratorPosition >= mBaseOffset );

        return (mIteratorPosition - mBaseOffset) >= int(mBuffer.Size());
    }
};

struct FbxWriterFbx7_Impl
{
    FbxIO*                      mFileObject;
    FbxManager&                 mManager;
    FbxStatus&				    mStatus;
    FbxExporter&                mExporter;
	FbxWriter*					mWriter;
    KTypeWriteReferences*       mDocumentReferences;
    FbxWriterFbx7::EExportMode  mExportMode;
	FbxIOSettings *             mIOSettings;

    // When collapsing external documents into our document we create a sub doc
    // to keep our scene as clean as possible.  
    FbxDocument*                mCollapseDocument;
    KTypeDefinition             mTypeDefinitions;

    FbxMultiMap mTextureAnimatedChannels;
    FbxMultiMap mMaterialAnimatedChannels;

    struct TextureAnimatedChannels
    {
        bool mTranslation;
        bool mRotation;
        bool mScaling;
        bool mAlpha;
    };

    struct SurfaceMaterialAnimatedChannels
    {
        bool mAmbient;
        bool mDiffuse;
        bool mSpecular;
        bool mEmissive;
        bool mOpacity;
        bool mShininess;
        bool mReflectivity;
    };

    FbxScene* mSceneExport;
    FbxDocument* mTopDocument;

    FbxProgress *mProgress;
    bool mProgressPause;
    bool mCanceled;

    FbxWriterFbx7_Impl(FbxManager& pManager, FbxExporter& pExporter, FbxWriter* pWriter, FbxStatus& pStatus) :
		mFileObject(NULL),
		mManager(pManager),
		mStatus(pStatus),
		mExporter(pExporter),
		mWriter(pWriter),
		mDocumentReferences(NULL),
		mExportMode(FbxWriterFbx7::eBINARY),
		mCollapseDocument(NULL),
		mSceneExport(NULL),
		mTopDocument(NULL),
        mProgress(NULL),
        mProgressPause(true),
        mCanceled(false)
    {
    }

    ~FbxWriterFbx7_Impl()
    {
        FBX_SAFE_DELETE(mDocumentReferences);
    }

    inline FbxLongLong GetObjectId(const FbxObject* pObject) const
    {
        return SceneRootNode(pObject) ? kRootNodeObjectId : (FbxLongLong)(FbxHandle(pObject));
    }

    inline bool SceneRootNode(const FbxObject* pObject) const
    {
        return mSceneExport && mSceneExport->GetRootNode() == pObject;
    }

    bool WriteFbxHeader(FbxDocument* pDocument);

    // Collapsing is a ... destructive operation.  Make sure it gets undone properly
    // using an automatic object on the stack.
    struct FbxObjectCollapserUndo
    {
        FbxWriterFbx7_Impl* mImpl;
        bool                mCollapseEnabled;

        FbxObjectCollapserUndo(FbxWriterFbx7_Impl* pImpl, bool pCollapseEnabled) 
            : mImpl(pImpl)
            , mCollapseEnabled(pCollapseEnabled) {}

        ~FbxObjectCollapserUndo()
        {
            if( mImpl && mCollapseEnabled )
            {
                mImpl->RemoveCollapsedExternalObjects();
            }
        }
    };

	void ConvertShapePropertyToOldStyle(FbxScene& pScene);
    bool CollapseExternalObjects(FbxDocument* pDocument);
    bool RemoveCollapsedExternalObjects();

	typedef FbxArray<FbxDocument*>    FbxDocumentList;

    void BuildObjectDefinition(FbxDocument* pTopDocument);
    void SetObjectWriteSupport();
    bool WriteDocumentsSection(FbxDocument* pTopDocument);
    bool WriteDocumentDescription(FbxDocument* pDocument, bool pOutputDocumentInfo);
    void CollectDocumentHiearchy(FbxDocumentList& pDocuments, FbxDocument* pDocument);
    bool WriteReferenceSection(FbxDocument* pTopDocument, KTypeWriteReferences& pReferences);
    void WriteObjectDefinition(FbxDocument* pTopDocument);
    void WriteObjectProperties(FbxDocument* pTopDocument);
    bool WriteGlobalSettings(FbxDocument* pTopDocument);

    bool WriteObjectHeaderAndReferenceIfAny(FbxObject& pObj, const char* pObjectType) const;
    bool WriteObjectHeaderAndReferenceIfAny(FbxObject& pObj, const char* pObjectType, const char* pObjectSubType) const;

    void WriteObjectConnections(FbxDocument* pTopDocument);
    bool WriteEmbeddedFiles(FbxDocument* pTopDocument);

    // This one is here, possibly, only for testing purposes
    void WriteTakes(FbxDocument* pDocument);

    void WriteConstraints(FbxScene& pScene);
    void WriteConstraint(FbxConstraint& pConstraint, FbxScene& pScene);

    void WriteGeometryWeightedMap(FbxGeometryWeightedMap& pGeometryWeightedMap);

    void WriteAllGeometryWeightedMaps(FbxDocument* pTopDocument);

	void WriteCharacter(FbxCharacter& pCharacter, FbxScene& pScene);
    int  WriteCharacterPose(FbxScene& pScene);
    void WriteCharacterPose(FbxCharacterPose& pCharacterPose, FbxScene& pScene);
	void WriteCharacterPoseNodeRecursive(FbxNode* pNode, FbxNode* pParent);

    bool WriteSelectionNode                 (FbxScene& pScene);
    void WriteSelectionNode             (FbxSelectionNode& pSelectionNode);

    bool WriteSelectionSet                  (FbxScene& pScene);
    void WriteSelectionSet              (FbxSelectionSet& pSelectionSet);

    void WriteCharacterLinkGroup(FbxCharacter& pCharacter, int pCharacterGroupId, FbxScene& pScene);
    void WriteCharacterLink(FbxCharacter& pCharacter, int pCharacterNodeId, FbxScene& pScene);
    void WriteCharacterLinkRotationSpace(FbxCharacterLink& pCharacterLink);

    void WriteControlSetPlug(FbxScene& pScene);

    /*************************** new writer ***************************/

    bool WriteNodes(FbxScene* pScene, bool pIncludeRoot);

    bool WriteGenericObjects(FbxDocument* pDocument);
    bool WriteAnimObjects(FbxDocument* pDocument);

    /*************************** kept functions ***************************/
    bool WriteObjectProperties      (FbxObject *pObject);
    bool WriteObjectPropertiesAndFlags(FbxObject* pObject);

    //bool WriteNode                      (FbxNode& pNode);
    bool WriteNodeParameters        (FbxNode& pNode);
    bool WriteNodeVersion           (FbxNode& pNode);
    bool WriteNodeShading           (FbxNode& pNode);
    bool WriteNodeCullingType       (FbxNode& pNode);
    bool WriteNodeAttribute         (FbxNodeAttribute* pNodeAttribute);
    bool WriteNodeProperties        (FbxNode& pNode);

    bool WriteNodeType                  (FbxNode& pNode);
    bool WriteNull                  (FbxNull* pNull);
    bool WriteMarker                (FbxNode& pNode);
    bool WriteSkeleton              (FbxSkeleton& pSkeleton);
    bool WriteSkeletonRoot          (FbxSkeleton& pSkeleton);
    bool WriteSkeletonLimb          (FbxSkeleton& pSkeleton);
    bool WriteSkeletonLimbNode      (FbxSkeleton& pSkeleton);
    bool WriteSkeletonEffector      (FbxSkeleton& pSkeleton);

    bool WriteGeometry              (FbxGeometry& pGeometry);
    bool WriteMesh                  (FbxMesh& pMesh);
    bool WriteMeshSmoothness        (FbxMesh& pMesh);
    bool WriteMeshVertices          (FbxMesh& pMesh);
    bool WriteMeshPolyVertexIndex   (FbxMesh& pMesh);
    bool WriteMeshEdges             (FbxMesh& pMesh);
#ifdef FBXSDK_SUPPORT_INTERNALEDGES
    bool WriteMeshInternalEdges    (FbxMesh& pMesh);
#endif
    bool WriteNurb                  (FbxNurbs& pNurbs);
    bool WriteNurbsSurface          (FbxNurbsSurface& pNurbs);
    bool WriteNurbsCurve            (FbxNurbsCurve& pNurbsCurve);
    bool WriteTrimNurbsSurface      (FbxTrimNurbsSurface& pNurbs);
    bool WriteBoundary              (FbxBoundary& pBoundary);
    bool WriteLine                  (FbxLine& pLine);

    bool WritePatch                 (FbxPatch& pPatch);
    bool WritePatchType             (FbxPatch& pPatch, int pType);

    bool WriteShape                 (FbxShape& pShape);
    void FindShapeValidIndices          (FbxArray<FbxVector4>& pGeometryControlPoints,
                                         FbxArray<FbxVector4>& pShapeControlPoints,
                                         FbxArray<int>& lValidIndices);

    bool WriteFbxLayerElementNormals      (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementBinormals    (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementTangents     (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementMaterials    (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementUVsChannel   (FbxLayerContainer& pLayerContainer, FbxLayerElement::EType pTextureType, FbxMultiMap& pLayerIndexSet);

    bool WriteFbxLayerElementPolygonGroups(FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementVertexColors (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementUVs          (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementSmoothing    (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementUserData     (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementVisibility   (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementEdgeCrease   (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementVertexCrease (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    bool WriteFbxLayerElementHole         (FbxLayerContainer& pLayerContainer, FbxMultiMap&);


    bool WriteLayers                   (FbxLayerContainer& pLayerContainer, FbxMultiMap&);
    //int  MapLayeredMaterialIndexToConnectionIndex(FbxNode* pNode, void* pLEM, int pIndex);
    int  MapLayeredTextureIndexToConnectionIndex(FbxNode* pNode, void* pLET, int pIndex);

    // Write Connections
    bool WriteFieldConnection               (FbxDocument *pDocument, FbxObject* pSrcObject, FbxDocument *pDstDocument);
    bool WriteFieldConnection               (FbxDocument *pDocument, FbxDocument *pChildDocument);
    bool WriteFieldConnection               (FbxDocument *pDocument, FbxObject* pSrc,FbxObject* pDst);
    bool WriteFieldConnection               (FbxDocument *pDocument, FbxObject* pSrc,FbxProperty& pDst);
    bool WriteFieldConnection               (FbxDocument *pDocument, FbxProperty& pSrc,FbxObject* pDst);
    bool WriteFieldConnection               (FbxDocument *pDocument, FbxProperty& pSrc,FbxProperty& pDst);

    void WriteCommentsForConnections        (const char* pSrcName, const char* pDstName);
    void WriteObjectConnections             (FbxDocument *pDocument, FbxObject* pObject);

    bool WriteCamera                    (FbxCamera& pCamera);
    bool WriteCameraStereo              (FbxCameraStereo& pCameraStereo);
    bool WriteLight                     (FbxLight& pLight );
    bool WriteCameraSwitcher            (FbxScene& pScene);                   // To be corrected.
    bool WriteCameraSwitcher            (FbxCameraSwitcher& pCameraSwitcher); // To be corrected.

    bool WriteCollections               (FbxDocument* pDocument);
    bool WriteCollection                (FbxCollection& pImplementation);

    bool WriteDocuments                 (FbxDocument* pDocument);
    bool WriteDocument                  (FbxDocument& pSubDocument);

    bool WriteVideos                    (FbxDocument* pDocument);
    bool WriteVideo                     (FbxVideo& pVideo, FbxString& pFileName, bool pEmbeddedMedia);

    void WritePose                      (FbxPose& pPose);
 
    bool WriteThumbnail                 (FbxThumbnail* pThumbnail);

    void WriteDocumentInfo              (FbxDocumentInfo*);
    void WriteGlobalSettings            (FbxGlobalSettings& pGlobalSettings);

    /*************************** kept functions ***************************/

    // ???
    void WritePassword();

    void WritePropertyTemplate( FbxClassId pClassId );

    void WriteProperty(FbxProperty& pProperty);

    void SetCompressionOptions();

	// IOSettings
	FbxIOSettings * GetIOSettings(){ return mIOSettings; }
	void SetIOSettings(FbxIOSettings * pIOSettings){ mIOSettings = pIOSettings; }

private:
    typedef FbxMap<FbxObject*, FbxDocument*> FbxCollapseMap;
    FbxCollapseMap mCollapsedObjects;

    typedef FbxMap<FbxDocument*, FbxDocument*> FbxExternalDocumentCollapseMap;
    FbxExternalDocumentCollapseMap mExternalDocMapping;

    bool CreateCollapseDocument(FbxDocument* pParentDocument);
    FbxDocument* GetOrCreateCollapedDocument(FbxDocument* pExternalDocument);

    void CollapseExternalObject(FbxObject* pObject, FbxDocument* pCollapseDocument);
    bool CollapseExternalObjectsImpl(FbxDocument* pDocument);
    bool CollapseExternalImplementations(FbxDocument* pCollapseDocument);

    void WriteFileAsBlob(const FbxString& pFileName);
    void WriteFileConsumers(const FbxEmbeddedFilesAccumulator::ObjectPropertyMap& pConsumers);
    void OutputBinaryBuffer(FbxBinaryDataReader& pReader);

    FbxString GetEmbeddedRelativeFilePath(const FbxString& pOriginalPropertyURL, const FbxString& pRootFolder, const FbxString& pAbsoluteFileName);

    // Not all file systems are case sensitive; so here all names must be unique, regardless of CasInG.
    typedef FbxSet<FbxString, FbxStringCompareNoCase> FbxStringNoCaseSet;
    FbxString MakeRelativePathUnique(const FbxString& pRelativeFileName, const FbxStringNoCaseSet& pUsedRelativePaths);

    // Recurse into the children, if any
    void GetEmbeddedPropertyFilter(FbxProperty pProperty, FbxSet<FbxString>& pPropertyFilter);
    void WriteControlPoints(const char* pFieldName, int pCount, const FbxVector4* pControlPoints, const FbxAMatrix& pPivot, bool pOutputWeight = true);

    template <typename T>
    void WriteValueArray(const char* pFieldName, int pCount, const T* pValues)
    {
        if( pCount > 0 )
        {
            mFileObject->FieldWriteBegin( pFieldName );
                WriteValueArray(pCount, pValues);
            mFileObject->FieldWriteEnd();
        }
    }

    void WriteValueArray(int pCount, const double* pValues);
    void WriteValueArray(int pCount, const float* pValues);
    void WriteValueArray(int pCount, const int* pValues);
    void WriteValueArray(int pCount, const bool* pValues);
    void WriteValueArray(int pCount, const FbxUChar* pValues);

    template <typename T>
    void WriteValueArray(const char* pFieldName, FbxLayerElementArrayTemplate<T>& pArray)
    {
        if( pArray.GetCount() > 0 )
        {
            FbxLayerElementArrayReadLock<T>  lData(pArray);
            WriteValueArray(pFieldName, pArray.GetCount(), lData.GetData());
        }
    }

    template <typename T>
    void WriteValueArray(FbxLayerElementArrayTemplate<T>& pArray)
    {
        if( pArray.GetCount() > 0 )
        {
            FbxLayerElementArrayReadLock<T>  lData(pArray);
            WriteValueArray(pArray.GetCount(), lData.GetData());
        }
    }

    template <typename T>
    bool WriteFbxObjects(FbxDocument* pTopDocument, const T* pType = NULL)
    {
        if (mCanceled)
            return false;
        FBX_ASSERT_RETURN_VALUE(pTopDocument, false);

        KReferenceDepthList lObjects;
        CollectObjectsByDepth(pTopDocument, lObjects, pType);

		for( size_t i = 0, c = lObjects.Size(); i < c && !mCanceled; ++i )
        {
            FBX_ASSERT( lObjects[i].mObject );

            T* lObject = FbxCast<T>(lObjects[i].mObject);
            FBX_ASSERT( lObject );

            if( lObject )
            {
				// skip objects that are not savable
				if (!lObject->GetObjectFlags(FbxObject::eSavable))
					continue;

                WriteFbxObject(*lObject);
            }
        }

        return true;
    }

    bool WriteDeformers(FbxDocument* pTopDocument);

    bool WriteFbxObject(FbxSkin& pSkin);
	bool WriteFbxObject(FbxCluster& pCluster);
	bool WriteFbxObject(FbxBlendShape& pBlendShape);
	bool WriteFbxObject(FbxBlendShapeChannel& pBlendShapeChannel);

	bool WriteFbxObject(FbxVertexCacheDeformer& pDeformer);
    bool WriteFbxObject(FbxSurfaceMaterial& pMaterial);
    bool WriteFbxObject(FbxGenericNode& pNode);
    bool WriteFbxObject(FbxImplementation& pImplementation);
    bool WriteFbxObject(FbxCache& pCache);
    bool WriteFbxObject(FbxBindingTable& pTable);
    bool WriteFbxObject(FbxBindingOperator& pOperator);
    bool WriteFbxObject(FbxThumbnail& pThumbnail);
    bool WriteFbxObject(FbxFileTexture& pTexture);
    bool WriteFbxObject(FbxPose& pPose);
    bool WriteFbxObject(FbxLayeredTexture& pTexture);
    bool WriteFbxObject(FbxProceduralTexture& pTexture);
    bool WriteFbxObject(FbxNode& pNode);
    bool WriteFbxObject(FbxNodeAttribute& pNodeAttribute);

    bool WriteFbxObject(FbxAnimCurve& pAnimCurve);
    bool WriteFbxObject(FbxAnimCurveNode& pAnimCurveNode);
    bool WriteFbxObject(FbxAnimLayer& pAnimLayer);
    bool WriteFbxObject(FbxAnimStack& pAnimStack);
	bool WriteFbxObject(FbxAudio& pAudio);
	bool WriteFbxObject(FbxAudioLayer& pAudioLayer);

    bool WriteFbxObject(FbxContainer& pContainer);
    bool WriteFbxObject(FbxSceneReference& pReference);
};

// *******************************************************************************
//  Constructor and file management
// *******************************************************************************
FbxWriterFbx7::FbxWriterFbx7(FbxManager& pManager, FbxExporter& pExporter, int pID, FbxStatus& pStatus) : 
    FbxWriter(pManager, pID, pStatus)
{
#if defined(__GNUC__) && (__GNUC__ < 4)
	mImpl = FbxNew<FbxWriterFbx7_Impl>(pManager, pExporter, (FbxWriterFbx7*)this, pStatus);
#else
	mImpl = FbxNew<FbxWriterFbx7_Impl>(pManager, pExporter, this, pStatus);
#endif
	SetIOSettings(pExporter.GetIOSettings());
	if(mImpl) mImpl->SetIOSettings(pExporter.GetIOSettings());
}

FbxWriterFbx7::FbxWriterFbx7(FbxManager& pManager, FbxExporter& pExporter, EExportMode pMode, int pID, FbxStatus& pStatus) : 
    FbxWriter(pManager, pID, pStatus)
{
#if defined(__GNUC__) && (__GNUC__ < 4)
	mImpl = FbxNew<FbxWriterFbx7_Impl>(pManager, pExporter, (FbxWriterFbx7*)this, pStatus);
#else
	mImpl = FbxNew<FbxWriterFbx7_Impl>(pManager, pExporter, this, pStatus);
#endif
	SetIOSettings(pExporter.GetIOSettings());
	if(mImpl) mImpl->SetIOSettings(pExporter.GetIOSettings());

    SetExportMode(pMode);
}

FbxWriterFbx7::~FbxWriterFbx7()
{
    if(mImpl->mFileObject)
    {
        FileClose();
    }

    FbxDelete(mImpl);
}


bool FbxWriterFbx7::FileCreate(char* pFileName)
{
	int lVersion = FBX_DEFAULT_FILE_VERSION;
	switch( FbxFileVersionStrToInt(mFileVersion) )
	{
		case 201900: lVersion = FBX_FILE_VERSION_7700; break;
		case 201800:
		case 201600: lVersion = FBX_FILE_VERSION_7500; break;
		case 201400: lVersion = FBX_FILE_VERSION_7400; break;
		case 201300: lVersion = FBX_FILE_VERSION_7300; break;
		case 201200: lVersion = FBX_FILE_VERSION_7200; break;
		case 201100: lVersion = FBX_FILE_VERSION_7100; break;
		case -1:
			// PATCH!!!!! just in case mFileVersion is never set.
			mFileVersion = FBX_DEFAULT_FILE_COMPATIBILITY;
			break;
	}

	if( !mImpl->mFileObject )
	{
		mImpl->mFileObject = FbxIO::Create(lVersion >= FBX_FILE_VERSION_7500 ? FbxIO::BinaryLarge : FbxIO::BinaryNormal, GetStatus());
		mImpl->mFileObject->CacheSize(IOS_REF.GetIntProp(EXP_CACHE_SIZE, 8));
	}

	FbxString lFullName = FbxPathUtils::Bind(FbxGetCurrentWorkPath(), pFileName);

	mImpl->mFileObject->Fbx7Support(true);

	if( !mImpl->mFileObject->ProjectCreateEmpty(lFullName, this, lVersion, mImpl->mExportMode == eBINARY || mImpl->mExportMode == eENCRYPTED, mImpl->mExportMode == eENCRYPTED) )
	{
		return false;
	}

	return true;
}

bool FbxWriterFbx7::FileCreate(FbxStream* pStream, void* pStreamData)
{
	int lVersion = FBX_DEFAULT_FILE_VERSION;
	switch( FbxFileVersionStrToInt(mFileVersion) )
	{
		case 201900: lVersion = FBX_FILE_VERSION_7700; break;
		case 201800:
		case 201600: lVersion = FBX_FILE_VERSION_7500; break;
		case 201400: lVersion = FBX_FILE_VERSION_7400; break;
		case 201300: lVersion = FBX_FILE_VERSION_7300; break;
		case 201200: lVersion = FBX_FILE_VERSION_7200; break;
		case 201100: lVersion = FBX_FILE_VERSION_7100; break;
		case -1:
			// PATCH!!!!! just in case mFileVersion is never set.
			mFileVersion = FBX_DEFAULT_FILE_COMPATIBILITY;
			break;
    }

    if( !mImpl->mFileObject )
    {
        mImpl->mFileObject = FbxIO::Create(lVersion >= FBX_FILE_VERSION_7500 ? FbxIO::BinaryLarge : FbxIO::BinaryNormal, GetStatus());
        mImpl->mFileObject->CacheSize(IOS_REF.GetIntProp(EXP_CACHE_SIZE, 8));
    }

    mImpl->mFileObject->Fbx7Support(true);

    if( !mImpl->mFileObject->ProjectCreateEmpty(pStream, pStreamData, this, lVersion, mImpl->mExportMode == eBINARY || mImpl->mExportMode == eENCRYPTED, mImpl->mExportMode == eENCRYPTED))
    {
        return false;
    }

    return true;
}

bool FbxWriterFbx7::FileClose()
{
    if(!mImpl->mFileObject)
    {
        GetStatus().SetCode(FbxStatus::eFailure, "File not opened");
        return false;
    }

    if(!mImpl->mFileObject->ProjectClose())
    {
        FBX_SAFE_DELETE(mImpl->mFileObject);
        return false;
    }
    else
    {
        FBX_SAFE_DELETE(mImpl->mFileObject);
        return true;
    }
}

bool FbxWriterFbx7::IsFileOpen()
{
    return mImpl->mFileObject != NULL;
}


void FbxWriterFbx7::SetExportMode(EExportMode pMode)
{
    mImpl->mExportMode = pMode;
}


void FbxWriterFbx7::GetWriteOptions()
{

	IOS_REF.SetBoolProp(EXP_FBX_EMBEDDED,           true);
    IOS_REF.SetBoolProp(EXP_FBX_COLLAPSE_EXTERNALS, true);
    IOS_REF.SetBoolProp(EXP_FBX_COMPRESS_ARRAYS,    true);

    const char* lInvalidPropertyNames[] =
    {
        "thumbnail",
        "DescAbsoluteURL",
        "DescRelativeURL",
        NULL
    };

    FbxProperty lPropParent = IOS_REF.GetProperty(EXP_FBX_EMBEDDED_PROPERTIES_SKIP);
	lPropParent.DestroyChildren();

    int iCounter = 0;

    for( const char** lPropertyName = lInvalidPropertyNames; *lPropertyName; ++lPropertyName, ++iCounter )
    {
        char szName[12];

        FBXSDK_sprintf(szName, 12, "#%d", iCounter);

        FbxProperty lSkipProperty = FbxProperty::Create(lPropParent, FbxStringDT, szName);
        lSkipProperty.Set(*lPropertyName);
    }

}

// *******************************************************************************
//  Writing General functions
// *******************************************************************************
bool FbxWriterFbx7::Write(FbxDocument* pDocument)
{
    if(!pDocument)
    {
        GetStatus().SetCode(FbxStatus::eFailure, "Invalid document handle");
        return false;
    }

    // Did we fail during creation?
    if( GetStatus().Error())
    {
        return false;
    }

	// note: sprintf() use the locale to find the decimal separator
	// French, Italian, German, ... use the comma as decimal separator
	// so we need a way to be un-localized into writing/reading our files formats

	// force usage of a period as decimal separator
	char lPrevious_Locale_LCNUMERIC[100]; memset(lPrevious_Locale_LCNUMERIC, 0, 100);
	FBXSDK_strcpy(lPrevious_Locale_LCNUMERIC, 100, setlocale(LC_NUMERIC, 0  ));	// query current setting for LC_NUMERIC
	char *lCurrent_Locale_LCNUMERIC  = setlocale(LC_NUMERIC, "C");  // set locale using period as decimal separator

    mImpl->mSceneExport = FbxCast<FbxScene>(pDocument);
    mImpl->mTopDocument = pDocument;

#ifndef FBXSDK_ENV_WINSTORE
    // This should really go into PreprocessScene,
    // but it really needs to be updated to PreprocessDocument
    // since we could be saving a library
    FbxEventPreExport lPreEvent( pDocument );
    pDocument->GetFbxManager()->EmitPluginsEvent(lPreEvent);
#endif

    if( mImpl->mSceneExport )
    {
        PreprocessScene(*mImpl->mSceneExport);
	#ifndef FBXSDK_ENV_WINSTORE
		PluginsWriteBegin(*mImpl->mSceneExport);
	#endif
    }

	bool lResult = Write(pDocument, NULL);

    if( mImpl->mSceneExport )
    {
	#ifndef	FBXSDK_ENV_WINSTORE
		PluginsWriteEnd(*mImpl->mSceneExport);
	#endif
        PostprocessScene(*mImpl->mSceneExport);
    }

#ifndef FBXSDK_ENV_WINSTORE
    // See comment for the pre export event above.
    FbxEventPostExport lPostEvent( pDocument );
    pDocument->GetFbxManager()->EmitPluginsEvent(lPostEvent);
#endif

    mImpl->mSceneExport = NULL;
    mImpl->mTopDocument = NULL;

	// set numeric locale back
	setlocale(LC_NUMERIC, lPrevious_Locale_LCNUMERIC);

    return lResult;
}

void FbxWriterFbx7::PluginWriteParameters(FbxObject& pParams)
{
	mImpl->WriteObjectPropertiesAndFlags(&pParams);
}

void FbxWriterFbx7::StoreUnsupportedProperty(FbxObject* pObject, FbxProperty& pProperty)
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

void FbxWriterFbx7::MakeNonSavableAndRemember(FbxObject* pObj)
{
	if (!pObj)
		return;

	if (pObj->GetObjectFlags(FbxObject::eSavable))
	{
		pObj->SetObjectFlags(FbxObject::eSavable, false);
		mSwitchedToNonSavablesObjects.Add(pObj);
	}
}

bool FbxWriterFbx7::PreprocessScene(FbxScene &pScene)
{
	FBX_ASSERT(mModifiedProperties.GetCount() == 0);
    int lVersion = FbxFileVersionStrToInt(mFileVersion);
    FBX_ASSERT(lVersion > 0);

	if( lVersion > 0 && lVersion <= 201100 )
	{
		mImpl->ConvertShapePropertyToOldStyle(pScene);

		FbxObject* lObject = NULL;
		for( int i = 0, lCount = pScene.GetSrcObjectCount(); i < lCount; ++i )
		{
			lObject = pScene.GetSrcObject(i);

			//Mark these objects as not savable so they don't get saved
			if( lObject->Is<FbxBlendShape>() ||
				lObject->Is<FbxBlendShapeChannel>() ||
				lObject->Is<FbxShape>() ||
				lObject->Is<FbxLine>() ||
				lObject->Is<FbxProceduralTexture>() )
			{
				lObject->SetObjectFlags(FbxObject::eSavable, false);
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

			if( lObject->Is<FbxLight>() )
			{
				FbxLight* lLight = FbxCast<FbxLight>(lObject);

				//Area lights not supported before 2012, exported as point lights
				if( lLight->LightType.Get() == FbxLight::eArea )
				{
					lLight->LightType.Set(FbxLight::ePoint);
					gAreaLights.Add(lLight);
				}
				StoreUnsupportedProperty(lObject, lLight->AreaLightShape);

				//Barn Doors were added for FBX 2012 and above
				StoreUnsupportedProperty(lObject, lLight->LeftBarnDoor);
				StoreUnsupportedProperty(lObject, lLight->RightBarnDoor);
				StoreUnsupportedProperty(lObject, lLight->TopBarnDoor);
				StoreUnsupportedProperty(lObject, lLight->BottomBarnDoor);
				StoreUnsupportedProperty(lObject, lLight->EnableBarnDoor);
			}
		}
	}

	if( lVersion > 0 && lVersion <= 201200 )
	{
		for( int i = 0, lCount = pScene.GetSrcObjectCount<FbxLight>(); i < lCount; i++ )
		{
			FbxLight* lLight = pScene.GetSrcObject<FbxLight>(i);
			if( lLight )
			{
				//In file version 7.3, HotSpot became InnerAngle
				FbxProperty lOldHotSpot = FbxProperty::Create(lLight, FbxDoubleDT, "HotSpot");
				if( lOldHotSpot.IsValid() )
				{
					lOldHotSpot.ModifyFlag(FbxPropertyFlags::eAnimatable, true);
					lOldHotSpot.CopyValue(lLight->InnerAngle);
                    FbxAnimUtilities::ShareAnimCurves(lOldHotSpot, lLight->InnerAngle, &pScene);
					StoreUnsupportedProperty(lLight, lLight->InnerAngle);
				}

				//In file version 7.3, ConeAngle became OuterAngle
				FbxProperty lOldConeAngle = FbxProperty::Create(lLight, FbxDoubleDT, "Cone angle");
				if( lOldConeAngle.IsValid() )
				{
					lOldConeAngle.ModifyFlag(FbxPropertyFlags::eAnimatable, true);
					lOldConeAngle.CopyValue(lLight->OuterAngle);
                    FbxAnimUtilities::ShareAnimCurves(lOldConeAngle, lLight->OuterAngle, &pScene);
					StoreUnsupportedProperty(lLight, lLight->OuterAngle);
				}
			}
		}

		for (int c = 0, lCount = pScene.GetSrcObjectCount<FbxCharacter>(); c < lCount; c++)
		{
			FbxCharacter* lCharacter = pScene.GetSrcObject<FbxCharacter>(c);
			if ( lCharacter )
			{
				#define StoreProperty(propName,linktoo) \
					{ \
						FbxProperty lProp = lCharacter->FindProperty(propName); \
						if (lProp.IsValid()) StoreUnsupportedProperty(lCharacter, lProp); \
						if (linktoo) \
						{ \
							FbxString linkName = FbxString(propName) + "Link"; \
							FbxProperty lLinkProp = lCharacter->FindProperty(linkName.Buffer()); \
							if (lLinkProp.IsValid()) StoreUnsupportedProperty(lCharacter, lLinkProp); \
						} \
					}

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

				lCharacter->SetValuesForLegacySave(0);
			}
		}
	}

	if( lVersion > 0 && lVersion <= 201400 )
	{
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
	}

	if( lVersion > 0 && lVersion <= 201600 )
	{
		// Audio and AudioLayers can only be saved from version 201800 (201700 does not exist)
		FbxObject* lObject = NULL;
		for( int i = 0, lCount = pScene.GetSrcObjectCount(); i < lCount; ++i )
		{
			lObject = pScene.GetSrcObject(i);

			//Mark these objects as not savable so they don't get saved
			if( lObject->Is<FbxAudio>() ||
				lObject->Is<FbxAudioLayer>())
			{
				lObject->SetObjectFlags(FbxObject::eSavable, false);
			}
		}
	}

	// Disable writing of Animation and/or audio data based on the IOS_REF flags
	if (!IOS_REF.GetBoolProp(EXP_FBX_AUDIO, true)) 
	{
		for (int i = 0, lCount = pScene.GetSrcObjectCount<FbxAudioLayer>(); i < lCount; ++i) MakeNonSavableAndRemember(pScene.GetSrcObject<FbxAudioLayer>(i));
		for (int i = 0, lCount = pScene.GetSrcObjectCount<FbxAudio>()     ; i < lCount; ++i) MakeNonSavableAndRemember(pScene.GetSrcObject<FbxAudio>(i));
	}

	if (!IOS_REF.GetBoolProp(EXP_FBX_ANIMATION, true))
	{
		for (int i = 0, lCount = pScene.GetSrcObjectCount<FbxAnimStack>()    ; i < lCount; ++i) MakeNonSavableAndRemember(pScene.GetSrcObject<FbxAnimStack>(i));
		for (int i = 0, lCount = pScene.GetSrcObjectCount<FbxAnimLayer>()    ; i < lCount; ++i) MakeNonSavableAndRemember(pScene.GetSrcObject<FbxAnimLayer>(i));
		for (int i = 0, lCount = pScene.GetSrcObjectCount<FbxAnimCurveNode>(); i < lCount; ++i) MakeNonSavableAndRemember(pScene.GetSrcObject<FbxAnimCurveNode>(i));
		for (int i = 0, lCount = pScene.GetSrcObjectCount<FbxAnimCurve>()    ; i < lCount; ++i) MakeNonSavableAndRemember(pScene.GetSrcObject<FbxAnimCurve>(i));
	}

	// Now, test if we need to enable the AnimStack (and consequently, make sure we have one AnimLayer)
	for (int i = 0; i < pScene.GetSrcObjectCount<FbxAnimStack>(); i++)
	{
		FbxAnimStack* lStack = pScene.GetSrcObject<FbxAnimStack>(i);

		// if this stack was made non-savable by the loop above, maybe it needs to be reverted to 'savable'
		int lPos = mSwitchedToNonSavablesObjects.Find(lStack);
		if (lPos != -1 && IOS_REF.GetBoolProp(EXP_FBX_AUDIO, true) && lStack->GetMemberCount<FbxAudioLayer>() > 0)
		{
			// we have audio data that needs to be written out, so switch the 'savable' state back!			
			lStack->SetObjectFlags(FbxObject::eSavable, true);
			mSwitchedToNonSavablesObjects.RemoveAt(lPos);
		}
		
		if (lStack->GetObjectFlags(FbxObject::eSavable))
		{
			// Now we need to make sure that we have at least one AnimLayer (by definition, an AnimStack cannot be
			// without an AnimLayer).
			// We only check for first layer:
			//   1) if there are none, we create a new one as the new base (1st connection)
			//   2) If it is non savable "by the user", we create a new one as the new base (1st connection)
			//   3) If it is non savable "by the loop above", we make it savable and use it
			//   All the other layers are left as is
			FbxAnimLayer* lLayer = lStack->GetMember<FbxAnimLayer>();
			lPos = mSwitchedToNonSavablesObjects.Find(lLayer);
			if (lPos >= 0)
			{
				// case 3)
				lLayer->SetObjectFlags(FbxObject::eSavable, true);
				mSwitchedToNonSavablesObjects.RemoveAt(lPos);
			}
			else
			if (!lLayer || !lLayer->GetObjectFlags(FbxObject::eSavable))
			{
				// case 1 and 2)
				lLayer = FbxAnimLayer::Create(&pScene, "Base Layer");
				mAnimLayerInternallyAdded.Add(lLayer);
				
				// Insert the new layer at the beginning
				FbxArray<FbxAnimLayer*> lList;
				for (int j = 0, c = lStack->GetMemberCount<FbxAnimLayer>(); j < c; j++)
					lList.Add(lStack->GetMember<FbxAnimLayer>(j));
				
				for (int j = 0, c = lList.GetCount(); j < c; j++) lStack->RemoveMember(lList[j]);
				lStack->AddMember(lLayer);
				for (int j = 0, c = lList.GetCount(); j < c; j++) lStack->AddMember(lList[j]);
			}
		}
	}

    return false;
}

bool FbxWriterFbx7::PostprocessScene(FbxScene &pScene)
{
    int lVersion = FbxFileVersionStrToInt(mFileVersion);
    FBX_ASSERT(lVersion > 0);
	if(lVersion > 0 && lVersion < 201200 )
	{
		FbxObject* lObject = NULL;
		for( int i = 0, c = pScene.GetSrcObjectCount(); i < c; ++i )
		{
			lObject = pScene.GetSrcObject(i);

			//Clear not savable flag on these objects to restore their original state
			if( lObject->Is<FbxBlendShape>() ||
				lObject->Is<FbxBlendShapeChannel>() ||
				lObject->Is<FbxShape>() ||
				lObject->Is<FbxLine>() ||
				lObject->Is<FbxProceduralTexture>() )
			{
				lObject->SetObjectFlags(FbxObject::eSavable, true);
			}
		}

		//Area lights not supported before 2012, exported as point lights, so revert them to area lights
		for( int i = 0, lCount = gAreaLights.GetCount(); i < lCount; i++ )
		{
			FbxLight* lLight = gAreaLights[i];
			lLight->LightType.Set(FbxLight::eArea);
		}
		gAreaLights.Clear();
	}

	if(lVersion > 0 && lVersion < 201300 )
	{
		for( int i = 0, lCount = pScene.GetSrcObjectCount<FbxLight>(); i < lCount; i++ )
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

		for (int c = 0, lCount = pScene.GetSrcObjectCount<FbxCharacter>(); c < lCount; c++)
		{
			FbxCharacter* lCharacter = pScene.GetSrcObject<FbxCharacter>(c);
			if ( lCharacter )
			{
				lCharacter->RestoreValuesFromLegacySave();
			}
		}
	}

	if( lVersion > 0 && lVersion <= 201600 )
	{
		// Audio and AudioLayers can only be saved from version 201700
		FbxObject* lObject = NULL;
		for( int i = 0, lCount = pScene.GetSrcObjectCount(); i < lCount; ++i )
		{
			lObject = pScene.GetSrcObject(i);

			//Mark these objects as savable so they can get saved on a future save iteration
			if( lObject->Is<FbxAudio>() ||
				lObject->Is<FbxAudioLayer>())
			{
				lObject->SetObjectFlags(FbxObject::eSavable, true);
			}
		}
	}

	// Restore Audio and Animation objects that we switched to unsavable back to their savable state
	for (int i = 0, c = mSwitchedToNonSavablesObjects.GetCount(); i < c; ++i)
	{
		FbxObject* lObj = mSwitchedToNonSavablesObjects.GetAt(i);
		lObj->SetObjectFlags(FbxObject::eSavable, true);
	}
	mSwitchedToNonSavablesObjects.Clear();

	// Destroy the internally allocated AnimLayers because one was missing in the AnimStack
	for (int i = 0, c = mAnimLayerInternallyAdded.GetCount(); i < c; ++i)
	{
		FbxObject* lObj = mAnimLayerInternallyAdded.GetAt(i);
		lObj->Destroy();
	}
	mAnimLayerInternallyAdded.Clear();

	//Restore unsupported properties to their original state
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

    return false;
}

void FbxWriterFbx7::SetProgressHandler(FbxProgress *pProgress)
{
    mImpl->mProgress = pProgress;
}

void FbxWriterFbx7_Impl::ConvertShapePropertyToOldStyle(FbxScene& pScene)
{
	FbxAnimStack* lAnimStack = pScene.GetMember<FbxAnimStack>();
	if(lAnimStack)
	{
		FbxAnimLayer* lAnimLayer = lAnimStack->GetMember<FbxAnimLayer>();

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
						FbxAnimCurveNode* lShapeCurveNode = lBlendShapeChannel->DeformPercent.GetCurveNode(lAnimLayer, false);
						FbxAnimCurve* lShapeCurve = lBlendShapeChannel->DeformPercent.GetCurve(lAnimLayer, false);
						//For FBX file before 7200, there is only one shape on each blend shape channel.
						FbxShape* lShape = lBlendShapeChannel->GetTargetShape(0);
						if( lShape )
						{
							FbxString lShapeName = lShape->GetName();
							lGeometry->CreateShapeChannelProperties(lShapeName);

							FbxProperty lProp = lGeometry->FindProperty(lShapeName);
							if (lProp.IsValid())
							{
								lProp.CopyValue(lBlendShapeChannel->DeformPercent);
								if(lShapeCurve)
								{
									FbxAnimCurve* lShapeCurveNew = lProp.GetCurve(lAnimLayer, true);
									if(lShapeCurveNew)
									{
										lShapeCurveNew->CopyFrom(*lShapeCurve);
									}

									lShapeCurve->Destroy();
								}
							}// If shape deform property exist
						}// If lShape is valid
						//Delete all DeformPercent property and their animcurve on all blend shape channel.
						if(lShapeCurveNode)
						{
							lShapeCurveNode->Destroy();
						}

						lBlendShapeChannel->DeformPercent.Destroy();
					}//If lBlendShapeChannel is valid
				}//For each blend shape channel
			}// If we have at least one blend shape deformer on this geometry.
		}//For each geometry
	}//If animstack is valid

}

bool FbxWriterFbx7_Impl::WriteFbxHeader(FbxDocument* pDocument)
{
    FBX_ASSERT( pDocument );

    if( mFileObject->ProjectWrite_BeginFileHeader() )
    {
        FbxIOFileHeaderInfo lFileHeaderInfo;
    
        // Todo: fill the mDefaultRenderResolution for the Quicktime viewer
        lFileHeaderInfo.mDefaultRenderResolution.mIsOK  = false;
        lFileHeaderInfo.mCreationTimeStampPresent       = false;
        lFileHeaderInfo.mFileVersion                    = FBX_DEFAULT_FILE_VERSION;
    
        mFileObject->ProjectWrite_BeginExtendedHeader();
        {           
            mFileObject->ProjectWrite_WriteExtendedHeader(&lFileHeaderInfo);
        
            FbxDocumentInfo* lDocInfo = pDocument->GetDocumentInfo();
        
            if( lDocInfo )
            {
                WriteDocumentInfo(lDocInfo);
            }       
        }
        mFileObject->ProjectWrite_EndExtendedHeader();
        mFileObject->ProjectWrite_EndFileHeader();

        // for V7.1 and later files, the GlobalSettings should in the header section
        int maj, min, rev;
        FbxIO::ProjectConvertVersionNumber(mFileObject->ProjectGetSectionVersion(FBX_MAIN_SECTION), maj, min, rev);    
        FBX_ASSERT(maj == 7 && min >= 0);

        if (min > 0 || rev > 0)
        {
            WriteGlobalSettings(pDocument);
        } 

        if( !mStatus.Error() )
        {
            return true;
        }
    }

    if (mStatus.Error())
        mStatus.SetCode(FbxStatus::eFailure, "File is corrupted (or invalid)");

    return false;
}

void FbxWriterFbx7_Impl::SetCompressionOptions()
{
    mFileObject->CompressArrays(IOS_REF.GetBoolProp(EXP_FBX_COMPRESS_ARRAYS, true));
 
    mFileObject->CompressMinimumSize(IOS_REF.GetIntProp(EXP_FBX_COMPRESS_MINSIZE,1024));
    
    mFileObject->CompressLevel( IOS_REF.GetIntProp(EXP_FBX_COMPRESS_LEVEL,1) );
}

bool FbxWriterFbx7::Write(FbxDocument* pDocument, FbxIO* pFbx)
{
    if(!pDocument)
    {
        mImpl->mStatus.SetCode(FbxStatus::eFailure, "Invalid document handle");
        return false;
    }

    mImpl->mSceneExport = FbxCast<FbxScene>(pDocument);
    mImpl->mTopDocument = pDocument;

    FbxIO*   lInternalFbx = NULL;
    int lMediaCount = 0;
    bool lResult = true;

    if(pFbx)
    {
        lInternalFbx = mImpl->mFileObject;
        mImpl->mFileObject = pFbx;
    }
    else if(!mImpl->mFileObject)
    {
        mImpl->mStatus.SetCode(FbxStatus::eFailure, "File not created");
        lResult = false;
    }


    mImpl->SetCompressionOptions();

    lResult = lResult && mImpl->WriteFbxHeader(pDocument);

    bool lCollapseObjects = IOS_REF.GetBoolProp(EXP_FBX_COLLAPSE_EXTERNALS, true);

    {
        // Make sure that if we collapsed some objects, they get restored no matter how we
        // get out of this method.  
        FbxWriterFbx7_Impl::FbxObjectCollapserUndo lUndoCollapse(mImpl, lCollapseObjects);
    
        if( lResult && lCollapseObjects )
        {
            lResult = mImpl->CollapseExternalObjects(pDocument);
        }

        //
        // Pre-processing
        //
        if(mImpl->mSceneExport)
        {
            mImpl->mSceneExport->ConnectMaterials();
            mImpl->mSceneExport->ConnectTextures();
        }
    
        pDocument->ConnectVideos();
    
        //
        // Definition section
        //
        if(lResult)
        {
            lResult = mImpl->WriteDocumentsSection(pDocument);
        }
    
        mImpl->mDocumentReferences = FbxNew< KTypeWriteReferences >();
    
        //
        // Reference section
        //
        if(lResult)
        {
            lResult = mImpl->WriteReferenceSection(pDocument, *mImpl->mDocumentReferences);
        }
    
        //
        // Object definition Section
        //
        if(lResult)
        {
            mImpl->WriteObjectDefinition(pDocument);
            if(mImpl->mStatus.Error())
            {
                mImpl->mStatus.SetCode(FbxStatus::eFailure, "out of disk space");
                lResult = false;
            }
        }
    
        //
        // Object properties
        //
        if(lResult)
        {
            mImpl->WriteObjectProperties(pDocument);
    
            if(mImpl->mStatus.Error())
            {
                mImpl->mStatus.SetCode(FbxStatus::eFailure, "out of disk space");
                lResult = false;
            }
            if (mImpl->mCanceled)
            {
                lResult = false;
            }
        }
    
        mImpl->WritePassword();
    
        //
        // Object connections
        //
        if(lResult)
        {
            mImpl->WriteObjectConnections(pDocument);
    
            if(mImpl->mStatus.Error())
            {
                mImpl->mStatus.SetCode(FbxStatus::eFailure, "out of disk space");
                lResult = false;
            }
        }
    
        //
        // Takes
        //
		if(IOS_REF.GetBoolProp(EXP_FBX_ANIMATION, true))
		{
			if(lResult)
			{
				mImpl->WriteTakes(pDocument);
	    
				if(mImpl->mStatus.Error())
				{
					mImpl->mStatus.SetCode(FbxStatus::eFailure, "out of disk space");
					lResult = false;
	         
				}
			}
		}
    
        //
        // Post processing
        //
        if(pFbx)
        {
            mImpl->mFileObject = lInternalFbx;
        }
        else
        {
            mImpl->mFileObject->ProjectCloseSection();
        }
    
        // If any external objects were collapsed into our document, replace the originals.
        // automatically with the lUndoCollapse destructor
    }

    return lResult;
}

bool FbxWriterFbx7_Impl::WriteObjectHeaderAndReferenceIfAny(FbxObject& pObj, const char* pObjectType) const
{
    if(pObjectType == NULL)
    {
        return false;
    }

    return WriteObjectHeaderAndReferenceIfAny(pObj, pObjectType, pObj.GetTypeName());
}

bool FbxWriterFbx7_Impl::WriteObjectHeaderAndReferenceIfAny(FbxObject& pObj, const char* pObjectType, const char* pObjectSubType) const
{
    if(!pObjectType || !pObjectSubType)
    {
        return false;
    }

    FbxObject* lReferencedObject = pObj.GetReferenceTo();
    FbxString     lObjName = pObj.GetNameWithNameSpacePrefix();

    mFileObject->FieldWriteBegin(pObjectType);
    mFileObject->FieldWriteLL( GetObjectId(&pObj) );
    mFileObject->FieldWriteC(lObjName);
    mFileObject->FieldWriteC(pObjectSubType);

    if(lReferencedObject != NULL)
    {
        FbxString lRefName;

        if( mDocumentReferences && mDocumentReferences->GetReferenceName(lReferencedObject, lRefName))
        {
            mFileObject->FieldWriteC(FIELD_KFBXOBJECT_REFERENCE_TO);
            mFileObject->FieldWriteC(lRefName);
        }
        else
        {
            return false;
        }
    }

    if( pObj.GetDocument() != mTopDocument )
    {
        mFileObject->FieldWriteC(FIELD_KFBXOBJECT_DOCUMENT);
        mFileObject->FieldWriteLL(GetObjectId(pObj.GetDocument()));
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteGlobalSettings(FbxDocument* pDocument)
{
    bool lResult = true;
    FbxScene* lScene = FbxCast<FbxScene>(pDocument);
    FBX_ASSERT(lScene == mSceneExport);

    if (lScene)
    {
        WriteGlobalSettings(lScene->GetGlobalSettings());
    }
    return lResult;
}

void FbxWriterFbx7_Impl::WritePassword()
{
    if( IOS_REF.GetBoolProp(EXP_FBX_PASSWORD_ENABLE, true) && !IOS_REF.GetStringProp(EXP_FBX_PASSWORD, FbxString("")).IsEmpty() )
    {
        FbxString lPwd = IOS_REF.GetStringProp(EXP_FBX_PASSWORD, FbxString(""));
        mFileObject->WritePassword(lPwd);
    }
}

bool FbxWriterFbx7_Impl::WriteThumbnail(FbxThumbnail* pThumbnail)
{
    if(pThumbnail->GetSize() != FbxThumbnail::eNotSet)
    {
        // This is a non-empty thumbnail, so save it
        FbxUChar* lImagePtr   = pThumbnail->GetThumbnailImage();
        unsigned long lSize = pThumbnail->GetSizeInBytes();

        mFileObject->FieldWriteBegin(FIELD_THUMBNAIL);
        mFileObject->FieldWriteBlockBegin();

            mFileObject->FieldWriteI(FIELD_THUMBNAIL_VERSION, 100);
            mFileObject->FieldWriteI(FIELD_THUMBNAIL_FORMAT, pThumbnail->GetDataFormat());
            mFileObject->FieldWriteI(FIELD_THUMBNAIL_SIZE,   pThumbnail->GetSize());
    
            // hard code an encoding of "0" for "raw data". In future version, encoding
            // will indicate if the file is stored with OpenEXR or RAW.
            mFileObject->FieldWriteI(FIELD_THUMBNAIL_ENCODING, 0);
    
            mFileObject->FieldWriteBegin(FIELD_THUMBNAIL_IMAGE);
                mFileObject->FieldWriteArrayBytes(lSize, lImagePtr);
            mFileObject->FieldWriteEnd();
    
            WriteObjectPropertiesAndFlags(pThumbnail);

        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    return true;
}

void FbxWriterFbx7_Impl::WriteDocumentInfo(FbxDocumentInfo* pSceneInfo)
{
    if(!pSceneInfo) return;

    mFileObject->FieldWriteBegin(FIELD_SCENEINFO);
    mFileObject->FieldWriteC("SceneInfo::GlobalInfo");
    mFileObject->FieldWriteC("UserData");
    {
        mFileObject->FieldWriteBlockBegin();
        {
            mFileObject->FieldWriteS(FIELD_SCENEINFO_TYPE,"UserData");
            mFileObject->FieldWriteI(FIELD_SCENEINFO_VERSION, 100);

            // Thumbnail
            if(pSceneInfo->GetSceneThumbnail())
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

void FbxWriterFbx7_Impl::WriteGlobalSettings(FbxGlobalSettings& pGlobalSettings)
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
//  Definitions
// *******************************************************************************
void FbxWriterFbx7_Impl::BuildObjectDefinition(FbxDocument* pTopDocument)
{
    FBX_ASSERT_RETURN(pTopDocument );

    // We do not need to check for root nodes if the document is not a scene.
    FbxScene*  lScene = FbxCast<FbxScene>(pTopDocument);
	FbxObject* lRoot  = lScene ? lScene->GetRootNode() : NULL;

	for( int i = 0, n = pTopDocument->GetSrcObjectCount(); i < n; ++i )
	{
		FbxObject* lFbxObject = pTopDocument->GetSrcObject(i);
		if( lFbxObject != lRoot && lFbxObject->GetObjectFlags(FbxObject::eSavable) )
		{
			FbxDocument* lSubDocument = FbxCast<FbxDocument>(lFbxObject);

			if( lSubDocument )
			{
				BuildObjectDefinition(lSubDocument);
			}
			else
			{
				// We want to separate the characters from the other constraints :-(
				FbxConstraint* lConstraint = FbxCast<FbxConstraint>(lFbxObject);
				if (lConstraint && 
					lConstraint->GetConstraintType() == FbxConstraint::eCharacter && 
					IOS_REF.GetBoolProp(EXP_FBX_CHARACTER, true))
				{
					FbxClassId lClassId = lFbxObject->GetRuntimeClassId();
					FbxString     lTypeName = lClassId.GetFbxFileTypeName(false);                        
					mTypeDefinitions.AddObject(TOKEN_KFBXCONSTRAINT_CHARACTER, lClassId, lFbxObject->IsRuntimePlug());
				}
				else
					mTypeDefinitions.AddObject(lFbxObject);
			}			
		}//If object is not roor or system object
	}//For all objects
}

void FbxWriterFbx7_Impl::SetObjectWriteSupport()
{
    int lDefinitionCount = mTypeDefinitions.GetDefinitionCount();

    // Iterates through the definitions to see if they are
    // supported by the new save mechanism
    for(int i = 0; i < lDefinitionCount; i++)
    {
        KTypeDefinitionInfo* lDef = mTypeDefinitions.GetDefinition(i);

        if (    ( lDef->mClassId.Is(FbxCollectionExclusive::ClassId) )
            )
        {
            lDef->SetGenericWriteEnabled(true);
        }
        // Those class ids are not supported by the new writing mechanism
        else if(    ( lDef->mClassId.Is(FbxSurfaceMaterial::ClassId) )
               || ( lDef->mClassId.Is(FbxVideo::ClassId) )
               || ( lDef->mClassId.Is(FbxTexture::ClassId) )
               || ( lDef->mClassId.Is(FbxImplementation::ClassId) )
               || ( lDef->mClassId.Is(FbxBindingTable::ClassId) )
               || ( lDef->mClassId.Is(FbxBindingOperator::ClassId) )
               || ( lDef->mClassId.Is(FbxFileTexture::ClassId) )
               || ( lDef->mClassId.Is(FbxLayeredTexture::ClassId) )
               || ( lDef->mClassId.Is(FbxProceduralTexture::ClassId) )
               || ( lDef->mClassId.Is(FbxCollection::ClassId) )
               || ( lDef->mClassId.Is(FbxSelectionNode::ClassId) )
               || ( lDef->mClassId.Is(FbxSelectionSet::ClassId) )
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
               || ( lDef->mClassId.Is(FbxBlendShape::ClassId) )
			   || ( lDef->mClassId.Is(FbxBlendShapeChannel::ClassId) )
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
               || ( lDef->mClassId.Is(FbxGlobalSettings::ClassId) )
			   || ( lDef->mClassId.Is(FbxDocumentInfo::ClassId) )
               || ( lDef->mClassId.Is(FbxAnimCurve::ClassId) )
               || ( lDef->mClassId.Is(FbxAnimCurveNode::ClassId) )
               || ( lDef->mClassId.Is(FbxAnimLayer::ClassId) )
               || ( lDef->mClassId.Is(FbxAnimStack::ClassId) )
			   || ( lDef->mClassId.Is(FbxAudio::ClassId) )
			   || ( lDef->mClassId.Is(FbxAudioLayer::ClassId) )
               || ( lDef->mClassId.Is(FbxContainer::ClassId) )
               || ( lDef->mClassId.Is(FbxSceneReference::ClassId) )
          )
        {
            lDef->SetGenericWriteEnabled(false);
        }

        // These have typically a count of a few objects, there's no point in
        // storing templates for those.
        // 
        // We never ever ever ever want to use a template for the document info,
        // since it would mean we couldn't read the header of the file without
        // also parsing the template section.

		if(     lDef->mClassId.Is(FbxDocumentInfo::ClassId)
			||  lDef->mClassId.Is(FbxGlobalSettings::ClassId) 
			||  lDef->mClassId.Is(FbxDocument::ClassId)
          )
        {
            lDef->SetPropertyTemplateEnabled(false);
        }
    }
}

void FbxWriterFbx7_Impl::CollectDocumentHiearchy(FbxDocumentList& pDocuments, FbxDocument* pDocument)
{
	FBX_ASSERT( pDocument );
	int lIndex = pDocuments.Find(pDocument);
	if( lIndex == -1 )
	{
		pDocuments.Add(pDocument);
		for(int i = 0, n = pDocument->GetMemberCount<FbxDocument>(); i < n; ++i )
		{
			FbxDocument* lSubDocument = pDocument->GetMember<FbxDocument>(i);
			FBX_ASSERT( lSubDocument );

			if( lSubDocument )
			{
				CollectDocumentHiearchy(pDocuments, lSubDocument);
			}
		}
	}
}

bool FbxWriterFbx7_Impl::WriteDocumentsSection(FbxDocument* pTopDocument)
{
    FBX_ASSERT_RETURN_VALUE(pTopDocument, false);

    bool lResult = true;

    FbxWriterFbx7_Impl::FbxDocumentList    lDocuments;
    CollectDocumentHiearchy(lDocuments, pTopDocument);

    FBX_ASSERT_RETURN_VALUE(lDocuments.Size() > 0, false);

    //Write Document description
    mFileObject->WriteComments("");
    mFileObject->WriteComments(" Documents Description");
    mFileObject->WriteComments("------------------------------------------------------------------");
    mFileObject->WriteComments("");

    mFileObject->FieldWriteBegin("Documents");
    mFileObject->FieldWriteBlockBegin();

    {
        mFileObject->FieldWriteI(FIELD_OBJECT_DEFINITION_COUNT, static_cast<int>(lDocuments.Size()));

        // First document (and usually the only one) does not get written out with its document info,
        // since it's found in the file header.

		for( int i = 0, c = lDocuments.Size(); i < c && lResult; ++i )
        {
			lResult = WriteDocumentDescription(lDocuments[i], i != 0);
        }
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    if(mStatus.Error())
    {
        mStatus.SetCode(FbxStatus::eFailure, "out of disk space");
        lResult = false;
    }
    return lResult;
}

bool FbxWriterFbx7_Impl::WriteDocumentDescription(FbxDocument* pDocument, bool pOutputDocumentInfo)
{
    FBX_ASSERT( pDocument );

    FbxString lDocumentType = pDocument->GetRuntimeClassId().GetFbxFileTypeName(true);

    mFileObject->FieldWriteBegin(FIELD_OBJECT_DESCRIPTION);

    mFileObject->FieldWriteLL( GetObjectId(pDocument) );
    mFileObject->FieldWriteC( pDocument->GetName() );
    mFileObject->FieldWriteC( lDocumentType );

    mFileObject->FieldWriteBlockBegin();

    {
        WriteObjectPropertiesAndFlags(pDocument);

        FbxScene* lScene = FbxCast<FbxScene>(pDocument);

        if( lScene )
        {
            mFileObject->FieldWriteLL("RootNode", GetObjectId(lScene->GetRootNode()));
        }
    }

    if( pOutputDocumentInfo && pDocument->GetDocumentInfo() )
    {
        WriteDocumentInfo(pDocument->GetDocumentInfo());
    }

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    return !mStatus.Error();
}

bool FbxWriterFbx7_Impl::WriteReferenceSection(FbxDocument* pDocument, KTypeWriteReferences& pReferences)
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

    if(lDocRefCount > 0)
    {
        FbxArray<FbxDocument*>   lDocumentPath;
        FbxArray<FbxObject*>     lReferencedObjects;

        FbxString  lRefBaseName    = "Reference_";
        int             lRefCurNum      = 1;

        FbxDocument* lDocRoot = pDocument->GetRootDocument();

        int i, j;
        for(i = 0; i < lDocRefCount; i++)
        {
            // Write the document file path so that it can be found again.
            // Used in OOP renderer, for example, to reload asset libraries.
            FbxDocument* lRefDoc       = lReferencedDocuments[i];
            FbxDocumentInfo * lDocInfo = lRefDoc->GetDocumentInfo();
            if(lDocInfo)
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
                    {
                        mFileObject->FieldWriteBegin(FIELD_OBJECT_DEFINITION_OBJECT_TYPE);
                        {
                            FbxClassId lClassId = lRefDoc->GetRuntimeClassId();
                            FbxString     lTypeName = lClassId.GetFbxFileTypeName(true);
                            if( FbxLibrary::ClassId == lClassId )
                            {
                                // The FbxLibrary class is a real class but it's not given
                                // its own typename... not even a sub-type or something :-(
                                // So we detect it and force a different name so that we
                                // can distinguish between documents and libraries.
                                lTypeName = FIELD_OBJECT_DEFINITION_OBJECT_TYPE_LIBRARY;
                            }
                            mFileObject->FieldWriteC(lTypeName);
                        }
                        mFileObject->FieldWriteEnd();
    
                        lRefDoc->GetDocumentPathToRootDocument(lDocumentPath);
                        int k, lDocPathCount = lDocumentPath.GetCount();
    
                        mFileObject->FieldWriteBegin("DocumentPath");
                            for(k = 0; k < lDocPathCount; k++)
                            {
                                FbxString lDocName = lDocumentPath[k]->GetNameOnly();
                                mFileObject->FieldWriteC(lDocName);
                            }
                        mFileObject->FieldWriteEnd();
                    }

                    mFileObject->FieldWriteBlockEnd();
                    mFileObject->FieldWriteEnd();
                }
            }
        }

        for(i = 0; i < lDocRefCount; i++)
        {
            FbxDocument* lRefDoc       = lReferencedDocuments[i];

            int lObjRefCount = pDocument->GetReferencedObjects(lRefDoc, lReferencedObjects);

            // Now write objects references referencing that document.
            for(j = 0; j < lObjRefCount; j++)
            {
                FbxString     lRefName        = lRefBaseName + (lRefCurNum++);
                FbxObject* lRefObj         = lReferencedObjects[j];
                FbxString     lRefObjName     = lRefObj->GetNameWithNameSpacePrefix();
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

                pReferences.AddReference(lRefObj, lRefName);
                mFileObject->FieldWriteBegin("Reference");
                mFileObject->FieldWriteC(lRefName);
                mFileObject->FieldWriteC(lRefIsInternal ? "Internal" : "External");
                mFileObject->FieldWriteBlockBegin();
                {
                    mFileObject->FieldWriteBegin("Object");
                        if( lRefIsInternal )
                        {
                            mFileObject->FieldWriteLL( GetObjectId(lRefObj) );
                        }
        
                        mFileObject->FieldWriteC(lRefObjName);
                    mFileObject->FieldWriteEnd();
    
                    mFileObject->FieldWriteBegin("DocumentPath");
                        for(k = 0; k < lDocPathCount; k++)
                        {
                            FbxString lDocName = lDocumentPath[k]->GetNameOnly();
                            mFileObject->FieldWriteC(lDocName);
                        }
                    mFileObject->FieldWriteEnd();
                }
                mFileObject->FieldWriteBlockEnd();
                mFileObject->FieldWriteEnd();
            }
        }
    }

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    if(mStatus.Error())
    {
        mStatus.SetCode(FbxStatus::eFailure, "out of disk space");
        return false;
    }
    return true;
}

void FbxWriterFbx7_Impl::WriteObjectDefinition(FbxDocument* pTopDocument)
{
    FBX_ASSERT_RETURN(pTopDocument);

    BuildObjectDefinition(pTopDocument);
    mProgress->SetTotal(static_cast<float>(mTypeDefinitions.GetObjectCount()));
    SetObjectWriteSupport();
    mProgressPause = true;

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
        mFileObject->FieldWriteI(FIELD_OBJECT_DEFINITION_COUNT, mTypeDefinitions.GetObjectCount() + (WritablePluginCount > 0 ? 1 : 0));
	#endif

        int lIter, lCount = mTypeDefinitions.GetDefinitionCount();
        for( lIter = 0; lIter < lCount; lIter++ )
        {
            mFileObject->FieldWriteBegin(FIELD_OBJECT_DEFINITION_OBJECT_TYPE);
            {
                mFileObject->FieldWriteC(mTypeDefinitions.GetDefinition(lIter)->mName);
                mFileObject->FieldWriteBlockBegin();
                {
                    mFileObject->FieldWriteI(FIELD_OBJECT_DEFINITION_COUNT, mTypeDefinitions.GetDefinition(lIter)->mCount);

                    if( mTypeDefinitions.GetDefinition(lIter)->IsPropertyTemplateEnabled() )
                    {
                        WritePropertyTemplate( mTypeDefinitions.GetDefinition(lIter)->mClassId );
                    }
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

    mProgressPause = false;
}

void FbxWriterFbx7_Impl::WritePropertyTemplate( FbxClassId pClassId )
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

    mFileObject->FieldWriteC( pClassId.GetName() );
    mFileObject->FieldWriteBlockBegin();

        mFileObject->FieldWriteBegin(kENHANCED_PROPERTIES);
            mFileObject->FieldWriteBlockBegin();
            {
                FbxProperty lProperty = lClassRoot.GetFirstDescendent();

                while( lProperty.IsValid() )
                {
                    WriteProperty(lProperty);
                    lProperty = lClassRoot.GetNextDescendent(lProperty);
                }
            }
            mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

// *******************************************************************************
//  Objects section
// *******************************************************************************
bool FbxWriterFbx7_Impl::WriteFbxObject(FbxAnimCurve& pAnimCurve)
{
    if (pAnimCurve.KeyGetCount() == 0)
        return true;
	
    WriteObjectHeaderAndReferenceIfAny(pAnimCurve, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_CURVE);
    mFileObject->FieldWriteBlockBegin ();
    {
        WriteObjectPropertiesAndFlags(&pAnimCurve);

        FbxWriterFbx7* lWriter = (FbxWriterFbx7*)mWriter;
        switch (FbxFileVersionStrToInt(lWriter->mFileVersion))
        {
			case 201100:
            {
                // check if we have the PROGRESSIVE_CLAMP flag set
                int k;
                bool lHasProgressive = false;
                for (k = 0; lHasProgressive == false && k < pAnimCurve.KeyGetCount(); k++)
                {
                    FbxAnimCurveDef::ETangentMode lTm = pAnimCurve.KeyGetTangentMode(k, true);
                    if ((lTm & FbxAnimCurveDef::eTangentGenericClampProgressive)==FbxAnimCurveDef::eTangentGenericClampProgressive)
                        lHasProgressive = true;
                }

                if (lHasProgressive)
                {
                    // make a duplicate and replace the GENERIC_CLAMP_PROGRESSIVE flag with the GENERIC_CLAMP
                    FbxAnimCurve* lTmpCurve = FbxAnimCurve::Create(pAnimCurve.GetScene(), pAnimCurve.GetName());
                    FBX_ASSERT(lTmpCurve != NULL);
                    lTmpCurve->CopyFrom(pAnimCurve);
                    for (k = 0; k < lTmpCurve->KeyGetCount(); k++)
                    {
                        FbxAnimCurveDef::ETangentMode lTm = lTmpCurve->KeyGetTangentMode(k, true);
                        if( (lTm & FbxAnimCurveDef::eTangentGenericClampProgressive) == FbxAnimCurveDef::eTangentGenericClampProgressive )
                        {
                            lTm = FbxAnimCurveDef::eTangentUser;
                            lTmpCurve->KeySetTangentMode(k, lTm);
                            FbxTime t = pAnimCurve.KeyGetTime(k);
                            float leftDeriv = pAnimCurve.EvaluateLeftDerivative(t);
                            float rightDeriv = pAnimCurve.EvaluateRightDerivative(t);
                            lTmpCurve->KeySetLeftDerivative(k, leftDeriv);
                            lTmpCurve->KeySetRightDerivative(k, rightDeriv);
                        }
                    }
                    lTmpCurve->Store(mFileObject, true);
                    lTmpCurve->Destroy();
                }
                else
                    pAnimCurve.Store(mFileObject, true);
            }
            break;

			default:
				pAnimCurve.Store(mFileObject);
				break;
        }
    }

    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();
    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxAnimCurveNode& pAnimCurveNode)
{
    WriteObjectHeaderAndReferenceIfAny(pAnimCurveNode, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_CURVENODE);
    mFileObject->FieldWriteBlockBegin ();
    {
        WriteObjectPropertiesAndFlags(&pAnimCurveNode);
    }

    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();
    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxAnimLayer& pAnimLayer)
{
    WriteObjectHeaderAndReferenceIfAny(pAnimLayer, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_LAYER);
    mFileObject->FieldWriteBlockBegin ();
    {
        WriteObjectPropertiesAndFlags(&pAnimLayer);
    }

    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();
    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxAnimStack& pAnimStack)
{
    WriteObjectHeaderAndReferenceIfAny(pAnimStack, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_STACK);
    mFileObject->FieldWriteBlockBegin ();
    {
        WriteObjectPropertiesAndFlags(&pAnimStack);

		if(FbxFileVersionStrToInt(mWriter->mFileVersion) <= 201400)
		{
			FbxThumbnail* lTakeThumbnail = NULL;
			if (mSceneExport && mSceneExport->GetSceneInfo())
				lTakeThumbnail = mSceneExport->GetSceneInfo()->GetSceneThumbnail();

			if (lTakeThumbnail)
			{
				WriteObjectPropertiesAndFlags(lTakeThumbnail);
				WriteThumbnail(lTakeThumbnail);
			}
		}
    }

    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();
    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxAudio& pAudio)
{
	if (!WriteObjectHeaderAndReferenceIfAny(pAudio, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_AUDIO))
	{
		return false;
	}

    bool lStatus = true;
    bool pEmbeddedMedia = IOS_REF.GetBoolProp(EXP_FBX_EMBEDDED, false);

    mFileObject->FieldWriteBlockBegin ();
    {
		FbxString lFileName = pAudio.GetFileName();
		FbxString lRelativeFileName = mFileObject->GetRelativeFilePath(lFileName.Buffer());
		pAudio.SetRelativeFileName(lRelativeFileName.Buffer());

        WriteObjectPropertiesAndFlags(&pAudio);
      
        if (pEmbeddedMedia)
        {
			if (!FbxFileUtils::Exist(lFileName.Buffer()))
			{
				if(!FbxFileUtils::Exist(lRelativeFileName.Buffer()))
				{
					FbxUserNotification* lUserNotification = mManager.GetUserNotification();
					if( lUserNotification )
					{
						lUserNotification->AddDetail( FbxUserNotification::eEmbedMediaNotify, lFileName);
					}
					return false;
				}
			}			

            mFileObject->FieldWriteBegin(FIELD_MEDIA_CONTENT);
            lStatus = (mFileObject->FieldWriteEmbeddedFile(lFileName, lRelativeFileName) ? true : false);
            mFileObject->FieldWriteEnd();
        }
    }

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    return lStatus;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxAudioLayer& pAudioLayer)
{
	WriteObjectHeaderAndReferenceIfAny(pAudioLayer, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_AUDIO_LAYER);
	mFileObject->FieldWriteBlockBegin ();
    {
        WriteObjectPropertiesAndFlags(&pAudioLayer);
	}

	mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();
    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxGenericNode& pNode)
{
    WriteObjectHeaderAndReferenceIfAny(pNode, FIELD_KFBXGENERICNODE_GENERICNODE);
    mFileObject->FieldWriteBlockBegin ();
    {
        mFileObject->FieldWriteI(FIELD_KFBXGENERICNODE_VERSION, 100);

        WriteObjectPropertiesAndFlags(&pNode);
    }
    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();

    return true;
}

void FbxWriterFbx7_Impl::WriteControlSetPlug(FbxScene& pScene)
{
    int i, lCount = pScene.GetControlSetPlugCount();

    FbxControlSetPlug* lPlug;

    for(i=0; i < lCount; i++)
    {
        lPlug = pScene.GetControlSetPlug(i);

        WriteObjectHeaderAndReferenceIfAny(*lPlug, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONTROLSET_PLUG);

        FbxString lPlugTypeName = lPlug->GetTypeName();

        mFileObject->FieldWriteBlockBegin ();
        {
            mFileObject->FieldWriteC("Type",lPlugTypeName);
            mFileObject->FieldWriteI("MultiLayer",0);

            WriteObjectPropertiesAndFlags(lPlug);
        }
        mFileObject->FieldWriteBlockEnd ();
        mFileObject->FieldWriteEnd ();
    }
}

void FbxWriterFbx7_Impl::WriteCharacterLinkGroup(FbxCharacter& pCharacter, int pCharacterGroupId, FbxScene& pScene)
{
    int i, lCount = FbxCharacter::GetCharacterGroupCount((FbxCharacter::EGroupId) pCharacterGroupId);

    for(i = 0; i < lCount; i++)
    {
        FbxCharacter::ENodeId lCharacterNodeId = FbxCharacter::GetCharacterGroupElementByIndex((FbxCharacter::EGroupId) pCharacterGroupId, i);
        FbxCharacterLink lCharacterLink;

        bool lFound = pCharacter.GetCharacterLink(lCharacterNodeId, &lCharacterLink);

        if(lFound)
        {
            lFound = (lCharacterLink.mNode && lCharacterLink.mNode->GetScene() == &pScene) || !lCharacterLink.mTemplateName.IsEmpty();
        }

        if(lFound)
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

void FbxWriterFbx7_Impl::WriteCharacterLink(FbxCharacter& pCharacter, int pCharacterNodeId, FbxScene& pScene)
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
    if(lFound)
    {
        if(!lCharacterLink->mTemplateName.IsEmpty())
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
void FbxWriterFbx7_Impl::WriteCharacterLinkRotationSpace(FbxCharacterLink& pCharacterLink)
{
    mFileObject->FieldWriteBegin("ROTATIONSPACE");
    mFileObject->FieldWriteBlockBegin();
    {
        mFileObject->FieldWrite3D("PRE", pCharacterLink.mPreRotation);
        mFileObject->FieldWrite3D("POST", pCharacterLink.mPostRotation);
    
        mFileObject->FieldWriteD("AXISLEN",pCharacterLink.mAxisLen);
        mFileObject->FieldWriteI("ORDER",pCharacterLink.mRotOrder);
    
        mFileObject->FieldWriteI("XMINENABLE", pCharacterLink.mRLimits.GetMinXActive());
        mFileObject->FieldWriteI("YMINENABLE", pCharacterLink.mRLimits.GetMinYActive());
        mFileObject->FieldWriteI("ZMINENABLE", pCharacterLink.mRLimits.GetMinZActive());
    
        mFileObject->FieldWriteI("XMAXENABLE", pCharacterLink.mRLimits.GetMaxXActive());
        mFileObject->FieldWriteI("YMAXENABLE", pCharacterLink.mRLimits.GetMaxYActive());
        mFileObject->FieldWriteI("ZMAXENABLE", pCharacterLink.mRLimits.GetMaxZActive());
    
		mFileObject->FieldWrite3D("MIN", pCharacterLink.mRLimits.GetMin().mData);
        mFileObject->FieldWrite3D("MAX", pCharacterLink.mRLimits.GetMax().mData);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

void FbxWriterFbx7_Impl::WriteCharacter(FbxCharacter& pCharacter, FbxScene& pScene)
{
    mFileObject->FieldWriteB("CHARACTERIZE", true);
    mFileObject->FieldWriteB("LOCK_XFORM", false);
    mFileObject->FieldWriteB("LOCK_PICK", false);

    mFileObject->FieldWriteBegin("REFERENCE");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLink(pCharacter, FbxCharacter::eReference, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("LEFT_FLOOR");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLink(pCharacter, FbxCharacter::eLeftFloor, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("RIGHT_FLOOR");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLink(pCharacter, FbxCharacter::eRightFloor, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("LEFT_HANDFLOOR");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLink(pCharacter, FbxCharacter::eLeftHandFloor, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("RIGHT_HANDFLOOR");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLink(pCharacter, FbxCharacter::eRightHandFloor, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("BASE");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLinkGroup(pCharacter, FbxCharacter::eGroupBase, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("AUXILIARY");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLinkGroup(pCharacter, FbxCharacter::eGroupAuxiliary, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("SPINE");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLinkGroup(pCharacter, FbxCharacter::eGroupSpine, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("NECK");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLinkGroup(pCharacter, FbxCharacter::eGroupNeck, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("ROLL");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLinkGroup(pCharacter, FbxCharacter::eGroupRoll, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("SPECIAL");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLinkGroup(pCharacter, FbxCharacter::eGroupSpecial, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("LEFTHAND");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLinkGroup(pCharacter, FbxCharacter::eGroupLeftHand, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("RIGHTHAND");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLinkGroup(pCharacter, FbxCharacter::eGroupRightHand, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("LEFTFOOT");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLinkGroup(pCharacter, FbxCharacter::eGroupLeftFoot, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("RIGHTFOOT");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLinkGroup(pCharacter, FbxCharacter::eGroupRightFoot, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    mFileObject->FieldWriteBegin("PROPS");
    mFileObject->FieldWriteBlockBegin();
    {
        WriteCharacterLinkGroup(pCharacter, FbxCharacter::eGroupProps, pScene);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

int FbxWriterFbx7_Impl::WriteCharacterPose(FbxScene& pScene)
{
    int i, lCharacterPoseCount = pScene.GetCharacterPoseCount();

    if(lCharacterPoseCount == 0)
    {
        return 0;
    }

    for(i = 0; i < lCharacterPoseCount; i++)
    {
        FbxCharacterPose*  CharacterPose = pScene.GetCharacterPose(i);

        WriteObjectHeaderAndReferenceIfAny(*CharacterPose, FIELD_KFBXPOSE_POSE);
        mFileObject->FieldWriteBlockBegin();
        {
            mFileObject->FieldWriteBegin("PoseScene");
            mFileObject->FieldWriteBlockBegin();
    
                WriteCharacterPose(*CharacterPose, pScene);
    
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    return 1;
}

void FbxWriterFbx7_Impl::WriteCharacterPoseNodeRecursive(FbxNode* pNode, FbxNode* pParent)
{
	if (pNode && pParent)
	{
		mFileObject->FieldWriteBegin("PoseNode");
		mFileObject->FieldWriteBlockBegin();
    
			mFileObject->FieldWriteS("Node", pNode->GetName());
			mFileObject->FieldWriteS("Parent", pParent->GetName());
			WriteProperty(pNode->LclTranslation);
			WriteProperty(pNode->LclRotation);
			WriteProperty(pNode->LclScaling);
                
		mFileObject->FieldWriteBlockEnd();
		mFileObject->FieldWriteEnd();

		for (int i = 0; i < pNode->GetChildCount(); i++)
		{
			FbxNode* lNode = pNode->GetChild(i);
			WriteCharacterPoseNodeRecursive(lNode, pNode);
		}
	}
}

void FbxWriterFbx7_Impl::WriteCharacterPose(FbxCharacterPose& pCharacterPose, FbxScene& pScene)
{
	if (mFileObject->GetFileVersionNumber() < FBX_FILE_VERSION_7300)
	{
		// for backward compatibility: any FBX file version 7.2 and less will
		// write using the old stuff
		FbxExporter* lExporter = FbxExporter::Create(&mManager,"");
		lExporter->SetIOSettings( GetIOSettings() );

		// Store original values
		bool bModel     = IOS_REF.GetBoolProp(EXP_FBX_MODEL,			false);
		bool bMaterial  = IOS_REF.GetBoolProp(EXP_FBX_MATERIAL,			false);
		bool bTexture   = IOS_REF.GetBoolProp(EXP_FBX_TEXTURE,			false);
		bool bShape     = IOS_REF.GetBoolProp(EXP_FBX_SHAPE,			false);
		bool bGobo      = IOS_REF.GetBoolProp(EXP_FBX_GOBO,				false);
		bool bPivot     = IOS_REF.GetBoolProp(EXP_FBX_PIVOT,			false);
		bool bAnimation = IOS_REF.GetBoolProp(EXP_FBX_ANIMATION,        false);
		bool bSettings  = IOS_REF.GetBoolProp(EXP_FBX_GLOBAL_SETTINGS,  false);
		bool bEmbedded  = IOS_REF.GetBoolProp(EXP_FBX_EMBEDDED,         false);

		IOS_REF.SetBoolProp(EXP_FBX_MODEL,				false);
		IOS_REF.SetBoolProp(EXP_FBX_MATERIAL,			false);
		IOS_REF.SetBoolProp(EXP_FBX_TEXTURE,			false);
		IOS_REF.SetBoolProp(EXP_FBX_SHAPE,				false);
		IOS_REF.SetBoolProp(EXP_FBX_GOBO,				false);
		IOS_REF.SetBoolProp(EXP_FBX_PIVOT,				false);
		IOS_REF.SetBoolProp(EXP_FBX_ANIMATION,			false);
		IOS_REF.SetBoolProp(EXP_FBX_GLOBAL_SETTINGS,	false);
		IOS_REF.SetBoolProp(EXP_FBX_EMBEDDED,			false);

		bool isBeforeV6 = mFileObject->IsBeforeVersion6();
		mFileObject->SetIsBeforeVersion6(true);
		lExporter->Export(pCharacterPose.GetPoseScene(), mFileObject);
		mFileObject->SetIsBeforeVersion6(isBeforeV6);

		lExporter->Destroy();

		// Keep original values
		IOS_REF.SetBoolProp(EXP_FBX_MODEL,              bModel     );
		IOS_REF.SetBoolProp(EXP_FBX_MATERIAL,           bMaterial  );
		IOS_REF.SetBoolProp(EXP_FBX_TEXTURE,            bTexture   );
		IOS_REF.SetBoolProp(EXP_FBX_SHAPE,              bShape     );
		IOS_REF.SetBoolProp(EXP_FBX_GOBO,               bGobo      );
		IOS_REF.SetBoolProp(EXP_FBX_PIVOT,              bPivot     );
		IOS_REF.SetBoolProp(EXP_FBX_ANIMATION,          bAnimation );
		IOS_REF.SetBoolProp(EXP_FBX_GLOBAL_SETTINGS,    bSettings  );
		IOS_REF.SetBoolProp(EXP_FBX_EMBEDDED,           bEmbedded  );
	}
	else
	{
		// from FBX version 7.3, write a new structure

		FbxCharacter* lCharacter = pCharacterPose.GetCharacter();
		FBX_ASSERT(lCharacter != NULL);
		if (lCharacter == NULL)
			return;
		
		FbxNode* lRoot = pCharacterPose.GetRootNode();
		FBX_ASSERT(lRoot != NULL);
		if (lRoot == NULL)
			return;

		FbxScene* lScene = pCharacterPose.GetPoseScene();
		FBX_ASSERT(lScene != NULL);
		if (lScene == NULL)
			return;


		// Write the nodes local transforms (no animation)
		int count = lScene->GetSrcObjectCount<FbxNode>();
		if (count > 1) // ignore RootNode
		{
			mFileObject->FieldWriteI("NbPoseNodes", count-1); // remove RootNode
			for (int i = 0; i < lRoot->GetChildCount(); i++)
			{
				FbxNode* lNode = lRoot->GetChild(i);
				WriteCharacterPoseNodeRecursive(lNode, lRoot);
			}
		}

		// and now the local character definition!
		WriteCharacter(*lCharacter, *pCharacterPose.GetPoseScene());
	}
}

bool FbxWriterFbx7_Impl::WriteSelectionNode(FbxScene& pScene)
{
    int lSelectionNodeCount = pScene.GetMemberCount<FbxSelectionNode>();

    for (int i = 0; i < lSelectionNodeCount; i++)
    {
        FbxSelectionNode* lSelectionNode = (FbxSelectionNode *)pScene.GetMember<FbxSelectionNode>(i);

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

void FbxWriterFbx7_Impl::WriteSelectionNode(FbxSelectionNode& pSelectionNode)
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

bool FbxWriterFbx7_Impl::WriteSelectionSet(FbxScene& pScene)
{
    int i, lSelectionSetCount = pScene.GetMemberCount<FbxSelectionSet>();

    for (i = 0; i < lSelectionSetCount; i++)
    {
        FbxSelectionSet* lSelectionSet = pScene.GetMember<FbxSelectionSet>(i);

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

void FbxWriterFbx7_Impl::WriteSelectionSet(FbxSelectionSet& pSelectionSet)
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

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxPose& pPose)
{
    if( !WriteObjectHeaderAndReferenceIfAny(pPose, FIELD_KFBXPOSE_POSE) )
    {
        return false;
    }

    mFileObject->FieldWriteBlockBegin();
    {
        mFileObject->FieldWriteS(FIELD_KFBXPOSE_TYPE, pPose.IsBindPose() ? FIELD_KFBXPOSE_BIND_POSE : FIELD_KFBXPOSE_REST_POSE);
        mFileObject->FieldWriteI(FIELD_KFBXPOSE_VERSION, 100);

        WriteObjectPropertiesAndFlags(&pPose);   
        WritePose(pPose);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    return true;
}


void FbxWriterFbx7_Impl::WritePose(FbxPose& pPose)
{
    int count = pPose.GetCount();
    mFileObject->FieldWriteI("NbPoseNodes", count);

    for(int i = 0; i < count; i++)
    {
        mFileObject->FieldWriteBegin("PoseNode");
        mFileObject->FieldWriteBlockBegin();
        {
            mFileObject->FieldWriteLL("Node", GetObjectId(pPose.GetNode(i)));
    
            WriteValueArray("Matrix", 16, (double*)pPose.GetMatrix(i).mData);
            if(!pPose.IsBindPose()) // for bindPoses the matrix is always global
                mFileObject->FieldWriteB("Local", pPose.IsLocalMatrix(i));
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }
}


bool FbxWriterFbx7_Impl::WriteFbxObject(FbxSceneReference& pReference)
{
    if( !WriteObjectHeaderAndReferenceIfAny(pReference, FIELD_KFBXREFERENCE_REFERENCE) )
    {
        return false;
    }

    mFileObject->FieldWriteBlockBegin();
    {
        mFileObject->FieldWriteI(FIELD_KFBXREFERENCE_VERSION, 100);

        WriteObjectPropertiesAndFlags(&pReference); 
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    return true;
}

void FbxWriterFbx7_Impl::WriteProperty(FbxProperty& lFbxProperty)
{
    /*
      Legacy format:
          Name,type,flags, Value [ min,max,enumlist, NodeAttribute flag]

          flags: ('A' : Animatable, '+' : Animated, 'O' : ObjectList, 'U' : User, 'N' : Node Attribute)

      Enhanced format:
          Name,type,typename*,flags, Value [, min,max,enumlist, NodeAttribute flag][{
              extra info for value (ie: blob)
          } ]

          typename will be empty if it's the same as type, to keep the file smaller.
    */

    FBX_ASSERT(lFbxProperty.IsValid());

    if( !lFbxProperty.IsValid() )
    {
        return;
    }

    if( lFbxProperty.GetFlag(FbxPropertyFlags::eNotSavable) )
    {
        // This property is flagged as ... not savable.  So, don't save it.
        // FIXME: Need to skip over the children, too, and what about connections to this
        // object?  I suppose they should be skipped as well.
        return;
    }

    // Available flags :
    // 'A', 'U', '+', 'H', 'L' + char , 'M' + char , NULL
    char          flags[10] = {0};
    char*         pflags   = flags;

    mFileObject->FieldWriteBegin(kENHANCED_PROPERTY_FIELD);

    mFileObject->FieldWriteS( lFbxProperty.GetHierarchicalName() );

    const char* lType;

    if( lFbxProperty.GetFlag(FbxPropertyFlags::eAnimatable) )
    {
        *pflags++ = 'A';
        // DataType
        lType = lFbxProperty.GetPropertyDataType().GetName();
        if(lFbxProperty.GetFlag(FbxPropertyFlags::eAnimated)) *pflags++ = '+';
    }
    else
    {
        // Not animatable types
        lType = FbxGetDataTypeNameForIO(lFbxProperty.GetPropertyDataType());
    }

    mFileObject->FieldWriteS( lType );

    // Space saving -- if it's the same name, don't bother writing the subtype,
    // but we still need to reserve the value field.
    // Do a case insensitive compare, so we don't have "float", "Float".
    const char* lSubType = lFbxProperty.GetPropertyDataType().GetName();
    mFileObject->FieldWriteS( FBXSDK_stricmp(lSubType, lType) ? lSubType : "" );

    if (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined))
        *pflags++ = 'U';
    if (lFbxProperty.GetFlag(FbxPropertyFlags::eHidden))
        *pflags++ = 'H';

    // Locked status.
    {
    int lLockedFlags = lFbxProperty.GetFlags() & FbxPropertyFlags::eLockedAll;
    if (lLockedFlags != 0)
    {
        // Something is locked.
        *pflags++ = 'L';

        const char lFlagChar = FbxGetCharFromFlags( lLockedFlags >> FbxPropertyFlags::sLockedMembersBitOffset );
        if ( lFlagChar != 0)
        {
            *pflags++ = lFlagChar;
        }
    }
    }

    // Muted status.
    {
        int lMutedFlags = lFbxProperty.GetFlags() & FbxPropertyFlags::eMutedAll;
        if (lMutedFlags != 0)
        {
            // Something is muted.
            *pflags++ = 'M';

            const char lFlagChar = FbxGetCharFromFlags( lMutedFlags >> FbxPropertyFlags::sMutedMembersBitOffset );
            if ( lFlagChar != 0)
            {
                *pflags++ = lFlagChar;
            }
        }
    }

    FBX_ASSERT( flags + sizeof( flags ) > pflags );
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
            mFileObject->FieldWriteDn( (double *)&lDouble2, 2 );
        } break;
    case eFbxDouble3: {
            FbxDouble3 lDouble3 = lFbxProperty.Get<FbxDouble3>();
            mFileObject->FieldWrite3D( (double *)&lDouble3 );
        } break;
    case eFbxDouble4: {
            FbxDouble4 lDouble4 = lFbxProperty.Get<FbxDouble4>();
            mFileObject->FieldWrite4D((double *)&lDouble4 );
        } break;
    case eFbxDouble4x4: {
            FbxDouble4x4 lDouble44 = lFbxProperty.Get<FbxDouble4x4>();
            // These are stored using the 'old' format to keep properties similar,
            // for now at least.
            mFileObject->FieldWrite4D((double *)&lDouble44[0] );
            mFileObject->FieldWrite4D((double *)&lDouble44[1] );
            mFileObject->FieldWrite4D((double *)&lDouble44[2] );
            mFileObject->FieldWrite4D((double *)&lDouble44[3] );
        } break;
    case eFbxEnumM:
    case eFbxEnum:
        mFileObject->FieldWriteI( lFbxProperty.Get<FbxEnum>() );
        break;
    case eFbxString:
        mFileObject->FieldWriteS( lFbxProperty.Get<FbxString>() );
        break;
    case eFbxTime:
        mFileObject->FieldWriteT(lFbxProperty.Get<FbxTime>());
        break;
    case eFbxReference:  // used as a port entry to reference object or properties
        break;
    case eFbxBlob:
        {
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
            FbxBinaryBlobReader lBlobReader(lFbxProperty);
            OutputBinaryBuffer(lBlobReader);

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

    if((lFbxProperty.GetPropertyDataType().GetType()==eFbxEnum || lFbxProperty.GetPropertyDataType().GetType()==eFbxEnumM) && (lFbxProperty.GetFlag(FbxPropertyFlags::eUserDefined)))
    {
        // Enum list
        // ===========
        FbxString EnumString;
        for(int i=0; i<lFbxProperty.GetEnumCount(); i++) {
            EnumString += lFbxProperty.GetEnumValue(i);
            if(i<lFbxProperty.GetEnumCount()-1) EnumString += "~";
        }
        mFileObject->FieldWriteS( EnumString );
    }

    mFileObject->FieldWriteEnd();
}

bool FbxWriterFbx7_Impl::WriteObjectProperties(FbxObject *pObject)
{
    FbxObject*       lReferencedObject = pObject->GetReferenceTo();

    const FbxObject*   lPropertyObject = pObject;
    FbxProperty        lFbxProperty    = lPropertyObject->GetFirstProperty();
    FbxProperty        lFbxNextProperty;

    // reset the iterators
    lPropertyObject     = pObject;
    lFbxProperty        = lPropertyObject->GetFirstProperty();
    lFbxNextProperty    = FbxProperty();

    bool lSkipDefaultValues = false;    // If we don't have the type definition don't attempt to get cute and skip default values.

    {
        KTypeDefinitionInfo* pTypeDefinition = mTypeDefinitions.GetDefinitionFromName(pObject->GetRuntimeClassId().GetFbxFileTypeName(true));

        if( pTypeDefinition )
        {
            lSkipDefaultValues = pTypeDefinition->IsPropertyTemplateEnabled();
        }
    }

    // Patch for node attribute;
    bool lPropertyBlockOpened = false;
    {
        while(lFbxProperty!=0) 
        {
            lFbxNextProperty = lPropertyObject->GetNextProperty(lFbxProperty);
    
            // Skipping properties that equals the ones in the referenced object if any
            if(lReferencedObject)
            {
                bool            lDoContinue = false;
    
                FbxIterator<FbxProperty>  lFbxReferencePropertyIter(lReferencedObject);
                FbxProperty                lFbxReferenceProperty;
    
                FbxForEach(lFbxReferencePropertyIter,lFbxReferenceProperty) {
                    if(lFbxProperty.GetName() == lFbxReferenceProperty.GetName() ) {
                        if(lFbxProperty.CompareValue(lFbxReferenceProperty))
                        {
                            lDoContinue = true;
                            break;
                        }
                    }
                }
                if(lDoContinue) {
                    lFbxProperty = lFbxNextProperty;
                    continue;
                }
            }
    
            // Skipping a default value property, unless this object doesn't support
            // templates.
            // We also write it if something is locked or muted.
            if(lSkipDefaultValues && FbxProperty::HasDefaultValue(lFbxProperty) &&
                ((lFbxProperty.GetFlags() & (FbxPropertyFlags::eLockedAll | FbxPropertyFlags::eMutedAll)) == 0))
            {
                lFbxProperty = lFbxNextProperty;
                continue;   //Do not write property
            }
    
            if( !lPropertyBlockOpened )
            {
                lPropertyBlockOpened  = true;

                mFileObject->FieldWriteBegin(kENHANCED_PROPERTIES);
                mFileObject->FieldWriteBlockBegin();
            }

            WriteProperty(lFbxProperty);
            lFbxProperty = lFbxNextProperty;
        }
    }

    if( lPropertyBlockOpened )
    {
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteObjectPropertiesAndFlags(FbxObject* pObject)
{
    FBX_ASSERT( pObject );

    if (mProgress && !mProgressPause)
    {
        mProgress->Update(1.0f, pObject->GetName());
        mCanceled = mProgress->IsCanceled();
    }
    return WriteObjectProperties(pObject);
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxNode& pNode)
{
    WriteObjectHeaderAndReferenceIfAny(pNode, FIELD_KFBXNODE_MODEL);
    mFileObject->FieldWriteBlockBegin ();

    WriteNodeParameters(pNode);

    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();

    return(true);
}

bool FbxWriterFbx7_Impl::WriteNodeParameters(FbxNode& pNode)
{
    WriteNodeVersion(pNode);

    WriteNodeProperties(pNode);

    WriteNodeShading(pNode);

    WriteNodeCullingType(pNode);

    //WriteNodeLimits(pNode);

    //WriteNodeTarget(pNode);

    //WriteNodeAnimatedProperties(pNode);

    // Never write the node attributes with tne nodes; they are now stored
    // separately, by themselves.

    return true;
}


bool FbxWriterFbx7_Impl::WriteNodeVersion(FbxNode& pNode)
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

    return(true);
}


bool FbxWriterFbx7_Impl::WriteNodeShading(FbxNode& pNode)
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

    return(true);
}



bool FbxWriterFbx7_Impl::WriteNodeCullingType(FbxNode& pNode)
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


bool FbxWriterFbx7_Impl::WriteNodeAttribute(FbxNodeAttribute* pNodeAttribute)
{
    //
    // Store Geometry if applicable...
    //

    if(pNodeAttribute != NULL)
    {
		// Storing only savable objects
		if (!pNodeAttribute->GetObjectFlags(FbxObject::eSavable) )
			return true;

        if(!pNodeAttribute->ContentIsLoaded() )
            pNodeAttribute->ContentLoad();

        if(IOS_REF.GetBoolProp(EXP_FBX_MODEL, true))
        {
            switch(pNodeAttribute->GetAttributeType())
            {
            case FbxNodeAttribute::eLODGroup:
                // nothing extra to do. This attribute only contains properties
                // who gets saved automatically!
                break;

            case FbxNodeAttribute::eCachedEffect:
                // nothing extra to do. This attribute only contains properties
                // who gets saved automatically!
                break;

            case FbxNodeAttribute::eNull:
                WriteNull( (FbxNull*)pNodeAttribute);
                break;

            case FbxNodeAttribute::eMarker:
                if( pNodeAttribute->GetNode() )
                    WriteMarker( *pNodeAttribute->GetNode() );
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

			case FbxNodeAttribute::eLine:
				WriteLine( *(FbxLine*)(pNodeAttribute) );
				break;

			case FbxNodeAttribute::eShape:
				WriteShape( *(FbxShape*)(pNodeAttribute) );
				break;

            case FbxNodeAttribute::eUnknown:
            default:
                FBX_ASSERT_NOW("Unknown node attribute type!");
                // fall-through:

            case FbxNodeAttribute::eOpticalReference:
            case FbxNodeAttribute::eOpticalMarker:
                break;
            }
        }
        else
        {
            // We don't export the geometry. But we still want the animation
            // and properites on Lights and cameras
            switch(pNodeAttribute->GetAttributeType ())
            {
            case FbxNodeAttribute::eLODGroup:
                // nothing extra to do. This attribute only contains properties
                // who gets saved automatically!
                break;

            case FbxNodeAttribute::eCachedEffect:
                // nothing extra to do. This attribute only contains properties
                // who gets saved automatically!                
                break;

            case FbxNodeAttribute::eNull:
                WriteNull( (FbxNull*)pNodeAttribute );
                break;

            case FbxNodeAttribute::eMarker:
                if( pNodeAttribute->GetNode() )
                    WriteMarker( *pNodeAttribute->GetNode() );
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

                //case FbxNodeAttribute::eOpticalReference:
                //WriteOpticalReference(*(FbxOpticalReference*)(pNodeAttribute));
                //break;
                // 

            case FbxNodeAttribute::eUnknown:
                break;

            default:
                WriteNull(NULL);
                break;
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

    return(true);
}


bool FbxWriterFbx7_Impl::WriteNodeProperties(FbxNode& pNode)
{
    pNode.UpdatePropertiesFromPivotsAndLimits();
    WriteObjectPropertiesAndFlags(&pNode);
    return true;
}


bool FbxWriterFbx7_Impl::WriteGeometry(FbxGeometry& pGeometry)
{
    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYMESH_GEOMETRY_VERSION, 124);

    FbxMultiMap lLayerIndexSet;

    if(pGeometry.GetAttributeType() == FbxNodeAttribute::eMesh)
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
    WriteLayers(pGeometry, lLayerIndexSet);

	if(FbxFileVersionStrToInt(mWriter->mFileVersion) == 201100)
	{
		// the shapes on a trim nurb will be written out with the untrimmed surface
		// don't write them out here.
		if( pGeometry.GetAttributeType() != FbxNodeAttribute::eTrimNurbsSurface )
		{
			// Only write out the first shape of each blend shape channel.
			// Because we did not support In-between blend shape for version 7.1 FBX file. 
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
						WriteShape(*lShape);
					}
				}
			}
		}
	}

    return true;
}

bool FbxWriterFbx7_Impl::WriteNodeType(FbxNode& pNode)
{
    mFileObject->FieldWriteBegin(FIELD_KFBXNODE_TYPE_FLAGS);
    {
        int i;
        for(i=0; i<pNode.GetTypeFlags().GetCount(); i++)
        {
            mFileObject->FieldWriteC(pNode.GetTypeFlags()[i]);
        }
    }
    mFileObject->FieldWriteEnd();

    return true;
}

bool FbxWriterFbx7_Impl::WriteNull(FbxNull* pNull)
{
    double     lSize = 100.0;
    FbxString     lTypeName = "Null";
    FbxStringList lTypeFlags;
    int         i;

    if(pNull)
    {
        lSize      = pNull->Size.Get();
        lTypeName  = pNull->GetTypeName();
        lTypeFlags = pNull->GetTypeFlags();
    }
    else
    {
        lTypeFlags.Add("Null");
    }

    mFileObject->FieldWriteBegin(FIELD_KFBXNODE_TYPE_FLAGS);
    {
        for(i=0; i<lTypeFlags.GetCount(); i++)
        {
            mFileObject->FieldWriteC(lTypeFlags[i]);
        }
    }
    mFileObject->FieldWriteEnd();

    return true;
}

bool FbxWriterFbx7_Impl::WriteMarker(FbxNode& pNode)
{
    return WriteNodeType(pNode);
}

bool FbxWriterFbx7_Impl::WriteCamera(FbxCamera& pCamera)
{
    if(!pCamera.GetNode()) return false;

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

    // !!! TBV
    //mFileObject->FieldWriteD (FIELD_KFBXGEOMETRYCAMERA_CAMERA_ROLL, pCamera.GetRoll());

    // Background Properties

    /*
    if (pCamera.GetBackgroundMediaName() != NULL)
    {
        mFileObject->FieldWriteC(FIELD_KFBXGEOMETRYCAMERA_BACKGROUND_MEDIA_NAME, pCamera.GetBackgroundMediaName());
    }*/

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

bool FbxWriterFbx7_Impl::WriteCameraStereo(FbxCameraStereo& pCameraStereo)
{
    if(!pCameraStereo.GetNode()) return false;

    WriteNodeType(*pCameraStereo.GetNode());

    mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYCAMERA_STEREO_VERSION, 100);

    bool lStatus = true;
    bool lEmbeddedMedia;
    lEmbeddedMedia = IOS_REF.GetBoolProp(EXP_FBX_EMBEDDED, false);

    FbxString pFileName;
    if (lEmbeddedMedia)
    {
        FbxString lPrecompFileName = pCameraStereo.PrecompFileName.Get();
        FbxString lRelativeName = pCameraStereo.RelativePrecompFileName.Get();
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYCAMERA_STEREO_PRECOMP_FILE_CONTENT);
        lStatus = mFileObject->FieldWriteEmbeddedFile(lPrecompFileName, lRelativeName);

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


bool FbxWriterFbx7_Impl::WriteCameraSwitcher(FbxCameraSwitcher& pCameraSwitcher)
{
    mFileObject->FieldWriteI (FIELD_KFBXNODE_VERSION, 101);
    mFileObject->FieldWriteC (FIELD_KFBXGEOMETRYCAMERASWITCHER_NAME, "Model::Camera Switcher");
    mFileObject->FieldWriteI (FIELD_KFBXGEOMETRYCAMERASWITCHER_CAMERA_ID, pCameraSwitcher.GetDefaultCameraIndex());
    mFileObject->FieldWriteI (FIELD_KFBXGEOMETRYCAMERASWITCHER_CAMERA_NAME, 100);

    // Additional field useful to merge scenes in FiLMBOX.
    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYCAMERASWITCHER_CAMERA_INDEX_NAME);

    for(int i = 0, n = pCameraSwitcher.GetCameraNameCount(); i < n; ++i)
    {
        mFileObject->FieldWriteS(pCameraSwitcher.GetCameraName(i));
    }
    mFileObject->FieldWriteEnd();
    return true;
}


bool FbxWriterFbx7_Impl::WriteLight(FbxLight& pLight)
{
    mFileObject->FieldWriteC(FIELD_KFBXNODE_TYPE_FLAGS, "Light");
	mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYLIGHT_GEOMETRY_VERSION, 124);
	return true;
}

bool FbxWriterFbx7_Impl::WriteMeshSmoothness(FbxMesh& pMesh)
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


bool FbxWriterFbx7_Impl::WriteMeshVertices(FbxMesh& pMesh)
{
    FbxAMatrix lPivot;
    pMesh.GetPivot(lPivot);

    //
    // Vertices
    //
    WriteControlPoints(FIELD_KFBXGEOMETRYMESH_VERTICES, pMesh.GetControlPointsCount(), pMesh.GetControlPoints(), lPivot, false);

    return true;
}

bool FbxWriterFbx7_Impl::WriteMeshPolyVertexIndex(FbxMesh& pMesh)
{
    int lPolygonCount = pMesh.GetPolygonCount();
    if(lPolygonCount > 0)
    {
        int lVerticesCount      = pMesh.mPolygonVertices.GetCount();
        int lLastPolygonVertex  = lVerticesCount - 1;
        int NextPolygonIndex    = 1;

        int* lVertexIndex = FbxNewArray<int>(lVerticesCount);
        int* lSrc         = pMesh.mPolygonVertices.GetArray();
        int* lDest        = lVertexIndex;

        for( int i = 0; i < lVerticesCount; ++i, ++lSrc, ++lDest)
        {
            int Index = *lSrc;

            // Set a polygon vertex to it's complement to mark the end of a polygon.

            // All polygons except the last one.
            if(NextPolygonIndex < lPolygonCount)
            {
                // Last polygon vertex in the current polygon
                if(i == (pMesh.GetPolygonVertexIndex(NextPolygonIndex) - 1))
                {
                    Index = -Index - 1;
                    NextPolygonIndex ++;
                }
            }
            // Last polygon vertex of the last polygon.
            else if(i == lLastPolygonVertex)
            {
                Index = -Index-1;
            }

            *lDest = Index;
        }

        WriteValueArray(FIELD_KFBXGEOMETRYMESH_POLYGON_INDEX, lVerticesCount, lVertexIndex);
        FbxDeleteArray(lVertexIndex);
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxLayerElementNormals(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eNormal);

    // New 102 version writes all 4 fields instead of only x,y,z but in 2 separate arrays to remain backward compatible
    unsigned int lLayerVersion = (FbxFileVersionStrToInt(mWriter->mFileVersion) >= 201400) ? 102 : 101;
    for(i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementNormal* lLayerElementNormal = pLayerContainer.GetLayer(i, FbxLayerElement::eNormal)->GetNormals();

        pLayerIndexSet.Add((FbxHandle)lLayerElementNormal, i);

        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_ELEMENT_NORMAL);
        {
            // Layer Element index
            mFileObject->FieldWriteI(i);

            mFileObject->FieldWriteBlockBegin();
            {   
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, lLayerVersion);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElementNormal->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElementNormal->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElementNormal->GetReferenceMode()));

                unsigned int lNormalCount = lLayerElementNormal->GetDirectArray().GetCount();
                if( lNormalCount > 0 )
                {
                    // Not using any of our WriteValueArray() helpers because we're using 
                    // a size/stride combo to skip copying of the data.
                    FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementNormal->GetDirectArray();
                    FbxLayerElementArrayReadLock<FbxVector4>  lData(lDirectArray);

                    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_NORMALS);
                    mFileObject->FieldWriteArrayD(lNormalCount, (const double*) lData.GetData(), 3, (int) sizeof(FbxVector4));
                    mFileObject->FieldWriteEnd();

                    if (lLayerVersion >= 102)
                    {
                        // write the 4th component of the normals array
                        FbxArray<double> lFourth;
                        lFourth.Reserve(lNormalCount);
                        for (unsigned int j = 0; j < lNormalCount; j++)
                        {
                            lFourth.SetAt(j, ((FbxVector4*)lData.GetData())[j][3]);
                        }
                        WriteValueArray(FIELD_KFBXGEOMETRYMESH_NORMALS_WCOMPONENT, lNormalCount, lFourth.GetArray());
                    }
                }

                // Write the index if the mapping type is index to direct
                if(lLayerElementNormal->GetReferenceMode() != FbxLayerElement::eDirect)
                {
                    WriteValueArray(FIELD_KFBXGEOMETRYMESH_NORMALS_INDEX, lLayerElementNormal->GetIndexArray());
                }

            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxLayerElementBinormals(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eBiNormal);
    
    // New 102 version writes all 4 fields instead of only x,y,z but in 2 separate arrays to remain backward compatible
    unsigned int lLayerVersion = (FbxFileVersionStrToInt(mWriter->mFileVersion) >= 201400) ? 102 : 101;
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
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, lLayerVersion);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElementBinormal->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElementBinormal->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElementBinormal->GetReferenceMode()));

				unsigned int lBinormalCount = lLayerElementBinormal->GetDirectArray().GetCount();
				if( lBinormalCount > 0 )
				{
					// Not using any of our WriteValueArray() helpers because we're using 
					// a size/stride combo to skip copying of the data.
					FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementBinormal->GetDirectArray();
					FbxLayerElementArrayReadLock<FbxVector4>  lData(lDirectArray);

					mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_BINORMALS);
					mFileObject->FieldWriteArrayD(lBinormalCount, (const double*) lData.GetData(), 3, (int) sizeof(FbxVector4));
					mFileObject->FieldWriteEnd();

                    if (lLayerVersion >= 102)
                    {
                        // write the 4th component of the binormals array
                        FbxArray<double> lFourth;
                        lFourth.Reserve(lBinormalCount);
                        for (unsigned int j = 0; j < lBinormalCount; j++)
                        {
                            lFourth.SetAt(j, ((FbxVector4*)lData.GetData())[j][3]);
                        }
                        WriteValueArray(FIELD_KFBXGEOMETRYMESH_BINORMALS_WCOMPONENT, lBinormalCount, lFourth.GetArray());
                        mFileObject->FieldWriteEnd();
                    }
				}

				// Write the index if the mapping type is index to direct
				if(lLayerElementBinormal->GetReferenceMode() != FbxLayerElement::eDirect)
				{
					WriteValueArray(FIELD_KFBXGEOMETRYMESH_BINORMALS_INDEX, lLayerElementBinormal->GetIndexArray());
				}
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxLayerElementTangents(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eTangent);
    
    // New 102 version writes all 4 fields instead of only x,y,z but in 2 separate arrays to remain backward compatible
    unsigned int lLayerVersion = (FbxFileVersionStrToInt(mWriter->mFileVersion) >= 201400) ? 102 : 101;
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
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, lLayerVersion);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElementTangent->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElementTangent->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElementTangent->GetReferenceMode()));

				unsigned int lTangentCount = lLayerElementTangent->GetDirectArray().GetCount();
				if( lTangentCount > 0 )
				{
					// Not using any of our WriteValueArray() helpers because we're using 
					// a size/stride combo to skip copying of the data.
					FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementTangent->GetDirectArray();
					FbxLayerElementArrayReadLock<FbxVector4>  lData(lDirectArray);

					mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_TANGENTS);
					mFileObject->FieldWriteArrayD(lTangentCount, (const double*) lData.GetData(), 3, (int) sizeof(FbxVector4));
					mFileObject->FieldWriteEnd();

                    if (lLayerVersion >= 102)
                    {
                        // write the 4th component of the tangent array
                        FbxArray<double> lFourth;
                        lFourth.Reserve(lTangentCount);
                        for (unsigned int j = 0; j < lTangentCount; j++)
                        {
                            lFourth.SetAt(j, ((FbxVector4*)lData.GetData())[j][3]);
                        }
                        WriteValueArray(FIELD_KFBXGEOMETRYMESH_TANGENTS_WCOMPONENT, lTangentCount, lFourth.GetArray());
                    }
				}

				// Write the index if the mapping type is index to direct
				if(lLayerElementTangent->GetReferenceMode() != FbxLayerElement::eDirect)
				{
					WriteValueArray(FIELD_KFBXGEOMETRYMESH_TANGENTS_INDEX, lLayerElementTangent->GetIndexArray());
				}
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxLayerElementMaterials(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eMaterial);

    for(int i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementMaterial*        lLayerElementMaterial = pLayerContainer.GetLayer(i, FbxLayerElement::eMaterial)->GetMaterials();
        FbxLayerElement::EReferenceMode lReferenceMode = lLayerElementMaterial->GetReferenceMode();

        if(lReferenceMode == FbxLayerElement::eDirect) continue;

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

                int lMaterialCount    = lLayerElementMaterial->GetMappingMode() == FbxLayerElement::eAllSame? 1 : lLayerElementMaterial->GetIndexArray().GetCount();
                if( lMaterialCount > 0 )
                {
                    FbxLayerElementArrayTemplate<int>& lIndexArray = lLayerElementMaterial->GetIndexArray();
                    FbxLayerElementArrayReadLock<int>  lData(lIndexArray);

#ifdef _DEBUBG
                    for(int j=0; j<lMaterialCount; j++)
                    {
                        int lConnectionIndex = lData.GetData()[j];
                        FBX_ASSERT( lConnectionIndex >= -1 );
                    }
#endif
                    WriteValueArray(FIELD_KFBXGEOMETRYMESH_MATERIALS_ID, lMaterialCount, lData.GetData());
                }
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}


bool FbxWriterFbx7_Impl::WriteFbxLayerElementPolygonGroups(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::ePolygonGroup);

    for(i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementPolygonGroup* lLayerElement = pLayerContainer.GetLayer(i, FbxLayerElement::ePolygonGroup)->GetPolygonGroups();

        if(lLayerElement->GetReferenceMode() == FbxLayerElement::eDirect) continue;

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

                WriteValueArray(FIELD_KFBXGEOMETRYMESH_POLYGON_GROUP, lLayerElement->GetIndexArray());
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}



bool FbxWriterFbx7_Impl::WriteFbxLayerElementVertexColors(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eVertexColor);

    for(i=0; i<lLayerElementCount; i++)
    {
        FbxLayerElementVertexColor* lLayerElement = pLayerContainer.GetLayer(i, FbxLayerElement::eVertexColor)->GetVertexColors();

        if(lLayerElement->GetReferenceMode() == FbxLayerElement::eIndex) continue;

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
                int lElementCount = lLayerElement->GetDirectArray().GetCount();
                if( lElementCount > 0 )
                {                
                    // Static assert -- FbxColor should be the same as an array of 4 doubles.
                    // If it's not the case, this will not compile.
                    // We check the size and the layout of the struct.
                    FBX_ASSERT_STATIC(sizeof(FbxColor) == sizeof(FbxDouble4));

                    // Assert to check the positions of the color in the FbxColor class.
					// Liang: The assert was moved to the unit test for FbxColor to avoid gcc warnings for the offsetof

                    FbxLayerElementArrayTemplate<FbxColor>& lDirectArray = lLayerElement->GetDirectArray();
                    FbxLayerElementArrayReadLock<FbxColor>  lData(lDirectArray);

                    WriteValueArray(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_VALUES, lElementCount*4, (double *) lData.GetData());
                }

                if(lLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                {
                    WriteValueArray(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_INDEX, lLayerElement->GetIndexArray());
                }
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}                                                           


bool FbxWriterFbx7_Impl::WriteFbxLayerElementUVsChannel( FbxLayerContainer& pLayerContainer, FbxLayerElement::EType pTextureType, FbxMultiMap& pLayerIndexSet )
{
    int i, lLayerCount = pLayerContainer.GetLayerCount();

    //We should write for the number of UVs
    //and not for the number of textures, as we do have non-associated
    //UVs
    //LVV
    int relIndex = 0;
    for(i=0; i<lLayerCount; i++)
    {
        FbxLayer* lLayer = pLayerContainer.GetLayer(i);
        if(lLayer==NULL)
            continue;
        FbxLayerElementUV* lLayerElement= lLayer->GetUVs(pTextureType);

        if(lLayerElement==NULL)
            continue;

        if(lLayerElement->GetReferenceMode() == FbxLayerElement::eIndex) continue;

        pLayerIndexSet.Add((FbxHandle)lLayerElement, relIndex);

        mFileObject->FieldWriteBegin(FbxLayerElement::sTextureUVNames[FBXSDK_TEXTURE_INDEX(pTextureType)]);
        {
            // Layer Element index
            mFileObject->FieldWriteI(relIndex++);

            mFileObject->FieldWriteBlockBegin();
            {
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                // Write the UV values
                if(lLayerElement->GetReferenceMode() == FbxLayerElement::eDirect || lLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                {
                    int lElementCount = lLayerElement->GetDirectArray().GetCount();
                    if( lElementCount > 0 )
                    {
                        FbxLayerElementArrayTemplate<FbxVector2>& lDirectArray = lLayerElement->GetDirectArray();
                        FbxLayerElementArrayReadLock<FbxVector2>  lData(lDirectArray);

                        mFileObject->FieldWriteBegin(FIELD_KFBXLAYER_UV);
                            mFileObject->FieldWriteArrayD(lElementCount * 2, (const double*) lData.GetData());
                        mFileObject->FieldWriteEnd();
                    }
                }

                // Write the index if the mapping type is index to direct
                if(lLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                {
                    WriteValueArray(FIELD_KFBXLAYER_UV_INDEX, lLayerElement->GetIndexArray());
                }
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }
    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxLayerElementUVs(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    //for now only write the default ElementUV
    /*
        the Default UV channel LayerElementUV is the Diffuse channel one
    */

    int lLayerIndex;
    FBXSDK_FOR_EACH_TEXTURE(lLayerIndex)
    {
        WriteFbxLayerElementUVsChannel(pLayerContainer, FBXSDK_TEXTURE_TYPE(lLayerIndex), pLayerIndexSet);
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxLayerElementSmoothing(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eSmoothing);

    for(i=0; i<lLayerElementCount; i++)
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
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 102);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                WriteValueArray(FIELD_KFBXGEOMETRYMESH_SMOOTHING, lLayerElement->GetDirectArray());
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxLayerElementUserData(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eUserData);

    for(i=0; i<lLayerElementCount; i++)
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

                for( int j = 0; j < lLayerElement->GetDirectArrayCount(); ++j )
                {
                    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_USER_DATA_ARRAY);
                    mFileObject->FieldWriteBlockBegin();
                    {
                        // Write out the data type
                        FbxString lDataTypeName( lLayerElement->GetDataType(j).GetName() );
                        mFileObject->FieldWriteC( FIELD_KFBXGEOMETRYMESH_USER_DATA_TYPE, lDataTypeName );

                        // Write out the data name
                        FbxString lNameCopy( lLayerElement->GetDataName(j) );
                        mFileObject->FieldWriteC( FIELD_KFBXGEOMETRYMESH_USER_DATA_NAME, lNameCopy );

                        // Write the user data values
                        int lCount = lLayerElement->GetArrayCount(j);
                        if( lCount > 0 )
                        {
                            mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYMESH_USER_DATA);

                            switch( lLayerElement->GetDataType(j).GetType() )
                            {
                                case eFbxBool:    WriteValueArray( FbxGetDirectArray<bool>(lLayerElement, j) );      break;
                                case eFbxInt: WriteValueArray( FbxGetDirectArray<int>(lLayerElement, j) );       break;
                                case eFbxFloat:   WriteValueArray( FbxGetDirectArray<float>(lLayerElement, j) );     break;
                                case eFbxDouble:  WriteValueArray( FbxGetDirectArray<double>(lLayerElement, j) );    break;

                                default: FBX_ASSERT_NOW("Unsupported User Data type."); break;
                            }

                            mFileObject->FieldWriteEnd();
                        }
                    }
                    mFileObject->FieldWriteBlockEnd();
                    mFileObject->FieldWriteEnd();
                }

                // Write the index if the mapping type is index to direct
                if(lLayerElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                {
                    WriteValueArray(FIELD_KFBXGEOMETRYMESH_USER_DATA_INDEX, lLayerElement->GetIndexArray());
                }
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;

}

bool FbxWriterFbx7_Impl::WriteFbxLayerElementVisibility(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lLayerElementCount = pLayerContainer.GetLayerCount(FbxLayerElement::eVisibility);

    for(i=0; i<lLayerElementCount; i++)
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

                WriteValueArray(FIELD_KFBXGEOMETRYMESH_VISIBILITY, lLayerElement->GetDirectArray());
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;

}

bool FbxWriterFbx7_Impl::WriteFbxLayerElementEdgeCrease(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
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
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                WriteValueArray(FIELD_KFBXGEOMETRYMESH_EDGE_CREASE, lLayerElement->GetDirectArray());
            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;

}

bool FbxWriterFbx7_Impl::WriteFbxLayerElementVertexCrease(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
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
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                WriteValueArray(FIELD_KFBXGEOMETRYMESH_VERTEX_CREASE, lLayerElement->GetDirectArray());

            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;

}


bool FbxWriterFbx7_Impl::WriteFbxLayerElementHole(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
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
                mFileObject->FieldWriteI(FIELD_KFBXLAYER_ELEMENT_VERSION, 101);
                mFileObject->FieldWriteS(FIELD_KFBXLAYER_ELEMENT_NAME, lLayerElement->GetName());

                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE,   GetMappingModeToken(lLayerElement->GetMappingMode()));
                mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE, GetReferenceModeToken(lLayerElement->GetReferenceMode()));

                WriteValueArray(FIELD_KFBXGEOMETRYMESH_HOLE, lLayerElement->GetDirectArray());

            }
            mFileObject->FieldWriteBlockEnd();
        }
        mFileObject->FieldWriteEnd();
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteLayers(FbxLayerContainer& pLayerContainer, FbxMultiMap& pLayerIndexSet)
{
    int i, lSavedLayerCount = 0, lLayerCount = pLayerContainer.GetLayerCount();

    for(i=0; i<lLayerCount; i++)
    {
        FbxLayer* lLayer = pLayerContainer.GetLayer(i);

        //Make sure there is some layer element in the layer
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
                        mFileObject->FieldWriteC(FIELD_KFBXLAYER_ELEMENT_TYPE,        FbxLayerElement::sNonTextureNames[lLayerIndex]);
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

/*
int  FbxWriterFbx7::MapLayeredMaterialIndexToConnectionIndex(FbxNode* pNode, void* pLEM, int pIndex)
{
    FbxLayerElementMaterial*       lLEM            = static_cast<FbxLayerElementMaterial*>(pLEM);
    int                             lLayeredIndex   = lLEM->GetIndexArray().GetAt(pIndex);
    FbxSurfaceMaterial* lMaterial= lLEM->GetDirectArray().GetAt(lLayeredIndex);

    for (int i=0; i<FbxGetSrcCount<FbxSurfaceMaterial>(pNode); i++) {
        if (lMaterial==FbxGetSrc<FbxSurfaceMaterial>(pNode,i)) {
            return i;
        }
    }
    return -1;
}
*/

int  FbxWriterFbx7_Impl::MapLayeredTextureIndexToConnectionIndex(FbxNode* pNode, void* pLET, int pIndex)
{
    FbxLayerElementTexture* lLET   = static_cast<FbxLayerElementTexture*>(pLET);
    int           lLayeredIndex     = lLET->GetIndexArray().GetAt(pIndex);
    FbxTexture*  lTexture          = lLET->GetDirectArray().GetAt(lLayeredIndex);

	for(int i=0; i < pNode->GetSrcObjectCount<FbxTexture>(); i++)
	{
        if( lTexture == pNode->GetSrcObject<FbxTexture>(i) )
		{
            return i;
        }
    }
    return -1;
}

bool FbxWriterFbx7_Impl::WriteMeshEdges(FbxMesh& pMesh)
{
    //
    // Only write them out if we have some data
    //
    if( pMesh.GetMeshEdgeCount() )
    {
        WriteValueArray(FIELD_KFBXGEOMETRYMESH_EDGES, pMesh.GetMeshEdgeCount(), pMesh.mEdgeArray.GetArray());
    }

    return true;
}

#ifdef FBXSDK_SUPPORT_INTERNALEDGES
bool FbxWriterFbx7_Impl::WriteMeshInternalEdges(FbxMesh& pMesh)
{
    // Bail out if we are writing before FBX 2014 version
    if (FbxFileVersionStrToInt(mWriter->mFileVersion) < 201400)
        return false;

    //
    // Only write them out if we have some data
    //
    int lInternalEdgeCount = pMesh.mInternalEdgeArray.GetCount();
    
    if(lInternalEdgeCount > 0 )
    {
        FbxArray<int> lInternalEdgesAsInt;

        // Convert to int so we can export it easily
        lInternalEdgesAsInt.Reserve(lInternalEdgeCount * 2);
        for(int i = 0, j = 0; i < lInternalEdgeCount; ++i, j+=2)
        {
            lInternalEdgesAsInt.SetAt(  j, pMesh.mInternalEdgeArray.GetAt(i).mStartingPoint);
            lInternalEdgesAsInt.SetAt(j+1, pMesh.mInternalEdgeArray.GetAt(i).mEndingPoint);
        }

        WriteValueArray(FIELD_KFBXGEOMETRYMESH_INTERNAL_EDGES, lInternalEdgesAsInt.GetCount(), lInternalEdgesAsInt.GetArray());
    }

    return true;
}
#endif

bool FbxWriterFbx7_Impl::WriteMesh(FbxMesh& pMesh)
{
    //
    // Store what is needed for a mesh
    //
    if(pMesh.GetControlPointsCount ())
    {
        WriteMeshSmoothness(pMesh);
        WriteMeshVertices(pMesh);
        WriteMeshPolyVertexIndex(pMesh);
        WriteMeshEdges(pMesh);
#ifdef FBXSDK_SUPPORT_INTERNALEDGES
        WriteMeshInternalEdges(pMesh);
#endif
        WriteGeometry(pMesh);
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteTrimNurbsSurface(FbxTrimNurbsSurface& pNurbs)
{
    // we need a untrimmed surface and at least 1 boundary, with at least 1 curve to be valid.
    if( pNurbs.GetSrcObjectCount<FbxNurbsSurface>() < 1 || pNurbs.GetBoundaryCount() < 1 || pNurbs.GetBoundary(0)->GetCurveCount() < 1 )
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

bool FbxWriterFbx7_Impl::WriteBoundary( FbxBoundary& pBoundary )
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

bool FbxWriterFbx7_Impl::WriteLine( FbxLine& pLine )
{
    // only write if we have some control points to write out
    if( pLine.GetControlPointsCount() )
    {
        // write out all the good geometry stuff
        WriteGeometry( pLine );

        mFileObject->FieldWriteC(FIELD_KFBXNODE_TYPE, FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_LINE );

        mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYLINE_VERSION, 100);

        // control points
        //
        FbxAMatrix lPivot;
        pLine.GetPivot( lPivot );

        WriteControlPoints(FIELD_KFBXGEOMETRYLINE_POINTS, pLine.GetControlPointsCount(), pLine.GetControlPoints(), lPivot, false);

        // Index Array
        // Only write them out if we have some data
        //
        int lIndexSize = pLine.GetIndexArraySize();
        FbxArray<int>* lIndexArray = pLine.GetIndexArray();
        int lEndPointCount = pLine.GetEndPointCount();
        if( lIndexArray && lIndexSize )
        {
            int* lFixedIndexArray = FbxNewArray<int>(lIndexSize);
            int* lSrc         = lIndexArray->GetArray();
            int* lDest        = lFixedIndexArray;
            for(int i = 0; i < lIndexSize; i++,lSrc++, lDest++)
            {
                int Index = *lSrc;
                for(int j = 0; j < lEndPointCount; j++)
                {
                    if(pLine.GetEndPointAt(j) == i && Index >= 0)
                        Index = -Index - 1;
                }
                *lDest = Index;
            }

            WriteValueArray(FIELD_KFBXGEOMETRYLINE_POINTS_INDEX, lIndexSize, lFixedIndexArray);
        }

        // Properties
        WriteObjectPropertiesAndFlags(&pLine);
    }

    return true;
}

void FbxWriterFbx7_Impl::WriteControlPoints(const char* pFieldName, int pCount, const FbxVector4* pControlPoints, const FbxAMatrix& pPivot, bool pOutputWeight)
{
    FBX_ASSERT( pFieldName );
    FBX_ASSERT( pControlPoints );

    if( pCount > 0 )
    {
        static FbxAMatrix  sIdentity;
#ifdef _DEBUG
        static bool sComparedIdentity;
        if( !sComparedIdentity )
        {
            FbxAMatrix lSecondMatrix;
            lSecondMatrix.SetIdentity();

            FBX_ASSERT( lSecondMatrix == sIdentity );
            sComparedIdentity = true;
        }
#endif

        if( pPivot != sIdentity )
        {
	        int     lPointsCount = pCount * (pOutputWeight ? 4 : 3);
	        double* lDestPoints  = FbxNewArray<double>(lPointsCount);

            FbxVector4 lDestPoint;

            double* lDest = lDestPoints;
            for( const FbxVector4* lSrc = pControlPoints, *lEnd = pControlPoints + pCount; lSrc != lEnd; ++lSrc )
            {
                FbxVector4 lSrcPoint = lSrc->mData;
				FbxAMatrix lPivot = pPivot;
				lDestPoint = lPivot.MultT(lSrcPoint);

                *lDest++ = lDestPoint.mData[0];     // X
                *lDest++ = lDestPoint.mData[1];     // Y
                *lDest++ = lDestPoint.mData[2];     // Z

                if( pOutputWeight )
                {
                    *lDest++ = lSrc->mData[3];      // Weight
                }
            }

            WriteValueArray(pFieldName, lPointsCount, lDestPoints);
            FbxDeleteArray(lDestPoints);
        }
        else
        {
            // No need to create a temp array; there's no pivot in the way.
            if( pOutputWeight )
            {
                WriteValueArray(pFieldName, pCount * 4, (const double*)pControlPoints);
            }
            else
            {
                // Write 3 doubles, skip over the weight.
                mFileObject->FieldWriteBegin(pFieldName);
                    mFileObject->FieldWriteArrayD(pCount, (const double*) pControlPoints, 3, 4 * sizeof(double));
                mFileObject->FieldWriteEnd();
            }
        }
    }
}

bool FbxWriterFbx7_Impl::WriteNurbsCurve(FbxNurbsCurve& pNurbs)
{
    // only write if we have some control points to writeout
    if( pNurbs.GetControlPointsCount() )
    {
        // write out all the good geometry stuff
        WriteGeometry( pNurbs );

        mFileObject->FieldWriteC(FIELD_KFBXNODE_TYPE, "NurbsCurve");

        mFileObject->FieldWriteI(FIELD_KFBXGEOMETRYNURBS_CURVE_VERSION, 100);

        // order
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_CURVE_ORDER);
        mFileObject->FieldWriteI(pNurbs.GetOrder());
        mFileObject->FieldWriteEnd ();

        // dimension
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_CURVE_DIMENSION);
        mFileObject->FieldWriteI( (int)pNurbs.GetDimension() );
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
        else
        { // must be open
            mFileObject->FieldWriteC("Open");
        }
        mFileObject->FieldWriteEnd ();

        // rational
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_CURVE_RATIONAL);
        mFileObject->FieldWriteB( pNurbs.IsRational() );
        mFileObject->FieldWriteEnd ();

        // span.. we dont really need to save this, we can compute it from the degree and number of control points

        // control points

        FbxAMatrix lPivot;
        pNurbs.GetPivot( lPivot );

        WriteControlPoints(FIELD_KFBXGEOMETRYNURBS_CURVE_POINTS, pNurbs.GetControlPointsCount(), pNurbs.GetControlPoints(), lPivot);

        // Knot Vector
        //

        WriteValueArray(FIELD_KFBXGEOMETRYNURBS_CURVE_KNOTVECTOR, pNurbs.GetKnotCount(), pNurbs.GetKnotVector());
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteNurb(FbxNurbs& pNurbs)
{
    FbxNurbs* lNurbs;

    if(pNurbs.GetApplyFlip())
    {
        FbxGeometryConverter lConverter(&mManager);
        lNurbs = lConverter.FlipNurbs(&pNurbs, pNurbs.GetApplyFlipUV(), pNurbs.GetApplyFlipLinks());
    }
    else
    {
        lNurbs = &pNurbs;
    }

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
        mFileObject->FieldWriteI(lNurbs->GetSurfaceMode());
        mFileObject->FieldWriteI(lNurbs->GetUStep());
        mFileObject->FieldWriteI(lNurbs->GetVStep());
        mFileObject->FieldWriteEnd ();

        //
        // NURB ORDER...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURB_NURB_ORDER);
        mFileObject->FieldWriteI(lNurbs->GetUOrder());
        mFileObject->FieldWriteI(lNurbs->GetVOrder());
        mFileObject->FieldWriteEnd ();

        //
        // Surface DIMENSION
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURB_DIMENSION);
        mFileObject->FieldWriteI(lNurbs->GetUCount());
        mFileObject->FieldWriteI(lNurbs->GetVCount());
        mFileObject->FieldWriteEnd ();

        //
        // STEP
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURB_STEP);
        mFileObject->FieldWriteI(lNurbs->GetUStep());
        mFileObject->FieldWriteI(lNurbs->GetVStep());
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

        FbxAMatrix lPivot;
        lNurbs->GetPivot(lPivot);

        WriteControlPoints(FIELD_KFBXGEOMETRYNURB_POINTS, lNurbs->GetControlPointsCount(), lNurbs->GetControlPoints(), lPivot);

        FBX_ASSERT_MSG((lNurbs->GetUMultiplicityVector()!=NULL) && (lNurbs->GetVMultiplicityVector()!=NULL), "FbxWriterFbx7::WriteNurb : Null multiplicity vector.");

        //
        // MultiplicityU...
        //

        WriteValueArray(FIELD_KFBXGEOMETRYNURB_MULTIPLICITY_U, lNurbs->GetUCount(), lNurbs->GetUMultiplicityVector());

        //
        // MultiplicityV...
        //

        WriteValueArray(FIELD_KFBXGEOMETRYNURB_MULTIPLICITY_V, lNurbs->GetVCount(), lNurbs->GetVMultiplicityVector());

        FBX_ASSERT_MSG((lNurbs->GetUKnotVector()!=NULL) && (lNurbs->GetVKnotVector()!=NULL), "FbxWriterFbx7::WriteNurb : Null knot vector.");

        //
        // Knot Vector U
        //

        WriteValueArray(FIELD_KFBXGEOMETRYNURB_KNOTVECTOR_U, pNurbs.GetUKnotCount(), lNurbs->GetUKnotVector());

        //
        // Knot Vector V
        //
        WriteValueArray(FIELD_KFBXGEOMETRYNURB_KNOTVECTOR_V, pNurbs.GetVKnotCount(), lNurbs->GetVKnotVector());

        WriteGeometry(*lNurbs);

    }

    if(pNurbs.GetApplyFlip())
    {
        lNurbs->Destroy();
    }

    return true;
}


bool FbxWriterFbx7_Impl::WriteNurbsSurface(FbxNurbsSurface& pNurbs)
{
    FbxNurbsSurface* lNurbs;

    if(pNurbs.GetApplyFlip())
    {
        FbxGeometryConverter lConverter(&mManager);
        lNurbs = lConverter.FlipNurbsSurface(&pNurbs, pNurbs.GetApplyFlipUV(), pNurbs.GetApplyFlipLinks());
    }
    else
    {
        lNurbs = &pNurbs;
    }

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
        mFileObject->FieldWriteI(lNurbs->GetSurfaceMode());
        mFileObject->FieldWriteI(lNurbs->GetUStep());
        mFileObject->FieldWriteI(lNurbs->GetVStep());
        mFileObject->FieldWriteEnd ();

        //
        // NURB ORDER...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_SURFACE_NURB_ORDER);
        mFileObject->FieldWriteI(lNurbs->GetUOrder());
        mFileObject->FieldWriteI(lNurbs->GetVOrder());
        mFileObject->FieldWriteEnd ();

        //
        // Surface DIMENSION
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYNURBS_SURFACE_DIMENSION);
        mFileObject->FieldWriteI(lNurbs->GetUCount());
        mFileObject->FieldWriteI(lNurbs->GetVCount());
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

        mFileObject->FieldWriteEnd ();

        //
        // Control points
        //

        FbxAMatrix lPivot;
        lNurbs->GetPivot(lPivot);

        WriteControlPoints(FIELD_KFBXGEOMETRYNURBS_SURFACE_POINTS, lNurbs->GetControlPointsCount(), lNurbs->GetControlPoints(), lPivot);

        FBX_ASSERT_MSG((lNurbs->GetUKnotVector()!=NULL) && (lNurbs->GetVKnotVector()!=NULL), "FbxWriterFbx7::WriteNurb : Null knot vector.");

        //
        // Knot Vector U
        //
        WriteValueArray(FIELD_KFBXGEOMETRYNURBS_SURFACE_KNOTVECTOR_U, lNurbs->GetUKnotCount(), lNurbs->GetUKnotVector());

        //
        // Knot Vector V
        //
        WriteValueArray(FIELD_KFBXGEOMETRYNURBS_SURFACE_KNOTVECTOR_V, lNurbs->GetVKnotCount(), lNurbs->GetVKnotVector());

        WriteGeometry(*lNurbs);

        //
        // FlipNormal flag
        //
        mFileObject->FieldWriteI( FIELD_KFBXGEOMETRYNURBS_SURFACE_FLIP_NORMALS, (int) lNurbs->GetFlipNormals() );

    }

    if(pNurbs.GetApplyFlip())
    {
        lNurbs->Destroy();
    }

    return true;
}


bool FbxWriterFbx7_Impl::WritePatch(FbxPatch& pPatch)
{
    //
    // Store the Patch...
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
        mFileObject->FieldWriteI(pPatch.GetSurfaceMode());
        mFileObject->FieldWriteI(pPatch.GetUStep());
        mFileObject->FieldWriteI(pPatch.GetVStep());
        mFileObject->FieldWriteEnd ();

        //
        // Store the PATCHTYPE...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_PATCH_TYPE);
        WritePatchType(pPatch, pPatch.GetPatchUType());
        WritePatchType(pPatch, pPatch.GetPatchVType());
        mFileObject->FieldWriteEnd();

        //
        // Store the DIMENSION...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_DIMENSIONS);
        mFileObject->FieldWriteI(pPatch.GetUCount());
        mFileObject->FieldWriteI(pPatch.GetVCount());
        mFileObject->FieldWriteEnd ();

        //
        // Store the STEP...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_STEP);
        mFileObject->FieldWriteI(pPatch.GetUStep());
        mFileObject->FieldWriteI(pPatch.GetVStep());
        mFileObject->FieldWriteEnd ();

        //
        // Store the CLOSED...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_CLOSED);
        mFileObject->FieldWriteI(pPatch.GetUClosed());
        mFileObject->FieldWriteI(pPatch.GetVClosed());
        mFileObject->FieldWriteEnd ();

        //
        // Store the UCAPPED...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_UCAPPED);
        mFileObject->FieldWriteI(pPatch.GetUCappedBottom());
        mFileObject->FieldWriteI(pPatch.GetUCappedTop());
        mFileObject->FieldWriteEnd ();

        //
        // Store the VCAPPED...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRYPATCH_VCAPPED);
        mFileObject->FieldWriteI(pPatch.GetVCappedBottom());
        mFileObject->FieldWriteI(pPatch.GetVCappedTop());
        mFileObject->FieldWriteEnd ();


        //
        // Store the Control points
        //
        FbxAMatrix lPivot;
        pPatch.GetPivot(lPivot);

        WriteControlPoints(FIELD_KFBXGEOMETRYPATCH_POINTS, pPatch.GetControlPointsCount(), pPatch.GetControlPoints(), lPivot, false);

        WriteGeometry(pPatch);
    }

    return true;
}

void FbxWriterFbx7_Impl::WriteGeometryWeightedMap(FbxGeometryWeightedMap& pGeometryWeightedMap)
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
    mFileObject->FieldWriteI(lSrcCount);
    mFileObject->FieldWriteEnd ();

    //
    // Store destination count...
    //
    mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRY_WEIGHTED_MAP_DST_COUNT);
    mFileObject->FieldWriteI(lDstCount);
    mFileObject->FieldWriteEnd ();


    FbxWeightedMapping::Element   lElem;
    int                             i, j, lRelCount;

    for(i = 0; i < lSrcCount; i++)
    {
        lRelCount = lMapping->GetRelationCount(FbxWeightedMapping::eSource, i);

        if(lRelCount < 1)
        {
            continue;
        }

        //
        // Store index mapping...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXGEOMETRY_WEIGHTED_MAP_INDEX_MAPPING);

        //
        // Store source Index
        //
        mFileObject->FieldWriteI(i);

        //
        // Store relation count
        //
        mFileObject->FieldWriteI(lRelCount);

        for(j = 0; j < lRelCount; j++)
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

        mFileObject->FieldWriteEnd();
    }

}

bool FbxWriterFbx7_Impl::WriteDeformers(FbxDocument* pTopDocument)
{
    FBX_ASSERT_RETURN_VALUE(pTopDocument, false);

	WriteFbxObjects(pTopDocument, FBX_TYPE(FbxSkin));
	WriteFbxObjects(pTopDocument, FBX_TYPE(FbxCluster));
	WriteFbxObjects(pTopDocument, FBX_TYPE(FbxVertexCacheDeformer));

	WriteFbxObjects(pTopDocument, FBX_TYPE(FbxBlendShape));
	WriteFbxObjects(pTopDocument, FBX_TYPE(FbxBlendShapeChannel));

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxVertexCacheDeformer& pDeformer)
{
    WriteObjectHeaderAndReferenceIfAny(pDeformer, FIELD_KFBXDEFORMER_DEFORMER);
    mFileObject->FieldWriteBlockBegin();
    {
        mFileObject->FieldWriteI(FILED_KFBXVERTEXCACHEDEFORMER_VERSION, 100);
        WriteObjectPropertiesAndFlags(&pDeformer);
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxSkin& pSkin)
{
    WriteObjectHeaderAndReferenceIfAny(pSkin, FIELD_KFBXDEFORMER_DEFORMER);
    mFileObject->FieldWriteBlockBegin();
    {
        mFileObject->FieldWriteI(FIELD_KFBXSKIN_VERSION, 101);
        WriteObjectPropertiesAndFlags(&pSkin);

		mFileObject->FieldWriteD(FIELD_KFBXSKIN_DEFORM_ACCURACY, pSkin.GetDeformAccuracy());
		
		if(FbxFileVersionStrToInt(mWriter->mFileVersion) > 201100)
		{
			//
			// Store the Field SkinningType...
			//
			switch(pSkin.GetSkinningType())
			{
			case FbxSkin::eRigid:
				break;

			case FbxSkin::eLinear:
				mFileObject->FieldWriteC(FIELD_KFBXSKIN_SKINNINGTYPE, TOKEN_KFBXSKIN_LINEAR);
				break;

			case FbxSkin::eDualQuaternion:
				mFileObject->FieldWriteC(FIELD_KFBXSKIN_SKINNINGTYPE, TOKEN_KFBXSKIN_DUALQUATERNION);
				break;

			case FbxSkin::eBlend:
				mFileObject->FieldWriteC(FIELD_KFBXSKIN_SKINNINGTYPE, TOKEN_KFBXSKIN_BLEND);
				break;

			default:
				FBX_ASSERT_NOW("Unexpected skinning type.");
				break;
			}

			//
			// Store the Field INDICES...
			//
			WriteValueArray(FIELD_KFBXSKIN_INDEXES, pSkin.GetControlPointIndicesCount(), pSkin.GetControlPointIndices());

			//
			// Store the Field BLENDWEIGHTS...
			//
			if(pSkin.GetSkinningType() == FbxSkin::eBlend)
			{
				WriteValueArray(FIELD_KFBXSKIN_BLENDWEIGHTS, pSkin.GetControlPointIndicesCount(), pSkin.GetControlPointBlendWeights());
			}
		}
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxCluster& pCluster)
{
    WriteObjectHeaderAndReferenceIfAny(pCluster, FIELD_KFBXDEFORMER_DEFORMER);
    mFileObject->FieldWriteBlockBegin();
    {
        mFileObject->FieldWriteI(FIELD_KFBXCLUSTER_VERSION, 100);
        WriteObjectPropertiesAndFlags(&pCluster);

        //
        // Store the Field MODE...
        //
        switch(pCluster.GetLinkMode())
        {
			case FbxCluster::eNormalize:
				break;

			case FbxCluster::eAdditive:
				mFileObject->FieldWriteC(FIELD_KFBXCLUSTER_MODE, TOKEN_KFBXCLUSTER_ADDITIVE);
				break;

			case FbxCluster::eTotalOne:
				mFileObject->FieldWriteC(FIELD_KFBXCLUSTER_MODE, TOKEN_KFBXCLUSTER_TOTAL1);
				break;

			default:
				FBX_ASSERT_NOW("Unexpected deformer mode.");
				break;
        }

        //
        // Store the Field USERDATA...
        //
        mFileObject->FieldWriteBegin(FIELD_KFBXCLUSTER_USERDATA);
        {
            mFileObject->FieldWriteC(pCluster.GetUserDataID());
            mFileObject->FieldWriteC(pCluster.GetUserData());
        }
        mFileObject->FieldWriteEnd();

        //
        // Store the Field INDICES...
        //
        WriteValueArray(FIELD_KFBXCLUSTER_INDEXES, pCluster.GetControlPointIndicesCount(), pCluster.GetControlPointIndices());

        //
        // Store the Field WEIGHTS...
        //
        WriteValueArray(FIELD_KFBXCLUSTER_WEIGHTS, pCluster.GetControlPointIndicesCount(), pCluster.GetControlPointWeights());

		FbxAMatrix Transform;
        pCluster.GetTransformMatrix(Transform);
		FbxAMatrix TransformLink;
        pCluster.GetTransformLinkMatrix(TransformLink);
		Transform = TransformLink.Inverse() * Transform;

        //
        // Store the transformation matrix
        //
        WriteValueArray(FIELD_KFBXCLUSTER_TRANSFORM, 16, (double*)Transform.mData);

        //
        // Store the transformation link matrix
        //
        WriteValueArray(FIELD_KFBXCLUSTER_TRANSFORM_LINK, 16, (double*)TransformLink.mData);

        //
        // Associate Model
        //
        FbxProperty lProperty = pCluster.FindProperty("SrcModelReference");
        if(lProperty.IsValid())
        {
            FbxNode* lNode = lProperty.GetSrcObject<FbxNode>();
            if(lNode)
            {
                mFileObject->FieldWriteBegin(FIELD_KFBXCLUSTER_ASSOCIATE_MODEL);
                mFileObject->FieldWriteBlockBegin();
                {
					FbxAMatrix TransformAssociate;
                    pCluster.GetTransformAssociateModelMatrix(TransformAssociate);
					TransformAssociate = TransformLink.Inverse() * TransformAssociate;

                    //
                    // Store the transformation associate matrix
                    //
                    WriteValueArray(FIELD_KFBXCLUSTER_TRANSFORM, 16, (double*)TransformAssociate.mData);
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
            WriteValueArray(FIELD_KFBXCLUSTER_TRANSFORM, 16, (double*)TransformParent.mData);
        }
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxBlendShape& pBlendShape)
{
	WriteObjectHeaderAndReferenceIfAny(pBlendShape, FIELD_KFBXDEFORMER_DEFORMER);
	mFileObject->FieldWriteBlockBegin();
	{
		mFileObject->FieldWriteI(FIELD_KFBXBLENDSHAPE_VERSION, 100);
		WriteObjectPropertiesAndFlags(&pBlendShape);
	}

	mFileObject->FieldWriteBlockEnd();
	mFileObject->FieldWriteEnd();

	return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxBlendShapeChannel& pBlendShapeChannel)
{
	WriteObjectHeaderAndReferenceIfAny(pBlendShapeChannel, FIELD_KFBXDEFORMER_DEFORMER);
	mFileObject->FieldWriteBlockBegin();
	{
		mFileObject->FieldWriteI(FIELD_KFBXBLENDSHAPECHANNEL_VERSION, 100);
		WriteObjectPropertiesAndFlags(&pBlendShapeChannel);

		mFileObject->FieldWriteD(FIELD_KFBXBLENDSHAPECHANNEL_DEFORMPERCENT, pBlendShapeChannel.DeformPercent.Get());

		//
		// Store the Field Full Weight Array...
		//
		WriteValueArray(FIELD_KFBXBLENDSHAPECHANNEL_FULLWEIGHTS, pBlendShapeChannel.GetTargetShapeCount(), pBlendShapeChannel.GetTargetShapeFullWeights());
	}

	mFileObject->FieldWriteBlockEnd();
	mFileObject->FieldWriteEnd();

	return true;
}

bool FbxWriterFbx7_Impl::WriteShape(FbxShape& pShape)
{
	FbxGeometry* lGeometry = pShape.GetBaseGeometry();
	if(lGeometry)
	{
		FbxString pShapeName = pShape.GetName();

		FbxAMatrix lPivot;
		lGeometry->GetPivot(lPivot);
		FbxVector4 lSrcPoint;
		FbxVector4 lDestPoint;
		FbxVector4 lRefSrcPoint;
		FbxVector4 lRefDestPoint;
		int lCount;
		bool lControlPointsValid = true, lNormalsValid = true;
		FbxArray<int> lValidIndices;

		if(lGeometry && lGeometry->GetControlPointsCount() != pShape.GetControlPointsCount())
		{
			FBX_ASSERT_NOW("Control points in shape incompatible with control points in geometry.");
			lControlPointsValid = false;
			lValidIndices.Add(0);
		}
		else
		{
			FindShapeValidIndices(lGeometry->mControlPoints, pShape.mControlPoints, lValidIndices);

			if(lValidIndices.GetCount() == 0)
			{
				lControlPointsValid = false;
				lValidIndices.Add(0);
			}
		}

		lCount = lValidIndices.GetCount();

		if(FbxFileVersionStrToInt(mWriter->mFileVersion) == 201100)
		{
			mFileObject->FieldWriteBegin(FIELD_KFBXSHAPE_SHAPE);

			mFileObject->FieldWriteC(pShapeName);

			mFileObject->FieldWriteBlockBegin ();
		}
		else
		{
			mFileObject->FieldWriteI(FIELD_KFBXSHAPE_VERSION, 100);
		}

		//
		// Save the indices.
		//
		WriteValueArray(FIELD_KFBXSHAPE_INDEXES, lCount, lValidIndices.GetArray());

		FbxArray<FbxVector4> lWorkControlPoints;

		if(lControlPointsValid)
		{
			FbxArray<FbxVector4>& lGeometryControlPoints = lGeometry->mControlPoints;
			lWorkControlPoints = pShape.mControlPoints;

			//
			// Convert control points to relative vectors.
			//
			for(int i = 0; i < lCount; i++)
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
		{
			int	    lPointsCount   = lCount * 3;
			double* lControlPoints = FbxNewArray<double>(lPointsCount);


			if( lControlPointsValid )
			{
				for(int i = 0, j = 0; i < lCount; ++i /* j increment is in loop */)
				{
					const FbxVector4& lVector = lWorkControlPoints[lValidIndices[i]];
					lControlPoints[j++] = lVector[0];
					lControlPoints[j++] = lVector[1];
					lControlPoints[j++] = lVector[2];
				}
			}
            else
            {
                // zero out
                memset(lControlPoints, 0, sizeof(double) * lPointsCount);
            }

			WriteValueArray(FIELD_KFBXSHAPE_VERTICES, lPointsCount, lControlPoints);
			FbxDeleteArray(lControlPoints);
		}

		if (lGeometry->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			FbxMesh* lMesh = (FbxMesh*) lGeometry;
			FbxVector4* lWorkNormals = NULL;
			int* indices = NULL;
			int arrayCount = lCount;
			bool fatalCondition = (lMesh->GetLayer(0, FbxLayerElement::eNormal) == NULL || lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals() == NULL);
			if (fatalCondition ||  pShape.GetLayer(0, FbxLayerElement::eNormal) == NULL || pShape.GetLayer(0, FbxLayerElement::eNormal)->GetNormals() == NULL)
			{                
				// The case of the mesh not having the Normals layer is very improbable, however, the case of a shape without normals can
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
					arrayCount = lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetDirectArray().GetCount();
					if (lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetReferenceMode() != FbxLayerElement::eDirect)
						indices = lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetIndexArray().GetLocked(indices);

					lWorkNormals = lShapeNormals.GetLocked(lWorkNormals);
					if (lWorkNormals)
					{
						//
						// Convert the normals to relative vectors.
						//
						for (int i = 0; i < arrayCount; i++)
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
			if (fatalCondition)
			{
				FBX_ASSERT_NOW("FATAL CONDITION: Mesh object does not have normals.");
			}
			else
			{
				int     lNormalsCount = lCount * 3;
				double* lNormals      = FbxNewArray<double>(lNormalsCount);
                memset(lNormals, 0, lNormalsCount*sizeof(double));

				if (lNormalsValid && lWorkNormals)  // Else save all zeroes
				{
					int nbVertices = lMesh->GetPolygonVertexCount();
					bool byPolyVertex = lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetMappingMode() == FbxLayerElement::eByPolygonVertex;
					for (int i = 0, j = 0; i < lCount; i++ /* j increment is in loop */)
					{
						int index = lValidIndices[i];
						if (byPolyVertex)
						{
							// get the normal of the first vertex pointing to ctrlPt
							int k = 0;
							while (k < nbVertices && lMesh->GetPolygonVertices()[k] != index) k++;
							FBX_ASSERT(k < nbVertices);
							index = k;
						}
							
						if (indices)
							index = indices[index];

						FbxVector4& lVector = lWorkNormals[index];

						lNormals[j++] = lVector[0];
						lNormals[j++] = lVector[1];
						lNormals[j++] = lVector[2];
					}   
				}

				WriteValueArray(FIELD_KFBXSHAPE_NORMALS, lNormalsCount, lNormals);
				FbxDeleteArray(lNormals);
			} // fatalCondition

			if (lWorkNormals)
				pShape.GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetDirectArray().Release(&lWorkNormals, lWorkNormals);

			if (indices)
				lMesh->GetLayer(0, FbxLayerElement::eNormal)->GetNormals()->GetIndexArray().Release(&indices, indices);
		} // attribute == mesh

		if(FbxFileVersionStrToInt(mWriter->mFileVersion) == 201100)
		{
			mFileObject->FieldWriteBlockEnd ();
			mFileObject->FieldWriteEnd ();
		}
		else
		{
			// Properties
			WriteObjectPropertiesAndFlags(&pShape);
		}

		return true;
	}
	
	return false;
}



void FbxWriterFbx7_Impl::FindShapeValidIndices(FbxArray<FbxVector4>& pGeometryControlPoints,
                                               FbxArray<FbxVector4>& pShapeControlPoints,
                                               FbxArray<int>& lValidIndices)
{
    int i, lCount = pGeometryControlPoints.GetCount();

    for(i = 0; i < lCount; i++)
    {
        FbxVector4& lGeometryPoint = pGeometryControlPoints[i];
        FbxVector4& lShapePoint = pShapeControlPoints[i];

        if(!FbxEqual(lGeometryPoint[0], lShapePoint[0]) ||
           !FbxEqual(lGeometryPoint[1], lShapePoint[1]) ||
           !FbxEqual(lGeometryPoint[2], lShapePoint[2]))
        {
            lValidIndices.Add(i);
        }
    }
}

bool FbxWriterFbx7_Impl::WritePatchType(FbxPatch& pPatch, int pType)
{
    switch((FbxPatch::EType) pType)
    {
    case FbxPatch::eBezier:            mFileObject->FieldWriteC("Bezier");
        break;

    case FbxPatch::eBezierQuadric:   mFileObject->FieldWriteC("BezierQuadric");
        break;

    case FbxPatch::eCardinal:          mFileObject->FieldWriteC("Cardinal");
        break;

    case FbxPatch::eBSpline:           mFileObject->FieldWriteC("BSpline");
        break;

    case FbxPatch::eLinear:            mFileObject->FieldWriteC("Linear");
        break;
    }

    return true;
}


bool FbxWriterFbx7_Impl::WriteSkeleton(FbxSkeleton& pSkeleton)
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


bool FbxWriterFbx7_Impl::WriteSkeletonRoot(FbxSkeleton& pSkeleton)
{
    if(pSkeleton.GetNode())
    {
        return WriteNodeType(*pSkeleton.GetNode());
    }
    return false;
}


bool FbxWriterFbx7_Impl::WriteSkeletonLimb (FbxSkeleton& pSkeleton)
{
    if(pSkeleton.GetNode())
    {
        return WriteNodeType(*pSkeleton.GetNode());
    }
    return false;
}


bool FbxWriterFbx7_Impl::WriteSkeletonLimbNode (FbxSkeleton& pSkeleton)
{
    if(pSkeleton.GetNode())
    {
        return WriteNodeType(*pSkeleton.GetNode());
    }
    return false;
}


bool FbxWriterFbx7_Impl::WriteSkeletonEffector (FbxSkeleton& pSkeleton)
{
    return WriteSkeletonRoot(pSkeleton);
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxCache& pCache)
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

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxBindingTable& pTable)
{
    WriteObjectHeaderAndReferenceIfAny(pTable, FIELD_KFBXBINDINGTABLE_BINDING_TABLE);

    mFileObject->FieldWriteBlockBegin();
    {
        mFileObject->FieldWriteI(FIELD_KFBXBINDINGTABLE_VERSION, 100);

        // Properties
        WriteObjectPropertiesAndFlags(&pTable);

        // Write out the binding table entries
        size_t i = 0, lCount = pTable.GetEntryCount();
        for( i = 0; i < lCount; ++i )
        {
            // entries: source, source type, destination, destination type.
            mFileObject->FieldWriteBegin(FIELD_KFBXBINDINGTABLE_ENTRY);

            FbxBindingTableEntry const& lEntry = pTable.GetEntry(i);

            mFileObject->FieldWriteC(lEntry.GetSource());
            mFileObject->FieldWriteC(lEntry.GetEntryType(true));
            mFileObject->FieldWriteC(lEntry.GetDestination());
            mFileObject->FieldWriteC(lEntry.GetEntryType(false));

            mFileObject->FieldWriteEnd();
        }


    }
    mFileObject->FieldWriteBlockEnd ();

    mFileObject->FieldWriteEnd ();
    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxBindingOperator& pOperator)
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

            FbxBindingTableEntry const& lEntry = pOperator.GetEntry(i);

            mFileObject->FieldWriteC(lEntry.GetSource());
            mFileObject->FieldWriteC(lEntry.GetEntryType(true));
            mFileObject->FieldWriteC(lEntry.GetDestination());
            mFileObject->FieldWriteC(lEntry.GetEntryType(false));

            mFileObject->FieldWriteEnd();
        }


    }
    mFileObject->FieldWriteBlockEnd ();

    mFileObject->FieldWriteEnd ();
    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxImplementation& pImplementation)
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

bool FbxWriterFbx7_Impl::WriteCollections(FbxDocument* pDocument)
{
	int i, lCount = pDocument ? pDocument->GetSrcObjectCount<FbxCollection>() : 0;
    FbxCollection* lCollection = NULL;

    for(i=0; i<lCount; i++)
    {
        lCollection = pDocument->GetSrcObject<FbxCollection>(i);

        // Following test is used to skip sub-classes like FbxDocument & FbxScene
        if(FbxCollection::ClassId == lCollection->GetRuntimeClassId())
        {
            WriteCollection(*lCollection);
        }
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteCollection(FbxCollection& pCollection)
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

bool FbxWriterFbx7_Impl::WriteDocuments(FbxDocument* pDocument)
{
	int i, lCount = pDocument ? pDocument->GetSrcObjectCount<FbxDocument>() : 0;
    FbxDocument* lDoc = NULL;

    for(i=0; i<lCount; i++)
    {
        lDoc = pDocument->GetSrcObject<FbxDocument>(i);
        WriteDocument(*lDoc);
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteDocument(FbxDocument& pSubDocument)
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

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxLayeredTexture& pTexture)
{
    WriteObjectHeaderAndReferenceIfAny(pTexture, FIELD_KFBXLAYEREDTEXTURE_LAYERED_TEXTURE);

    mFileObject->FieldWriteBlockBegin ();
    {
        mFileObject->FieldWriteI(FIELD_KFBXLAYEREDTEXTURE_LAYERED_TEXTURE, 
                FbxFileVersionStrToInt(mWriter->mFileVersion) > 201100 ? 101 : 100);

        // Properties
        WriteObjectPropertiesAndFlags(&pTexture);


        // Blend Modes
        mFileObject->FieldWriteBegin(FIELD_KFBXLAYEREDTEXTURE_BLENDMODES);

        for( int i = 0; i < pTexture.mInputData.GetCount(); ++i )
        {
            mFileObject->FieldWriteI( pTexture.mInputData[i].mBlendMode );
        }

        mFileObject->FieldWriteEnd();


        // Alphas
        if (FbxFileVersionStrToInt(mWriter->mFileVersion) > 201100)
        {
            mFileObject->FieldWriteBegin(FIELD_KFBXLAYEREDTEXTURE_ALPHAS);

            for( int i = 0; i < pTexture.mInputData.GetCount(); ++i )
            {
                mFileObject->FieldWriteD( pTexture.mInputData[i].mAlpha );
            }

            mFileObject->FieldWriteEnd();
        }


        mFileObject->FieldWriteBlockEnd ();
    }
    mFileObject->FieldWriteEnd ();
    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxProceduralTexture& pTexture)
{
    WriteObjectHeaderAndReferenceIfAny(pTexture, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_PROCEDURAL_TEXTURE);

    mFileObject->FieldWriteBlockBegin ();
    {
        mFileObject->FieldWriteI(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_PROCEDURAL_TEXTURE, 100);

        // Properties
        WriteObjectPropertiesAndFlags(&pTexture);

        mFileObject->FieldWriteBlockEnd ();
    }
    mFileObject->FieldWriteEnd ();
    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxFileTexture& pTexture)
{
    FbxString         OnlyFileName;
    FbxVector4     lVector;

    WriteObjectHeaderAndReferenceIfAny(pTexture, FIELD_KFBXTEXTURE_TEXTURE);

    mFileObject->FieldWriteBlockBegin ();
    {
        FbxFileTexture*    lReferencedObject = FbxCast<FbxFileTexture>(pTexture.GetReferenceTo());
        bool            lDoWrite;
        FbxString         lTextureType = pTexture.GetTextureType();
    
        lDoWrite = (lReferencedObject == NULL)
                   ? true
                   : (lTextureType != lReferencedObject->GetTextureType());
        if(lDoWrite)
        {
            mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_TYPE, lTextureType.Buffer ());
        }
    
        mFileObject->FieldWriteI(FIELD_KFBXTEXTURE_VERSION, 202);
    
        FbxString lTextureName = pTexture.GetNameWithNameSpacePrefix();
        lDoWrite = (lReferencedObject == NULL)
                   ? true
                   : (lTextureName != lReferencedObject->GetNameWithNameSpacePrefix());
        if(lDoWrite)
        {
            mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_TEXTURE_NAME, lTextureName);
        }
    
        // Write properties
        WriteObjectPropertiesAndFlags(&pTexture);
    
        FbxString lName;
    
        lName = pTexture.GetMediaName();
        lDoWrite = (lReferencedObject == NULL)
                   ? true
                   :(lName.Compare(lReferencedObject->GetMediaName()) != 0);
        if(lDoWrite)
        {
            FbxString lTmpName = FbxManager::PrefixName(VIDEO_PREFIX,  lName);
            mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_MEDIA, lTmpName);
        }
    
        lName = pTexture.GetFileName();
        lDoWrite = (lReferencedObject == NULL)
                   ? true
                   :(lName.Compare(lReferencedObject->GetFileName()) != 0);
        if(lDoWrite)
        {
            mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_FILENAME, lName);
        }
    
        lName = pTexture.GetRelativeFileName();
        lDoWrite = (lReferencedObject == NULL)
                   ? true
                   :(lName.Compare(lReferencedObject->GetRelativeFileName()) != 0);
        if(lDoWrite)
        {
            mFileObject->FieldWriteC(FIELD_KFBXTEXTURE_RELATIVE_FILENAME, lName);
        }
    
        lDoWrite = (lReferencedObject == NULL)
                   ? true
                   :(pTexture.GetUVTranslation() != lReferencedObject->GetUVTranslation());
        if(lDoWrite)
        {
            mFileObject->FieldWriteBegin (FIELD_KFBXTEXTURE_UV_TRANSLATION);
            mFileObject->FieldWriteD(pTexture.GetUVTranslation()[FbxTexture::eU]);
            mFileObject->FieldWriteD(pTexture.GetUVTranslation()[FbxTexture::eV]);
            mFileObject->FieldWriteEnd ();
        }
    
        lDoWrite = (lReferencedObject == NULL)
                   ? true
                   :(pTexture.GetUVScaling() != lReferencedObject->GetUVScaling());
        if(lDoWrite)
        {
            mFileObject->FieldWriteBegin (FIELD_KFBXTEXTURE_UV_SCALING);
            mFileObject->FieldWriteD(pTexture.GetUVScaling()[FbxTexture::eU]);
            mFileObject->FieldWriteD(pTexture.GetUVScaling()[FbxTexture::eV]);
            mFileObject->FieldWriteEnd ();
        }
    
        lDoWrite = (lReferencedObject == NULL)
                   ? true
                   :(pTexture.GetAlphaSource() != lReferencedObject->GetAlphaSource());
        if(lDoWrite)
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
    
        lDoWrite = (lReferencedObject == NULL)
                   ? true
                   :(pTexture.GetCroppingLeft() != lReferencedObject->GetCroppingLeft()) ||
                   (pTexture.GetCroppingRight() != lReferencedObject->GetCroppingRight()) ||
                   (pTexture.GetCroppingTop() != lReferencedObject->GetCroppingTop()) ||
                   (pTexture.GetCroppingBottom() != lReferencedObject->GetCroppingBottom());
        if(lDoWrite)
        {
            mFileObject->FieldWriteBegin(FIELD_KFBXTEXTURE_CROPPING);
            mFileObject->FieldWriteI(pTexture.GetCroppingLeft());
            mFileObject->FieldWriteI(pTexture.GetCroppingRight());
            mFileObject->FieldWriteI(pTexture.GetCroppingTop());
            mFileObject->FieldWriteI(pTexture.GetCroppingBottom());
            mFileObject->FieldWriteEnd ();
        }
    }
    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxThumbnail& pThumbnail)
{
    if(pThumbnail.GetSize() != FbxThumbnail::eNotSet)
    {
        // This is a non-empty thumbnail, so save it
        FbxUChar* lImagePtr   = pThumbnail.GetThumbnailImage();
        unsigned long lSize = pThumbnail.GetSizeInBytes();
        unsigned long i;

        WriteObjectHeaderAndReferenceIfAny(pThumbnail, FIELD_THUMBNAIL);

        mFileObject->FieldWriteBlockBegin();
        {
            bool lDoWrite = true;
            FbxThumbnail* lReferencedObject = FbxCast<FbxThumbnail>(pThumbnail.GetReferenceTo());
    
            if(lReferencedObject != NULL)
            {
                lDoWrite = (pThumbnail.GetDataFormat() != lReferencedObject->GetDataFormat());
                if(!lDoWrite)
                {
                    lDoWrite = (pThumbnail.GetSize() != lReferencedObject->GetSize());
                }
                if(!lDoWrite) {
                    FbxUChar* lRefImagePtr = lReferencedObject->GetThumbnailImage();
                    for(i=0; i<lSize; i++)
                    {
                        if((lRefImagePtr[i] != lImagePtr[i]))
                        {
                            lDoWrite = true;
                            break;
                        }
                    }
                }
            }
    
            mFileObject->FieldWriteI(FIELD_THUMBNAIL_VERSION, 100);
    
            if(lDoWrite)
            {
                mFileObject->FieldWriteI(FIELD_THUMBNAIL_FORMAT, pThumbnail.GetDataFormat());
    
                mFileObject->FieldWriteI(FIELD_THUMBNAIL_SIZE,   pThumbnail.GetSize());
    
                // hard code an encoding of "0" for "raw data". In future version, encoding
                // will indicate if the file is stored with OpenEXR or RAW.
                mFileObject->FieldWriteI(FIELD_THUMBNAIL_ENCODING, 0);

                WriteValueArray(FIELD_THUMBNAIL_IMAGE, lSize, lImagePtr);
            }
    
            WriteObjectPropertiesAndFlags(&pThumbnail);
        }
        mFileObject->FieldWriteBlockEnd ();
        mFileObject->FieldWriteEnd ();

        return true;
    }
    return false;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxSurfaceMaterial& pMaterial)
{
    WriteObjectHeaderAndReferenceIfAny(pMaterial, FIELD_KFBXMATERIAL_MATERIAL);
    mFileObject->FieldWriteBlockBegin ();
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
        lDoWrite = (lReferencedMaterial == NULL)
                   ? true
                   : !(pMaterial.ShadingModel.CompareValue(lReferencedMaterial->ShadingModel));
        if(lDoWrite)
        {
            FbxString lTmpStr = lString.Lower();
            mFileObject->FieldWriteC(FIELD_KFBXMATERIAL_SHADING_MODEL, lTmpStr);
        }
    
        lBool = pMaterial.MultiLayer.Get();
        lDoWrite = (lReferencedMaterial == NULL)
                   ? true
                   : !(pMaterial.MultiLayer.CompareValue(lReferencedMaterial->MultiLayer));
        if(lDoWrite)
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
    
        FbxDouble3              lColor;
        double                  lFactor;
    
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
            lColor = lPhong->Emissive.Get();
            lFactor = lPhong->EmissiveFactor.Get();
            lDoCreateProp = true;
            if(lReferencedPhong)
            {
                lDoCreateProp = !(
                                 (lReferencedPhong->Emissive.Get() == lColor) &&
                                 (lReferencedPhong->EmissiveFactor.Get() == lFactor)
                                 );
            }
            if(lDoCreateProp)
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
                if(lEmissiveProp.IsValid())
                {
                    lEmissiveProp.Destroy();
                }
            }
    
            //
            // lAmbientProp
            //
            lColor = lPhong->Ambient.Get();
            lFactor = lPhong->AmbientFactor.Get();
            lDoCreateProp = true;
            if(lReferencedPhong)
            {
                lDoCreateProp = !(
                                 (lReferencedPhong->Ambient.Get() == lColor) &&
                                 (lReferencedPhong->AmbientFactor.Get() == lFactor)
                                 );
            }
            if(lDoCreateProp)
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
                if(lAmbientProp.IsValid())
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
            if(lReferencedPhong)
            {
                lDoCreateProp = !(
                                 (lReferencedPhong->Diffuse.Get() == lColor) &&
                                 (lReferencedPhong->DiffuseFactor.Get() == lFactor)
                                 );
            }
            if(lDoCreateProp)
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
                if(lDiffuseProp.IsValid())
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
            if(lReferencedPhong)
            {
                lDoCreateProp = !(
                                 (lReferencedPhong->Specular.Get() == lColor) &&
                                 (lReferencedPhong->SpecularFactor.Get() == lFactor)
                                 );
            }
            if(lDoCreateProp)
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
                if(lSpecularProp.IsValid())
                {
                    lSpecularProp.Destroy();
                }
            }
    
            //
            // lShininessProp
            //
            lFactor = lPhong->Shininess.Get();
            lDoCreateProp = true;
            if(lReferencedPhong)
            {
                lDoCreateProp = !(
                                 (lReferencedPhong->Shininess.Get() == lFactor)
                                 );
            }
            if(lDoCreateProp)
            {
                lShininessProp = FbxProperty::Create(&pMaterial, FbxDoubleDT, "Shininess");
                lShininessProp.Set(lFactor);
            }
            else
            {
                lShininessProp = lPhong->FindProperty("Shininess");
                if(lShininessProp.IsValid())
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
            if(lReferencedPhong)
            {
                lDoCreateProp = !(
                                 (lReferencedPhong->TransparentColor.Get() == lColor) &&
                                 (lReferencedPhong->TransparencyFactor.Get() == lFactor)
                                 );
            }
            if(lDoCreateProp)
            {
                double lOpacity = 1. - (lColor[0] + lColor[1] + lColor[2]) / 3. * lFactor;
                lOpacityProp = FbxProperty::Create(&pMaterial, FbxDoubleDT, "Opacity");
                lOpacityProp.Set(lOpacity);
            }
            else
            {
                lOpacityProp = lPhong->FindProperty("Opacity");
                if(lOpacityProp.IsValid())
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
            if(lReferencedPhong)
            {
                lDoCreateProp = !(
                                 (lReferencedPhong->Reflection.Get() == lColor) &&
                                 (lReferencedPhong->ReflectionFactor.Get() == lFactor)
                                 );
            }
            if(lDoCreateProp)
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
                if(lReflProp.IsValid())
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
            if(lReferencedLambert)
            {
                lDoCreateProp = !(
                                 (lReferencedLambert->Emissive.Get() == lColor) &&
                                 (lReferencedLambert->EmissiveFactor.Get() == lFactor)
                                 );
            }
            if(lDoCreateProp)
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
                if(lEmissiveProp.IsValid())
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
            if(lReferencedLambert)
            {
                lDoCreateProp = !(
                                 (lReferencedLambert->Ambient.Get() == lColor) &&
                                 (lReferencedLambert->AmbientFactor.Get() == lFactor)
                                 );
            }
            if(lDoCreateProp)
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
                if(lAmbientProp.IsValid())
                {
                    lAmbientProp.Destroy();
                }
            }
    
            //
            // lDiffuseProp
            //
            lColor = lLambert->Diffuse.Get();
            lFactor = lLambert->DiffuseFactor.Get();
            lDoCreateProp = true;
            if(lReferencedLambert)
            {
                lDoCreateProp = !(
                                 (lReferencedLambert->Diffuse.Get() == lColor) &&
                                 (lReferencedLambert->DiffuseFactor.Get() == lFactor)
                                 );
            }
            if(lDoCreateProp)
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
                if(lDiffuseProp.IsValid())
                {
                    lDiffuseProp.Destroy();
                }
            }
    
            //
            // lOpacityProp
            //
            lColor = lLambert->TransparentColor.Get();
            lFactor = lLambert->TransparencyFactor.Get();
            lDoCreateProp = true;
            if(lReferencedLambert)
            {
                lDoCreateProp = !(
                                 (lReferencedLambert->TransparentColor.Get() == lColor) &&
                                 (lReferencedLambert->TransparencyFactor.Get() == lFactor)
                                 );
            }
            if(lDoCreateProp)
            {
                double lOpacity = 1. - (lColor[0] + lColor[1] + lColor[2]) / 3. * lFactor;
                lOpacityProp = FbxProperty::Create(&pMaterial, FbxDoubleDT, "Opacity");
                lOpacityProp.Set(lOpacity);
            }
            else
            {
                lOpacityProp = lLambert->FindProperty("Opacity");
                if(lOpacityProp.IsValid())
                {
                    lOpacityProp.Destroy();
                }
            }
        }
    
        // Properties
        WriteObjectPropertiesAndFlags(&pMaterial);
    
        if(lEmissiveProp.IsValid())
        {
            lEmissiveProp.Destroy();
        }
        if(lAmbientProp.IsValid())
        {
            lAmbientProp.Destroy();
        }
        if(lDiffuseProp.IsValid())
        {
            lDiffuseProp.Destroy();
        }
        if(lSpecularProp.IsValid())
        {
            lSpecularProp.Destroy();
        }
        if(lShininessProp.IsValid())
        {
            lShininessProp.Destroy();
        }
        if(lReflProp.IsValid())
        {
            lReflProp.Destroy();
        }
        if(lOpacityProp.IsValid())
        {
            lOpacityProp.Destroy();
        }
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    return true;
}

bool FbxWriterFbx7_Impl::WriteVideos(FbxDocument* pTopDocument)
{
    KReferenceDepthList lVideos;
    CollectObjectsByDepth(pTopDocument, lVideos, FBX_TYPE(FbxVideo));

	if( lVideos.Size() == 0 )
    {
        return true;
    }

    bool embeddedMedia = IOS_REF.GetBoolProp(EXP_FBX_EMBEDDED, false);
	for( size_t i = 0, c = lVideos.Size(); i < c && !mCanceled; ++i )
    {
        FBX_ASSERT( lVideos[i].mObject );

        FbxVideo* lVideo = FbxCast<FbxVideo>(lVideos[i].mObject);
        FBX_ASSERT( lVideo );

        if( lVideo )
        {
            WriteObjectHeaderAndReferenceIfAny(*lVideo, FIELD_MEDIA_VIDEO);
            mFileObject->FieldWriteBlockBegin();
            {
                FbxString lFileName = lVideo->GetFileName();
				bool lImageSequence = lVideo->GetImageSequence();

                // Don't set error code because it will break the rest of the writing since the object is
                // shared with FbxIO
				if( !WriteVideo(*lVideo, lFileName, (lImageSequence)?false:embeddedMedia) )
				{
					; //mStatus.SetCode(FbxStatus::eInsufficientMemory, "Out of memory resources for embedding textures");
				}
            }
            mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();
        }
    }
    return true;
}

bool FbxWriterFbx7_Impl::WriteVideo(FbxVideo& pVideo, FbxString& pFileName, bool pEmbeddedMedia)
{
	FbxObject* lReferencedVideo = pVideo.GetReferenceTo();
	if( lReferencedVideo != NULL )
	{
		return WriteObjectPropertiesAndFlags(&pVideo);
	}

    FbxString lVideoTypeName = pVideo.GetTypeName();
    mFileObject->FieldWriteC(FIELD_MEDIA_TYPE, lVideoTypeName.Buffer());

    FbxString lRelativeFileName = mFileObject->GetRelativeFilePath(pFileName.Buffer());
    pVideo.SetFileName(pFileName.Buffer());
    pVideo.SetRelativeFileName(lRelativeFileName.Buffer());

    WriteObjectPropertiesAndFlags(&pVideo);

    mFileObject->FieldWriteI(FIELD_KFBXVIDEO_USEMIPMAP, pVideo.ImageTextureGetMipMap());
	mFileObject->FieldWriteC(FIELD_MEDIA_FILENAME, pFileName.Buffer());
	mFileObject->FieldWriteC(FIELD_MEDIA_RELATIVE_FILENAME, lRelativeFileName.Buffer());

    if( pEmbeddedMedia )
    {
        if (!FbxFileUtils::Exist(pFileName))
        {
            if (!FbxFileUtils::Exist(lRelativeFileName))
            {
                FbxUserNotification* lUserNotification = mManager.GetUserNotification();
                if (lUserNotification)
                {
                    lUserNotification->AddDetail(FbxUserNotification::eEmbedMediaNotify, pFileName);
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

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxContainer& pContainer)
{
    if( !WriteObjectHeaderAndReferenceIfAny(pContainer, FIELD_KFBXCONTAINER_CONTAINER) )
    {
        return false;
    }

    bool lStatus;
    bool pEmbeddedMedia = IOS_REF.GetBoolProp(EXP_FBX_EMBEDDED, false);

    mFileObject->FieldWriteBlockBegin ();
    {
        mFileObject->FieldWriteI(FIELD_KFBXCONTAINER_VERSION, 100);

        WriteObjectPropertiesAndFlags(&pContainer);

        FbxString lFileName;
        if (pEmbeddedMedia)
        {
            lFileName = pContainer.TemplatePath.Get();
            mFileObject->FieldWriteBegin(FIELD_MEDIA_CONTENT);
            lStatus = mFileObject->FieldWriteEmbeddedFile(lFileName, lFileName);

            mFileObject->FieldWriteEnd();

            for( int i = 0; i < int(pContainer.mContainerTemplate->GetExtendTemplateCount()); i++ )
            {
                lFileName = pContainer.mContainerTemplate->GetExtendTemplatePathAt(i);
                mFileObject->FieldWriteBegin(FIELD_MEDIA_CONTENT);
                lStatus = mFileObject->FieldWriteEmbeddedFile(lFileName, lFileName);

                mFileObject->FieldWriteEnd();
            }
        }
    }

    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();

    return true;
}

void FbxWriterFbx7_Impl::WriteConstraints(FbxScene& pScene)
{
	int i, lCount = pScene.GetSrcObjectCount<FbxConstraint>();
    FbxConstraint* lConstraint;

    for(i=0; i<lCount; i++)
    {
        lConstraint = pScene.GetSrcObject<FbxConstraint>(i);
        if(lConstraint)
        {
            if (
                (lConstraint->GetConstraintType() == FbxConstraint::eCharacter && IOS_REF.GetBoolProp(EXP_FBX_CHARACTER,  true)) ||
                (lConstraint->GetConstraintType() != FbxConstraint::eCharacter && IOS_REF.GetBoolProp(EXP_FBX_CONSTRAINT, true))
            )
            {
                WriteConstraint(*lConstraint, pScene);
            }
        }
    }
}

void FbxWriterFbx7_Impl::WriteConstraint(FbxConstraint& pConstraint, FbxScene& pScene)
{
    FbxString OnlyFileName;
    FbxVector4 lVector;

    WriteObjectHeaderAndReferenceIfAny(pConstraint, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONSTRAINT);
    mFileObject->FieldWriteBlockBegin ();
    {
        FbxString lTypeName = pConstraint.GetTypeName();
        mFileObject->FieldWriteC("Type", lTypeName);
        mFileObject->FieldWriteI("MultiLayer",0);
		
		if (pConstraint.GetConstraintType() == FbxConstraint::eCharacter)
		{
			FbxCharacter& lCharacter = (FbxCharacter&)pConstraint;
			if (lCharacter.IsLegacy() == false)
				mFileObject->FieldWriteI("Version", lCharacter.Version());

			WriteObjectPropertiesAndFlags(&pConstraint);
			WriteCharacter(lCharacter, pScene);
        }
		else
		{
			WriteObjectPropertiesAndFlags(&pConstraint);
		}
    }

    mFileObject->FieldWriteBlockEnd ();
    mFileObject->FieldWriteEnd ();
}

void FbxWriterFbx7_Impl::WriteAllGeometryWeightedMaps(FbxDocument* pTopDocument)
{
    FBX_ASSERT_RETURN(pTopDocument);

    KReferenceDepthList lMaps;
    CollectObjectsByDepth(pTopDocument, lMaps, FBX_TYPE(FbxGeometryWeightedMap));

	for( size_t i = 0, c = lMaps.Size(); i < c && !mCanceled; ++i )
    {
		FBX_ASSERT( lMaps[i].mObject );

        FbxGeometryWeightedMap* lGeometryWeightedMap = FbxCast<FbxGeometryWeightedMap>(lMaps[i].mObject);
        FBX_ASSERT( lGeometryWeightedMap );

        if(lGeometryWeightedMap) 
        {
            // Open the Geometry Weighted Map block
            WriteObjectHeaderAndReferenceIfAny(*lGeometryWeightedMap, FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GEOMETRY_WEIGHTED_MAP);
            mFileObject->FieldWriteBlockBegin ();
            {
                WriteGeometryWeightedMap(*lGeometryWeightedMap);
            }
            // Close the Geometry Weighted Map
            mFileObject->FieldWriteBlockEnd ();
            mFileObject->FieldWriteEnd ();
        }
    }
}

bool FbxWriterFbx7_Impl::WriteNodes(FbxScene* pTopScene, bool pIncludeRoot)
{
    if( pIncludeRoot )
    {
        return WriteFbxObjects(pTopScene, FBX_TYPE(FbxNode));
    }

    KReferenceDepthList lNodes;
    CollectExcept       lFilter(pTopScene->GetRootNode());

    CollectObjectsByDepth(pTopScene, lNodes, FBX_TYPE(FbxNode), lFilter);

    bool lSuccess = true;

	for( size_t i = 0, c = lNodes.Size(); i < c && !mCanceled && lSuccess; ++i )
    {
        FBX_ASSERT( lNodes[i].mObject );

        FbxNode* lNode = FbxCast<FbxNode>(lNodes[i].mObject);
        FBX_ASSERT(lNode);

        if( lNode )
        {
            lSuccess = WriteFbxObject(*lNode);
        }
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteFbxObject(FbxNodeAttribute& pNodeAttribute)
{  
    if( pNodeAttribute.GetAttributeType() == FbxNodeAttribute::eUnknown )
    {
        // 
        // "New" way -- give as much information as we can about the the type
        // so on read back we can re-create the type properly.
        // 
        // We still write it out as Geometry: or NodeAttribute: though; we just write
        // a more explicit sub-type.
        // 

        FbxClassId      lClassId = pNodeAttribute.GetRuntimeClassId();

        if( WriteObjectHeaderAndReferenceIfAny( pNodeAttribute,
                                                    lClassId.GetFbxFileTypeName(),
                                                    lClassId.GetName()) )
        {
            mFileObject->FieldWriteBlockBegin();

            {
                WriteObjectPropertiesAndFlags(&pNodeAttribute);

                // That's all we write out -- properties.
            }

            mFileObject->FieldWriteBlockEnd ();
            mFileObject->FieldWriteEnd ();

            return true;
        }
    }
    else
    {
		if( WriteObjectHeaderAndReferenceIfAny( pNodeAttribute,
			                                        pNodeAttribute.Is<FbxGeometryBase>() ?
                                                    FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GEOMETRY :
		                                            FIELD_OBJECT_DEFINITION_OBJECT_TYPE_NODE_ATTRIBUTE ) )
        {
            mFileObject->FieldWriteBlockBegin();
            {
                WriteObjectPropertiesAndFlags(&pNodeAttribute);
                WriteNodeAttribute(&pNodeAttribute);
            }

            mFileObject->FieldWriteBlockEnd ();
            mFileObject->FieldWriteEnd ();

            return true;
        }
    }

    return false;
}

void FbxWriterFbx7_Impl::WriteObjectProperties(FbxDocument* pTopDocument)
{
    FBX_ASSERT_RETURN(pTopDocument);

    mFileObject->WriteComments ("");
    mFileObject->WriteComments (" Object properties");
    mFileObject->WriteComments ("------------------------------------------------------------------");
    mFileObject->WriteComments ("");

    mFileObject->FieldWriteBegin(FIELD_OBJECT_PROPERTIES);
    mFileObject->FieldWriteBlockBegin();
    {
        FbxScene*  lScene = FbxCast<FbxScene>(pTopDocument);

        if(lScene != NULL)
        {
		#ifndef FBXSDK_ENV_WINSTORE
			//Write all plugins parameters before any other objects
			mWriter->PluginsWrite(*mFileObject, /*Also write object ID in fbx 7*/true);
		#endif

            if (IOS_REF.GetBoolProp(EXP_FBX_MODEL, true))
            {
                if (!mCanceled)
                    WriteFbxObjects(lScene, FBX_TYPE(FbxNodeAttribute));
      
                // Export all geometry weighted maps
                if (!mCanceled)
                    WriteAllGeometryWeightedMaps(lScene);
            }

            // Export all nodes except the root node. (This includes the camera switcher)
            if(!mCanceled && lScene->GetRootNode())
                WriteNodes(lScene, false);

            // Export all the generic nodes
            if (!mCanceled)
                WriteFbxObjects(lScene, FBX_TYPE(FbxGenericNode));
    
            // Export all the poses
            if (!mCanceled)
                WriteFbxObjects(lScene, FBX_TYPE(FbxPose));
    
            // Export all the materials
            if (!mCanceled)
                WriteFbxObjects(lScene, FBX_TYPE(FbxSurfaceMaterial));
    
            // Export all the deformer
            if (!mCanceled)
                WriteDeformers(lScene);
    
            // Export all the videos
            // (we export the videos before the textures because the FileName and
            //  relative filename may get changed and we want the Texture to reference
            // the correct file).
            if (!mCanceled)
                WriteVideos(lScene);
    
            if (!mCanceled)
                WriteFbxObjects(lScene, FBX_TYPE(FbxFileTexture));   
            if (!mCanceled)
                WriteFbxObjects(lScene, FBX_TYPE(FbxCache));
    
            // Export all shader implementations
            if (!mCanceled)
                WriteFbxObjects(lScene, FBX_TYPE(FbxImplementation));
            if (!mCanceled)
                WriteFbxObjects(lScene, FBX_TYPE(FbxBindingTable));
            if (!mCanceled)
                WriteFbxObjects(lScene, FBX_TYPE(FbxBindingOperator));
    
            if (!mCanceled)
                WriteFbxObjects(lScene, FBX_TYPE(FbxLayeredTexture));

            if (!mCanceled)
    			WriteFbxObjects(lScene, FBX_TYPE(FbxProceduralTexture));
    
            //////////////////////////////////////////////////////////////////////////////////////////////////
            //
            // bob
            //
            if (IOS_REF.GetBoolProp(EXP_FBX_CHARACTER,  true))
            {
                if (!mCanceled)
                    WriteCharacterPose(*lScene);
                if (!mCanceled)
                    WriteControlSetPlug(*lScene);
            }
            //
            //////////////////////////////////////////////////////////////////////////////////////////////////

            if (!mCanceled)
                WriteFbxObjects(lScene, FBX_TYPE(FbxContainer));

            // export all the selection nodes
            if (!mCanceled)
                WriteSelectionNode(*lScene);

            // export all the selection sets
            if (!mCanceled)
                WriteSelectionSet(*lScene);

            if (!mCanceled)
                WriteFbxObjects(lScene, FBX_TYPE(FbxSceneReference));
    
            if( IOS_REF.GetBoolProp(EXP_FBX_CONSTRAINT, true) || IOS_REF.GetBoolProp(EXP_FBX_CHARACTER, true) )
            {
                if (!mCanceled)
                    WriteConstraints(*lScene);
            }
    
            // export all the animation & audio objects
            //    stacks, layers, curve nodes and curves
            if (!mCanceled)
                WriteAnimObjects(lScene);    

            // for V7.0 files, the GlobalSettings are still in the Object Properties section
            int maj, min, rev;
            FbxIO::ProjectConvertVersionNumber(mFileObject->ProjectGetSectionVersion(FBX_MAIN_SECTION), maj, min, rev);    
            FBX_ASSERT(maj == 7 && min >= 0);

            if (min == 0)
            {
                if (!mCanceled)
                    WriteGlobalSettings(lScene);
            } 
        }
        else 
        {
            // Export all the materials
            if (!mCanceled)
                WriteFbxObjects(pTopDocument, FBX_TYPE(FbxSurfaceMaterial));
    
            // Export all the videos
            // (we export the videos before the textures because the FileName and
            //  relative filename may get changed and we want the Texture to reference
            // the correct file).
            if (!mCanceled)
                WriteVideos(pTopDocument);
    
            // Export all file textures
            if (!mCanceled)
                WriteFbxObjects(pTopDocument, FBX_TYPE(FbxFileTexture));   
    
            // Export all shader implementations
            if (!mCanceled)
                WriteFbxObjects(pTopDocument, FBX_TYPE(FbxImplementation));
            if (!mCanceled)
                WriteFbxObjects(pTopDocument, FBX_TYPE(FbxBindingTable));
            if (!mCanceled)
                WriteFbxObjects(pTopDocument, FBX_TYPE(FbxBindingOperator));
    
            // Export all layered textures
            if (!mCanceled)
                WriteFbxObjects(pTopDocument, FBX_TYPE(FbxLayeredTexture));
    
			// Export all procedural textures
            if (!mCanceled)
        		WriteFbxObjects(pTopDocument, FBX_TYPE(FbxProceduralTexture));


            // Export all collections
            if (!mCanceled)
                WriteCollections(pTopDocument);
    
            // Export all thumbnails
            if (!mCanceled)
                WriteFbxObjects(pTopDocument, FBX_TYPE(FbxThumbnail));

            // Export all node attributes
            if (!mCanceled)
                WriteFbxObjects(pTopDocument, FBX_TYPE(FbxNodeAttribute));

            // Export all nodes
            if (!mCanceled)
                WriteFbxObjects(pTopDocument, FBX_TYPE(FbxNode));

            // export all the animation & audio objects
            //    stacks, layers, curve nodes and curves
            if (!mCanceled)
                WriteAnimObjects(pTopDocument);

            // for V7.0 files, the GlobalSettings are still in the Object Properties section
            int maj, min, rev;
            FbxIO::ProjectConvertVersionNumber(mFileObject->ProjectGetSectionVersion(FBX_MAIN_SECTION), maj, min, rev);    
            FBX_ASSERT(maj == 7 && min >= 0);

            if (min == 0)
            {
                if (!mCanceled)
                    WriteGlobalSettings(pTopDocument);
            } 
        }
    
        // Write objects supported by the generic writing mechanism
        if (!mCanceled)
            WriteGenericObjects(pTopDocument);   
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

bool FbxWriterFbx7_Impl::WriteGenericObjects(FbxDocument* pTopDocument)
{
    FBX_ASSERT_RETURN_VALUE(pTopDocument, false);

    KReferenceDepthList     lReferences;
    CollectGenericObject    lFilter(&mTypeDefinitions);

    CollectObjectsByDepth(pTopDocument, lReferences, FBX_TYPE(FbxObject), lFilter);

	for( size_t i = 0, c = lReferences.Size(); i < c && !mCanceled; ++i )
    {
        FBX_ASSERT(lReferences[i].mObject);

        if( lReferences[i].mObject )
        {
            // If we are storing a runtime type, we store the name in the sub-class type name?
            FbxClassId      lClassId = lReferences[i].mObject->GetRuntimeClassId();
            FbxString lFbxClassTypeName = lClassId.GetFbxFileTypeName(true);
    
            WriteObjectHeaderAndReferenceIfAny(*lReferences[i].mObject, lFbxClassTypeName);
                mFileObject->FieldWriteBlockBegin();
                {
                    WriteObjectPropertiesAndFlags(lReferences[i].mObject);
                }
                mFileObject->FieldWriteBlockEnd();
            mFileObject->FieldWriteEnd();
        }
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteAnimObjects(FbxDocument* pTopDocument)
{
    bool lResult = true;
		lResult = WriteFbxObjects(pTopDocument, FBX_TYPE(FbxAnimStack));

	if (lResult)
		lResult = WriteFbxObjects(pTopDocument, FBX_TYPE(FbxAnimCurve));

	if (lResult)
		lResult = WriteFbxObjects(pTopDocument, FBX_TYPE(FbxAnimCurveNode));

	if (lResult)
		lResult = WriteFbxObjects(pTopDocument, FBX_TYPE(FbxAnimLayer));

	if (lResult)
		lResult = WriteFbxObjects(pTopDocument, FBX_TYPE(FbxAudioLayer));

	if (lResult)
		lResult = WriteFbxObjects(pTopDocument, FBX_TYPE(FbxAudio));

    return lResult;
}


void FbxWriterFbx7_Impl::WriteObjectConnections(FbxDocument* pTopDocument)
{
    FBX_ASSERT_RETURN(pTopDocument);

    mFileObject->WriteComments ("");
    mFileObject->WriteComments (" Object connections");
    mFileObject->WriteComments ("------------------------------------------------------------------");
    mFileObject->WriteComments ("");

    mFileObject->FieldWriteBegin(FIELD_OBJECT_CONNECTIONS);
    mFileObject->FieldWriteBlockBegin();
    {
        FbxDocumentList    lDocuments;
        CollectDocumentHiearchy(lDocuments, pTopDocument);
    
		for( int i = 0, c = lDocuments.Size(); i < c; ++i )
        {
            FbxDocument* lDoc = lDocuments[i];
            FBX_ASSERT( lDoc );
    
            FbxIteratorSrc<FbxObject> lFbxObjectIterator(lDoc);
            FbxObject*                 lFbxObject;

            //Fix for character and HIK (this is to mimic what the Writer6 does. Apparently
            //it is a required condition.
            //write ConstrolSetPlug connections first before the character connections. 
            FbxScene* lScene = FbxCast<FbxScene>(lDoc);
            bool expCharacter = lScene && IOS_REF.GetBoolProp(EXP_FBX_CHARACTER, true);
            if (expCharacter)
            {
                FbxArray<FbxObject*> lControlSetPlugs;
                FbxArray<FbxObject*> lCharacterContraints;
                int lIter;

                // get all the controlsetplugs
                for(lIter = 0; lIter < lScene->GetControlSetPlugCount(); lIter++)
                {
                    lFbxObject= FbxCast<FbxObject>(lScene->GetControlSetPlug(lIter));
                    lControlSetPlugs.Add(lFbxObject);
                    WriteObjectConnections(lDoc, lFbxObject);
                }
            
                //then the character constraints
				for(lIter = 0; lIter < lScene->GetSrcObjectCount<FbxConstraint>(); lIter++)
                {
                    FbxConstraint* lConstraint = lScene->GetSrcObject<FbxConstraint>(lIter);
                    if(lConstraint != NULL)
                    {
                        if (lConstraint->GetConstraintType() == FbxConstraint::eCharacter)
                        {
                            lFbxObject = FbxCast<FbxObject>(lConstraint);
                            lCharacterContraints.Add(lFbxObject);
                            WriteObjectConnections(lDoc, lFbxObject);
                        }
                    }            
                }

                FbxForEach(lFbxObjectIterator, lFbxObject)
                {
                    if (lControlSetPlugs.Find(lFbxObject) != -1 || lCharacterContraints.Find(lFbxObject) != -1)
                        continue;
                    WriteObjectConnections(lDoc, lFbxObject);
                }
            }    
            //~Fix
            else
            {
                // no special treatment. We write the connections as they are 
                FbxForEach(lFbxObjectIterator, lFbxObject)
                {
                    WriteObjectConnections(lDoc, lFbxObject);
                }
            }
        }
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

// *****************************************************************************
//  Connections
// *****************************************************************************
bool FbxWriterFbx7_Impl::WriteFieldConnection(FbxDocument *pDocument, FbxObject* pSrcObject, FbxDocument *pDstDocument)
{
    if(pDocument == pDstDocument)
    {
        return true;
    }

    if((pDocument == NULL) || (pSrcObject == NULL) || (pDstDocument == NULL))
    {
        return false;
    }

    WriteCommentsForConnections(pSrcObject->GetNameWithNameSpacePrefix(), pDstDocument->GetNameWithNameSpacePrefix());

    mFileObject->FieldWriteBegin(kENHANCED_CONNECT_FIELD);
    mFileObject->FieldWriteC("OD");
    mFileObject->FieldWriteLL( GetObjectId(pSrcObject) );
    mFileObject->FieldWriteLL( GetObjectId(pDstDocument) );
    mFileObject->FieldWriteEnd();
    return true;
}

bool FbxWriterFbx7_Impl::WriteFieldConnection(FbxDocument *pDocument, FbxDocument *pChildDocument)
{
    if((pDocument == NULL) || (pChildDocument == NULL))
    {
        return false;
    }

    WriteCommentsForConnections(pChildDocument->GetNameWithNameSpacePrefix(), pDocument->GetNameWithNameSpacePrefix());

    // Store it as a regular object-object connection.  Nothing special to be done on read back
    mFileObject->FieldWriteBegin(kENHANCED_CONNECT_FIELD);
    mFileObject->FieldWriteC("OO");

    // Format is: Source object, Dst Object.
    // On read-back, we'll be doing: Dst->ConnectSrcObject(pChild)
    // So that's why we store it 'backwards'
    mFileObject->FieldWriteLL( GetObjectId(pChildDocument) );
    mFileObject->FieldWriteLL( GetObjectId(pDocument) );
    mFileObject->FieldWriteEnd();
    return true;
}

bool FbxWriterFbx7_Impl::WriteFieldConnection(FbxDocument *pDocument, FbxObject* pSrc,FbxObject* pDst)
{
    FbxObject*       SrcObject   = pSrc;
    FbxObject*       DstObject   = pDst;

    if( SrcObject && DstObject )
	{
        if( SrcObject == DstObject )
		{
            return false;
        }

        // Eleminate Connection between node and node attribute if we don't output the models.
        if( SrcObject->Is<FbxNodeAttribute>() && DstObject->Is<FbxNode>() )
		{
            if( !IOS_REF.GetBoolProp(EXP_FBX_MODEL, true) )
			{
                return true;
            }
        }

        WriteCommentsForConnections(SrcObject->GetNameWithNameSpacePrefix(), DstObject->GetNameWithNameSpacePrefix());

        mFileObject->FieldWriteBegin(kENHANCED_CONNECT_FIELD);
        mFileObject->FieldWriteC("OO");
        mFileObject->FieldWriteLL( GetObjectId(SrcObject) );
        mFileObject->FieldWriteLL( GetObjectId(DstObject) );
        mFileObject->FieldWriteEnd();
        return true;
    }
    return false;
}

bool FbxWriterFbx7_Impl::WriteFieldConnection(FbxDocument *pDocument, FbxObject* pSrc,FbxProperty& pDst)
{
    FbxObject*       SrcObject   = pSrc;
    FbxProperty*     DstProperty = pDst.IsValid() ? &pDst : 0;

    if( SrcObject && DstProperty )
    {
        if (SrcObject != pDocument)
        {
            WriteCommentsForConnections(SrcObject->GetNameWithNameSpacePrefix(),
                    DstProperty->GetFbxObject()->GetNameWithNameSpacePrefix());
        }
        else
        {
            WriteCommentsForConnections("", DstProperty->GetFbxObject()->GetNameWithNameSpacePrefix());
        }

        FbxString lDstPropHierName = DstProperty->GetHierarchicalName();

        mFileObject->FieldWriteBegin(kENHANCED_CONNECT_FIELD);

        if( SrcObject != pDocument )
        {
            mFileObject->FieldWriteC( "OP" );
            mFileObject->FieldWriteLL( GetObjectId(SrcObject) );
        }
        else
        {
            mFileObject->FieldWriteC( "EP" );
        }

        mFileObject->FieldWriteLL( GetObjectId(DstProperty->GetFbxObject()) );
        mFileObject->FieldWriteC(lDstPropHierName);
        mFileObject->FieldWriteEnd();

        return true;
    }
    return false;
}

bool FbxWriterFbx7_Impl::WriteFieldConnection(FbxDocument *pDocument, FbxProperty& pSrc,FbxObject* pDst)
{
    FbxObject*       DstObject   = pDst;
    FbxProperty*     SrcProperty = pSrc.IsValid() ? &pSrc: 0;

    if(SrcProperty && DstObject) {
        FbxString lSrcObjName     = SrcProperty->GetFbxObject()->GetNameWithNameSpacePrefix();
        FbxString lSrcPropHierName    = SrcProperty->GetHierarchicalName();
        FbxString lDstObjName         = DstObject->GetNameWithNameSpacePrefix();

        WriteCommentsForConnections(lSrcObjName, lDstObjName);

        mFileObject->FieldWriteBegin(kENHANCED_CONNECT_FIELD);
        mFileObject->FieldWriteC("PO");
        mFileObject->FieldWriteLL( GetObjectId(SrcProperty->GetFbxObject()) );
        mFileObject->FieldWriteC(lSrcPropHierName);
        mFileObject->FieldWriteLL( GetObjectId(DstObject) );
        mFileObject->FieldWriteEnd();
        return true;
    }
    return false;
}

bool FbxWriterFbx7_Impl::WriteFieldConnection(FbxDocument *pDocument, FbxProperty& pSrc,FbxProperty& pDst)
{
    FbxProperty*     SrcProperty = pSrc.IsValid() ? &pSrc: 0;
    FbxProperty*     DstProperty = pDst.IsValid() ? &pDst: 0;

    if(SrcProperty && DstProperty) {

        FbxString lSrcObjName         = SrcProperty->GetFbxObject()->GetNameWithNameSpacePrefix();
        FbxString lSrcPropHierName    = SrcProperty->GetHierarchicalName();
        FbxString lDstObjName         = DstProperty->GetFbxObject()->GetNameWithNameSpacePrefix();
        FbxString lDstPropHierName    = DstProperty->GetHierarchicalName();

        WriteCommentsForConnections(lSrcObjName, lDstObjName);

        mFileObject->FieldWriteBegin(kENHANCED_CONNECT_FIELD);
        mFileObject->FieldWriteC("PP");
        mFileObject->FieldWriteLL( GetObjectId(SrcProperty->GetFbxObject()) );
        mFileObject->FieldWriteC(lSrcPropHierName);
        mFileObject->FieldWriteLL( GetObjectId(DstProperty->GetFbxObject()) );
        mFileObject->FieldWriteC(lDstPropHierName);
        mFileObject->FieldWriteEnd();
        return true;
    }
    return false;
}

void FbxWriterFbx7_Impl::WriteCommentsForConnections(const char* pSrcName, const char* pDstName)
{
     FbxString lConnectionComments = pSrcName;
     lConnectionComments += ", ";
     lConnectionComments += pDstName;
     mFileObject->WriteComments("");
     mFileObject->WriteComments(lConnectionComments);
}

void FbxWriterFbx7_Impl::WriteObjectConnections(FbxDocument *pDocument, FbxObject* pObject)
{
	// store only savable objects
	if (!pObject->GetObjectFlags(FbxObject::eSavable))
		return;

    // Store all connections on the object; for sub-documents we'll be storing those
    // by document, but here we need to store the connection between the two documents.
    // -------------------------------------------------------------------------------

    FbxDocument* lSubDoc = FbxCast<FbxDocument>(pObject);

    if( lSubDoc )
    {
        WriteFieldConnection(pDocument, lSubDoc);
    }
    else
    {
        FbxIteratorSrc<FbxObject> lFbxObjectIterator(pObject);
        FbxObject*                 lFbxObject;
        FbxForEach(lFbxObjectIterator,lFbxObject) 
        {
			// store only savable objects
			if (!lFbxObject->GetObjectFlags(FbxObject::eSavable))
				continue;

            if(pDocument == lFbxObject->GetDocument())
            {
                WriteFieldConnection(pDocument, lFbxObject, pObject);
            }
        }
    }

    // Store all property connections
    FbxIterator<FbxProperty>  lFbxDstPropertyIter(pObject);
    FbxProperty                lFbxDstProperty;

    FbxForEach(lFbxDstPropertyIter,lFbxDstProperty) 
    {
        FbxProperty                lSrcFbxProperty;
        FbxObject*                 lSrcObject;
        int                         i;

        if (lFbxDstProperty.GetFlag(FbxPropertyFlags::eNotSavable))
        {
            // the property is not savable, no point on continuing...
            continue;
        }

        // PP Connections
        for(i=0; i<lFbxDstProperty.GetSrcPropertyCount(); i++) 
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
        for(i=0; i<lFbxDstProperty.GetSrcObjectCount(); i++) 
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

void FbxWriterFbx7_Impl::WriteTakes(FbxDocument* pDocument)
{
    if(pDocument == NULL)
        return;

    FbxScene* lScene = FbxCast<FbxScene>(pDocument);
    if (!lScene)
        return;

    FbxArray<FbxString*> lNameArray;
    FbxTimeSpan lAnimationInterval(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
    FbxTimeSpan lGlobalTimeSpan;
	lScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lGlobalTimeSpan);

    int lNameIter; //, lObjectIter;

	pDocument->FillAnimStackNameArray(lNameArray);
    int nbAnimStacks = lScene->GetSrcObjectCount<FbxAnimStack>();
    FBX_ASSERT(nbAnimStacks == lNameArray.GetCount());
	bool *lHasSomeAnimation = new bool[lNameArray.GetCount()+1];

	// For all takes
    for(lNameIter = 0; lNameIter < lNameArray.GetCount(); lNameIter++)
    {
        lHasSomeAnimation[lNameIter] = false;
		// Avoid writing the default take
		if(lNameArray.GetAt(lNameIter)->Compare(FBXSDK_TAKENODE_DEFAULT_NAME) == 0)
        {
			continue;
        }
	
		// Check if we really have anim curve nodes. This will indicate us if there
		// is some valid animation or not. 
		FbxAnimStack* lAnimStack = pDocument->GetMember<FbxAnimStack>(lNameIter);
		FBX_ASSERT(lAnimStack != NULL);
	
	    // for each layer defined in this stack
		for (int l = 0; l < lAnimStack->GetMemberCount<FbxAnimLayer>(); l++)
		{
			FbxAnimLayer* lAnimLayer = lAnimStack->GetMember<FbxAnimLayer>(l);
			lHasSomeAnimation[lNameIter] |= lAnimLayer->GetMemberCount<FbxAnimCurveNode>() > 0;
		}
	}

	// we need to keep writing the section for the plug-ins that are already out there
	// Failing to do so, will result in them selecting automatically the "No Animation"
	// option and this mode has serious sides effects (fixed in the 2012.1 and above) but,
	// still present in older apps.
		mFileObject->WriteComments("Takes section");
		mFileObject->WriteComments("----------------------------------------------------");
		mFileObject->WriteComments("");

		mFileObject->FieldWriteBegin("Takes");
		mFileObject->FieldWriteBlockBegin();
		{
	        
			mFileObject->FieldWriteC("Current", lScene->ActiveAnimStackName.Get().Buffer());    

			// For all takes
			for(lNameIter = 0; lNameIter < lNameArray.GetCount(); lNameIter++)
			{
				// Avoid writing the default take
				if(lNameArray.GetAt(lNameIter)->Compare(FBXSDK_TAKENODE_DEFAULT_NAME) == 0)
				{
					continue;
				}

				FbxAnimStack* lAnimStack = pDocument->GetMember<FbxAnimStack>(lNameIter);
				FBX_ASSERT(lAnimStack != NULL);

				if (lHasSomeAnimation[lNameIter] == false)
				{
					lAnimationInterval = lGlobalTimeSpan;
				}
				else
				{            
					// for each layer defined in this stack
					for (int l = 0; l < lAnimStack->GetMemberCount<FbxAnimLayer>(); l++)
					{
						FbxAnimLayer* lAnimLayer = lAnimStack->GetMember<FbxAnimLayer>(l);
						for (int n = 0; n < lAnimLayer->GetMemberCount<FbxAnimCurveNode>(); n++)
						{
							FbxAnimCurveNode* lCurveNode = lAnimLayer->GetMember<FbxAnimCurveNode>(n);
							lCurveNode->GetAnimationInterval(lAnimationInterval);
						}
					}
				}
	 
				mFileObject->FieldWriteBegin("Take");
				mFileObject->FieldWriteC(lAnimStack->GetName());
				mFileObject->FieldWriteBlockBegin();
				{
					FbxString lFileName(lAnimStack->GetName());
					lFileName += ".tak";
					while(lFileName.FindAndReplace(" ", "_")) {
					} // Make sure no whitespace is left in the file name
					while(lFileName.FindAndReplace("\t", "_")) {
					} // Make sure no tabulation is left in the file name
					mFileObject->FieldWriteC("FileName",lFileName);
	        
					if(lAnimationInterval.GetStart()>lAnimationInterval.GetStop() )
					{
						lAnimationInterval.SetStart(0) ;
						FbxTime lTmpTime ;
						lTmpTime.SetTime(0,0,1);
						lAnimationInterval.SetStop(lTmpTime)  ;
					}
	        
					// Local time span is set to current take animation interval if it was
					// left to it's default value i.e. both start and stop time at 0.
                    if (!FbxProperty::HasDefaultValue(lAnimStack->LocalStart) ||
					    !FbxProperty::HasDefaultValue(lAnimStack->LocalStop))
					{
						mFileObject->FieldWriteTS("LocalTime", lAnimStack->GetLocalTimeSpan());
					}
					else
					{
						mFileObject->FieldWriteTS("LocalTime", lAnimationInterval);
					}
	        
					// Reference time span is set to current take animation interval if it was
					// left to it's default value i.e. both start and stop time at 0.
                    if (!FbxProperty::HasDefaultValue(lAnimStack->ReferenceStart) ||
					    !FbxProperty::HasDefaultValue(lAnimStack->ReferenceStop))
					{
						mFileObject->FieldWriteTS("ReferenceTime", lAnimStack->GetReferenceTimeSpan());
					}
					else
					{
						mFileObject->FieldWriteTS("ReferenceTime", lAnimationInterval);
					}
	        
					if(lAnimStack->Description.Get().IsEmpty() == false)
					{
						mFileObject->FieldWriteC("Comments", lAnimStack->Description.Get());
					}
	                
					// Use sceneInfo thumbnail in the takes (to maintain backward compatibility)
					if (lScene->GetSceneInfo() && lScene->GetSceneInfo()->GetSceneThumbnail())
					{
						WriteThumbnail(lScene->GetSceneInfo()->GetSceneThumbnail());
   					}
				}
				mFileObject->FieldWriteBlockEnd();
				mFileObject->FieldWriteEnd();
			}
		}
	
		mFileObject->FieldWriteBlockEnd();
		mFileObject->FieldWriteEnd();
    
    FbxArrayDelete(lNameArray);
    delete[] lHasSomeAnimation;
}

bool FbxWriterFbx7_Impl::CreateCollapseDocument(FbxDocument* pParentDocument)
{
    FBX_ASSERT_RETURN_VALUE( !mCollapseDocument, false );
    FBX_ASSERT_RETURN_VALUE( pParentDocument, false );

    /*
        Create a sub-doc to collapse every external objects under.  This
        sub-doc will not contain any actual objects, except other sub-docs.

        These sub-docs will be used to replicate the document hiearchies of
        external documents.  ie:

        Fbx Libraries
        |
        Fbx System Libraries
        |
        ADSKLibrary          <-----     User Scene

        would end up as

        User Scene
        |
        Externals
        |
        Fbx Libraries
        | 
        Fbx System Libraries
        |
        ADSKLibrary

        If the scene used other externals, they'd be under the Externals
        document.  If you have oddball cases such as:

             R
             |
        ___________
        |         |
        A <--|    B
             |    |
             |--- C

        and you save 'A', then you'll end up with:

        A
        |
        Externals
        |
        R
        |
        B
        |
        C
    */

    mCollapseDocument = FbxDocument::Create(pParentDocument, "Externals");

    return mCollapseDocument != NULL;
}

FbxDocument* FbxWriterFbx7_Impl::GetOrCreateCollapedDocument(FbxDocument* pExternalDocument)
{
    FBX_ASSERT_RETURN_VALUE(mCollapseDocument, NULL);
    FBX_ASSERT_RETURN_VALUE(mCollapseDocument->GetDocument(), NULL);    // Should be a child of the top document
    FBX_ASSERT_RETURN_VALUE(pExternalDocument, NULL);

	FbxExternalDocumentCollapseMap::RecordType* iter = mExternalDocMapping.Find(pExternalDocument);
    if( iter ) return iter->GetValue();

    // First time we get to look at this external document; see if we need to create its parents, too.
    // Walk up until we run out of parents
    // 

    FbxDocument* lParentDoc = pExternalDocument->GetDocument();

    if( lParentDoc )
    {
        lParentDoc = GetOrCreateCollapedDocument(lParentDoc);
        FBX_ASSERT_RETURN_VALUE(lParentDoc, NULL);
    }
    else
    {
        // Parent it to the collapse document top dog.
        lParentDoc = mCollapseDocument;    
    }

    FbxDocument* lCollapsed = FbxDocument::Create(lParentDoc, pExternalDocument->GetName());
    FBX_ASSERT_RETURN_VALUE(lCollapsed, NULL);

    mExternalDocMapping[pExternalDocument] = lCollapsed;

    /*
        We copy over the Url so all documents will point to the same final
        destination, and file resolving will work reliably.  Otherwise
        if you save over an existing fbx file (with its fbm folder) you
        may have half the files resolving to the new .fbm folder, and
        half resolving to the old one.

        It's questionable whether or not we should resolve to the new fbm
        folder, or the old; possibly we should delete the .fbm folder
        when we create the file (FbxIO already deletes the .fbd folder); if we do
        this we'd need to look at the asset builder so it doesn't have to
        copy 800MB everytime it's run.

        Note that doing this we lose the information of where we're collapsing
        from.  To be nice, we copy these to backup properties, which are only
        used when inspecting the file.

    */
    FbxDocumentInfo* lNewDocumentInfo  = lCollapsed->GetDocumentInfo();
    FbxDocumentInfo* lDocumentInfo     = mCollapseDocument->GetDocument()->GetDocumentInfo();

    if( lNewDocumentInfo && lDocumentInfo )
    {
        FbxString lPropName = "Original_";
        lPropName += lNewDocumentInfo->Url.GetName();

        FbxProperty lOldUrl = FbxProperty::Create(lNewDocumentInfo, lNewDocumentInfo->Url.GetPropertyDataType(), lPropName);
        if( lOldUrl.IsValid() )
        {
            lOldUrl.Set(lNewDocumentInfo->Url.Get());
        }

        lPropName = "Original_";
        lPropName += lNewDocumentInfo->LastSavedUrl.GetName();

        FbxProperty lOldLastSavedUrl = FbxProperty::Create(lNewDocumentInfo, lNewDocumentInfo->LastSavedUrl.GetPropertyDataType(), lPropName);

        if( lOldLastSavedUrl.IsValid() )
        {
            lOldLastSavedUrl.Set(lNewDocumentInfo->LastSavedUrl.Get());
        }

        lNewDocumentInfo->Url.Set(lDocumentInfo->Url.Get());
        lNewDocumentInfo->LastSavedUrl.Set(lDocumentInfo->LastSavedUrl.Get());
    }

    return lCollapsed;
}

inline bool DocumentInHiearchy(FbxDocument* pRootDocument, FbxDocument* pDoc)
{
    FBX_ASSERT_RETURN_VALUE(pRootDocument, false);
    FBX_ASSERT_RETURN_VALUE(pDoc, false);

    // Walk up the document chain, until we hit the root document, or we run out
    // of parent docs.
    // 
    // The 'same doc' case should be pretty quick, since we won't even call GetDocument()
    for( ; pDoc; pDoc = pDoc->GetDocument() )
    {
        if( pDoc == pRootDocument )
        {
            return true;
        }
    }

    return false;
}

bool FbxWriterFbx7_Impl::CollapseExternalObjects(FbxDocument* pDocument)
{
    FBX_ASSERT_RETURN_VALUE(pDocument, false);

    if( !CreateCollapseDocument(pDocument) )
    {
        return false;
    }

    bool lResult = CollapseExternalObjectsImpl(pDocument);

	if( mCollapsedObjects.Empty() )
    {
        // Nothing to collapse, don't bother keeping the collapse document hiearchy
        mCollapseDocument->Destroy(true);
        mCollapseDocument = NULL;
    }

    return lResult;
}

bool FbxWriterFbx7_Impl::CollapseExternalObjectsImpl(FbxDocument* pDocument)
{
    FBX_ASSERT_RETURN_VALUE(pDocument, false);

    for( int i = 0, n = pDocument->GetMemberCount(); i < n; ++i )
    {
        FbxObject* lObject = pDocument->GetMember(i);
        FBX_ASSERT( lObject );

		FbxObject* lReference = lObject ? lObject->GetReferenceTo() : NULL;
		FbxDocument* lRefDoc  = lReference ? lReference->GetDocument() : NULL;

        if( lReference && !DocumentInHiearchy(pDocument, lRefDoc) )
        {
            FbxDocument* lCollapseDocument = GetOrCreateCollapedDocument(lRefDoc);
            FBX_ASSERT_RETURN_VALUE(lCollapseDocument, false);

            // Collapse this object, and all its connections, into our collapse document.
            CollapseExternalObject(lReference, lCollapseDocument);
            n = pDocument->GetMemberCount();
        }

        // Recurse into sub-documents; but don't recurse into the collapse document hiearchy!
        FbxDocument* pDoc = FbxCast<FbxDocument>(lObject);
        if( pDoc && pDoc != mCollapseDocument )
        {
            if( !CollapseExternalObjectsImpl(pDoc) )
            {
                return false;
            }
            n = pDocument->GetMemberCount();
        }
    }

    return CollapseExternalImplementations(pDocument);
}

bool FbxWriterFbx7_Impl::CollapseExternalImplementations(FbxDocument* pDocument)
{
    FBX_ASSERT_RETURN_VALUE(pDocument, false);

    // Collapse implementations, which are stored 'backwards' with regards to the
    // regular connections, and thus shouldn't have been collapsed already.
    FbxMap<FbxDocument*, FbxSet<FbxImplementation*> > lImplementations;
    for( FbxCollapseMap::Iterator iter = mCollapsedObjects.Begin(); iter != mCollapsedObjects.End(); ++iter )
    {
		FbxObject* lObject = iter->GetKey();
        if( lObject->GetDefaultImplementation() )
        {
            FbxImplementation* lImpl = lObject->GetDefaultImplementation();
            FbxDocument*       lDoc  = lImpl->GetDocument();

            if( !DocumentInHiearchy(pDocument, lDoc) )
            {  
                lImplementations[lDoc].Insert(lImpl);
            }
        }

        for( int i = 0, n = lObject->GetImplementationCount(); i < n; ++i )
        {
            FbxImplementation* lImpl = lObject->GetImplementation(i);
            FbxDocument*       lDoc  = lImpl->GetDocument();

            if( !DocumentInHiearchy(pDocument, lDoc) )
            {
                lImplementations[lDoc].Insert(lImpl);
            }
        }
    }

    // Keep the same order as we have in the source document(s)
    for( FbxMap<FbxDocument*, FbxSet<FbxImplementation*> >::Iterator iter = lImplementations.Begin(); iter != lImplementations.End(); ++iter )
    {
		if( iter->GetKey() )
        {
			FbxDocument* lCollapseDoc = GetOrCreateCollapedDocument(iter->GetKey());
            FBX_ASSERT_RETURN_VALUE(lCollapseDoc, false);

            // Iterate over known implementations; if it's one we were supposed to collapse, accumulate it
            // then collapse it afterwards; we don't collapse now so we don't mess up the iteration.

            FbxArray<FbxImplementation*> lOrderedImplementations;
			for( int i = 0, n = iter->GetKey()->GetSrcObjectCount<FbxImplementation>(); i < n; ++i )
            {
				FbxImplementation* lImpl = iter->GetKey()->GetSrcObject<FbxImplementation>(i);

				if( iter->GetValue().Find(lImpl) )
                {
                    lOrderedImplementations.Add(lImpl);
                }
            }

			for( int i = 0, c = lOrderedImplementations.Size(); i < c; ++i )
            {
                CollapseExternalObject(lOrderedImplementations[i], lCollapseDoc);
            }
        }
        else
        {
            // Floating implementations?  Just collapse them to our .. collapse document.
			for( FbxSet<FbxImplementation*>::Iterator implIter = iter->GetValue().Begin(); implIter != iter->GetValue().End(); ++implIter )
            {
				CollapseExternalObject(implIter->GetKey(), mCollapseDocument);
            }
        }
    }

    return true;
}

void FbxWriterFbx7_Impl::CollapseExternalObject(FbxObject* pObject, FbxDocument* pCollapseDocument)
{
    FBX_ASSERT_RETURN( pObject );

    FbxDocument* lDoc = pObject->GetDocument();
    if( lDoc != pCollapseDocument )
    {   
        FBX_ASSERT(!mCollapsedObjects.Find(pObject));
        mCollapsedObjects.Insert(pObject, lDoc);

        pObject->SetDocument(NULL);     // Disconnect from old document
        pObject->SetDocument(pCollapseDocument);
    }

    FbxObject* lReference = pObject->GetReferenceTo();
    if( lReference )
    {
        CollapseExternalObject(lReference, pCollapseDocument);
    }

    for (int i = 0, n = pObject->GetSrcObjectCount(); i < n; i++)
    {
        FbxObject* lSrcObject = pObject->GetSrcObject(i);
        CollapseExternalObject(lSrcObject, pCollapseDocument);
    }

    // Propagate the document on src properties
    FbxIterator<FbxProperty>  lFbxPropertyIter(pObject);
    FbxProperty                lProp;
    FbxForEach(lFbxPropertyIter, lProp)
    {
        for( int i = 0, n = lProp.GetSrcObjectCount(); i < n; ++i )
        {
            FbxObject* lSrcObject = lProp.GetSrcObject(i);
            if( lSrcObject )
            {
                CollapseExternalObject(lSrcObject, pCollapseDocument);
            }
        }
    }
}

bool FbxWriterFbx7_Impl::RemoveCollapsedExternalObjects()
{
    FBX_ASSERT(((mCollapseDocument == NULL) && mCollapsedObjects.Empty()) || (mCollapseDocument && !mCollapsedObjects.Empty()));
    for( FbxCollapseMap::Iterator iter = mCollapsedObjects.Begin(); iter != mCollapsedObjects.End(); ++iter )
    {
		FbxObject* lObject = iter->GetKey();
        FBX_ASSERT( lObject );

        lObject->SetDocument(NULL);                 // Disconnect from old document
		lObject->SetDocument(iter->GetValue());
    }

    mCollapsedObjects.Clear();

    if( mCollapseDocument )
    {
        mCollapseDocument->Destroy(true);           // Disconnect whole doc hiearchy
        mCollapseDocument = NULL;
    }

    return true;
}

bool FbxWriterFbx7_Impl::WriteEmbeddedFiles(FbxDocument* pTopDocument)
{
    FBX_ASSERT_RETURN_VALUE(pTopDocument, false);

    if(!IOS_REF.GetBoolProp(EXP_FBX_EMBEDDED, false))
    {
        return true;
    }

    FbxSet<FbxString>   lPropertyFilter;
    GetEmbeddedPropertyFilter(IOS_REF.GetProperty(EXP_FBX_EMBEDDED_PROPERTIES_SKIP), lPropertyFilter);

    FbxEmbeddedFilesAccumulator    lAccumulator(mManager, "", lPropertyFilter);

    // The accumulator automatically recurses into sub-document.
    lAccumulator.ProcessCollection(pTopDocument);

    if( !lAccumulator.mEmbeddedFiles.Empty() )
    {
        mFileObject->WriteComments ("");
        mFileObject->WriteComments (" Embedded Files");
        mFileObject->WriteComments ("------------------------------------------------------------------");
        mFileObject->WriteComments ("");

        // Write filenames relative to this fictious .fbm folder, which we'll create
        // when the .fbx is read back.
        // 
        // Filenames relative to this must be unique, so if there are more than one
        // file that resolves to the same relative file, unique filenames will be
        // assigned by creating sub-folders.
        FbxString lRootFolder = FbxPathUtils::ChangeExtension(FbxPathUtils::Resolve(mFileObject->GetFilename()), ".fbm");

        FbxStringNoCaseSet   lUsedRelativePaths;

        mFileObject->FieldWriteBegin(FIELD_EMBEDDED_FILES);
        mFileObject->FieldWriteBlockBegin();
        {
            for( FbxEmbeddedFilesAccumulator::EmbeddedFilesMap::Iterator iter = lAccumulator.mEmbeddedFiles.Begin();
                 iter != lAccumulator.mEmbeddedFiles.End(); ++iter  )
            {
                mFileObject->FieldWriteBegin(FIELD_EMBEDDED_FILE);
                mFileObject->FieldWriteBlockBegin();
                {
                    FbxString lAbsoluteFileName = iter->GetKey();
                    FBX_ASSERT( !FbxPathUtils::IsRelative(lAbsoluteFileName) );

                    FbxEmbeddedFilesAccumulator::EmbeddedFileInfo& lInfo = iter->GetValue();

                    FbxString lRelativeFileName       = FbxPathUtils::GetRelativeFilePath(lRootFolder, lAbsoluteFileName);
                    FbxString lEmbeddedRelativeFile   = GetEmbeddedRelativeFilePath(lInfo.mOriginalPropertyUrl, lRootFolder, lAbsoluteFileName);
                    FbxString lUniqueRelativeFileName = MakeRelativePathUnique(lEmbeddedRelativeFile, lUsedRelativePaths);

                    mFileObject->FieldWriteC(FIELD_EMBEDDED_FILENAME, lUniqueRelativeFileName);
                    mFileObject->FieldWriteC(FIELD_EMBEDDED_ORIGINAL_FILENAME, lRelativeFileName);

                    WriteFileAsBlob(lAbsoluteFileName);
                    WriteFileConsumers(lInfo.mConsumers);

                    lUsedRelativePaths.Insert(lUniqueRelativeFileName);
                }
                mFileObject->FieldWriteBlockEnd();
                mFileObject->FieldWriteEnd();
            }
        }
        mFileObject->FieldWriteBlockEnd();
        mFileObject->FieldWriteEnd();
    }

    return true;
}

void FbxWriterFbx7_Impl::WriteFileAsBlob(const FbxString& pFileName)
{
    FbxFile   lFile;

	if( lFile.Open(pFileName, FbxFile::eReadOnly) )
    {
        mFileObject->FieldWriteBegin(FIELD_MEDIA_CONTENT);

        FbxBinaryFileReader lFileReader(lFile);
        OutputBinaryBuffer(lFileReader);

        mFileObject->FieldWriteEnd();
    }
    else
    {
        // What I do?
    }
}

void FbxWriterFbx7_Impl::WriteFileConsumers(const FbxEmbeddedFilesAccumulator::ObjectPropertyMap& pConsumers)
{
    mFileObject->FieldWriteBegin(FIELD_EMBEDDED_CONSUMERS);
    mFileObject->FieldWriteBlockBegin();
    {
        for( FbxEmbeddedFilesAccumulator::ObjectPropertyMap::ConstIterator lObjectIter = pConsumers.Begin(); 
             lObjectIter != pConsumers.End(); ++lObjectIter )
        {
            FbxLongLong lObjectId = GetObjectId(lObjectIter->GetKey());

            for( FbxEmbeddedFilesAccumulator::PropertyUrlIndexSet::ConstIterator lPropIter = lObjectIter->GetValue().Begin();
                 lPropIter != lObjectIter->GetValue().End(); ++lPropIter )
            {
                mFileObject->FieldWriteBegin(FIELD_EMBEDDED_CONSUMER);
                    mFileObject->FieldWriteLL(lObjectId);
                    mFileObject->FieldWriteC(lPropIter->GetValue().mPropName);
                    mFileObject->FieldWriteI(lPropIter->GetValue().mIndex);
                mFileObject->FieldWriteEnd();
            }
        }
    }
    mFileObject->FieldWriteBlockEnd();
    mFileObject->FieldWriteEnd();
}

void FbxWriterFbx7_Impl::OutputBinaryBuffer(FbxBinaryDataReader& pReader)
{
    int lOutputSize = pReader.GetSize();
    mFileObject->FieldWriteI(lOutputSize);
    mFileObject->FieldWriteBlockBegin();
    {
        const int kMaxChunkSize = mFileObject->GetFieldRMaxChunkSize();
        FBX_ASSERT(kMaxChunkSize > 0);

        if( lOutputSize > 0 )
        {
            mFileObject->FieldWriteBegin("BinaryData");

            const char* lBuffer     = NULL;
            int         lChunkSize  = 0;

            while( pReader.GetNextChunk(lBuffer, lChunkSize, kMaxChunkSize) )
            {
                mFileObject->FieldWriteR(lBuffer, lChunkSize);
            }

            mFileObject->FieldWriteEnd();
        }
    }
    mFileObject->FieldWriteBlockEnd();
}

FbxString FbxWriterFbx7_Impl::GetEmbeddedRelativeFilePath(const FbxString& pOriginalPropertyURL, const FbxString& pRootFolder, const FbxString& pAbsoluteFileName)
{
    /*
        root folder:  c:/temp/bob.fbm

        abs filename: c:/data/somewhere/file.png
        rel:        : ../data/somewhere/file.png
    
        abs filename: d:/data/somewhere/file.png
        rel:          d:/data/somewhere/file.png
    
        abs filename: d:/temp/somewhere/file.png
        rel:          d:/temp/somewhere/file.png
    
        abs filename: c:/temp/jello/biafra.png
        rel:          jello/biafra.png

        If we can, re-use the original relative filename, since we may have:

        org: Maps/textures/blah.png
        abs: c:/temp/assetlib.fbm/Maps/textures/blah.png

        and that would give:

        rel: ../assetlib.fbm/Maps/textures/blah.png

        In this case, we'd want to use:

        rel: Maps/textures/blah.png
    */

    FbxString lRelativeFileName = pOriginalPropertyURL;

    if( !FbxPathUtils::IsRelative(lRelativeFileName) )  
    {
        lRelativeFileName = FbxPathUtils::GetRelativeFilePath(pRootFolder, pAbsoluteFileName);

        if( !FbxPathUtils::IsRelative(lRelativeFileName) )
        {
            // File an absolute.
            // Just use the filename itself, then.
            // 
            return FbxPathUtils::GetFileName(lRelativeFileName);
        }
    }

    int lPos = lRelativeFileName.Find("..");

    while( lPos >= 0 )
    {
        int lNextPos = lRelativeFileName.Find("..", lPos+1);

        if( lNextPos < 0 )
        {
            // Last one we found was the last '..' in the string; remove it,
            // including the '/'
            FBX_ASSERT( lRelativeFileName.Buffer()[lPos+2] == '/' ||
                      lRelativeFileName.Buffer()[lPos+2] == '\\' );
            lRelativeFileName = lRelativeFileName.Mid(lPos+3);  

            break;
        }
        else
        {
            lPos = lNextPos;
        }
    }

    return lRelativeFileName;
}

FbxString FbxWriterFbx7_Impl::MakeRelativePathUnique(const FbxString& pRelativeFileName, const FbxStringNoCaseSet& pUsedRelativePaths)
{
    FBX_ASSERT( FbxPathUtils::IsRelative(pRelativeFileName) );

    if( !pUsedRelativePaths.Find(pRelativeFileName) ) return pRelativeFileName;

    int iFolderCount = 0;

    FbxString lNewRelativeFilename;

    do {
        char szBuffer[8];

        FBXSDK_sprintf(szBuffer, 8, "%04d/", iFolderCount++);

        lNewRelativeFilename = szBuffer;
        lNewRelativeFilename += pRelativeFileName;

    } while( pUsedRelativePaths.Find(lNewRelativeFilename) );

    return lNewRelativeFilename;
}

void FbxWriterFbx7_Impl::GetEmbeddedPropertyFilter(FbxProperty pProperty, FbxSet<FbxString>& pPropertyFilter)
{
    if( pProperty.IsValid() )
    {   
        FbxString lPropertyName = pProperty.Get<FbxString>();

        if( !lPropertyName.IsEmpty() )
        {
            pPropertyFilter.Insert(lPropertyName);
        }

        for( pProperty = pProperty.GetChild(); pProperty.IsValid(); pProperty = pProperty.GetSibling() )
        {
            GetEmbeddedPropertyFilter(pProperty, pPropertyFilter);
        }
    }
}

void FbxWriterFbx7_Impl::WriteValueArray(int pCount, const double* pValues)
{
    if( pCount > 0 )
    {
        mFileObject->FieldWriteArrayD(pCount, pValues);
    }
}

void FbxWriterFbx7_Impl::WriteValueArray(int pCount, const float* pValues)
{
    if( pCount > 0 )
    {
        mFileObject->FieldWriteArrayF(pCount, pValues);
    }
}

void FbxWriterFbx7_Impl::WriteValueArray(int pCount, const int* pValues)
{
    if( pCount > 0 )
    {
        mFileObject->FieldWriteArrayI(pCount, pValues);
    }
}

void FbxWriterFbx7_Impl::WriteValueArray(int pCount, const bool* pValues)
{
    if( pCount > 0 )
    {
        mFileObject->FieldWriteArrayB(pCount, pValues);
    }
}

void FbxWriterFbx7_Impl::WriteValueArray(int pCount, const FbxUChar* pValues)
{
    if( pCount > 0 )
    {
        mFileObject->FieldWriteArrayBytes(pCount, pValues);
    }
}

#include <fbxsdk/fbxsdk_nsend.h>

