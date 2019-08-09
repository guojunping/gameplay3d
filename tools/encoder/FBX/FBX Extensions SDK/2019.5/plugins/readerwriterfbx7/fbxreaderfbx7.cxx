/****************************************************************************************
 
   Copyright (C) 2016 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/fileio/fbx/fbxreaderfbx7.h>
#include <fbxsdk/fileio/fbxglobalsettings.h>
#include <fbxsdk/fileio/fbximporter.h>
#include <fbxsdk/utils/fbxscenecheckutility.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

#if defined(FBXSDK_ENV_LINUX) || defined(FBXSDK_ENV_MAC)
	#include <stdint.h>
#endif

#define IOS_REF (*GetIOSettings())

FbxLayerElement::EMappingMode       ConvertMappingModeToken(const char* pToken);
FbxLayerElement::EReferenceMode     ConvertReferenceModeToken(const char* pToken);
FbxLayerElementTexture::EBlendMode  ConvertBlendModeToken(const char* pToken);

const FbxLongLong kInvalidObjectId  = -1;
const FbxLongLong kRootNodeObjectId = 0;

//
// Local class
//
typedef FbxDynamicArray<FbxString> FbxObjectTypeInfoList;

//
// Local struct
// used to manage external references
//
struct KTypeReadReferenceInfo
{
    bool        mReferenceIsExternal;
    FbxString     mReferenceName;
    FbxString     mReferencedObjectName;
    FbxString     mReferencedDocumentRootName;
    FbxString     mReferencedDocumentPathName;
    FbxObject* mReferencedObject;
    FbxLongLong   mReferencedObjectId;    // Only when the reference is not external
};

//
// Local class
// used to manage external references
//
class KTypeReadReferences
{
    public:

        KTypeReadReferences(void) {};
        virtual ~KTypeReadReferences(void) { FbxArrayDelete(mReferences); };
        int AddReference(
            bool  pExternalRef,
            const char* pReferenceName,
            const char* pReferencedObjectName,
            const char* pReferencedDocumentRootName,
            const char* pReferencedDocumentPathName,
            FbxLongLong   pReferencedObjectId
        );
        int ResolveForDocument(FbxDocument* pReferencingDocument, FbxDocument* pReferencedDocument, bool pForceExternal = false);
        bool AreAllExternalReferencesResolved(void);
        bool GetReferenceResolution(const char* pRefName, FbxString& pRefObjName, FbxLongLong& pRefObjId, bool& pRefExternal, FbxObject* &pRefObj);

        bool DocumentHasExternals(const char* pDocumentName) const;
    private:
        FbxArray< KTypeReadReferenceInfo* >   mReferences;
};

bool KTypeReadReferences::AreAllExternalReferencesResolved(void)
{
    KTypeReadReferenceInfo* lRefInfo;
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

bool KTypeReadReferences::DocumentHasExternals(const char* pDocumentName) const
{
    FBX_ASSERT( pDocumentName );

    int     j, lCount = mReferences.GetCount();

    for (j = 0; j < lCount; j++)
    {
        KTypeReadReferenceInfo* lInfo = mReferences[j];

        if( lInfo->mReferenceIsExternal && 
            lInfo->mReferencedDocumentPathName == pDocumentName )
        {
            return true;
        }
    }

    return false;
}

int KTypeReadReferences::ResolveForDocument(FbxDocument* pReferencingDocument, FbxDocument* pReferencedDocument, bool pForceExternal)
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

int KTypeReadReferences::AddReference
(
    bool          pExternalRef,
    const char*   pReferenceName,
    const char*   pReferencedObjectName,
    const char*   pReferencedDocumentRootName,
    const char*   pReferencedDocumentPathName,
    FbxLongLong     pReferencedObjectId
)
{
    KTypeReadReferenceInfo* lRefInfo = FbxNew< KTypeReadReferenceInfo >();
    lRefInfo->mReferenceIsExternal = pExternalRef;
    lRefInfo->mReferenceName = pReferenceName;
    lRefInfo->mReferencedObjectName = pReferencedObjectName;
    lRefInfo->mReferencedDocumentRootName = pReferencedDocumentRootName;
    lRefInfo->mReferencedDocumentPathName = pReferencedDocumentPathName;
    lRefInfo->mReferencedObjectId = pReferencedObjectId;
    lRefInfo->mReferencedObject = NULL;

    return mReferences.Add(lRefInfo);
}

bool KTypeReadReferences::GetReferenceResolution
(
    const char*     pRefName,
    FbxString&        pRefObjName,
    FbxLongLong&      pRefObjId,
    bool&           pRefExternal,
    FbxObject*&    pRefObj
 )
{
    FBX_ASSERT( pRefName );

    pRefObj         = NULL;
    pRefExternal    = false;

    int     i, lCount = mReferences.GetCount();
    FbxString lReferenceName(pRefName);

    KTypeReadReferenceInfo* lRefInfo;

    for (i = 0; i < lCount; i++)
    {
        lRefInfo = mReferences[i];
        if (lReferenceName == lRefInfo->mReferenceName)
        {
            pRefObjName     = lRefInfo->mReferencedObjectName;
            pRefObj         = lRefInfo->mReferencedObject;
            pRefObjId       = lRefInfo->mReferencedObjectId;
            pRefExternal    = lRefInfo->mReferenceIsExternal;
            return true;
        }
    }
    return false;
}

// Helper class to merge Class root property templates
// saved in the fbx files with new classes of that type.
//
class ClassTemplateMap
{
public:
    ClassTemplateMap();
    ~ClassTemplateMap();

    // ClassTemplateMap will own this template object.
    bool AddClassId( FbxClassId pId, FbxObject* pTemplateObject );
    bool MergeWithTemplate( FbxObject* pObject ) const;
    void Clear();

private:

    typedef FbxMap< FbxClassId, FbxObject*, FbxClassIdCompare > MapType;
    MapType mClassMap;

    bool HasModifiedFlags(FbxProperty lProp) const;
    inline FbxPropertyFlags::EFlags IndexToFlag( int i ) const { return static_cast<FbxPropertyFlags::EFlags>(1 << i); }
};

ClassTemplateMap::ClassTemplateMap()
{

}

ClassTemplateMap::~ClassTemplateMap()
{
    Clear();
}

bool ClassTemplateMap::AddClassId( FbxClassId pId, FbxObject* pTemplateObj )
{
    if( !pId.IsValid() )
    {
        FBX_SAFE_DESTROY(pTemplateObj);
        return false;
    }

    mClassMap.Insert( pId, pTemplateObj );

    return true;
}

bool ClassTemplateMap::HasModifiedFlags(FbxProperty lProp) const
{
    for( int i = 0; i < FbxPropertyFlags::eFlagCount; ++i )
    {
        if( lProp.ModifiedFlag(IndexToFlag(i)) )
            return true;
    }
    return false;
}

bool ClassTemplateMap::MergeWithTemplate( FbxObject* pObject ) const
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
            // should exist..
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

void ClassTemplateMap::Clear()
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
void ForceFileNameExtensionToTif(FbxString& FileName)
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
    ClassTemplateMap& pTemplateMap,
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
                lObject->SetInitialName(pNewObjectName);
                lObject->SetName(pNewObjectName);
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

FbxObject* CreateOrCloneReference
(
    FbxManager& pManager,
    FbxString&        pNewObjectName,
    FbxObject*     pReferencedObject,
    FbxClassId     pClassId,
    ClassTemplateMap& pTemplateMap
)
{
    FbxObject* lObject = pReferencedObject;
    if (lObject != NULL)
    {
        // we need to make sure the object is loaded,
        if( lObject->ContentIsLoaded() || lObject->ContentLoad() != 0 )
        {
            lObject = pReferencedObject->Clone(FbxObject::eReferenceClone);
            if( lObject )
            {
                lObject->SetInitialName(pNewObjectName);
                lObject->SetName(pNewObjectName);
            }
            else
            {
                FBX_ASSERT_NOW("Unable to clone object.");
            }
        }
        else
            FBX_ASSERT_NOW("Failed to load content");
    }

    if (lObject == NULL)
    {
        FBX_ASSERT( pClassId.IsValid() );

        if (pClassId.IsValid())
        {
            lObject = FbxCast<FbxObject>(pClassId.Create(pManager, pNewObjectName, NULL));
        }
    }

    FBX_ASSERT( lObject );
    if( lObject )
    {
        pTemplateMap.MergeWithTemplate( lObject );
    }

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
        ObjectClassId = mManager.FindFbxFileClass(pObjectType, pObjectSubType);
    }

    if (!ObjectClassId.IsValid())
    {
        // Try to look for subtype since it's use for runtime class type
        ObjectClassId = mManager.FindClass(pObjectSubType);

        // If there were no subtype, than try to get the base class type
        if (!ObjectClassId.IsValid())
            ObjectClassId = mManager.FindClass(pObjectType);

        if (!ObjectClassId.IsValid())
        {
            FbxString lClassName = pObjectType+"_"+pObjectSubType;
            ObjectClassId = mManager.RegisterRuntimeFbxClass( lClassName,
                                                           pParentClass,
                                                           pObjectType,
                                                           pObjectSubType );
        }
    }

    return ObjectClassId;
}  

FbxClassId CheckRuntimeClass(const FbxString& pObjectType, const FbxString& pObjectSubType, FbxManager& mManager)
{
    return CheckRuntimeClass(FBX_TYPE(FbxObject), pObjectType, pObjectSubType, mManager);
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
    else if(!strcmp(pToken, TOKEN_KFBXGEOMETRYMESH_BY_EDGE))
    {
        lMappingMode = FbxLayerElement::eByEdge;
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
    else if (!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_OVERLAY))
    {
        lBlendMode = FbxLayerElementTexture::eOverlay;
    }
    else if(!strcmp(pToken, TOKEN_KFBXTEXTURE_BLEND_MAXBLEND))
    {
        lBlendMode = FbxLayerElementTexture::eBlendModeCount;
    }

    return lBlendMode;
}

void FixDocumentNames(FbxDocument* pDocument)
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
            lObj->SetInitialName(lObjNewName);
            lObj->SetName(lObjNewName);
        }
    }
}

FbxString ReplaceStringToken(const FbxString& pValue, int pIndex, const FbxString& pNewToken)
{
    FbxString lNewValue;

    for(int i = 0, n = pValue.GetTokenCount("|"); i < n; ++i )
    {
        if( i > 0 )
        {
            lNewValue += "|";
        }

        if( i == pIndex )
        {
            lNewValue += pNewToken;
        }
        else
        {
            lNewValue += pValue.GetToken(i, "|");
        }
    }

    return lNewValue;
}

int FbxGetFlagsFromChar(char pChar)
{
    // This function assumes that all flags can be stored in one byte (actually, in four bits).
    FBX_ASSERT_STATIC( FbxPropertyFlags::sLockedMembersMax == 4 );
    FBX_ASSERT_STATIC( FbxPropertyFlags::eLockedAll == ( 0xF << FbxPropertyFlags::sLockedMembersBitOffset ) );
    FBX_ASSERT_STATIC( FbxPropertyFlags::sMutedMembersMax == 4 );
    FBX_ASSERT_STATIC( FbxPropertyFlags::eMutedAll == ( 0xF << FbxPropertyFlags::sMutedMembersBitOffset ) );

	if( pChar >= '1' && pChar <= '9' ) return pChar - '0';				//Character '1' to '9': Return corresponding integer value
	else if( pChar >= 'a' && pChar <= 'e' ) return (pChar - 'a') + 10;	//Character 'a' to 'e': Return corresponding hexa value

	return 0xF;	//No character (or anything else), the 'L' character was either alone or with an invalid number, meaning all members are locked.
}

class FbxBinaryTarget
{
public:
    virtual ~FbxBinaryTarget() {}

    virtual bool Initialize(int pSize) = 0;
    virtual bool AppendData(const char* pData, int pSize) = 0;
};

class FbxBinaryBlobTarget : public FbxBinaryTarget
{
public:
    FbxBinaryBlobTarget()
    : mOffset(0)
    {
    }

public:

    bool Initialize(int pSize)
    {
        if( pSize < 0 )
        {
            return false;
        }

        mBlob = FbxBlob(pSize);

        return true;
    }
    bool AppendData(const char* pData, int pSize)
    {
        if( mOffset + pSize > mBlob.Size() )
        {
            FBX_ASSERT( false );
            return false;
        }

        memcpy(((char*) mBlob.Modify()) + mOffset, pData, pSize);
        mOffset += pSize;

        return true;
    }

    void AssignToProperty(FbxProperty pProperty)
    {
        FBX_ASSERT( pProperty.IsValid() );

        pProperty.Set(mBlob);
    }

private:
    FbxBlob mBlob;
    int     mOffset;
};

class FbxBinaryFileTarget : public FbxBinaryTarget
{
public:
    FbxBinaryFileTarget(const FbxString& pFilePath)
    : mFileName(pFilePath)
    {
    }

public:
    bool Initialize(int /* pSize */)
    {
        if( mFile.IsOpen() )
        {
            mFile.Close();
        }

		return FbxPathUtils::Create(FbxPathUtils::GetFolderName(mFileName)) && mFile.Open(mFileName, FbxFile::eCreateWriteOnly);
    }

    bool AppendData(const char* pData, int pSize)
    {
        if( !mFile.IsOpen() )
        {
            return false;
        }

        return mFile.Write(pData, pSize) == pSize;
    }

private:
    FbxString mFileName;
    FbxFile   mFile;
};


//
// Implementation of fbx7 reader
//
struct FbxReaderFbx7_Impl
{
    FbxIO*                  mFileObject;
    FbxManager&             mManager;
    FbxImporter&            mImporter;
	FbxReader*				mReader;
    FbxStatus&				mStatus;
    FbxCharPtrSet           mNodeArrayName;

    typedef FbxMap<FbxLongLong, FbxObject*> FbxUniqueIdObjectMap;
    typedef FbxMap<FbxObject*, FbxLongLong> FbxObjectUniqueIdMap;

    FbxUniqueIdObjectMap       mIdObjectMap;
    FbxObjectUniqueIdMap       mObjectIdMap;   // Reverse lookup

    FbxAutoDestroyPtr<FbxDocumentInfo>   mDocumentInfoFromHeader;

    // Used to retrieve the default render resolution
    // stored in the extended header
    bool    mDefaultCameraResolutionIsOK;
    FbxString mDefaultCameraName;
    FbxString mDefaultResolutionMode;
    double  mDefaultResolutionW;
    double  mDefaultResolutionH;

    bool mParseGlobalSettings;
    FbxGlobalSettings* mQuickAccessGlobalSettings; // these global settings are used when opening the file
    // to quickly retrieve the System axis and units.

    bool mRetrieveStats;
    FbxStatistics* mDefinitionsStatistics;

    ClassTemplateMap    mClassTemplateMap;

    FbxScene*          mSceneImport;

	FbxArray<FbxTakeInfo *> mTakeInfo;
    FbxDocumentInfo * mSceneInfo;
	FbxIOSettings * mIOSettings;

    FbxProgress * mProgress;
    bool mProgressPause;
    bool mCanceled;

    //
    // Head information class for fbx 7
    //
    class Fbx7FileHeaderInfo : public FbxIOFileHeaderInfo
    {
    public:
        FbxAutoDestroyPtr<FbxDocumentInfo>   mDocumentInfo;
        FbxReaderFbx7_Impl*                     mImpl;

        Fbx7FileHeaderInfo(FbxReaderFbx7_Impl* pImpl) : FbxIOFileHeaderInfo(),
          mImpl(pImpl) {}

        virtual void Reset()
        {
            FbxIOFileHeaderInfo::Reset();
            mDocumentInfo.Reset();
        }

        // Derived classes can get funky and read more info out of the header.
        // Return false in case of failure that should stop loading the file.
        virtual bool ReadExtendedHeaderInformation(FbxIO* pFileObject)
        {
            FBX_ASSERT( pFileObject );
            FBX_ASSERT( mImpl->mFileObject == pFileObject );
            FBX_UNUSED(pFileObject);

            mDocumentInfo.Reset( mImpl->ReadDocumentInfo() );

            return true;
        }
    };

    FbxReaderFbx7_Impl(FbxManager& pManager, FbxImporter& pImporter, FbxReader* pReader, FbxStatus& pStatus) :
        mFileObject(NULL),
        mManager(pManager),
        mImporter(pImporter),
        mReader(pReader),
        mStatus(pStatus),
        mDefaultCameraResolutionIsOK(false),
        mParseGlobalSettings(false),
        mRetrieveStats(true),
        mDefinitionsStatistics(NULL),
        mSceneImport(NULL),
        mSceneInfo(NULL),
        mProgress(NULL),
        mProgressPause(true),
        mCanceled(false)
    {
        mNodeArrayName.SetCaseSensitive(true);
        mQuickAccessGlobalSettings = FbxGlobalSettings::Create(&mManager,"TempGlobalSettings");
        FBX_ASSERT(mQuickAccessGlobalSettings);
    }

    //
    // Destructor
    //
    ~FbxReaderFbx7_Impl()
    {
        FbxDelete(mDefinitionsStatistics);
        mQuickAccessGlobalSettings->Destroy();

		FBX_SAFE_DESTROY(mSceneInfo);

		FbxArrayDelete(mTakeInfo);
    }

    //
    // Get object from id
    //
    FbxObject* GetObjectFromId(FbxLongLong pId) const
    {
        if( pId == kRootNodeObjectId && mSceneImport )
        {
            return mSceneImport->GetRootNode();
        }

        FbxUniqueIdObjectMap::ConstIterator Iter = mIdObjectMap.Find(pId);

        return Iter == mIdObjectMap.End() ? NULL : Iter->GetValue();
    }

    //
    // Add object and corresponding id to maps
    //
    void AddObjectId(FbxLongLong pId, FbxObject* pObject)
    {
        mIdObjectMap.Insert(pId, pObject);
        mObjectIdMap.Insert(pObject, pId);
    }

    //
    // Add object id and connect the object with document
    //
    void AddObjectIdAndConnect(FbxLongLong pId, FbxObject* pObject, FbxDocument* pDoc)
    {
        AddObjectId(pId, pObject);
        pDoc->ConnectSrcObject(pObject);
    }

    //
    // Remove object by id
    //
    void RemoveObjectId(FbxLongLong pId)
    {
        FbxUniqueIdObjectMap::Iterator Iter = mIdObjectMap.Find(pId);

        if( Iter != mIdObjectMap.End() )
        {
            mObjectIdMap.Remove(Iter->GetValue());
            mIdObjectMap.Remove(pId);
        }
    }

    //
    // Remove object by object pointer
    //
    void RemoveObjectId(FbxObject* pObject)
    {
        FbxObjectUniqueIdMap::Iterator Iter = mObjectIdMap.Find(pObject);

        if( Iter != mObjectIdMap.End() )
        {
            mIdObjectMap.Remove(Iter->GetValue());
            mObjectIdMap.Remove(pObject);
        }
    }

    // Scene info/Meta data
    FbxDocumentInfo* ReadDocumentInfo();
    FbxDocumentInfo* ReadDocumentInfo(FbxString& pType);

	// IOSettings
	FbxIOSettings * GetIOSettings(){ return mIOSettings; }
	void SetIOSettings(FbxIOSettings * pIOSettings){ mIOSettings = pIOSettings; }

    // FBX File sections
    bool ReadDescriptionSection(FbxDocument *pDocument);
    bool ReadReferenceSection(FbxDocument *pDocument, KTypeReadReferences& pDocReferences);
    bool ReadDefinitionSection(FbxDocument *pDocument, FbxObjectTypeInfoList& pObjbectContent );
    bool ReadObjectSection(FbxDocument *pDocument, FbxObjectTypeInfoList& pObjectContent, KTypeReadReferences& pDocReferences );
    bool ReadObject(FbxDocument *pDocument, FbxString& pObjectType, FbxString& pObjectSubType, FbxString& pObjectName, FbxLongLong pObjectUniqueId, FbxObject* pReferencedObject, KTypeReadReferences& pDocReferences);
    bool ReadConnectionSection(FbxDocument *pDocument);
    bool ReadEmbeddedFiles(FbxDocument* pDocument);

    // Animation
    bool ReadDocumentAnimation(FbxDocument *pDocument);

    // Animation objects
    bool ReadAnimStack(FbxAnimStack& pAnimStack);
    bool ReadAnimLayer(FbxAnimLayer& pAnimLayer);
    bool ReadCurveNode(FbxAnimCurveNode& pAnimCurveNode);
    bool ReadCurve(FbxAnimCurve& pAnimCurve);
	bool ReadAudioLayer(FbxAudioLayer& pAudioLayer);
	bool ReadAudio(FbxAudio& pAudio);

    bool ReadReference(FbxSceneReference& pReference);

    FbxThumbnail* ReadThumbnail();
    bool TimeShiftNodeAnimation(FbxScene& pScene, FbxTakeInfo* pTakeInfo);

    // Camera switcher
    void ReadCameraSwitcher(FbxScene& pScene);
    bool ReadCameraSwitcher( FbxCameraSwitcher& pCameraSwitcher );
    void ReorderCameraSwitcherIndices(FbxScene& pScene);

    // Character
    void ReadCharacter(FbxCharacter& pCharacter,int& pInputType, int& pInputIndex);
    void ReadCharacterLinkGroup(FbxCharacter& pCharacter, int pCharacterGroupId);
    void ReadCharacterLink(FbxCharacter& pCharacter, int pCharacterNodeId);
    void ReadCharacterLinkRotationSpace(FbxCharacterLink& pCharacterLink);
    bool ReadCharacterPose(FbxCharacterPose& pCharacterPose);
	void ReadCharacterPoseNodeProperty(FbxProperty& pProperty, int pInstance);

    // Misc
    bool ReadPose(FbxScene& pScene, FbxPose* pPose, bool pAsBindPose);
    bool ReadMedia(FbxDocument *pDocument, const char* pEmbeddedMediaDirectory = "");
    bool ReadGlobalSettings(FbxGlobalSettings& pGlobalSettings);

    // Objects
    bool ReadNode                       ( FbxNode& pNode, FbxString& pObjectSubType, KTypeReadReferences& pDocReferences );
    bool ReadGenericNode                ( FbxGenericNode& pNode );
    bool ReadNodeShading                ( FbxNode& pNode );
    bool ReadNodeCullingType            ( FbxNode& pNode ); // TBV, probablement passe tout en property
    bool ReadNodeTarget                 ( FbxNode& pNode );
    FbxNodeAttribute* ReadNodeAttribute( FbxString& pObjectSubType, FbxString& pObjectName, FbxLongLong pObjectUniqueId, FbxObject* pReferencedObject);

    // Objects with geometry
    bool ReadGeometryLinks              ( FbxGeometry& pGeometry );
    bool ReadGeometryShapes             ( FbxGeometry& pGeometry );
    bool ReadNull                       ( FbxNull& pNull );
    bool ReadMarker                     ( FbxMarker& pMarker );
    bool ReadCameraStereo               ( FbxCameraStereo& pCameraStereo );
    bool ReadCameraStereoPrecomp        ( FbxCameraStereo& pCameraStereo );
    bool ReadCamera                     ( FbxCamera& pCamera );
    bool ReadLight                      ( FbxLight& pLight );
    bool ReadBindingTable               ( FbxBindingTable& pTable );
    bool ReadBindingOperator            ( FbxBindingOperator& pOperator );

    bool ReadMesh                       ( FbxMesh& pMesh );
    bool ReadMeshSmoothness             ( FbxMesh& pMesh );
    bool ReadMeshVertices               ( FbxMesh& pMesh );
    bool ReadMeshPolygonIndex           ( FbxMesh& pMesh );
    bool ReadMeshEdges                  ( FbxMesh& pMesh );
#ifdef FBXSDK_SUPPORT_INTERNALEDGES
    bool ReadMeshInternalEdges          ( FbxMesh& pMesh );
#endif
    bool ReadDocument                   ( FbxDocument& pSubDocument );
    bool ReadCollection                 ( FbxCollection& pCollection );
    bool ReadSelectionNode              ( FbxSelectionNode& pSelectionNode );
    bool ReadSelectionSet               ( FbxSelectionSet& pSelectionSet );

    bool ReadNurb                       ( FbxNurbs& pNurbs );
    bool ReadNurbsSurface               ( FbxNurbsSurface& pNurbs );
    bool ReadPatch                      ( FbxPatch& pPatch );
    int  ReadPatchType                  ( FbxPatch& pPatch );
    bool ReadNurbsCurve                 ( FbxNurbsCurve& pNurbsCurve );
    bool ReadTrimNurbsSurface           ( FbxTrimNurbsSurface& pNurbs );
    bool ReadBoundary                   ( FbxBoundary& pBoundary );
    bool ReadLine                       ( FbxLine& pLine );
    bool ReadImplementation             ( FbxImplementation& pImplementation );

	// Textures, Videos, Materials, Thumbnails
    bool ReadFileTexture                ( FbxFileTexture& pTexture);
    bool ReadLayeredTexture             ( FbxLayeredTexture& pTex );
	bool ReadProceduralTexture          ( FbxProceduralTexture& pTex );
    FbxSurfaceMaterial* ReadSurfaceMaterial(const char* pName, const char* pMaterialType, FbxSurfaceMaterial* pReferencedMaterial);
    bool ReadVideo                      (FbxVideo& pVideo);
    bool ReadThumbnail                  (FbxThumbnail& pThumbnail);

    bool ReadContainer                  (FbxContainer& pContainer);

    // Layer elements
    bool ReadLayerElements              (FbxGeometry& pGeometry);
    bool ReadLayerElementsMaterial      (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsMaterial);
    bool ReadLayerElementsNormal        (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsNormal);
    bool ReadLayerElementsTangent       (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsTangent);
    bool ReadLayerElementsBinormal      (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsBinormal);
    bool ReadLayerElementsVertexColor   (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsVertexColor);
    bool ReadLayerElementsChannelUV     (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsUV, FbxLayerElement::EType pTextureType);
    bool ReadLayerElementsPolygonGroup  (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsPolygonGroup);
    bool ReadLayerElementsSmoothing     (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsSmoothing);
    bool ReadLayerElementsUserData      (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsUserData);
    bool ReadLayerElementsVisibility    (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsVisibility);
    bool ReadLayerElementEdgeCrease     (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsEdgeCrease);
    bool ReadLayerElementVertexCrease   (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsVertexCrease);
    bool ReadLayerElementHole           (FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsHole);

    // Geometry weighted maps
    bool ReadGeometryWeightedMap(FbxGeometryWeightedMap& pGeometryWeightedMap);

    // Deformers / Constraints
    bool ReadLink                   (FbxCluster& pLink);
    bool ReadSkin                   (FbxSkin& pSkin);
    bool ReadVertexCacheDeformer    (FbxVertexCacheDeformer& pDeformer);
    bool ReadCluster                (FbxCluster& pCluster);
    bool ReadConstraint             (FbxConstraint& pPosition);
	bool ReadBlendShape             (FbxBlendShape& pBlendShape);
	bool ReadBlendShapeChannel      (FbxBlendShapeChannel& pBlendShapeChannel);
	bool ReadShapeOld               (FbxShape& pShape, FbxGeometry &pGeometry);
	bool ReadShape                  (FbxShape& pShape);

    // Cache
    bool ReadCache(FbxCache& pCache);

    // Post-processing / utility functions
    FbxString ConvertCameraName(FbxString pCameraName);
    int  FindString(FbxString pString, FbxArray<FbxString*>& pStringArray);
    bool ReadPassword(FbxString pPassword);
    void PublishProperties(FbxObject& pObject);
    bool ReadProperties(FbxObject *pFbxObject);
    bool ReadPropertiesAndFlags(FbxObject *pObject);
    bool ReadFlags(FbxObject *pFbxObject);

    void RebuildTrimRegions(FbxScene& pScene) const;
	void RebuildTargetShapes(FbxScene& pScene) const;
    void RebuildLayeredTextureAlphas(FbxScene& pScene) const;
	
    //---------------- in progress -------------------------------
    void ReadOptionsInMainSection(); // !!!! ne marche pas dans 6 !!! A RECODER
    void ReadTakeOptions(); // !!! semble ok pour extensionsection, mais ne marche pas pour le main section !!!
    //--------------- end in progress ----------------------------

    void ReadGlobalSettings();
    void ReadGlobalSettings(FbxGlobalSettings& gs, bool pOpenMainSection);
    void ReadDefinitionSectionForStats();

    FbxDocument* CreateChildDocument(const FbxString& pType, const FbxString& pName, bool pForceScene);
    void RegisterRootNodeId(FbxDocument* pDocument, FbxLongLong pRootNodeId);

private:
    bool ReadBinaryData(FbxBinaryTarget& pDest);

    void OrderTypeInfoSection(FbxObjectTypeInfoList& pObjectContent);
    void ReadAndAdjustEmbeddedConsumers(const FbxString& pFileName);

    template <typename T>
    int ReadValueArray(FbxLayerElementArrayTemplate<T>& pArray);

    template <typename T>
    int ReadValueArray(const char* pFieldName, FbxLayerElementArrayTemplate<T>& pArray)
    {
        int lCount = 0;
        if( mFileObject->FieldReadBegin(pFieldName) )
        {
            lCount = ReadValueArray(pArray);
            mFileObject->FieldReadEnd();
        }

        return lCount;
    }

    template <typename T>
    void ReadValueArray(int pExpectedCount, T* pDest)
    {
        if( pExpectedCount > 0 )
        {
            // 
            // fbx6 would read 'expected' points -- if the points are not in the
            // the file, 0 would be returned.
            // 
            // so zero out entries that that could not be read, for some reason.
            // 

            int lValueCount;
            const T* lValues = mFileObject->FieldReadArray(lValueCount, (T*) 0);

            int lLeftCount = pExpectedCount; 
    
            if( lValueCount > 0 && lValues )
            {
                int lCopyCount = FbxMin(pExpectedCount, lValueCount);
                memcpy(pDest, lValues, lCopyCount * sizeof(T));

                lLeftCount -= lCopyCount;
            }

            if( lLeftCount )
            {
                memset(pDest + (pExpectedCount - lLeftCount), 0, lLeftCount * sizeof(T));
            }
        }
    }

    template <typename T>
    void ReadValueArray(const char* pFieldName, int pExpectedCount, T* pDest, const T* pDefault = NULL)
    {
        if( pExpectedCount > 0 )
        {
            if( mFileObject->FieldReadBegin(pFieldName) )
            {
                ReadValueArray(pExpectedCount, pDest);
                mFileObject->FieldReadEnd();
            }
            else if( pDefault )
            {
                memcpy(pDest, pDefault, pExpectedCount * sizeof(T));
            }
        }
    }

    bool IsInternalDocument(FbxDocument* pDocument, FbxEventReferencedDocument& pEvent, KTypeReadReferences& pDocReferences);
    void FieldReadDocumentPath(FbxString& pRefDocRootName, FbxString& pRefDocPathName, FbxString& pLastDocName);

};

// *******************************************************************************************************
//  Constructor and File management
// *******************************************************************************************************

//
// Constructor
//
FbxReaderFbx7::FbxReaderFbx7(FbxManager& pManager, FbxImporter& pImporter, int pID, FbxStatus& pStatus) : 
    FbxReader(pManager, pID, pStatus),
    mImpl(NULL)
{
#if defined(__GNUC__) && (__GNUC__ < 4)
    mImpl = FbxNew< FbxReaderFbx7_Impl >(pManager, pImporter, (FbxReaderFbx7*)this, pStatus);
#else
	mImpl = FbxNew< FbxReaderFbx7_Impl >(pManager, pImporter, this, pStatus);
#endif    
	if(mImpl) mImpl->SetIOSettings(pImporter.GetIOSettings());

	SetIOSettings(pImporter.GetIOSettings());
}

//
// Destructor
//   
FbxReaderFbx7::~FbxReaderFbx7()
{
    if (mImpl->mFileObject)
    {
        FileClose();
    }

    FbxDelete(mImpl);
}


bool FbxReaderFbx7::FileOpen(FbxFile * pFile)
{
    bool lCheckCRC = false;
    bool lParse = false;

    mImpl->mDefaultCameraResolutionIsOK = false;

	//Try opening file using large file support
    if( !mImpl->mFileObject )
    {
        mImpl->mFileObject = FbxIO::Create(FbxIO::BinaryLarge, GetStatus());
        mImpl->mFileObject->CacheSize(IOS_REF.GetIntProp(IMP_CACHE_SIZE, 8));
		mImpl->mFileObject->Fbx7Support(true);
    }

    FbxReaderFbx7_Impl::Fbx7FileHeaderInfo lFileHeaderInfo(mImpl);
	if( !mImpl->mFileObject->ProjectOpen(pFile, this, lCheckCRC, lParse, &lFileHeaderInfo) )
    {
		//Large file support failed to open, try normal file
        FileClose();
        mImpl->mFileObject = FbxIO::Create(FbxIO::BinaryNormal, GetStatus());
        mImpl->mFileObject->CacheSize(IOS_REF.GetIntProp(IMP_CACHE_SIZE, 8));
		mImpl->mFileObject->Fbx7Support(true);
		if( !mImpl->mFileObject->ProjectOpen(pFile, this, lCheckCRC, lParse, &lFileHeaderInfo) )
		{
			return false;
		}
    }

    mImpl->mDocumentInfoFromHeader.Swap(lFileHeaderInfo.mDocumentInfo);

    // Get the default render resolution from the header
    if (lFileHeaderInfo.mDefaultRenderResolution.mResolutionW && lFileHeaderInfo.mDefaultRenderResolution.mResolutionH &&
        lFileHeaderInfo.mDefaultRenderResolution.mResolutionMode.GetLen())
    {
        mImpl->mDefaultCameraResolutionIsOK = true;

        mImpl->mDefaultCameraName     = lFileHeaderInfo.mDefaultRenderResolution.mCameraName;
        mImpl->mDefaultResolutionMode = lFileHeaderInfo.mDefaultRenderResolution.mResolutionMode;
        mImpl->mDefaultResolutionW    = lFileHeaderInfo.mDefaultRenderResolution.mResolutionW;
        mImpl->mDefaultResolutionH    = lFileHeaderInfo.mDefaultRenderResolution.mResolutionH;
    }

    // Get the Axis Information
    if (mImpl->mParseGlobalSettings)
        mImpl->ReadGlobalSettings();

    // Get the Definition Section
    if (mImpl->mRetrieveStats)
        mImpl->ReadDefinitionSectionForStats();

    if (mImpl->mImporter.GetFileHeaderInfo())
    {
        mImpl->mImporter.GetFileHeaderInfo()->mDefaultRenderResolution = lFileHeaderInfo.mDefaultRenderResolution;
        mImpl->mImporter.GetFileHeaderInfo()->mCreationTimeStampPresent = lFileHeaderInfo.mCreationTimeStampPresent;
        mImpl->mImporter.GetFileHeaderInfo()->mCreationTimeStamp = lFileHeaderInfo.mCreationTimeStamp;
        mImpl->mImporter.GetFileHeaderInfo()->mCreator = lFileHeaderInfo.mCreator;
        mImpl->mImporter.GetFileHeaderInfo()->mFileVersion = lFileHeaderInfo.mFileVersion;
    }
    return true;
}


bool FbxReaderFbx7::FileOpen(FbxStream * pStream, void* pStreamData)
{
    bool lCheckCRC = false;
    bool lParse = false;

    mImpl->mDefaultCameraResolutionIsOK = false;

	//Try opening file using large file support
    if( !mImpl->mFileObject )
    {
        mImpl->mFileObject = FbxIO::Create(FbxIO::BinaryLarge, GetStatus());
        mImpl->mFileObject->CacheSize(IOS_REF.GetIntProp(IMP_CACHE_SIZE, 8));
		mImpl->mFileObject->Fbx7Support(true);
    }

    FbxReaderFbx7_Impl::Fbx7FileHeaderInfo lFileHeaderInfo(mImpl);
	if( !mImpl->mFileObject->ProjectOpen(pStream, pStreamData, this, lCheckCRC, lParse, &lFileHeaderInfo) )
    {
		//Large file support failed to open, try normal file
        FileClose();
        mImpl->mFileObject = FbxIO::Create(FbxIO::BinaryNormal, GetStatus());
        mImpl->mFileObject->CacheSize(IOS_REF.GetIntProp(IMP_CACHE_SIZE, 8));
		mImpl->mFileObject->Fbx7Support(true);
		if( !mImpl->mFileObject->ProjectOpen(pStream, pStreamData, this, lCheckCRC, lParse, &lFileHeaderInfo) )
		{
			return false;
		}
    }

    mImpl->mDocumentInfoFromHeader.Swap(lFileHeaderInfo.mDocumentInfo);

    // Get the default render resolution from the header
    if (lFileHeaderInfo.mDefaultRenderResolution.mResolutionW && lFileHeaderInfo.mDefaultRenderResolution.mResolutionH &&
        lFileHeaderInfo.mDefaultRenderResolution.mResolutionMode.GetLen())
    {
        mImpl->mDefaultCameraResolutionIsOK = true;

        mImpl->mDefaultCameraName     = lFileHeaderInfo.mDefaultRenderResolution.mCameraName;
        mImpl->mDefaultResolutionMode = lFileHeaderInfo.mDefaultRenderResolution.mResolutionMode;
        mImpl->mDefaultResolutionW    = lFileHeaderInfo.mDefaultRenderResolution.mResolutionW;
        mImpl->mDefaultResolutionH    = lFileHeaderInfo.mDefaultRenderResolution.mResolutionH;
    }

    // Get the Axis Information
    if (mImpl->mParseGlobalSettings)
        mImpl->ReadGlobalSettings();

    // Get the Definition Section
    if (mImpl->mRetrieveStats)
        mImpl->ReadDefinitionSectionForStats();

    if (mImpl->mImporter.GetFileHeaderInfo())
    {
        mImpl->mImporter.GetFileHeaderInfo()->mDefaultRenderResolution = lFileHeaderInfo.mDefaultRenderResolution;
        mImpl->mImporter.GetFileHeaderInfo()->mCreationTimeStampPresent = lFileHeaderInfo.mCreationTimeStampPresent;
        mImpl->mImporter.GetFileHeaderInfo()->mCreationTimeStamp = lFileHeaderInfo.mCreationTimeStamp;
        mImpl->mImporter.GetFileHeaderInfo()->mCreator = lFileHeaderInfo.mCreator;
        mImpl->mImporter.GetFileHeaderInfo()->mFileVersion = lFileHeaderInfo.mFileVersion;
    }
    return true;
}


//
// Open file with flags
//
bool FbxReaderFbx7::FileOpen(char* pFileName, EFileOpenSpecialFlags pFlags)
{
    mImpl->mParseGlobalSettings = (pFlags & eParseForGlobalSettings);
    mImpl->mRetrieveStats = (pFlags & eParseForStatistics) != 0;
    return FileOpen(pFileName);
}

//
// Open file
//
bool FbxReaderFbx7::FileOpen(char* pFileName)
{
    bool lCheckCRC = false;
    bool lParse = false;

    mImpl->mDefaultCameraResolutionIsOK = false;

	//Try opening file using large file support
    if( !mImpl->mFileObject )
    {
        mImpl->mFileObject = FbxIO::Create(FbxIO::BinaryLarge, GetStatus());
        mImpl->mFileObject->CacheSize(IOS_REF.GetIntProp(IMP_CACHE_SIZE, 8));
		mImpl->mFileObject->Fbx7Support(true);
    }

    FbxString lFullName = FbxPathUtils::Bind(FbxGetCurrentWorkPath(), pFileName);

    FbxReaderFbx7_Impl::Fbx7FileHeaderInfo lFileHeaderInfo(mImpl);
	if( !mImpl->mFileObject->ProjectOpen(lFullName, this, lCheckCRC, lParse, &lFileHeaderInfo) )
    {
		//Large file support failed to open, try normal file
        FileClose();
        mImpl->mFileObject = FbxIO::Create(FbxIO::BinaryNormal, GetStatus());
        mImpl->mFileObject->CacheSize(IOS_REF.GetIntProp(IMP_CACHE_SIZE, 8));
		mImpl->mFileObject->Fbx7Support(true);
		if( !mImpl->mFileObject->ProjectOpen(lFullName, this, lCheckCRC, lParse, &lFileHeaderInfo) )
		{
			return false;
		}
    }

    mImpl->mDocumentInfoFromHeader.Swap(lFileHeaderInfo.mDocumentInfo);

    // Get the default render resolution from the header
    if (lFileHeaderInfo.mDefaultRenderResolution.mResolutionW && lFileHeaderInfo.mDefaultRenderResolution.mResolutionH &&
        lFileHeaderInfo.mDefaultRenderResolution.mResolutionMode.GetLen())
    {
        mImpl->mDefaultCameraResolutionIsOK = true;

        mImpl->mDefaultCameraName     = lFileHeaderInfo.mDefaultRenderResolution.mCameraName;
        mImpl->mDefaultResolutionMode = lFileHeaderInfo.mDefaultRenderResolution.mResolutionMode;
        mImpl->mDefaultResolutionW    = lFileHeaderInfo.mDefaultRenderResolution.mResolutionW;
        mImpl->mDefaultResolutionH    = lFileHeaderInfo.mDefaultRenderResolution.mResolutionH;
    }

    // Get the Axis Information
    if (mImpl->mParseGlobalSettings)
        mImpl->ReadGlobalSettings();

    // Get the Definition Section
    if (mImpl->mRetrieveStats)
        mImpl->ReadDefinitionSectionForStats();

    if (mImpl->mImporter.GetFileHeaderInfo())
    {
        mImpl->mImporter.GetFileHeaderInfo()->mDefaultRenderResolution = lFileHeaderInfo.mDefaultRenderResolution;
        mImpl->mImporter.GetFileHeaderInfo()->mCreationTimeStampPresent = lFileHeaderInfo.mCreationTimeStampPresent;
        mImpl->mImporter.GetFileHeaderInfo()->mCreationTimeStamp = lFileHeaderInfo.mCreationTimeStamp;
        mImpl->mImporter.GetFileHeaderInfo()->mCreator = lFileHeaderInfo.mCreator;
        mImpl->mImporter.GetFileHeaderInfo()->mFileVersion = lFileHeaderInfo.mFileVersion;
    }

    return true;
}

//
// Close file
//
bool FbxReaderFbx7::FileClose()
{
    if (!mImpl->mFileObject)
    {
        GetStatus().SetCode(FbxStatus::eFailure, "File not opened");
        return false;
    }

    if (!mImpl->mFileObject->ProjectClose())
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

//
// Whether the file is ready to read
//
bool FbxReaderFbx7::IsFileOpen()
{
    return mImpl->mFileObject != NULL;
}

//
// Get current import mode
//
FbxReaderFbx7::EImportMode FbxReaderFbx7::GetImportMode()
{
    FBX_ASSERT(mImpl->mFileObject);

    if (mImpl->mFileObject->IsEncrypted())
    {
        return eENCRYPTED;
    }
    else if (mImpl->mFileObject->IsBinary())
    {
        return eBINARY;
    }
    else
    {
        return eASCII;
    }
}

//
// Get file version
//
void FbxReaderFbx7::GetVersion(int& pMajor, int& pMinor, int& pRevision)
{
    FBX_ASSERT(mImpl->mFileObject);

    FbxIO::ProjectConvertVersionNumber(mImpl->mFileObject->ProjectGetSectionVersion(FBX_MAIN_SECTION),
                                      pMajor,
                                      pMinor,
                                      pRevision);
}

//
// Get axis system information
//
bool FbxReaderFbx7::GetAxisInfo(FbxAxisSystem* pAxisSystem, FbxSystemUnit* pSystemUnits)
{
    if (!pAxisSystem || !pSystemUnits)
        return false;

    *pAxisSystem  = mImpl->mQuickAccessGlobalSettings->GetAxisSystem();
    *pSystemUnits = mImpl->mQuickAccessGlobalSettings->GetSystemUnit();
    return true;
}

//
// Get frame rate
//

bool FbxReaderFbx7::GetFrameRate(FbxTime::EMode &pTimeMode)
{
	pTimeMode = mImpl->mQuickAccessGlobalSettings->GetTimeMode(); 

	if(pTimeMode == FbxTime::eDefaultMode) 
	{
		return false;
	}

	return true;
}


//
// Get statistics
//
bool FbxReaderFbx7::GetStatistics(FbxStatistics* pStats)
{
    if (!pStats)
        return false;

    if (!mImpl->mDefinitionsStatistics)
    {
        pStats->Reset();
        return false;
    }

    *pStats = *mImpl->mDefinitionsStatistics;
    return true;
}

//
// Get read options
//
bool FbxReaderFbx7::GetReadOptions(bool pParseFileAsNeeded)
{
    return GetReadOptions(NULL, pParseFileAsNeeded);
}

//
// Get read options from fbx file object
//
bool FbxReaderFbx7::GetReadOptions(FbxIO* pFbx, bool pParseFileAsNeeded)
{
	FbxIO*	lInternalFbx = NULL;
	bool	lResult = true;

    if( pFbx )
    {
        lInternalFbx = mImpl->mFileObject;
        mImpl->mFileObject = pFbx;
    }
    else if( mImpl->mFileObject )
    {
    }
    else
    {
        GetStatus().SetCode(FbxStatus::eFailure, "File not opened");
        lResult = false;
    }

	if( lResult && mImpl->mDocumentInfoFromHeader.Get() )
	{
		FBX_SAFE_DESTROY(mImpl->mSceneInfo);

		FbxDocumentInfo* lSceneInfo = mImpl->mDocumentInfoFromHeader.Get();
		if( lSceneInfo )
		{
			FbxThumbnail* tn = lSceneInfo->GetSceneThumbnail();
			mImpl->mSceneInfo = FbxDocumentInfo::Create(&mImpl->mManager, "");
			mImpl->mSceneInfo->Copy(*lSceneInfo);
			mImpl->mSceneInfo->SetSceneThumbnail(tn);
	    }
	}

	if( lResult )
	{
		if( mImpl->mFileObject->ProjectGetCurrentSection() == FBX_NO_SECTION )
		{
			if( pParseFileAsNeeded )
			{
				lResult = mImpl->mFileObject->ProjectOpenMainSection();
				if( lResult )
				{
					mImpl->ReadOptionsInMainSection();
					mImpl->mFileObject->ProjectCloseSection();
				}
			}
			else
			{
				lResult = false;
			}
		}
		else
		{
			if( pParseFileAsNeeded )
			{
				mImpl->ReadOptionsInMainSection();
			}
			else
			{
				lResult = false;
			}
		}
	}

	if( pFbx )
	{
		mImpl->mFileObject = lInternalFbx;
	}
	return lResult;
}

FbxDocumentInfo*  FbxReaderFbx7::GetSceneInfo()
{
    return mImpl->mSceneInfo;
}

FbxArray<FbxTakeInfo*>* FbxReaderFbx7::GetTakeInfo()
{
    return &mImpl->mTakeInfo;
}

// ************************************************************************************************
// Utility functions
// ************************************************************************************************

//
// Find string from string array
//
int FbxReaderFbx7_Impl::FindString(FbxString pString, FbxArray<FbxString*>& pStringArray)
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
bool FbxReaderFbx7_Impl::ReadPassword(FbxString pPassword)
{
    if (mFileObject->IsPasswordProtected())
    {
        if (!mFileObject->CheckPassword(pPassword))
        {
            return false;
        }
    }

    return true;
}

// *******************************************************************************************************
//  Constructor and reader
// *******************************************************************************************************

//
// Read current file data to document
//
bool FbxReaderFbx7::Read(FbxDocument* pDocument)
{
    if (!pDocument)
    {
        GetStatus().SetCode(FbxStatus::eFailure, "Invalid document handle");
        return false;
    }

	//Notify plugins before reading FBX file
	FbxScene* lScene = FbxCast<FbxScene>(pDocument);
	if( lScene )
	{
	#ifndef FBXSDK_ENV_WINSTORE
		PluginsReadBegin(*lScene);
	#endif
	}

	// note: sprintf() use the locale to find the decimal separator
	// French, Italian, German, ... use the comma as decimal separator
	// so we need a way to be un-localized into writing/reading our files formats

	// force usage of a period as decimal separator
	char lPrevious_Locale_LCNUMERIC[100]; memset(lPrevious_Locale_LCNUMERIC, 0, 100);
	FBXSDK_strcpy(lPrevious_Locale_LCNUMERIC, 100, setlocale(LC_NUMERIC, 0  ));	// query current setting for LC_NUMERIC
	char *lCurrent_Locale_LCNUMERIC  = setlocale(LC_NUMERIC, "C");  // set locale using period as decimal separator

    bool result = Read(pDocument, NULL);

	// set numeric locale back
	setlocale(LC_NUMERIC, lPrevious_Locale_LCNUMERIC);

	//Notify plugins after reading FBX file
	if( lScene )
	{
	#ifndef FBXSDK_ENV_WINSTORE
		PluginsReadEnd(*lScene);
	#endif
	}

	return result;
}

//
// Read data to document by file object
//
bool FbxReaderFbx7::Read(FbxDocument* pDocument, FbxIO* pFbx)
{
    if (!pDocument)
    {
        GetStatus().SetCode(FbxStatus::eFailure, "Invalid document handle");
        return false;
    }

    mImpl->mSceneImport = FbxCast<FbxScene>(pDocument);

    FbxIO*           lInternalFbx = NULL;
    bool            lResult = true;
    bool            lDontResetPosition = false;

#ifndef FBXSDK_ENV_WINSTORE
    // Notify the plugins we are about to do an import.
    FbxEventPreImport lPreEvent( pDocument );
    pDocument->GetFbxManager()->EmitPluginsEvent( lPreEvent );
#endif

    mImpl->mClassTemplateMap.Clear();
    mImpl->mIdObjectMap.Clear();

    if (lResult)
    {
        if (pFbx)
        {
            lInternalFbx = mImpl->mFileObject;
            mImpl->mFileObject = pFbx;

            lDontResetPosition = true;
        }
        else if (mImpl->mFileObject)
        {
        }
        else
        {
            GetStatus().SetCode(FbxStatus::eFailure, "File not opened");
            lResult = false;
        }
    }

    FbxIO::FbxAutoResetXRefManager lSetXRefManager(mImpl->mFileObject, mImpl->mManager.GetXRefManager());

    if (lResult)
    {
        if (mImpl->mFileObject->ProjectGetCurrentSection() == FBX_NO_SECTION)
        {
            if (!mImpl->mFileObject->ProjectOpenMainSection())
			{
                GetStatus().SetCode(FbxStatus::eInvalidFile, "File is corrupted %s", mImpl->mFileObject->GetFilename());
                lResult = false;
			}
        }
        else
        {
            if( !lDontResetPosition ) mImpl->mFileObject->FieldReadResetPosition();
        }
    }

    if (lResult)
    {
        if( !lDontResetPosition )
        {
            if (!mImpl->ReadPassword( IOS_REF.GetStringProp(IMP_FBX_PASSWORD, FbxString("")) ))
            {
                GetStatus().SetCode(FbxStatus::ePasswordError, "Wrong password");
                lResult = false;
            }
        }
    }

    if (mImpl->mSceneImport)
    {
        mImpl->mSceneImport->Clear();
        mImpl->mNodeArrayName.Clear();
    }

    if( mImpl->mDocumentInfoFromHeader.Get() )
    {
        pDocument->SetDocumentInfo(mImpl->mDocumentInfoFromHeader.Release());
    }
    else
    {
        // If there wasn't anything in the header should we clear the doc info?
    }

    // Read the GlobalSettings.
    // For FBX v7.1 files this is where we would find them. For FBX v7.0, the global
    // settings are still inside the OBJECT PROPERTIES section and will be read from there.
    if (mImpl->mSceneImport && lResult)
        mImpl->ReadGlobalSettings(mImpl->mSceneImport->GetGlobalSettings(), false);

    //
    // Read document description section
    //
    if( lResult )
    {
        lResult = mImpl->ReadDescriptionSection(pDocument);
    }

    KTypeReadReferences lDocumentReferences;

    //
    // Read document external references section
    //
    if( lResult )
    {
        lResult = mImpl->ReadReferenceSection(pDocument, lDocumentReferences);

        if (lResult)
        {
            int i, lCount = mManager.GetDocumentCount();

            FbxLibrary* lLibrary = FbxCast<FbxLibrary>(pDocument);

            for (i = 0; i < lCount; i++)
            {
                FbxDocument* lExtDoc = mManager.GetDocument(i);

                // I'm not sure here exactly what the goal here is -- we don't want to
                // resolve if we have the same root document, but with libraries
                // everything is assumed to be hooked to the 'RootLibrary', so for libraries
                // I'll do the check differently...
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

    FbxObjectTypeInfoList lObjectDefinitionContent;

    if( lResult )
    {
        lResult = mImpl->ReadDefinitionSection(pDocument, lObjectDefinitionContent);
    }

    mImpl->mProgressPause = false;
    if (lResult)
    {
        if (mImpl->mSceneImport)
        {
            mImpl->ReadCameraSwitcher(*mImpl->mSceneImport);
        }

        // Always read the nodes.
        mImpl->mFileObject->FieldReadResetPosition();

        lResult = mImpl->ReadObjectSection(pDocument, lObjectDefinitionContent, lDocumentReferences);
    }
    mImpl->mProgressPause = true;

    if (lResult)
    {
        // Import the textures.
        // Must be called after ReadNode() to set texture file names.
        
        if (IOS_REF.GetBoolProp(IMP_FBX_TEXTURE, true))
        {
            //mImporter.ProgressUpdate(NULL, "Retrieving medias", "", 0);
            mImpl->ReadMedia(pDocument);
        }
    }

    if (lResult)
    {
        mImpl->ReadConnectionSection(pDocument);
    }

    if (lResult)
    {
        // Import the animation.
        if (IOS_REF.GetBoolProp(IMP_FBX_ANIMATION, true) && mImpl->mTakeInfo.GetCount() > 0)
        {
            lResult = mImpl->ReadDocumentAnimation(pDocument);

            // at this point, all the AnimCurveNodes are connected to their respective properties
			// so we can sync the internal data (KFCurveNodeLayerType)
			if (mImpl->mSceneImport && lResult)
			{
				int lCount = mImpl->mSceneImport->GetMemberCount<FbxAnimCurveNode>();
				for (int i = 0; i < lCount; i++)
				{
					FbxAnimCurveNode* lCurveNode = mImpl->mSceneImport->GetMember<FbxAnimCurveNode>(i);
					if(lCurveNode->GetDstPropertyCount() == 1)// there should be only one property connected
					{
						FbxProperty lProp = lCurveNode->GetDstProperty(0);
						if(lProp.IsValid())// must exist!
						{
							lCurveNode->SetKFCurveNodeLayerType(lProp);
						}
					}
				}
			}
        }
    }

    if( lResult )
    {
        lResult = mImpl->ReadEmbeddedFiles(pDocument);
    }

	if (lResult)
	{
		if (!IOS_REF.GetBoolProp(IMP_RELAXED_FBX_CHECK, false))
		{
			FbxSceneCheckUtility lSceneCheck(mImpl->mSceneImport, &mStatus);
			lResult = lSceneCheck.Validate();
		}
	}

	// Post-process command goes here
    if( mImpl->mSceneImport && lResult )
    {
		// make sure the that all the animstacks have at least one anim layer!!!
		int lCount = mImpl->mSceneImport->GetMemberCount<FbxAnimStack>();
		for (int i = 0; i < lCount; i++)
		{
			FbxAnimStack* lStack = mImpl->mSceneImport->GetMember<FbxAnimStack>(i);
			if (lStack->GetMemberCount<FbxAnimLayer>() == 0)
			{
				FbxAnimLayer* layer = FbxAnimLayer::Create(mImpl->mSceneImport, "Base Layer");
				lStack->AddMember(layer);
			}
		}

		{
			// Let's try to give the PRODUCER cameras to the globalCameraSettings object.
			// Actually we are going to copy the camera attributes and (optionally) its src connections.
			int i;
			FbxArray<int> cameraSwitcherId;
			FbxArray<int> lProducerCamerasId;

			// scan all the nodes and only process cameras
			for (i = 0; i < mImpl->mNodeArrayName.GetCount(); i++)
			{
				FbxNode* lCurrentNode = (FbxNode*) mImpl->mNodeArrayName[i];
				if( lCurrentNode->GetParent() != NULL)
				{
					continue;
				}
				FbxCamera* cam = lCurrentNode->GetCamera(); // ignore anything that is not a camera
				if (cam)
				{
					// the CopyProducerCameras can ignore the cam argument if the name is not one of the
					// producer cameras
					if (mImpl->mSceneImport->GlobalCameraSettings().CopyProducerCamera(lCurrentNode->GetNameWithoutNameSpacePrefix(), cam, IOS_REF.GetBoolProp(IMP_KEEP_PRODUCER_CAM_SRCOBJ, false)))
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
					FbxCameraSwitcher* lPreviousCam = mImpl->mSceneImport->GlobalCameraSettings().GetCameraSwitcher();
					if (lPreviousCam)
					{
						FbxNode* lPreviousNode = lPreviousCam->GetNode();
						if (lPreviousNode)
						{
							mImpl->RemoveObjectId(lPreviousNode);
						}
					}

					mImpl->mSceneImport->GlobalCameraSettings().SetCameraSwitcher(cams);
				}
			}

			// actually destroy the producer cameras
			for (i = lProducerCamerasId.GetCount()-1; i >= 0; i--)
			{
				FbxNode* lProdCam = (FbxNode*) mImpl->mNodeArrayName[ lProducerCamerasId.GetAt(i) ];
				mImpl->mNodeArrayName.RemoveFromIndex(lProducerCamerasId.GetAt(i));
				mImpl->RemoveObjectId(lProdCam);
				lProdCam->Destroy();
			}		
		}

        mImpl->mSceneImport->FixInheritType(mImpl->mSceneImport->GetRootNode());

        mImpl->ReorderCameraSwitcherIndices(*mImpl->mSceneImport);
        mImpl->RebuildTrimRegions(*mImpl->mSceneImport);
		if(IOS_REF.GetBoolProp(IMP_FBX_SHAPE, true))
		{
			mImpl->RebuildTargetShapes(*mImpl->mSceneImport);
		}
        mImpl->RebuildLayeredTextureAlphas(*mImpl->mSceneImport);

		//7.2 or less post process
		if( mImpl->mFileObject->GetFileVersionNumber() <= FBX_FILE_VERSION_7200 )
		{
		    //Normally, we would have put this code in ReadLight, but since the properties are read
		    //after ReadLight, they wouldn't be initialized with file value. So we have no choice
		    //to process this after the file is done reading properties.
			for( int i = 0, lCount = mImpl->mSceneImport->GetSrcObjectCount<FbxLight>(); i < lCount; i++ )
			{
				FbxLight* lLight = mImpl->mSceneImport->GetSrcObject<FbxLight>(i);
				if( lLight )
				{
					//In file version 7.3, HotSpot became InnerAngle
					FbxProperty lHotSpot = lLight->FindProperty("HotSpot");
					if( lHotSpot.IsValid() && !lLight->InnerAngle.Modified() ) lLight->InnerAngle.CopyValue(lHotSpot);

					//In file version 7.3, ConeAngle became OutAngle
					FbxProperty lConeAngle = lLight->FindProperty("Cone angle");
					if( lConeAngle.IsValid() && !lLight->OuterAngle.Modified() ) lLight->OuterAngle.CopyValue(lConeAngle);
				}
			}

			for (int i = 0, lCount = mImpl->mSceneImport->GetSrcObjectCount<FbxCharacter>(); i < lCount; i++)
			{
				// In the case we have a character definition prior to the new interpretation of the roll bones (v.460)
				// Let's make the required adjustments. Note that the character received its version from the FBX file
				FbxCharacter* lCharacter = mImpl->mSceneImport->GetSrcObject<FbxCharacter>(i);
				if (lCharacter)
					lCharacter->SetValuesFromLegacyLoad();
			}
		}

		//7.4 or less post process
		if( mImpl->mFileObject->GetFileVersionNumber() <= FBX_FILE_VERSION_7400 )
		{
			//During FBX 2014.1 release, we changed FbxReference to be FbxObject* type rather than void*.
			//Some reference properties in constraints were connecting to the Dst of the property rather than the Src.
			//This was changed so that all reference properties are used the same way; via Src connections.
			//Because of this, for all files 2014 and lower, we need to make sure this is respected.
			//Gather specific reference properties that changed from Dst to Src connections:
			FbxArray<FbxProperty> lProperties;
			for( int i = 0, c = mImpl->mSceneImport->GetSrcObjectCount<FbxConstraint>(); i < c; ++i )
			{
				FbxConstraint* lConstraint = mImpl->mSceneImport->GetSrcObject<FbxConstraint>(i);
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
		}

		//Fix FbxVertexCacheDeformer types for older files
		for( int i = 0, c = mImpl->mSceneImport->GetSrcObjectCount<FbxVertexCacheDeformer>(); i < c; ++i )
		{
			FbxVertexCacheDeformer* lDeformer = mImpl->mSceneImport->GetSrcObject<FbxVertexCacheDeformer>(i);
			FbxCache* lCache = lDeformer->GetCache();
			if( !lCache->OpenFileForRead() ) continue;

			int lChannelIndex = lCache->GetChannelIndex(lDeformer->Channel.Get());
			FbxString lInterpretation = "";
			if( lCache->GetChannelInterpretation(lChannelIndex, lInterpretation) )
			{
				lInterpretation = lInterpretation.Lower();
				if( lInterpretation.Find("position") >= 0 && lDeformer->Type.Get() != FbxVertexCacheDeformer::ePositions )
				{
					lDeformer->Type = FbxVertexCacheDeformer::ePositions;
				}
				else if( (lInterpretation.Find("normal") >= 0 && lInterpretation.Find("binormal") < 0) && lDeformer->Type.Get() != FbxVertexCacheDeformer::eNormals )
				{
					lDeformer->Type = FbxVertexCacheDeformer::eNormals;
				}
				else if( lInterpretation.Find("uv") >= 0 && lDeformer->Type.Get() != FbxVertexCacheDeformer::eUVs )
				{
					lDeformer->Type = FbxVertexCacheDeformer::eUVs;
				}
				else if( lInterpretation.Find("tangent") >= 0 && lDeformer->Type.Get() != FbxVertexCacheDeformer::eTangents )
				{
					lDeformer->Type = FbxVertexCacheDeformer::eTangents;
				}
				else if( lInterpretation.Find("binormal") >= 0 && lDeformer->Type.Get() != FbxVertexCacheDeformer::eBinormals )
				{
					lDeformer->Type = FbxVertexCacheDeformer::eBinormals;
				}
			}
			lCache->CloseFile();
		}

		//Producer cameras exported by MB stored their position in Camera.Position property, so transfer it to the LclTranslation of the first node
		#define COPY_POSITION_TO_TRANSLATION(camera)\
			{FbxNode* node = camera ? camera->GetNode() : NULL;\
			if( node && node->LclTranslation.GetCurveNode() == NULL ) node->LclTranslation = camera->Position;}

		FbxGlobalCameraSettings& lGCS = mImpl->mSceneImport->GlobalCameraSettings();
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerPerspective());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerBack());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerBottom());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerFront());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerLeft());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerRight());
		COPY_POSITION_TO_TRANSLATION(lGCS.GetCameraProducerTop());
	}

    if (lResult)
    {
        // If we've created an fbm folder, let the client know about it.
        FbxString lDefaultPath = "";
        FbxString lUserDefinePath = IOS_REF.GetStringProp(IMP_EXTRACT_FOLDER, lDefaultPath);
        const char* lUserDefinePathBuffer = (lUserDefinePath == lDefaultPath) ? NULL : lUserDefinePath.Buffer();
        FbxString lMediaFolder = mImpl->mFileObject->GetMediaDirectory(false, lUserDefinePathBuffer);

        if( !lMediaFolder.IsEmpty() )
        {
            FbxDocumentInfo* lDocumentInfo = pDocument->GetDocumentInfo();

            if( !lDocumentInfo )
            {
                lDocumentInfo = FbxDocumentInfo::Create(&mImpl->mManager, "");
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
        mImpl->mFileObject = lInternalFbx;
    }

#ifndef FBXSDK_ENV_WINSTORE
    // once we are done post-processing the scene notify plugins.
    if( lResult )
    {
        FbxEventPostImport lEvent( pDocument );
        pDocument->GetFbxManager()->EmitPluginsEvent( lEvent );
    }
#endif

    mImpl->mSceneImport = NULL;

    if(!IOS_REF.GetBoolProp(IMP_RELAXED_FBX_CHECK, false))
    {
	    // if at some point the eInvalidFile error has been raised inside a block that still returned true,
	    // we should consider the file corrupted
	    if (lResult && mStatus.GetCode() == FbxStatus::eInvalidFile)
	  	    lResult = false;
    }
    return lResult;
}

void FbxReaderFbx7::SetProgressHandler(FbxProgress *pProgress)
{
    mImpl->mProgress = pProgress;
}

void FbxReaderFbx7::SetEmbeddingExtractionFolder(const char* pExtractFolder)
{
	mImpl->mFileObject->SetEmbeddingExtractionFolder(pExtractFolder);
}

void FbxReaderFbx7::PluginReadParameters(FbxObject& pParams)
{
    mImpl->ReadPropertiesAndFlags(&pParams);
}

// ****************************************************************************************
// Read Headers and sections
// ****************************************************************************************

//
// Read global settings
//
void FbxReaderFbx7_Impl::ReadGlobalSettings()
{
    ReadGlobalSettings(*mQuickAccessGlobalSettings, true);
}

void FbxReaderFbx7_Impl::ReadGlobalSettings(FbxGlobalSettings& gs, bool pOpenMainSection)
{
    bool ret = true;
    if (pOpenMainSection)
    {
        mFileObject->ProjectOpenMainSection();

		// for V7.0 files, the GlobalSettings are still in the Object Properties section
		int maj, min, rev;
		FbxIO::ProjectConvertVersionNumber(mFileObject->ProjectGetSectionVersion(FBX_MAIN_SECTION), maj, min, rev);    
		FBX_ASSERT(maj == 7 && min >= 0);
	       
		if (min == 0)
		{
			ret = mFileObject->FieldReadBegin(FIELD_OBJECT_PROPERTIES);
			if (ret)
				ret = mFileObject->FieldReadBlockBegin();
		}
	}

    if (ret)
    {
        if (mFileObject->FieldReadBegin(FIELD_GLOBAL_SETTINGS))
        {
            if (mFileObject->FieldReadBlockBegin())
            {
                ReadGlobalSettings(gs);
                mFileObject->FieldReadBlockEnd();
            }
            mFileObject->FieldReadEnd ();
        }
    }

    if (pOpenMainSection)
    {
        mFileObject->FieldReadBlockEnd();
        mFileObject->FieldReadEnd ();
    
        mFileObject->FieldReadResetPosition();
        mFileObject->ProjectCloseSection();
    }
}

//
// Read definition sections
//
void FbxReaderFbx7_Impl::ReadDefinitionSectionForStats()
{
    // will get deleted by the destructor
    if (!mDefinitionsStatistics)
        mDefinitionsStatistics = FbxNew< FbxStatisticsFbx >();

    if (mFileObject->ProjectOpenMainSection())
	{
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
	}
}

//
// Read options
//
void FbxReaderFbx7_Impl::ReadOptionsInMainSection()
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

    ReadTakeOptions();

    mFileObject->FieldReadResetPosition();
}

//
// Read take options
//
void FbxReaderFbx7_Impl::ReadTakeOptions()
{
    FbxString lString;

    FbxArrayDelete(mTakeInfo);
    
    IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString(""));

    //Retrieve fbx info for getting some takes infos out
    if (mFileObject->FieldReadBegin ("Takes"))
    {
        bool lCurrentTakeFound = false; // Used to validate if the CurrentTake name is valid

        if (mFileObject->FieldReadBlockBegin())
        {
            lString=mFileObject->FieldReadC ("Current");

            IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, lString);

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

                        if (lTakeFbxObject.ProjectOpenDirect(lFullFileName.Buffer(), mReader))
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

                if (IOS_REF.GetStringProp(IMP_FBX_CURRENT_TAKE_NAME, FbxString("")) == lNewInfo->mName)
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
                IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, mTakeInfo[0]->mName);
            }
            else
            {
                IOS_REF.SetStringProp(IMP_FBX_CURRENT_TAKE_NAME, "");
            }
        }
    }
}


// ****************************************************************************************
// Global
// ****************************************************************************************

//
// Read thumbnail
//
FbxThumbnail* FbxReaderFbx7_Impl::ReadThumbnail()
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

                        ReadValueArray(lSize, lImagePtr);

                        mFileObject->FieldReadEnd();
                    }

                    lImageRead = true;
                }
            }

            ReadPropertiesAndFlags( lThumbnail );

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
// Read document information by type
//
FbxDocumentInfo* FbxReaderFbx7_Impl::ReadDocumentInfo(FbxString& pType)
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
        ReadPropertiesAndFlags( lSceneInfo );
    }

    return lSceneInfo;
}

//
// Read document info
//
FbxDocumentInfo* FbxReaderFbx7_Impl::ReadDocumentInfo()
{
    FbxDocumentInfo* lSceneInfo = NULL;

    if (mFileObject->FieldReadBegin(FIELD_SCENEINFO))
    {
        // There should be only one block of this type.
        // So read only the first one
        if (mFileObject->FieldReadBlockBegin())
        {
            FbxString lType = mFileObject->FieldReadS(FIELD_SCENEINFO_TYPE);
            lSceneInfo = ReadDocumentInfo(lType);
            mFileObject->FieldReadBlockEnd();
        }
        mFileObject->FieldReadEnd();
    }

    return lSceneInfo;
}

//
// Create child document
//
FbxDocument* FbxReaderFbx7_Impl::CreateChildDocument(const FbxString& pType, const FbxString& pName, bool pForceScene)
{
    FbxClassId lClassId = CheckRuntimeClass(pType, "", mManager);

    if( !lClassId.IsValid() )
    {
		lClassId = pForceScene ? FbxScene::ClassId : FbxDocument::ClassId;
    }

    return FbxCast<FbxDocument>(mManager.CreateNewObjectFromClassId(lClassId, pName));
}

//
// Register root node id
//
void FbxReaderFbx7_Impl::RegisterRootNodeId(FbxDocument* pDocument, FbxLongLong pRootNodeId)
{
    FbxScene* lScene = FbxCast<FbxScene>(pDocument);

    if( lScene )
    {
        AddObjectId(pRootNodeId, lScene->GetRootNode());
    }
    else
    {
        // Someone asked to load a document that's flagged as a scene, but they gave
        // us a document.
        // 
        // All connections will thus fail...  Create a fake root node, and connect it
        // to the document.

        FbxNode* lNode = FbxNode::Create(pDocument, "RootNode");

        if( lNode )
        {
            AddObjectId(pRootNodeId, lNode);
        }
    }
}

//
// Read description section
//
bool FbxReaderFbx7_Impl::ReadDescriptionSection(FbxDocument *pDocument)
{
    bool lRet = true;

    if (mFileObject->FieldReadBegin("Documents"))
    {
        // First document is the 'main' document, and maps onto pDocument given in. 
        // The others are child documents (if the writer wrote a document hierarchy) 
        // and need to be created.

        if(mFileObject->FieldReadBlockBegin())
        {
            int lDocCount = mFileObject->FieldReadI(FIELD_OBJECT_DEFINITION_COUNT);

            if( lDocCount < 1 )
            {
                mStatus.SetCode(FbxStatus::eInvalidFile, "File is corrupted %s", mFileObject->GetFilename());
                lRet = false;
            }
            else
            {
                int lDocCounter = 0;

                // Don't enforce that the number of documents founds matches the
                // count above, although perhaps we should.

                while( lRet && mFileObject->FieldReadBegin(FIELD_OBJECT_DESCRIPTION) )
                {
                    ++lDocCounter;

                    FbxLongLong lObjectId = mFileObject->FieldReadLL();
                    FbxString   lName     = mFileObject->FieldReadC();
                    FbxString   lType     = mFileObject->FieldReadC();
   
                    if( mFileObject->FieldReadBlockBegin() )
                    {  
                        FbxLongLong lRootNodeId = mFileObject->FieldReadLL("RootNode", kInvalidObjectId);   // Only for scenes

                        // FIXME: If anything fails, do we need to reclaim the child docs?
                        // (ie: if we can't reach the 'read connections' section)
                        // 
                        FbxDocument* lCurrentDoc = (lDocCounter == 1) ? pDocument : CreateChildDocument(lType, lName, (lRootNodeId != kInvalidObjectId));

                        if( !lCurrentDoc )
                        {
                            lRet = false;
                        }
                        else
                        {
                            if( lCurrentDoc == pDocument )
                            {
                                pDocument->SetInitialName(lName);
                                pDocument->SetName(lName);
                            }

                            AddObjectId(lObjectId, lCurrentDoc);

                            if( lRootNodeId != kInvalidObjectId )
                            {
                                RegisterRootNodeId(lCurrentDoc, lRootNodeId);
                            }

                            lRet = ReadPropertiesAndFlags(lCurrentDoc);

                            if( lRet )
                            {
                                // The root document stores its scene info in the header,
                                // no here.  This should only return for sub-documents.
                                FbxDocumentInfo* lDocInfo = ReadDocumentInfo();

                                if( lDocInfo )
                                {
                                    FBX_ASSERT( lCurrentDoc != pDocument );
                                    lCurrentDoc->SetDocumentInfo(lDocInfo);
                                }
                            }
                        }

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
// whether is internal document
//
bool FbxReaderFbx7_Impl::IsInternalDocument(FbxDocument* pDocument, FbxEventReferencedDocument& pEvent, KTypeReadReferences& pDocReferences)
{
    FBX_ASSERT( pDocument );

    /*
        somefile.fbx
        copy somefile.fbx someotherfile.fbx
        open someotherfile.fbx

        the reference table in someotherfile.fbx will have an entry like this:

        FilePathUrl: "somefile.fbx" {           <---- old name
            ObjectType: "Scene"
            Document: "Revit Document" {
            }
        }

        Since the file is COPIED, not renamed, we'll end up loading both files,
        thinking 'somefile.fbx' is an external document we need.

        We lookup through the object references, see if any of the references are external.

        When this method is called, we are currently reading the FilePathUrl block
        and so can read values through mFileObject.

        Looking for 'externals' rather than 'internal' is potentially quicker, since
        as soon as we find one external we can stop; looking for internal we'd need to
        check all of them.

        The event contains the parent name of the document, and the document name
        itself.  These need to be combined into the full path name of the
        document so we can compare against what the doc references use.
    */

    FbxString lFullDocumentName = pEvent.mParentFullName;

    if( !lFullDocumentName.IsEmpty() )
    {
        lFullDocumentName += "::";
    }

    lFullDocumentName += pEvent.mDocumentName;

    return !pDocReferences.DocumentHasExternals(lFullDocumentName);
}

//
// Read field from document path
//
void FbxReaderFbx7_Impl::FieldReadDocumentPath(FbxString& pRefDocRootName, FbxString& pRefDocPathName, FbxString& pLastDocName)
{
    //
    // Original, fbx6 format, up to the first few weeks of fbx7:
    // 
    //     Reference: "Reference_1", "Internal" {
    //          Object: 98589560, "NodeAttribute::GenericPhotometricLight" {
    //          }
    //          Document: "Viewer Document" {
    //              Document: "Protein scene" {
    //                  Document: "Collapsed Externals" {
    //                      Document: "Fbx Libraries" {
    //                          Document: "Fbx System Libraries" {
    //                              Document: "ADSKLibrary" {
    //                              }
    //                          }
    //                      }
    //                  }
    //              }
    //          }
    //     }
    // 
    // New format:
    // 
    //     Reference: "Reference_1", "Internal" {
    //          Object: 98286696, "FbxEnvironment::SunAndSky-002"
    //          DocumentPath: "Viewer Document", "Protein scene", "Collapsed Externals", "Fbx Libraries", "Fbx System Libraries", "ADSKLibrary"
    //     }
    //

    FbxString lSep("::");

    if( mFileObject->FieldReadBegin("DocumentPath") )
    {
        for( int i = 0, n = mFileObject->FieldReadGetCount(); i < n; ++i )
        {
            pLastDocName = mFileObject->FieldReadC();

            if( i == 0 )
            {
                pRefDocRootName = pRefDocPathName = pLastDocName;
            }
            else
            {
                pRefDocPathName += lSep;
                pRefDocPathName += pLastDocName;
            }
        }

        mFileObject->FieldReadEnd();
    }
    else
    {
        int i, lNumDocRead = 0;
        while (mFileObject->FieldReadBegin("Document"))
        {
            pLastDocName = mFileObject->FieldReadC();

            if (mFileObject->FieldReadBlockBegin())
            {
                if (lNumDocRead == 0)
                {
                    pRefDocRootName = pRefDocPathName = pLastDocName;
                }
                else
                {
                    pRefDocPathName += lSep;
                    pRefDocPathName += pLastDocName;
                }
                lNumDocRead++;
            }

        }
        for (i = 0; i < lNumDocRead; i++)
        {
            mFileObject->FieldReadBlockEnd();
        }
    }
}

//
// Read reference section
//
bool FbxReaderFbx7_Impl::ReadReferenceSection(FbxDocument *pDocument, KTypeReadReferences& pDocReferences)
{
    bool            lRet = true;

    // Parse object reference section first, to accumulate external/internal references;
    // parse document references (FilePathUrl) second, and for any document with
    // external references emit an event.  Purely internal documents do not emit an
    // event.  

    if (mFileObject->FieldReadBegin(FIELD_OBJECT_REFERENCES))
    {
        if(mFileObject->FieldReadBlockBegin())
        {
            FbxString lRefDocRootName;
            FbxString lRefDocPathName;
            FbxString lRefDocName;

            while (mFileObject->FieldReadBegin(FIELD_OBJECT_REFERENCES_REFERENCE))
            {
                FbxString lRefName = mFileObject->FieldReadC();
                FbxString lRefTypeName = mFileObject->FieldReadC();
                bool    lRefIsExternal = (lRefTypeName == "External");
                FbxString lRefObjName;

                if (mFileObject->FieldReadBlockBegin())
                {
                    FbxLongLong lRefObjectId = kInvalidObjectId;

                    if (mFileObject->FieldReadBegin("Object"))
                    {
                        if( !lRefIsExternal )
                        {
                            lRefObjectId = mFileObject->FieldReadLL();
                        }
                        lRefObjName = mFileObject->FieldReadC();
                        mFileObject->FieldReadEnd();
                    }

                    FieldReadDocumentPath(lRefDocRootName, lRefDocPathName, lRefDocName);

                    pDocReferences.AddReference(lRefIsExternal, lRefName, lRefObjName, lRefDocRootName, lRefDocPathName, lRefObjectId);

                    mFileObject->FieldReadBlockEnd();
                }

                mFileObject->FieldReadEnd();
            }

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

                    FieldReadDocumentPath(lRefDocRootName, lEvent.mParentFullName, lEvent.mDocumentName);

                    if( lEvent.mParentFullName == lEvent.mDocumentName )
                    {
                        lEvent.mParentFullName.Clear();
                    }
                    else
                    {
                        // Should::Have::Someting::Like::lRefDocName
                        int lDocNameLength = (int)(lEvent.mDocumentName.GetLen());
                        int lDocPathLength = (int)(lEvent.mParentFullName.GetLen());
                        FBX_ASSERT( lDocPathLength > (lDocNameLength + 2) );
                        FBX_ASSERT( lEvent.mParentFullName[lDocPathLength - lDocNameLength - 1] == ':' );
                        FBX_ASSERT( lEvent.mParentFullName[lDocPathLength - lDocNameLength - 2] == ':' );

                        lEvent.mParentFullName = lEvent.mParentFullName.Left(lDocPathLength - lDocNameLength - 2);
                    }

                    mFileObject->FieldReadBlockEnd();
                }

                if( !IsInternalDocument(pDocument, lEvent, pDocReferences) )
                {
                    pDocument->Emit(lEvent);
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
bool FbxReaderFbx7_Impl::ReadDefinitionSection(FbxDocument* pDocument, FbxObjectTypeInfoList& pObjectContent)
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

				if( pObjectContent.Find(lType) == -1 )
                {
					pObjectContent.PushBack(lType);

                    if( mFileObject->FieldReadBlockBegin() )
                    {
                        // property templates for this object definition
                        // and yes, there might be more than one, since some
                        // kfbxnodeattributes get merged with their nodes.
                        while (mFileObject->FieldReadBegin(FIELD_OBJECT_PROPERTY_TEMPLATE))
                        {
                            FbxString lFbxClassName = mFileObject->FieldReadC();

                            if (mFileObject->FieldReadBlockBegin())
                            {
                                bool lReadStream = false;
                                FbxClassId lId = mManager.FindClass(lFbxClassName);

                                //FBX_ASSERT( lId.IsValid() );

                                if (lId.IsValid())
                                {
                                    FbxObject* lTemplateObj = static_cast<FbxObject*>(mManager.CreateNewObjectFromClassId(lId, lFbxClassName + "_TemplateObject"));
                                    mManager.UnregisterObject(lTemplateObj);

                                    if (ReadProperties(lTemplateObj))
                                    {
                                        bool lSuccess = mClassTemplateMap.AddClassId(lId, lTemplateObj);
                                        FBX_ASSERT(lSuccess);
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


// ****************************************************************************************
// Read Objects sections
// ****************************************************************************************

//
// Read object section
//
bool FbxReaderFbx7_Impl::ReadObjectSection
(
    FbxDocument*                       pDocument,
    FbxObjectTypeInfoList&           pObjectContent,
    KTypeReadReferences&                pDocReferences
)
{
    if (mFileObject->FieldReadBegin(FIELD_OBJECT_PROPERTIES))
    {
        if (mFileObject->FieldReadBlockBegin())
        {
            OrderTypeInfoSection(pObjectContent);

			for( size_t i = 0, c = pObjectContent.Size(); i < c; ++i )
            {
                while( !mCanceled && mFileObject->FieldReadBegin(pObjectContent[i]) )
                {
                    FbxLongLong lObjectUniqueId       = mFileObject->FieldReadLL();
                    FbxString lObjectNameWithPrefix   = mFileObject->FieldReadC();
                    FbxString lObjectSubType          = mFileObject->FieldReadC();
                    FbxString lObjectName             = FbxObject::StripPrefix(lObjectNameWithPrefix);

                    /*
                        We may have 2 pairs of arguments on the line:

                        "ReferenceTo", "RefName", "Doc", DocumentID

                        Each one is optional, but they would be in this order.
                    */

                    // Default to top document, unless overriden.
                    FbxDocument* lContainerDocument= pDocument;

                    FbxString lObjectArg  = mFileObject->FieldReadC();

                    FbxObject* lReferencedObject = NULL;
                    if (lObjectArg.Compare(FIELD_KFBXOBECT_REFERENCE_TO) == 0)
                    {
                        FbxObject* lTmpObj;
                        FbxString     lRefStr = mFileObject->FieldReadC();
                        FbxString     lRefObjStr;
                        FbxLongLong   lRefObjId;
                        bool        lRefExternal;

                        if (pDocReferences.GetReferenceResolution(lRefStr, lRefObjStr, lRefObjId, lRefExternal, lTmpObj))
                        {
                            if (lRefExternal)
                            {
                                lReferencedObject = lTmpObj;
                            }
                            else
                            {
                                lReferencedObject = GetObjectFromId(lRefObjId);

                                // Objects are supposed to be written in the proper
                                // reference-depth order; see fbx7 writter.
                                FBX_ASSERT_MSG( lReferencedObject, "FBX7 writer did not output references in the proper depth order" );  
                            }
                        }
                    }

                    // If we read an argument already and it's NOT the one we're after, see
                    // if we have one waiting in the queue.
                    if( !lObjectArg.IsEmpty() && lObjectArg.Compare(FIELD_KFBXOBJECT_DOCUMENT) != 0 )
                    {
                        lObjectArg = mFileObject->FieldReadC();
                    }

                    if( lObjectArg.Compare(FIELD_KFBXOBJECT_DOCUMENT) == 0)
                    {
                        FbxLongLong   lDocumentID = mFileObject->FieldReadLL();
                        FBX_ASSERT_MSG(GetObjectFromId(lDocumentID), "Object read back using an invalid container ID.");

                        FbxDocument* lDoc      = FbxCast<FbxDocument>(GetObjectFromId(lDocumentID));
                        FBX_ASSERT_MSG(lDoc, "Object being read back in invalid document, will be stored in top document.");

                        if( lDoc )
                        {
                            lContainerDocument = lDoc;
                        }
                    }

                    if (mFileObject->FieldReadBlockBegin())
                    {
						ReadObject(lContainerDocument, pObjectContent[i], lObjectSubType, lObjectName, lObjectUniqueId, lReferencedObject, pDocReferences);
                        if (mProgress)
                            mCanceled = mProgress->IsCanceled();

                        mFileObject->FieldReadBlockEnd();
                    }
					
					if(mStatus.GetCode() == FbxStatus::eInvalidFile)
					{
						// The File is corrupted, Canceling the reading process 
						mCanceled = true;
					}

                    mFileObject->FieldReadEnd();
                }
            }

            mFileObject->FieldReadBlockEnd();
        }
        mFileObject->FieldReadEnd();
    }

    return !mCanceled;
}

//
// Read object
//    
bool FbxReaderFbx7_Impl::ReadObject(
    FbxDocument*   pDocument,
    FbxString&        pObjectType,
    FbxString&        pObjectSubType,
    FbxString&        pObjectName,
    FbxLongLong       pObjectUniqueId,
    FbxObject*     pReferencedObject,
    KTypeReadReferences& pDocReferences
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
        if( mReader ) mReader->PluginsRead(pObjectName.Buffer(), pObjectSubType.Buffer());
	#endif
    }
    if( pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_SCENEINFO )
    {
        // Document infos should be written in the header or in the 'Documents' section
        FBX_ASSERT( false );   
    }
    else if( pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MODEL )
    {
        FbxNode* lNode = CreateOrCloneReference<FbxNode>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lNode )
        {
            mNodeArrayName.Add (const_cast<char*>(lNode->GetName()), (FbxHandle)lNode);
            ReadNode(*lNode, pObjectSubType, pDocReferences);
    
            AddObjectIdAndConnect(pObjectUniqueId, lNode, pDocument);
        }
    }
    else if( pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_NODE_ATTRIBUTE )
    {
        FbxNodeAttribute* lAttr = ReadNodeAttribute( pObjectSubType, pObjectName, pObjectUniqueId, pReferencedObject );
        if( lAttr )
        {
            AddObjectIdAndConnect(pObjectUniqueId, lAttr, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GEOMETRY)
    {
        FbxGeometryBase* lGeometry = NULL;

        if (pObjectSubType == FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_MESH)
        {
            FbxMesh* lMesh = CreateOrCloneReference<FbxMesh>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
            ReadMesh(*lMesh);
            lGeometry = lMesh;
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
        else if (pObjectSubType == FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_LINE)
        {
            FbxLine* lLine = CreateOrCloneReference<FbxLine>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
            ReadLine( *lLine );
            lGeometry = lLine;
        }
		else if(pObjectSubType == FIELD_OBJECT_TYPE_GEOMETRY_SUBTYPE_SHAPE && IOS_REF.GetBoolProp(IMP_FBX_SHAPE, true))
		{
			FbxShape* lShape = CreateOrCloneReference<FbxShape>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
			ReadShape( *lShape );
			lGeometry = lShape;
		}
        else
        {
            InstanciateUnknownObject = true;
            ObjectClassId = CheckRuntimeClass(FBX_TYPE(FbxGeometry), pObjectType, pObjectSubType, mManager);
        }

        if (lGeometry != NULL)
        {
            AddObjectIdAndConnect(pObjectUniqueId, lGeometry, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MATERIAL)
    {
        // Read a FbxSurfaceMaterial
        if (!mIdObjectMap.Find(pObjectUniqueId)) {
            FbxSurfaceMaterial* lMaterial = ReadSurfaceMaterial(pObjectName, pObjectSubType, FbxCast<FbxSurfaceMaterial>(pReferencedObject));

            if( lMaterial)
            {
                AddObjectIdAndConnect(pObjectUniqueId, lMaterial, pDocument);
            }
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TEXTURE)
    {
		FbxFileTexture* lTexture = CreateOrCloneReference<FbxFileTexture>(mManager, pObjectName, pReferencedObject, mClassTemplateMap, ADSK_TYPE_TEXTURE);

        if( lTexture )
        {
            ReadFileTexture(*lTexture);
            AddObjectIdAndConnect(pObjectUniqueId, lTexture, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_THUMBNAIL)
    {
        FbxThumbnail* lThumbnail = CreateOrCloneReference<FbxThumbnail>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lThumbnail )
        {
            ReadThumbnail(*lThumbnail);
            AddObjectIdAndConnect(pObjectUniqueId, lThumbnail, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_VIDEO)
    {
        FbxVideo* lVideo = CreateOrCloneReference<FbxVideo>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lVideo )
        {
            ReadVideo(*lVideo);
            AddObjectIdAndConnect(pObjectUniqueId, lVideo, pDocument);
        }
    }
    else if (lIsAScene && (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONTAINER))
    {
        // Read a AssetContainer
        FbxContainer* lContainer = CreateOrCloneReference<FbxContainer>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if(lContainer)
        {
            ReadContainer(*lContainer);
            AddObjectIdAndConnect(pObjectUniqueId, lContainer, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_DEFORMER)
    {
        if (lIsAScene)
        {
            // Read a FbxDeformer
            if (pObjectSubType == "Skin")
            {
                FbxSkin* lSkin = CreateOrCloneReference<FbxSkin>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

                if( lSkin )
                {
                    ReadSkin(*lSkin);
                    AddObjectIdAndConnect(pObjectUniqueId, lSkin, pDocument);
                }
            }
            else if (pObjectSubType == "Cluster")
            {
                FbxCluster* lCluster = CreateOrCloneReference<FbxCluster>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

                if( lCluster )
                {
                    ReadCluster(*lCluster);
                    AddObjectIdAndConnect(pObjectUniqueId, lCluster, pDocument);
                }
            }
            else if (pObjectSubType == "VertexCacheDeformer")
            {
                FbxVertexCacheDeformer* lDeformer = CreateOrCloneReference<FbxVertexCacheDeformer>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

                if( lDeformer)
                {
                    ReadVertexCacheDeformer(*lDeformer);
                    AddObjectIdAndConnect(pObjectUniqueId, lDeformer, pDocument);
                }
            }
			else if(pObjectSubType == "BlendShape")
			{
				if(IOS_REF.GetBoolProp(IMP_FBX_SHAPE, true))
				{
					FbxBlendShape* lBlendShape = CreateOrCloneReference<FbxBlendShape>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

					if( lBlendShape )
					{
						ReadBlendShape(*lBlendShape);
						AddObjectIdAndConnect(pObjectUniqueId, lBlendShape, pDocument);
					}
				}
			}
			else if (pObjectSubType == "BlendShapeChannel")
			{
				if(IOS_REF.GetBoolProp(IMP_FBX_SHAPE, true))
				{
					FbxBlendShapeChannel* lBlendShapeChannel = CreateOrCloneReference<FbxBlendShapeChannel>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

					if( lBlendShapeChannel )
					{
						ReadBlendShapeChannel(*lBlendShapeChannel);
						AddObjectIdAndConnect(pObjectUniqueId, lBlendShapeChannel, pDocument);
					}
				}
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
				int lCharacterPoseIndex = lScene->CreateCharacterPose(pObjectName.Buffer());
                if (lCharacterPoseIndex != -1)
                {
                    FbxCharacterPose* lCharacterPose = lScene->GetCharacterPose(lCharacterPoseIndex);
                    if (!ReadCharacterPose(*lCharacterPose))
                    {
                        lScene->DestroyCharacterPose(lCharacterPoseIndex);
                    }
                    else
                    {
                        AddObjectId(pObjectUniqueId, lCharacterPose);
                    }
                }
            }
            else if (pObjectSubType == "BindPose" || pObjectSubType == "RestPose")
            {
                bool isBindPose = pObjectSubType == "BindPose";
                FbxPose* lPose = FbxPose::Create(&mManager, pObjectName);

                if( lPose )
                {
                    lPose->SetIsBindPose(isBindPose);
    
                    if (!ReadPose(*lScene, lPose, isBindPose))
                    {
                        lPose->Destroy();
                    }
                    else
                    {
                        AddObjectIdAndConnect(pObjectUniqueId, lPose, pDocument);
                    }
                }
            }
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GENERIC_NODE)
    {
        // Read a FbxGenericNode
        FbxGenericNode* lNode = FbxGenericNode::Create(&mManager, pObjectName);

        if( lNode )
        {
            ReadGenericNode(*lNode);
            AddObjectIdAndConnect(pObjectUniqueId, lNode, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONSTRAINT)
    {
        //TODO -> merge those two sections when the character will be added to the entity
        if( !strcmp(pObjectSubType.Buffer(), TOKEN_KFBXCONSTRAINT_CHARACTER) )
        {            
            FbxCharacter* lCharacter = FbxCharacter::Create(pDocument, pObjectName);
            if( lCharacter && !InstanciateUnknownObject )
            {
                int lInputType;
                int lInputIndex;

                ReadCharacter(*lCharacter, lInputType, lInputIndex);
                AddObjectId(pObjectUniqueId, lCharacter);
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
                    AddObjectIdAndConnect(pObjectUniqueId, lConstraint, pDocument);
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
    // todo: Actor
    //
    else if (lIsAScene && (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONTROLSET_PLUG))
    {
        if(pObjectSubType == "ControlSetPlug")
        {
            FbxControlSetPlug* lPlug = FbxControlSetPlug::Create(pDocument, pObjectName);

            if( lPlug )
            {
                mFileObject->FieldReadI(FIELD_KFBXGENERICNODE_VERSION,100);   
                ReadPropertiesAndFlags(lPlug);

                AddObjectId(pObjectUniqueId, lPlug);
            }
        }
    }
    else if (lIsAScene && (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CACHE))
    {
        FbxCache* lCache = CreateOrCloneReference<FbxCache>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lCache )
        {
            ReadCache( *lCache );
            AddObjectIdAndConnect(pObjectUniqueId, lCache, pDocument);
        }
    }
    else if(lIsAScene && (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_GLOBAL_SETTINGS))
    {
        ReadGlobalSettings(lScene->GetGlobalSettings());
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_IMPLEMENTATION)
    {
        FbxImplementation* lNode = CreateOrCloneReference<FbxImplementation>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lNode )
        {   
            ReadImplementation( *lNode );
            AddObjectIdAndConnect(pObjectUniqueId, lNode, pDocument);
        }
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_BINDINGTABLE)
    {
        FbxBindingTable* lNode = CreateOrCloneReference<FbxBindingTable>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lNode )
        {
            ReadBindingTable( *lNode );
            AddObjectIdAndConnect(pObjectUniqueId, lNode, pDocument);
        }
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_BINDINGOPERATOR)
    {
        FbxBindingOperator* lNode = CreateOrCloneReference<FbxBindingOperator>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lNode )
        {
            ReadBindingOperator( *lNode );
            AddObjectIdAndConnect(pObjectUniqueId, lNode, pDocument);
        }
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_SELECTIONNODE)
    {
        FbxSelectionNode* lNode = CreateOrCloneReference<FbxSelectionNode>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lNode )
        {
            ReadSelectionNode( *lNode );
            AddObjectIdAndConnect(pObjectUniqueId, lNode, pDocument);
        }
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_COLLECTION)
    {
        if(pObjectSubType == "SelectionSet")
        {
            FbxSelectionSet* lNode = CreateOrCloneReference<FbxSelectionSet>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

            if( lNode )
            {
                ReadSelectionSet( *lNode );
                AddObjectIdAndConnect(pObjectUniqueId, lNode, pDocument);
            }
        }
        else
        {
             FbxCollection* lNode = CreateOrCloneReference<FbxCollection>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

            if( lNode )
            {
                ReadCollection( *lNode );
                AddObjectIdAndConnect(pObjectUniqueId, lNode, pDocument);
            }
        }
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_DOCUMENT)
    {
        FbxDocument* lNode = CreateOrCloneReference<FbxDocument>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lNode )
        {
            ReadDocument( *lNode );
            AddObjectIdAndConnect(pObjectUniqueId, lNode, pDocument);
        }
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_LAYERED_TEXTURE)
    {
        FbxLayeredTexture* lTex = CreateOrCloneReference<FbxLayeredTexture>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lTex )
        {
            ReadLayeredTexture( *lTex );
            AddObjectIdAndConnect(pObjectUniqueId, lTex, pDocument);
        }
    }
    else if(pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_PROCEDURAL_TEXTURE)
    {
        FbxProceduralTexture* lTex = CreateOrCloneReference<FbxProceduralTexture>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lTex )
        {
            ReadProceduralTexture( *lTex );
            AddObjectIdAndConnect(pObjectUniqueId, lTex, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_OBJECTMETADATA)
    {
        FbxObjectMetaData* lMeta = CreateOrCloneReference<FbxObjectMetaData>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lMeta )
        {
            ReadPropertiesAndFlags(lMeta);
            AddObjectIdAndConnect(pObjectUniqueId, lMeta, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_STACK)
    {
        FbxAnimStack* lAnimStack = CreateOrCloneReference<FbxAnimStack>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        if ( lAnimStack )
        {
            ReadAnimStack(*lAnimStack);
            AddObjectIdAndConnect(pObjectUniqueId, lAnimStack, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_LAYER)
    {
        FbxAnimLayer* lAnimLayer = CreateOrCloneReference<FbxAnimLayer>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        if ( lAnimLayer )
        {
            ReadAnimLayer(*lAnimLayer);
            AddObjectIdAndConnect(pObjectUniqueId, lAnimLayer, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_CURVENODE)
    {
        FbxAnimCurveNode* lAnimCurveNode = CreateOrCloneReference<FbxAnimCurveNode>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        if ( lAnimCurveNode )
        {
            ReadCurveNode(*lAnimCurveNode);
            AddObjectIdAndConnect(pObjectUniqueId, lAnimCurveNode, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_ANIM_CURVE)
    {
        FbxAnimCurve* lAnimCurve = CreateOrCloneReference<FbxAnimCurve>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        if ( lAnimCurve )
        {
            ReadCurve(*lAnimCurve);
            AddObjectIdAndConnect(pObjectUniqueId, lAnimCurve, pDocument);
        }
    }
	else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_AUDIO_LAYER)
    {
        FbxAudioLayer* lAudioLayer = CreateOrCloneReference<FbxAudioLayer>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        if ( lAudioLayer )
        {
            ReadAudioLayer(*lAudioLayer);
            AddObjectIdAndConnect(pObjectUniqueId, lAudioLayer, pDocument);
        }
    }
	else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_AUDIO)
    {
        FbxAudio* lAudio = CreateOrCloneReference<FbxAudio>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        if ( lAudio )
        {
            ReadAudio(*lAudio);
            AddObjectIdAndConnect(pObjectUniqueId, lAudio, pDocument);
        }
    }
    else if (pObjectType == FIELD_OBJECT_DEFINITION_OBJECT_TYPE_REFERENCE)
    {
        FbxSceneReference* lReference = CreateOrCloneReference<FbxSceneReference>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        if ( lReference )
        {
            ReadReference(*lReference);
            //AddObjectIdAndConnect(pObjectUniqueId, lReference, pDocument);

            mManager.AddReference(lReference);
        }
    }
    else
    {
        InstanciateUnknownObject = true;
        ObjectClassId = CheckRuntimeClass(FBX_TYPE(FbxObject), pObjectType, pObjectSubType, mManager);
    }

    if (InstanciateUnknownObject) 
    {
        FbxObject* lFbxObject=0;
        // Register a new class type objects that are not known to the SDK
        // The object will be viewed as a generic FbxObject
        if(pReferencedObject == NULL)
        {
            lFbxObject = (FbxObject*)mManager.CreateNewObjectFromClassId(ObjectClassId, pObjectName);

            if( lFbxObject )
            {
                mClassTemplateMap.MergeWithTemplate(lFbxObject);
            }
        }
        else
        {
            lFbxObject = (FbxObject*)CreateOrCloneReference(mManager, pObjectName, pReferencedObject, ObjectClassId, mClassTemplateMap);
        }

        // Create and register the object
        FBX_ASSERT_MSG( lFbxObject,"Could not create object" );
        if (lFbxObject) 
        {
            ReadPropertiesAndFlags(lFbxObject);
            AddObjectIdAndConnect(pObjectUniqueId, lFbxObject, pDocument);
        }
    }

	// the mStatus may have been cleared while reading a valid section but a previous one may have been
	// detected as corrupted so let's return the error we had before processing the object
	if (!mStatus.Error() && lPrevStatus.Error())
		mStatus = lPrevStatus;

    return true;
}

void FbxReaderFbx7_Impl::ReadCameraSwitcher(FbxScene& pScene)
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
// Read camera switcher
//
bool FbxReaderFbx7_Impl::ReadCameraSwitcher( FbxCameraSwitcher& pCameraSwitcher )
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
// reorder the indices of camera switchers
//
void FbxReaderFbx7_Impl::ReorderCameraSwitcherIndices(FbxScene& pScene)
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
            if( lAnimStack )
            {
                int lCount = lAnimStack->GetMemberCount<FbxAnimLayer>();
                for( int lAnimLayerIter = 0; lAnimLayerIter < lCount; ++lAnimLayerIter )
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
                            lNewCameraIndex = lCameraIndexArray[lCameraIndex - 1];
                            if( lNewCameraIndex != -1 )
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
                                if (lCameraIndex >= 1 && lCameraIndex <= lCameraIndexCount)
                                {
                                    lNewCameraIndex = lCameraIndexArray[lCameraIndex - 1];
                                    if (lNewCameraIndex != -1)
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
void FbxReaderFbx7_Impl::ReadCharacter(FbxCharacter& pCharacter, int& pInputType, int& pInputIndex)
{
	// Try to read the "Version" field (will always exist for 2013 - except files generated between Jan and September 11 before
    // all the impacted DCCs make the transition to the new definition - and up version but not before)
	int lCharacterVersion = mFileObject->FieldReadI("Version", 0);	
	pCharacter.SetVersion(lCharacterVersion);

	// Read the character properties
    ReadPropertiesAndFlags(&pCharacter);

    // Need to publish all the character property
    FbxProperty    lProp;
    bool            lValue;

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
void FbxReaderFbx7_Impl::ReadCharacterLinkGroup(FbxCharacter& pCharacter, int pCharacterGroupId)
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
void FbxReaderFbx7_Impl::ReadCharacterLink(FbxCharacter& pCharacter, int pCharacterNodeId)
{
    FbxCharacterLink* lCharacterLink = pCharacter.GetCharacterLinkPtr((FbxCharacter::ENodeId)pCharacterNodeId);
    FBX_ASSERT(lCharacterLink != NULL);
    if (lCharacterLink == NULL)
        return;

    FbxString lTemplateName = mFileObject->FieldReadS("NAME");

    if( !lTemplateName.IsEmpty())
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
// Read fbx character link rotation space
//
void FbxReaderFbx7_Impl::ReadCharacterLinkRotationSpace(FbxCharacterLink& pCharacterLink)
{
    if(mFileObject->FieldReadBegin("ROTATIONSPACE"))
    {
        pCharacterLink.mHasRotSpace = true;

        FbxVector4 lLimits;
        bool lActiveness[3];

        if(mFileObject->FieldReadBlockBegin())
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
        }

        mFileObject->FieldReadBlockEnd();
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
void FbxReaderFbx7_Impl::ReadCharacterPoseNodeProperty(FbxProperty& pProperty, int pInstance)
{
	mFileObject->FieldReadBegin("P", pInstance);

	FbxString lPropertyName   = mFileObject->FieldReadS( );    // Name
    FbxString lTypeName       = mFileObject->FieldReadS( );    // TypeName
    FbxString lDataTypeName   = mFileObject->FieldReadS();
    FbxString lFlags          = mFileObject->FieldReadS( );	

	bool                lIsAnimatable   = strchr(lFlags,'A')!=NULL;
    bool                lIsUser         = strchr(lFlags,'U')!=NULL;
    bool                lIsAnimated     = strchr(lFlags,'+')!=NULL;
    bool                lIsHidden       = strchr(lFlags,'H')!=NULL;

	FbxDataType        lDataType;
	if( !lDataTypeName.IsEmpty() )
    {
		lDataType = mManager.GetDataTypeFromName(lDataTypeName);
        if( !lDataType.Valid() ) 
			lDataType = mManager.GetDataTypeFromName(lTypeName);

        if( !lDataType.Valid() )
        {
            FbxDataType lBaseType = mManager.GetDataTypeFromName(lDataTypeName);
            if( lBaseType.Valid() )
            {
                lDataType = mManager.CreateDataType(lDataTypeName, lBaseType.GetType());
            }
        }
    }
    else
    {
        lDataType = mManager.GetDataTypeFromName(lTypeName);
        if (!lDataType.Valid()) lDataType=mManager.GetDataTypeFromName(lTypeName);
    }

    // Make sure it has the correct flags
    if( pProperty.GetFlag(FbxPropertyFlags::eAnimatable) != lIsAnimatable )
        pProperty.ModifyFlag(FbxPropertyFlags::eAnimatable,lIsAnimatable);

    if( pProperty.GetFlag(FbxPropertyFlags::eUserDefined) != lIsUser )
        pProperty.ModifyFlag(FbxPropertyFlags::eUserDefined,lIsUser);

    if( pProperty.GetFlag(FbxPropertyFlags::eAnimated) != lIsAnimated )
        pProperty.ModifyFlag(FbxPropertyFlags::eAnimated,lIsAnimated);

    if( pProperty.GetFlag(FbxPropertyFlags::eHidden) != lIsHidden )
        pProperty.ModifyFlag(FbxPropertyFlags::eHidden,lIsHidden);

	switch (pProperty.GetPropertyDataType().GetType()) 
	{
		case eFbxDouble3: 
		{
			FbxDouble3 lValue;
			mFileObject->FieldRead3D((double *)&lValue);
			pProperty.Set( lValue );
		} 
		break;
		default:
            FBX_ASSERT_NOW("Unsupported type!");
        break;
	}

	mFileObject->FieldReadEnd();
}

bool FbxReaderFbx7_Impl::ReadCharacterPose(FbxCharacterPose& pCharacterPose)
{
    ReadPropertiesAndFlags(&pCharacterPose);

    bool lResult = false;
    if( mFileObject->FieldReadBegin("PoseScene") )
    {
        if( mFileObject->FieldReadBlockBegin() )
        {
			if (mFileObject->GetFileVersionNumber() < FBX_FILE_VERSION_7300)
			{
				// for backward compatibility: any FBX file version 7.2 and less will
				// read using the old stuff
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
			}
			else
			{
				// from FBX version 7.3, the new structure
				int count = mFileObject->FieldReadI("NbPoseNodes", 0);

				FbxScene* lScene = pCharacterPose.GetPoseScene();
				FbxArray<FbxNode*> lNodes;
				int inputType = -1;
				int inputIndex = -1;
				
				for (int i = 0; i < count; i++)
				{
					mFileObject->FieldReadBegin("PoseNode");
					if (mFileObject->FieldReadBlockBegin())
					{
						FbxString lNodeName = mFileObject->FieldReadS("Node");
						FbxString lParentName= mFileObject->FieldReadS("Parent");
			
						FbxNode *lNode = FbxNode::Create(lScene, lNodeName);
						lNodes.Add(lNode);
							
						ReadCharacterPoseNodeProperty(lNode->LclTranslation, 0);
						ReadCharacterPoseNodeProperty(lNode->LclRotation, 1);
						ReadCharacterPoseNodeProperty(lNode->LclScaling, 2);

						FbxNode* lParent = NULL;
						if (lParentName == "RootNode")
							lParent = lScene->GetRootNode();
						else
						{
							// find the parent (should already be contained in the lNodes list)
							for (int j = lNodes.GetCount()-1; j >= 0; j--)
							{
								FbxString name = lNodes[j]->GetName();
								if (name == lParentName)
								{
									lParent = lNodes[j];
									break;
								}
							}							
						}

						FBX_ASSERT(lParent != NULL);
						lParent->AddChild(lNode);
                
						mFileObject->FieldReadBlockEnd();
					}
					mFileObject->FieldReadEnd();
				}

				// and now the local character definition!
				ReadCharacter(*pCharacterPose.GetCharacter(), inputType, inputIndex);
                lResult = true;
			}
            mFileObject->FieldReadBlockEnd();
        }
        mFileObject->FieldReadEnd();
    }

    return lResult;
}

//
// Read kfbx pose
//
bool FbxReaderFbx7_Impl::ReadPose(FbxScene& pScene, FbxPose* pPose, bool pAsBindPose)
{
    ReadPropertiesAndFlags(pPose);

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
                FbxLongLong lNodeId = mFileObject->FieldReadLL("Node");

                ReadValueArray("Matrix", 16, &m.mData[0][0], &identity.mData[0][0]);
                if (!pAsBindPose) // for bindPoses the matrix is always global
                {
                    local = mFileObject->FieldReadI("Local") != 0;
                }

                // add an entry to the pose object.
                if (pPose && lNodeId != kInvalidObjectId )
                {
                    FbxUniqueIdObjectMap::Iterator Iter = mIdObjectMap.Find(lNodeId);

                    if( Iter != mIdObjectMap.End() )
                    {
                        FbxObject* lFbxObject = Iter->GetValue();

                        FbxNode* node = NULL;
                        if (lFbxObject->Is<FbxNode>())
                        {
                            node = (FbxNode*)lFbxObject;
                        }

                        int index = pPose->Add(node, m, local);
                        if (index == -1)
                        {
                            // an error occurred!
                        }

                    }
                    else
                    {
                        FBX_ASSERT_NOW("Node not found for pose");
                    }
                }

                mFileObject->FieldReadBlockEnd();
            }
            mFileObject->FieldReadEnd();
        }
    }

    return lResult;
}

//
// Read media from file
//
bool FbxReaderFbx7_Impl::ReadMedia(FbxDocument* pDocument, const char* pEmbeddedMediaDirectory /* = "" */)
{
    FbxScene*                  lScene = FbxCast<FbxScene>(pDocument);
    bool                        lIsAScene = (lScene != NULL);
    FbxArray<FbxString*>    lMediaNames;
    FbxArray<FbxString*>    lFileNames;
    int                         i, lTextureCount;

    // Set file names in textures.
	lTextureCount = pDocument->GetSrcObjectCount<FbxFileTexture>();

    for (i = 0; i < lTextureCount; i++)
    {
        FbxFileTexture* lTexture = pDocument->GetSrcObject<FbxFileTexture>(i);

        int lMediaIndex = FindString(lTexture->GetMediaName(), lMediaNames);

        if (lMediaIndex != -1)
        {
            FbxString lCorrectedFileName(FbxPathUtils::Clean(*lFileNames[lMediaIndex]));
            lTexture->SetFileName(lCorrectedFileName);

            if (FbxString(lTexture->GetRelativeFileName()) == "")
            {
                lTexture->SetRelativeFileName(mFileObject->GetRelativeFilePath(lCorrectedFileName));
            }
        }
    }

    if( lIsAScene )
    {
        // Set file names in cameras.
		FbxCamera*					lCamera = NULL;
		FbxIteratorSrc<FbxCamera>	lCameraIter(lScene);
		FbxForEach(lCameraIter, lCamera)
		{
            //background
            int lMediaIndex = FindString(lCamera->GetBackgroundMediaName(), lMediaNames);
            if( lMediaIndex != -1 )
            {
                FbxString lCorrectedFileName(FbxPathUtils::Clean(*lFileNames[lMediaIndex]));
                lCamera->SetBackgroundFileName(lCorrectedFileName);
            }
            //foreground
            lMediaIndex = FindString(lCamera->GetForegroundMediaName(), lMediaNames);
            if( lMediaIndex != -1 )
            {
                FbxString lCorrectedFileName(FbxPathUtils::Clean(*lFileNames[lMediaIndex]));
                lCamera->SetForegroundFileName(lCorrectedFileName);
            }
		}

        // and while at it, do the same on the lights that have gobos
		FbxLight*					lLight = NULL;
		FbxIteratorSrc<FbxLight>	lLightIter(lScene);
		FbxForEach(lLightIter, lLight)
		{
            int lMediaIndex = FindString(lLight->FileName.Get(), lMediaNames);
            if( lMediaIndex != -1 )
            {
                FbxString lCorrectedFileName(FbxPathUtils::Clean(*lFileNames[lMediaIndex]));
                lLight->FileName.Set(lCorrectedFileName);
            }
		}

        // Delete local lists.
        FbxArrayDelete(lMediaNames);
        FbxArrayDelete(lFileNames);
    }
	
    return true;
}

//
// Read fbx node
//
bool FbxReaderFbx7_Impl::ReadNode(FbxNode& pNode, FbxString& pObjectSubType, KTypeReadReferences& pDocReferences)
{
    int lNodeVersion = mFileObject->FieldReadI(FIELD_KFBXNODE_VERSION, 100);

    if(lNodeVersion < 232)
    {
        pNode.mCorrectInheritType = true;
    }

    ReadNodeShading(pNode);
    ReadNodeCullingType(pNode); // Probably obsolete (in property now)

    ReadNodeTarget(pNode);

    ReadPropertiesAndFlags(&pNode);
    pNode.UpdatePivotsAndLimitsFromProperties();

    return true;
}

//
// Read generic node information
//
bool FbxReaderFbx7_Impl::ReadGenericNode(FbxGenericNode& pNode)
{
    /*int lVersion = */mFileObject->FieldReadI(FIELD_KFBXGENERICNODE_VERSION, 100);

    // Read the properties
    ReadPropertiesAndFlags(&pNode);

    return true;
}

//
// Read shading information of node
//
bool FbxReaderFbx7_Impl::ReadNodeShading(FbxNode& pNode)
{
    //
    // Retrieve The Hidden and Shading Informations...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXNODE_HIDDEN))
    {
        FbxString lHidden = mFileObject->FieldReadC ();

/*        if (stricmp(lHidden, "True") == 0)
        {
            pNode.SetVisibility (false);
        }
*/
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
bool FbxReaderFbx7_Impl::ReadNodeCullingType(FbxNode& pNode)
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
// Read node target
//
bool FbxReaderFbx7_Impl::ReadNodeTarget( FbxNode& pNode )
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
// Read node attribute
//
FbxNodeAttribute* FbxReaderFbx7_Impl::ReadNodeAttribute( FbxString& pObjectSubType, FbxString& pObjectName, FbxLongLong pObjectUniqueId, FbxObject* pReferencedObject)
{
    FBX_ASSERT( pObjectUniqueId );

    if( !pObjectUniqueId )
    {
        return NULL;
    }

    FbxNodeAttribute* lAttr = NULL;

    if (strcmp (pObjectSubType.Buffer(), "CachedEffect") == 0)
    {
        FbxCachedEffect* lCachedEffect = CreateOrCloneReference<FbxCachedEffect>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lAttr = (lCachedEffect);
    }
    else if (strcmp (pObjectSubType.Buffer(), "LodGroup") == 0)
    {
        FbxLODGroup* lLodGroup = CreateOrCloneReference<FbxLODGroup>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lAttr = (lLodGroup );
    }
    else if (pObjectSubType == "Null")
    {
        FbxNull* lNull = CreateOrCloneReference<FbxNull>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lNull )
        {
            ReadNull(*lNull);
            lAttr = (lNull );
        }
    }
    else if (pObjectSubType == "Light")
    {
        FbxLight* lLight = CreateOrCloneReference<FbxLight>(mManager, pObjectName, pReferencedObject, mClassTemplateMap, ADSK_TYPE_LIGHT);
        if( lLight )
        {
            ReadLight(*lLight);
            lAttr = (lLight );
        }
    }
    else if (pObjectSubType == "Camera")
    {
        FbxCamera* lCamera = CreateOrCloneReference<FbxCamera>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        if( lCamera )
        {
            ReadCamera(*lCamera);
            lAttr = (lCamera );
        }
    }
    else if (pObjectSubType == "CameraStereo")
    {
        FbxCameraStereo* lCameraStereo = CreateOrCloneReference<FbxCameraStereo>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        if( lCameraStereo )
        {
            ReadCameraStereo(*lCameraStereo);
            lAttr = (lCameraStereo );
        }
    }
    else if (pObjectSubType == "CameraSwitcher")
    {
        // The camera switcher attribute read here only exist from fbx v6.0 files
        FbxCameraSwitcher* lSwitcher = CreateOrCloneReference<FbxCameraSwitcher>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        if( lSwitcher )
        {
            ReadCameraSwitcher(*lSwitcher);
            lAttr = (lSwitcher );
        }
    }
    else if (pObjectSubType == "Optical")
    {
        FbxOpticalReference *lOpticalReference = CreateOrCloneReference<FbxOpticalReference>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);
        lAttr = (lOpticalReference );
    }
    else if (pObjectSubType == "Marker" || pObjectSubType == "OpticalMarker" || pObjectSubType == "IKEffector" || pObjectSubType == "FKEffector" )
    {
        FbxMarker *lMarker = CreateOrCloneReference<FbxMarker>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lMarker )
        {
            if( pObjectSubType == "Marker" )
            {
                lMarker->SetType(FbxMarker::eStandard);
            }
            else if( pObjectSubType == "OpticalMarker" )
            {
                lMarker->SetType(FbxMarker::eOptical);
            }
            else if( pObjectSubType == "IKEffector" )
            {
                lMarker->SetType(FbxMarker::eEffectorIK);
            }
            else if( pObjectSubType == "FKEffector" )
            {
                lMarker->SetType(FbxMarker::eEffectorFK);
            }
            else
            {
                FBX_ASSERT(false);
            }

            ReadMarker(*lMarker);
            lAttr = (lMarker );
        }
    }
    else if (pObjectSubType == "Root")
    {
        FbxSkeleton *lSkeletonRoot = CreateOrCloneReference<FbxSkeleton>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lSkeletonRoot )
        {
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
            lAttr = (lSkeletonRoot );
        }
    }
    else if (pObjectSubType == "Effector")
    {
        FbxSkeleton *lSkeletonEffector = CreateOrCloneReference<FbxSkeleton>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lSkeletonEffector )
        {
            lSkeletonEffector->SetSkeletonType(FbxSkeleton::eEffector);
            lAttr = (lSkeletonEffector );
        }
    }
    else if (strcmp (pObjectSubType.Buffer (), "Limb") == 0)
    {
        FbxSkeleton *lSkeletonLimb = CreateOrCloneReference<FbxSkeleton>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lSkeletonLimb )
        {
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
            lAttr = (lSkeletonLimb );
        }
    }
    else if (pObjectSubType == "LimbNode")
    {
        FbxSkeleton *lSkeletonLimbNode = CreateOrCloneReference<FbxSkeleton>(mManager, pObjectName, pReferencedObject, mClassTemplateMap);

        if( lSkeletonLimbNode )
        {
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
            lAttr = (lSkeletonLimbNode );
        }
    }
    else
    {
        // Attempt to create a custom node attribute type, or at least a node attribute.
        FbxClassId lNodeAttributeId = CheckRuntimeClass(FBX_TYPE(FbxNodeAttribute), FIELD_OBJECT_DEFINITION_OBJECT_TYPE_NODE_ATTRIBUTE, pObjectSubType, mManager);

        if( lNodeAttributeId.IsValid() )
        {
            if(pReferencedObject == NULL)
            {
                lAttr = (FbxNodeAttribute*)mManager.CreateNewObjectFromClassId(lNodeAttributeId, pObjectName);

                if( lAttr )
                {
                    mClassTemplateMap.MergeWithTemplate(lAttr);
                }
            }
            else
            {
                lAttr = (FbxNodeAttribute*)CreateOrCloneReference(mManager, pObjectName, pReferencedObject, lNodeAttributeId, mClassTemplateMap);
            }           
        }
        else
        {
            FBX_ASSERT_NOW("Unknown subType, or geometry-based subtype not stored as a Geometry");
        }
    }

    if( lAttr )
    {
        ReadPropertiesAndFlags(lAttr);
        FBX_ASSERT( GetObjectFromId(pObjectUniqueId) == NULL );
    }

    //extract precomp file
    if (pObjectSubType == "CameraStereo")
    {
        FbxCameraStereo* lCameraStereo = (FbxCameraStereo*)lAttr;
        if( lCameraStereo )
            ReadCameraStereoPrecomp(*lCameraStereo);
    }

    return lAttr;
}

//
// Read geometry layer elements
//
bool FbxReaderFbx7_Impl::ReadLayerElements(FbxGeometry& pGeometry)
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
        ReadLayerElementsChannelUV(&pGeometry, lElementsTextureUVs[lLayerIndex], FBXSDK_TEXTURE_TYPE(lLayerIndex));
    }

    while(mFileObject->FieldReadBegin(FIELD_KFBXLAYER))
    {
        lLayerIndex = mFileObject->FieldReadI();

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

                    const char* lLayerElementType  = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_TYPE);
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
										lLayer->SetTextures(FBXSDK_TEXTURE_TYPE(lLayerIndex), static_cast<FbxLayerElementTexture*>(lElementsTextures[lLayerIndex1][lLayerElementIndex]));
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
bool FbxReaderFbx7_Impl::ReadLayerElementsMaterial(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsMaterial)
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
                lLayerElementMaterial->SetName(lLayerName);
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementMaterial->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementMaterial->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));

            FBX_ASSERT(ConvertReferenceModeToken(lReferenceMode) != FbxLayerElement::eDirect) ;

            ReadValueArray(FIELD_KFBXGEOMETRYMESH_MATERIALS_ID, lLayerElementMaterial->GetIndexArray());
            mFileObject->FieldReadBlockEnd();
        }

        mFileObject->FieldReadEnd();

        int lAddedIndex  = pElementsMaterial.Add(lLayerElementMaterial);

        FBX_ASSERT( lAddedIndex == lLayerElementIndex);
    }
    return true;
}

template<typename T>
int FbxReaderFbx7_Impl::ReadValueArray(FbxLayerElementArrayTemplate<T>& pArray)
{
    int lValueCount;
    const T * lValues = mFileObject->FieldReadArray(lValueCount, (T*) 0);

    pArray.Resize( lValueCount );

    // FBX7 FIXME: Replace with a write-lock-set-all-unlock
    for (int i = 0; i < lValueCount; ++i)
    {
        pArray.SetAt(i, lValues[i]);
    }

    return lValueCount;
}

//
// Read layer element normal for geometry
//
bool FbxReaderFbx7_Impl::ReadLayerElementsNormal(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsNormal)
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
                lLayerElementNormal->SetName(lLayerName);
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementNormal->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementNormal->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));

            if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_NORMALS))
            {
                int lValueCount;
                
                const double* lValues = mFileObject->FieldReadArrayD(lValueCount);
                int lNormalCount  = lValueCount / 3;

                FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementNormal->GetDirectArray();
                lDirectArray.Resize( lNormalCount );

                const double* lSrc = lValues;

                FbxVector4 lNormal;
                int lNormalCounter;
                for(lNormalCounter = 0 ; lNormalCounter < lNormalCount  ; lNormalCounter ++, lSrc += 3 )
                {
                    memcpy(lNormal, lSrc, 3 * sizeof(double));
                    lDirectArray.SetAt(lNormalCounter, lNormal);
                }
                mFileObject->FieldReadEnd();
            
                if (lLayerElementVersion >= 102)
                {
                    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_NORMALS_WCOMPONENT))
                    {
                        const double* lValuesW = mFileObject->FieldReadArrayD(lValueCount);
						FBX_ASSERT(lValueCount == lNormalCount);
						if (lValueCount != lNormalCount)
							mStatus.SetCode(FbxStatus::eInvalidParameter, "Data array size mismatch (NormalsW)");

                        for(lNormalCounter = 0 ; lNormalCounter < lNormalCount  ; lNormalCounter++)
                        {
                            lNormal = lDirectArray.GetAt(lNormalCounter);
							if (lNormalCounter < lValueCount)
								lNormal.mData[3] = lValuesW[lNormalCounter];                            
                            lDirectArray.SetAt(lNormalCounter, lNormal);
                        }
                        mFileObject->FieldReadEnd();
                    }
                }
            }

            if (ConvertReferenceModeToken(lReferenceMode) != FbxLayerElement::eDirect)
            {
                ReadValueArray(FIELD_KFBXGEOMETRYMESH_NORMALS_INDEX, lLayerElementNormal->GetIndexArray());
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
bool FbxReaderFbx7_Impl::ReadLayerElementsTangent(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsTangent)
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
				int lValueCount;

				const double* lValues = mFileObject->FieldReadArrayD(lValueCount);
				int lTangentCount  = lValueCount / 3;

				FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementTangent->GetDirectArray();
				lDirectArray.Resize( lTangentCount );

				const double* lSrc = lValues;

				FbxVector4 lTangent;
                int lTangentCounter;
				for(lTangentCounter = 0 ; lTangentCounter < lTangentCount  ; lTangentCounter ++, lSrc += 3 )
				{
					memcpy(lTangent, lSrc, 3 * sizeof(double));
					lDirectArray.SetAt(lTangentCounter, lTangent);
				}
				mFileObject->FieldReadEnd();

                if (lLayerElementVersion >= 102)
                {
                    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_TANGENTS_WCOMPONENT))
                    {
                        const double* lValuesW = mFileObject->FieldReadArrayD(lValueCount);
						FBX_ASSERT(lValueCount == lTangentCount);
						if (lValueCount != lTangentCount)
							mStatus.SetCode(FbxStatus::eInvalidParameter, "Data array size mismatch (TangentsW)");

                        for(lTangentCounter = 0 ; lTangentCounter < lTangentCount  ; lTangentCounter++)
                        {
                            lTangent = lDirectArray.GetAt(lTangentCounter);
							if (lTangentCounter < lValueCount)
								lTangent.mData[3] = lValuesW[lTangentCounter];                            
                            lDirectArray.SetAt(lTangentCounter, lTangent);
                        }
                        mFileObject->FieldReadEnd();
                    }
                }
			}

            if (ConvertReferenceModeToken(lReferenceMode) != FbxLayerElement::eDirect)
            {
				ReadValueArray(FIELD_KFBXGEOMETRYMESH_TANGENTS_INDEX, lLayerElementTangent->GetIndexArray());
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
bool FbxReaderFbx7_Impl::ReadLayerElementsBinormal(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsBinormal)
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
				int lValueCount;

				const double* lValues = mFileObject->FieldReadArrayD(lValueCount);
				int lBinormalCount  = lValueCount / 3;

				FbxLayerElementArrayTemplate<FbxVector4>& lDirectArray = lLayerElementBinormal->GetDirectArray();
				lDirectArray.Resize( lBinormalCount );

				const double* lSrc = lValues;

				FbxVector4 lBinormal;
                int lBinormalCounter;
				for(lBinormalCounter = 0 ; lBinormalCounter < lBinormalCount  ; lBinormalCounter ++, lSrc += 3 )
				{
					memcpy(lBinormal, lSrc, 3 * sizeof(double));
					lDirectArray.SetAt(lBinormalCounter, lBinormal);
				}

                if (lLayerElementVersion >= 102)
                {
                    if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_BINORMALS_WCOMPONENT))
                    {
                        const double* lValuesW = mFileObject->FieldReadArrayD(lValueCount);
						FBX_ASSERT(lValueCount == lBinormalCount);
						if (lValueCount != lBinormalCount)
							mStatus.SetCode(FbxStatus::eInvalidParameter, "Data array size mismatch (BinormalsW)");

                        for(lBinormalCounter = 0 ; lBinormalCounter < lBinormalCount  ; lBinormalCounter++)
                        {
                            lBinormal = lDirectArray.GetAt(lBinormalCounter);
							if (lBinormalCounter < lValueCount)
								lBinormal.mData[3] = lValuesW[lBinormalCounter];                            
                            lDirectArray.SetAt(lBinormalCounter, lBinormal);
                        }
                        mFileObject->FieldReadEnd();
                    }
                }
				mFileObject->FieldReadEnd();
			}

			if (ConvertReferenceModeToken(lReferenceMode) != FbxLayerElement::eDirect)
			{
				ReadValueArray(FIELD_KFBXGEOMETRYMESH_BINORMALS_INDEX, lLayerElementBinormal->GetIndexArray());
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
bool FbxReaderFbx7_Impl::ReadLayerElementsVertexColor(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsVertexColor)
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
                lLayerElementVertexColor->SetName(lLayerName);
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementVertexColor->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementVertexColor->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));
            FBX_ASSERT(ConvertReferenceModeToken(lReferenceMode) != FbxLayerElement::eIndex) ;

            if (mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_VALUES))
            {
                int lValueCount;
                const double* lValues  = mFileObject->FieldReadArrayD(lValueCount);
                FbxLayerElementArrayTemplate<FbxColor>& lDirectArray = lLayerElementVertexColor->GetDirectArray();

                const double* lSrc     = lValues;
                int lVertexColorCount  = lValueCount / 4;

                for(int lVertexColorCounter = 0 ; lVertexColorCounter < lVertexColorCount  ; ++lVertexColorCounter)
                {
                    FbxColor lColor;
                    lColor.mRed     = *lSrc++;
                    lColor.mGreen   = *lSrc++;
                    lColor.mBlue    = *lSrc++;
                    lColor.mAlpha   = *lSrc++;
                    lDirectArray.Add(lColor);
                }
                mFileObject->FieldReadEnd();
            }

            if (lLayerElementVertexColor->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
            {
                ReadValueArray(FIELD_KFBXGEOMETRYMESH_VERTEX_COLOR_INDEX, lLayerElementVertexColor->GetIndexArray());
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
// Read layer element UV channel for geometry
//
bool FbxReaderFbx7_Impl::ReadLayerElementsChannelUV(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsUV, FbxLayerElement::EType pTextureType)
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
                lLayerElementUV->SetName(lLayerName);
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementUV->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementUV->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));

            if (mFileObject->FieldReadBegin(FIELD_KFBXLAYER_UV))
            {
                int lValueCount;
                const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

                int lUVCount  = lValueCount / 2 ;

                FbxLayerElementArrayTemplate<FbxVector2>& lDirectArray = lLayerElementUV->GetDirectArray();
                lDirectArray.Resize( lUVCount );

                const double* lSrc = lValues;
                FbxVector2 lUV;

                for(int lUVCounter = 0 ; lUVCounter < lUVCount  ; ++lUVCounter, lSrc += 2)
                {
                    memcpy(lUV.mData, lSrc, 2 * sizeof(double));
                    lDirectArray.SetAt(lUVCounter, lUV);
                }
                mFileObject->FieldReadEnd();
            }

            if (lLayerElementUV->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
            {
                ReadValueArray(FIELD_KFBXLAYER_UV_INDEX, lLayerElementUV->GetIndexArray());
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
bool FbxReaderFbx7_Impl::ReadLayerElementsPolygonGroup(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsPolygonGroup)
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
                lLayerElementPolygonGroup->SetName(lLayerName);
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementPolygonGroup->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementPolygonGroup->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));

            ReadValueArray(FIELD_KFBXGEOMETRYMESH_POLYGON_GROUP, lLayerElementPolygonGroup->GetIndexArray());
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
bool FbxReaderFbx7_Impl::ReadLayerElementsSmoothing(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsSmoothing)
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
                lLayerElementSmoothing->SetName(lLayerName);
                if(lLayerElementVersion >=102)
                    lIsIntArray=true;
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementSmoothing->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementSmoothing->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));
            FBX_ASSERT(lLayerElementSmoothing->GetReferenceMode() == FbxLayerElement::eDirect) ;

            if(lIsIntArray==true)
                ReadValueArray(FIELD_KFBXGEOMETRYMESH_SMOOTHING, lLayerElementSmoothing->GetDirectArray());
            else
            {
                //it means it was saved as a boolean.
                FbxLayerElementArrayTemplate<bool> lTemp(eFbxBool);
                ReadValueArray(FIELD_KFBXGEOMETRYMESH_SMOOTHING, lTemp);
                lLayerElementSmoothing->GetDirectArray().Resize(lTemp.GetCount());
                for(int i=0; i<lTemp.GetCount(); ++i)
                    lLayerElementSmoothing->GetDirectArray().SetAt(i,(int)lTemp.GetAt(i));
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
bool FbxReaderFbx7_Impl::ReadLayerElementsUserData(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsUserData)
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
			FbxDynamicArray<FbxString> lDataNamesList; 
            bool lAllDataTypesOk = true;
            while(mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYMESH_USER_DATA_ARRAY))
            {
                if(mFileObject->FieldReadBlockBegin())
                {
                    lDataNamesList.PushBack(mFileObject->FieldReadC(FIELD_KFBXGEOMETRYMESH_USER_DATA_NAME));
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
                continue;
            }

            FbxArray<const char*> lDataNames;    // Memory is not owned; we just point to the strings in the list above.
			for (int i=0, n=(int)lDataNamesList.Size(); i < n; i++)
				lDataNames.Add(lDataNamesList[i].Buffer());

            lLayerElementUserData = FbxLayerElementUserData::Create(pGeometry, "", lUserDataId, lDataTypes, lDataNames );

            // read version, name
            int lLayerElementVersion    = mFileObject->FieldReadI(FIELD_KFBXLAYER_ELEMENT_VERSION);
            if (lLayerElementVersion >= 101)
            {
                FbxString lLayerName = FbxObject::StripPrefix(mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_NAME));
                lLayerElementUserData->SetName(lLayerName);
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
                    int lArrayCount = 0;

                    switch( lLayerElementUserData->GetDataType(lDataIndex).GetType() )
                    {
                        case eFbxBool:    
                        {
                            lArrayCount = ReadValueArray(FIELD_KFBXGEOMETRYMESH_USER_DATA, FbxGetDirectArray<bool>(lLayerElementUserData,lDataIndex)); 
                            break;
                        }
                        case eFbxInt: 
                        {
                            lArrayCount = ReadValueArray(FIELD_KFBXGEOMETRYMESH_USER_DATA, FbxGetDirectArray<int>(lLayerElementUserData,lDataIndex));
                            break;
                        }
                        case eFbxFloat:   
                        {
                            lArrayCount = ReadValueArray(FIELD_KFBXGEOMETRYMESH_USER_DATA, FbxGetDirectArray<float>(lLayerElementUserData,lDataIndex));
                            break;
                        }
                        case eFbxDouble:
                        {
                            lArrayCount = ReadValueArray(FIELD_KFBXGEOMETRYMESH_USER_DATA, FbxGetDirectArray<double>(lLayerElementUserData,lDataIndex));
                            break;
                        }
                        default:
                        {
                            FBX_ASSERT(false);
                            break; // should not get here
                        }
                    }

                    // FBX6 does this resizing for every type, which is mighty strange.
                    lLayerElementUserData->ResizeAllDirectArrays(lArrayCount);

                    mFileObject->FieldReadBlockEnd();
                }
                mFileObject->FieldReadEnd();

                lDataIndex++;
            }

            // read the indices
            if (lLayerElementUserData->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
            {
                ReadValueArray(FIELD_KFBXGEOMETRYMESH_USER_DATA_INDEX, lLayerElementUserData->GetIndexArray());
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
bool FbxReaderFbx7_Impl::ReadLayerElementsVisibility(FbxGeometry* pGeometry, FbxArray<FbxLayerElement*>& pElementsVisibility)
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
                lLayerElementVisibility->SetName(lLayerName);
            }

            const char* lMappingMode   = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_MAPPING_INFO_TYPE);
            const char* lReferenceMode = mFileObject->FieldReadC(FIELD_KFBXLAYER_ELEMENT_REFERENCE_INFO_TYPE);

            lLayerElementVisibility->SetMappingMode(ConvertMappingModeToken(lMappingMode));
            lLayerElementVisibility->SetReferenceMode(ConvertReferenceModeToken(lReferenceMode));
            //FBX_ASSERT(lLayerElementVisibility->GetReferenceMode() == FbxLayerElement::eDirect) ;

            // read the direct array data
            ReadValueArray(FIELD_KFBXGEOMETRYMESH_VISIBILITY, lLayerElementVisibility->GetDirectArray());

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
bool FbxReaderFbx7_Impl::ReadLayerElementEdgeCrease(FbxGeometry*pGeometry, FbxArray<FbxLayerElement*>& pElementsEdgeCrease)
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
            ReadValueArray(FIELD_KFBXGEOMETRYMESH_EDGE_CREASE, lLayerElementCrease->GetDirectArray());

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
bool FbxReaderFbx7_Impl::ReadLayerElementVertexCrease(FbxGeometry*pGeometry, FbxArray<FbxLayerElement*>& pElementsVertexCrease)
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
            ReadValueArray(FIELD_KFBXGEOMETRYMESH_VERTEX_CREASE, lLayerElementCrease->GetDirectArray());

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
bool FbxReaderFbx7_Impl::ReadLayerElementHole(FbxGeometry*pGeometry, FbxArray<FbxLayerElement*>& pElementsHole)
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
            ReadValueArray(FIELD_KFBXGEOMETRYMESH_HOLE, lLayerElementHole->GetDirectArray());

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
bool FbxReaderFbx7_Impl::ReadGeometryLinks (FbxGeometry& pGeometry)
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
// This function is kept only for the compatibility with fbx files with old shape system before monument.
bool FbxReaderFbx7_Impl::ReadGeometryShapes(FbxGeometry& pGeometry)
{
	if (IOS_REF.GetBoolProp(IMP_FBX_SHAPE, true))
	{
		FbxString lBlendShapeName("");
		FbxBlendShape* lBlendShape = FbxBlendShape::Create(mSceneImport,"");

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

			//Extract blend shape deformer and channel name from shape name
			//Only need to extract once, all shapes of one blend shape deformer should have the same prefix name.
			//For example, Dino:bodyBlendShape.legFixL and Dino:bodyBlendShape.legFixR are two shapes on one geoemtry.
			//Dino:bodyBlendShape is the same, which will be blend shape deformer name.
			//legFixL and legFixR are the channels name.
			//This mostly happen in FBX files exported from Maya.
			if(lCounter == 0)
			{
				if (i != -1)
				{
					lBlendShapeName = lShapeName.Left(i);
				}
			}

			FbxShape* lShape = FbxShape::Create(mSceneImport, lShapeName);
			if(!lShape)
			{
				return false;
			}

			// Before monument, we only support one layer of shapes in parallel.
			// So put each shape on one separate blend shape channel to imitate this behavior.
			if (ReadShapeOld(*lShape, pGeometry))
			{
				FbxBlendShapeChannel* lBlendShapeChannel = FbxBlendShapeChannel::Create(mSceneImport,lBlendShapeChannelName);
				lBlendShape->AddBlendShapeChannel(lBlendShapeChannel);

				double lFullDeform = 100.0;
				if(!lBlendShapeChannel->AddTargetShape(lShape, lFullDeform))
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
bool FbxReaderFbx7_Impl::ReadNull(FbxNull& pNull)
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

//
// Read fbx marker
//
bool FbxReaderFbx7_Impl::ReadMarker(FbxMarker& pMarker)
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

    c[0] = lColor.mRed;
    c[1] = lColor.mGreen;
    c[2] = lColor.mBlue;
    pMarker.Color.Set(c);

    if (pMarker.GetType() == FbxMarker::eEffectorIK)
    {
        pMarker.SetDefaultIKReachTranslation(mFileObject->FieldReadD(FIELD_KFBXMARKER_IK_REACH_TRANSLATION, 0.0));
        pMarker.SetDefaultIKReachRotation(mFileObject->FieldReadD(FIELD_KFBXMARKER_IK_REACH_ROTATION, 0.0));
        pMarker.SetDefaultIKPull(mFileObject->FieldReadD(FIELD_KFBXMARKER_IK_PULL, 0.0));
        pMarker.SetDefaultIKPullHips(mFileObject->FieldReadD(FIELD_KFBXMARKER_IK_PULL_HIPS, 0.0));

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

        const char *lIKPull = "IK Pull";
        lFbxProperty = pMarker.FindProperty(lIKPull);
        if (!lFbxProperty.IsValid()) {
            lFbxProperty = FbxProperty::Create(&pMarker, FbxDoubleDT, lIKPull);
            lFbxProperty.ModifyFlag( FbxPropertyFlags::eAnimatable,true );
        }

        const char *lIKPullHips = "IK Pull Hips";
        lFbxProperty = pMarker.FindProperty(lIKPullHips);
        if (!lFbxProperty.IsValid()) {
            lFbxProperty = FbxProperty::Create(&pMarker, FbxDoubleDT, lIKPullHips);
            lFbxProperty.ModifyFlag( FbxPropertyFlags::eAnimatable,true );
        }
    }

    return true;
}

//
// Read fbx camera
//
bool FbxReaderFbx7_Impl::ReadCamera(FbxCamera& pCamera)
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
// Read fbx camera stereo
//
bool FbxReaderFbx7_Impl::ReadCameraStereo(FbxCameraStereo& pCameraStereo)
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
bool FbxReaderFbx7_Impl::ReadCameraStereoPrecomp(FbxCameraStereo& pCameraStereo)
{
    //Extract the precomp file
    FbxString lFileName = pCameraStereo.PrecompFileName.Get();
    FbxString lRelativeFileName = pCameraStereo.RelativePrecompFileName.Get();

    if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true)==false)
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
bool FbxReaderFbx7_Impl::ReadLight(FbxLight& pLight)
{
    return true;
}

//
// Read fbx mesh smoothness
//
bool FbxReaderFbx7_Impl::ReadMeshSmoothness(FbxMesh& pMesh)
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
bool FbxReaderFbx7_Impl::ReadMeshVertices(FbxMesh& pMesh)
{
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_VERTICES))
    {
        int lValueCount;
        const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

        int lPointCount = lValueCount / 3;
        pMesh.mControlPoints.Resize(lPointCount);

        const double* lSrc = lValues;

        for (int i = 0; i < lPointCount; ++i, lSrc += 3)
        {
            FbxVector4& lVector = pMesh.GetControlPoints()[i];

            memcpy(lVector.mData, lSrc, 3 * sizeof(double));
        }

        mFileObject->FieldReadEnd ();
    }

    return true;
}

//
// Read polygon index for mesh
//
bool FbxReaderFbx7_Impl::ReadMeshPolygonIndex(FbxMesh& pMesh)
{
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_POLYGON_INDEX))
    {
        int        lIndexCount;
        const int* lIndices = mFileObject->FieldReadArrayI(lIndexCount);

        if (lIndexCount > 0)
        {
            int lIndex;

            pMesh.BeginPolygon ();

            for( int lCount = 0; lCount < lIndexCount; lCount ++)
            {
                lIndex = lIndices[lCount];

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
bool FbxReaderFbx7_Impl::ReadMeshEdges(FbxMesh& pMesh)
{
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_EDGES))
    {
        int lIndexCount;
        const int* lIndices  = mFileObject->FieldReadArrayI(lIndexCount);

        if ((lIndexCount > 0) && (lIndexCount < pMesh.mPolygons.GetCount()))
        {
            pMesh.SetMeshEdgeCount( lIndexCount );

            for( int i = 0; i < lIndexCount; ++i )
            {
                pMesh.SetMeshEdge(i, lIndices[i]);
            }
        }

        mFileObject->FieldReadEnd();
    }

    return true;
}

#ifdef FBXSDK_SUPPORT_INTERNALEDGES
//
// Read edge data for mesh
//
bool FbxReaderFbx7_Impl::ReadMeshInternalEdges(FbxMesh& pMesh)
{
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYMESH_INTERNAL_EDGES))
    {
        int lIndexCount;
        const int* lIndices  = mFileObject->FieldReadArrayI(lIndexCount);

        if (lIndexCount > 0)
        {
            for( int i = 0; i < lIndexCount - 1; i+=2 )
            {
                pMesh.mInternalEdgeArray.Add(FbxMesh::InternalEdge(lIndices[i], lIndices[i+1]));
            }
        }

        mFileObject->FieldReadEnd();
    }

    return true;
}
#endif

//
// Read fbx mesh
//
bool FbxReaderFbx7_Impl::ReadMesh (FbxMesh& pMesh)
{
    int    lVersion = mFileObject->FieldReadI (FIELD_KFBXGEOMETRYMESH_GEOMETRY_VERSION, 0);

    ReadPropertiesAndFlags(&pMesh);

    ReadMeshSmoothness(pMesh);
    ReadMeshVertices(pMesh);
    ReadMeshPolygonIndex(pMesh);
    ReadMeshEdges(pMesh);
#ifdef FBXSDK_SUPPORT_INTERNALEDGES
    ReadMeshInternalEdges(pMesh);
#endif
    ReadLayerElements(pMesh);
    ReadGeometryLinks(pMesh);

	// This function is called only for the compatibility with fbx files with old shape system before monument.
    ReadGeometryShapes(pMesh);

    return true;
}

//
// Read fbx boundary
//
bool FbxReaderFbx7_Impl::ReadBoundary( FbxBoundary& pBoundary )
{
    int lVersion;

    // read in the version number
    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYBOUNDARY_VERSION);

    ReadPropertiesAndFlags(&pBoundary);

    return true;
}

//
// Read Line
//
bool FbxReaderFbx7_Impl::ReadLine( FbxLine& pLine )
{
    int lVersion;
    bool lReturn = true;

    // version number
    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYLINE_VERSION);

    //read control points
    if( lReturn && mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYLINE_POINTS) )
    {
        int lValueCount;
        const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

        if( lValueCount % 3 != 0 )
        {
            mStatus.SetCode(FbxStatus::eFailure, "Control point definition error (wrong number of data)");
            FBX_ASSERT_NOW("Control point definition error (wrong number of data).");
            lReturn = false;
        }
        else
        {
            int lPointCount = lValueCount / 3;
            pLine.mControlPoints.Resize(lPointCount);

            const double* lSrc = lValues;

            for (int i = 0; i < lPointCount; ++i, lSrc += 3)
            {
                FbxVector4& lVector = pLine.GetControlPoints()[i];

                memcpy(lVector.mData, lSrc, 3 * sizeof(double));
            }
        }

        mFileObject->FieldReadEnd ();
    }

    //read index array
    if (lReturn && mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYLINE_POINTS_INDEX))
    {
        int lIndexCount;
        const int* lIndices  = mFileObject->FieldReadArrayI(lIndexCount);

        if(pLine.GetIndexArray())
            pLine.GetIndexArray()->Clear();

        if (lIndexCount > 0)
        {
            int lIndex;
            pLine.SetIndexArraySize( lIndexCount );

            for( int i = 0; i < lIndexCount; ++i )
            {
                lIndex = lIndices[i];
                if(lIndex < 0)//end point
                {
                    lIndex = (FbxAbs (lIndex)) - 1;
                    pLine.AddEndPoint(i);
                }

                pLine.SetPointIndexAt(lIndex, i);
            }
        }

        mFileObject->FieldReadEnd ();
    }

    ReadPropertiesAndFlags( &pLine );

    return lReturn;
}

//
// Read trim NurbsSurface
//
bool FbxReaderFbx7_Impl::ReadTrimNurbsSurface( FbxTrimNurbsSurface& pNurbs )
{
    int lVersion;
    bool lFlipNormals;
    bool lReturn = true;

    // read in the version number
    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYTRIM_NURBS_SURFACE_VERSION);

    ReadPropertiesAndFlags(&pNurbs);

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
    //ReadGeometryLinks(pNurbs);

	// This function is called only for the compatibility with fbx files with old shape system before monument.
    //ReadGeometryShapes(pNurbs);

    return lReturn;
}

//
// Read NurbsCurve
//
bool FbxReaderFbx7_Impl::ReadNurbsCurve( FbxNurbsCurve& pNurbs )
{
    int lVersion, lOrder, lDimension;
    bool lIsRational, lReturn = true;
    const char* lLine;
    FbxNurbsCurve::EType lType = (FbxNurbsCurve::EType)-1;

    // version number
    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYNURBS_CURVE_VERSION);

    ReadPropertiesAndFlags(&pNurbs);

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
            mStatus.SetCode(FbxStatus::eFailure, "Type of nurbs curve unknown (invalid data)");
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

    if( lReturn && mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYNURBS_CURVE_POINTS) )
    {
        int lValueCount;
        const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

        if( lValueCount % 4 != 0 )
        {
            mStatus.SetCode(FbxStatus::eFailure, "Control point definition error (wrong number of data)");
            FBX_ASSERT_NOW("Control point definition error (wrong number of data).");
            lReturn = false;
        }
        else
        {
            lValueCount /= 4;
            pNurbs.InitControlPoints(lValueCount, lType);

            const double* lSrc = lValues;

            for (int i=0; i< lValueCount; ++i, lSrc += 4)
            {
                FbxVector4& lVector = pNurbs.GetControlPoints()[i];
                memcpy(lVector.mData, lSrc, 4 * sizeof(double));

                if (lVector[3] <= 0.00001)
                {
                    mStatus.SetCode(FbxStatus::eFailure, "Weight must be greater than 0 (invalid data)");
                    FBX_ASSERT_NOW("Weight must be greater than 0 (invalid data).");
                    lReturn = false;
                }
            }
        }

        mFileObject->FieldReadEnd ();
    }

    if (lReturn && mFileObject->FieldReadBegin(FIELD_KFBXGEOMETRYNURBS_CURVE_KNOTVECTOR))
    {
        int lCount;
        const double* lValues = mFileObject->FieldReadArrayD(lCount);

        if( lCount != pNurbs.GetKnotCount() )
        {
            mStatus.SetCode(FbxStatus::eFailure, "Knot vector definition error (wrong number of data)");
            FBX_ASSERT_NOW("Knot vector definition error (wrong number of data).");
            lReturn = false;
        }
        else
        {
            memcpy(pNurbs.GetKnotVector(), lValues, lCount * sizeof(double));
        }

        mFileObject->FieldReadEnd ();
    }

    ReadPropertiesAndFlags( &pNurbs );

    return lReturn;
}

//
// Read FbxNurbs
//
bool FbxReaderFbx7_Impl::ReadNurb(FbxNurbs& pNurbs)
{
    int            lVersion,U,V;
    bool            Return = true;
    int            TotalCount;
    double*        Points;

    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYNURB_NURB_VERSION);

    ReadPropertiesAndFlags(&pNurbs);

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
            mStatus.SetCode(FbxStatus::eFailure, "Type of nurb unknown (invalid data)");
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
            mStatus.SetCode(FbxStatus::eFailure, "Type of nurb unknown (invalid data)");
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

        int lValueCount;
        const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

        if(lValueCount != (TotalCount*4))
        {
            mStatus.SetCode(FbxStatus::eFailure, "Control point definition error (wrong number of data)");
            FBX_ASSERT_NOW("Control point definition error (wrong number of data).");
            Return = false;
        }
        else
        {
            const double* lSrc = lValues;

            for (int i=0; i<TotalCount; ++i, lSrc += 4)
            {
                FbxVector4& lVector = pNurbs.GetControlPoints()[i];
                memcpy(lVector.mData, lSrc, 4 * sizeof(double));

                if (lVector[3] <= 0.00001)
                {
                    mStatus.SetCode(FbxStatus::eFailure, "Weight must be greater than 0 (invalid data)");
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

        int lValueCount;
        const int* lValues = mFileObject->FieldReadArrayI(lValueCount);

        if (lValueCount != TotalCount)
        {
            mStatus.SetCode(FbxStatus::eFailure, "Multiplicity U definition error (wrong number of data)");
            FBX_ASSERT_NOW("Multiplicity U definition error (wrong number of data).");
            Return = false;
        }
        else
        {
            memcpy(Pts, lValues, TotalCount * sizeof(int));
        }

        mFileObject->FieldReadEnd ();
    }

    //
    // MULTIPLICITY_V...
    //
    if (mFileObject->FieldReadBegin (FIELD_KFBXGEOMETRYNURB_MULTIPLICITY_V))
    {
        TotalCount  = pNurbs.GetVCount ();
        int *Pts= pNurbs.GetVMultiplicityVector ();

        int lValueCount;
        const int* lValues = mFileObject->FieldReadArrayI(lValueCount);

        if(lValueCount != TotalCount)
        {
            mStatus.SetCode(FbxStatus::eFailure, "Multiplicity V definition error (wrong number of data)");
            FBX_ASSERT_NOW("Multiplicity V definition error (wrong number of data).");
            Return = false;
        }
        else
        {
            memcpy(Pts, lValues, TotalCount * sizeof(int));
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

        int lValueCount;
        const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

        if (lValueCount != TotalCount)
        {
            mStatus.SetCode(FbxStatus::eFailure, "U knot vector definition error (wrong number of data)");
            FBX_ASSERT_NOW("U knot vector definition error (wrong number of data).");
            Return = false;
        }
        else
        {
            memcpy(Points, lValues, TotalCount * sizeof(double));
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

        int lValueCount;
        const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

        if (lValueCount != TotalCount)
        {
            mStatus.SetCode(FbxStatus::eFailure, "V knot vector definition error (wrong number of data)"); 
            FBX_ASSERT_NOW("V knot vector definition error (wrong number of data).");
            Return = false;
        }
        else
        {
            memcpy(Points, lValues, TotalCount * sizeof(double));
        }

        mFileObject->FieldReadEnd();
    }

    ReadLayerElements(pNurbs);
    ReadGeometryLinks(pNurbs);

	// This function is called only for the compatibility with fbx files with old shape system before monument.
    ReadGeometryShapes(pNurbs);

    return Return;
}

//
// Read NurbsSurface
//
bool FbxReaderFbx7_Impl::ReadNurbsSurface(FbxNurbsSurface& pNurbs)
{
    int            lVersion,U,V;
    bool            Return = true;
    int            TotalCount;
    double*        Points;

    lVersion = mFileObject->FieldReadI(FIELD_KFBXGEOMETRYNURBS_SURFACE_NURB_VERSION);

    ReadPropertiesAndFlags(&pNurbs);

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
            mStatus.SetCode(FbxStatus::eFailure, "Type of nurb unknown (invalid data)");
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
            mStatus.SetCode(FbxStatus::eFailure, "Type of nurb unknown (invalid data)");
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

        int lValueCount;
        const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

        if(lValueCount != (TotalCount*4))
        {
            mStatus.SetCode(FbxStatus::eFailure, "Control point definition error (wrong number of data)");
            FBX_ASSERT_NOW("Control point definition error (wrong number of data).");
            Return = false;
        }
        else
        {
            const double* lSrc = lValues;

            for (int i = 0; i < TotalCount; ++i, lSrc += 4)
            {
                FbxVector4& lVector = pNurbs.GetControlPoints()[i];

                memcpy(lVector.mData, lSrc, 4 * sizeof(double));

                if (lVector[3] <= 0.00001)
                {
                    mStatus.SetCode(FbxStatus::eFailure, "Weight must be greater than 0 (invalid data)");
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

        int lValueCount;
        const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

        if (lValueCount != TotalCount)
        {
            mStatus.SetCode(FbxStatus::eFailure, "U knot vector definition error (wrong number of data)");
            FBX_ASSERT_NOW("U knot vector definition error (wrong number of data).");
            Return = false;
        }
        else
        {
            memcpy(Points, lValues, lValueCount * sizeof(double));
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

        int lValueCount;
        const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

        if (lValueCount != TotalCount)
        {
            mStatus.SetCode(FbxStatus::eFailure, "V knot vector definition error (wrong number of data)");
            FBX_ASSERT_NOW("V knot vector definition error (wrong number of data).");
            Return = false;
        }
        else
        {
            memcpy(Points, lValues, lValueCount * sizeof(double));
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

	// This function is called only for the compatibility with fbx files with old shape system before monument.
    ReadGeometryShapes(pNurbs);

    ReadPropertiesAndFlags( &pNurbs );

    return Return;
}

//
// Read Fbx path
//
bool FbxReaderFbx7_Impl::ReadPatch(FbxPatch& pPatch)
{
    int    Version;
    int    IU, IV;
    bool    BU, BV;
    FbxPatch::EType lUType = FbxPatch::eLinear, lVType = FbxPatch::eLinear;

    Version = mFileObject->FieldReadI (FIELD_KFBXGEOMETRYPATCH_PATCH_VERSION, 100);

    ReadPropertiesAndFlags(&pPatch);

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
        int lValueCount;
        const double* lValues = mFileObject->FieldReadArrayD(lValueCount);
        const double* lSrc = lValues;

        int lCopyPoint = FbxMin((lValueCount / 3), pPatch.GetControlPointsCount());

        for( int i = 0; i < lCopyPoint; ++i, lSrc += 3)
        {
            FbxVector4& lVector = pPatch.GetControlPoints()[i];
            memcpy(lVector.mData, lSrc, 3 * sizeof(double));
            lVector[3]  = 1.0;
        }

        // fbx6 zeroes out extra points; let's do the same...
        int lOverhangPoint = pPatch.GetControlPointsCount() - lCopyPoint;

        if( lOverhangPoint > 0 )
        {
            FbxVector4 lVector(0, 0, 0, 1.0);

            for( int i = 0, j = lCopyPoint; i < lOverhangPoint; ++i, ++j )
            {
                pPatch.GetControlPoints()[j] = lVector;
            }  
        }

        mFileObject->FieldReadEnd();
    }

    ReadLayerElements(pPatch);
    ReadGeometryLinks(pPatch);

	// This function is called only for the compatibility with fbx files with old shape system before monument.
    ReadGeometryShapes(pPatch);

    return true;
}

//
// Read patch type
//
int FbxReaderFbx7_Impl::ReadPatchType(FbxPatch& pPatch)
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
// Read shapes. 
// Shapes only take geometry info of control points that are different from base geometry.
// The complete geometry info of the shape will be calculated by combining with base geometry
// in post-process part of Read process by RebuildTargetShapes.
//
bool FbxReaderFbx7_Impl::ReadShapeOld(FbxShape& pShape, FbxGeometry &pGeometry)
{
	if ( mFileObject->FieldReadBlockBegin () )
	{
		//
		// Read the indices.
		//
		if ( mFileObject->FieldReadBegin ( FIELD_KFBXSHAPE_INDEXES ) )
		{
			int lIndicesCount;
			const int* lIndices = mFileObject->FieldReadArrayI(lIndicesCount);

			// We must copy into the other array, as the buffer returned is a temporary.
			pShape.SetControlPointIndicesCount(lIndicesCount);
			memcpy(pShape.GetControlPointIndices(), lIndices, lIndicesCount * sizeof(int));

			pShape.InitControlPoints(lIndicesCount);
			pShape.InitNormals(lIndicesCount);

			mFileObject->FieldReadEnd ();
		}

		//
		// Read the control points.
		//
		if ( mFileObject->FieldReadBegin ( FIELD_KFBXSHAPE_VERTICES ) )
		{
			int lValueCount;
			const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

			int lTotalCount = lValueCount / 3;
			for (int i = 0; i < lTotalCount; ++i, lValues += 3)
			{
				FbxVector4& lVector = pShape.GetControlPoints()[i];             
				lVector[0] = lValues[0];
				lVector[1] = lValues[1];
				lVector[2] = lValues[2];
			}

			mFileObject->FieldReadEnd ();
		}

		//
		// Read the normals.
		//
		if ( mFileObject->FieldReadBegin ( FIELD_KFBXSHAPE_NORMALS ) )
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

			int lValueCount;
			const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

			int lTotalCount = lValueCount / 3;
			for (int i = 0; i < lTotalCount; ++i, lValues += 3)
			{
				FbxVector4 lVector = lNormals.GetAt(i);
				lVector[0] = lValues[0];
				lVector[1] = lValues[1];
				lVector[2] = lValues[2];

				lNormals.SetAt(i, lVector);
			}
			mFileObject->FieldReadEnd ();
		}

		mFileObject->FieldReadBlockEnd();
	}

	return true;
}

//
// Read shapes. 
// Shapes only take geometry info of control points that are different from base geometry.
// The complete geometry info of the shape will be calculated by combining with base geometry
// in post-process part of Read process by RebuildTargetShapes.
//
bool FbxReaderFbx7_Impl::ReadShape(FbxShape& pShape)
{
	int lVersion = mFileObject->FieldReadI (FIELD_KFBXSHAPE_VERSION, 100);

	//
	// Read the indices.
	//
	if ( mFileObject->FieldReadBegin ( FIELD_KFBXSHAPE_INDEXES ) )
	{
		int lIndicesCount;
		const int* lIndices = mFileObject->FieldReadArrayI(lIndicesCount);

		// We must copy into the other array, as the buffer returned is a temporary.
		pShape.SetControlPointIndicesCount(lIndicesCount);
		memcpy(pShape.GetControlPointIndices(), lIndices, lIndicesCount * sizeof(int));

		pShape.InitControlPoints(lIndicesCount);
		pShape.InitNormals(lIndicesCount);

		mFileObject->FieldReadEnd ();
	}

	//
	// Read the control points.
	//
	if ( mFileObject->FieldReadBegin ( FIELD_KFBXSHAPE_VERTICES ) )
	{
		int lValueCount;
		const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

		int lTotalCount = lValueCount / 3;
		FBX_ASSERT(lTotalCount <= pShape.GetControlPointsCount());
		if (lTotalCount > pShape.GetControlPointsCount())
		{
			mStatus.SetCode(FbxStatus::eInvalidParameter, "Invalid parameter while reading shape vertices");
			mFileObject->FieldReadEnd();
			return false;
		}

		for (int i = 0; i < lTotalCount; ++i, lValues += 3)
		{
			FbxVector4& lVector = pShape.GetControlPoints()[i];
			// drop precision errors
			lVector.Set(0, 0, 0, 0);
			if (!FbxEqual(lValues[0], 0.0)) lVector[0] = lValues[0];
			if (!FbxEqual(lValues[1], 0.0)) lVector[1] = lValues[1];
			if (!FbxEqual(lValues[2], 0.0)) lVector[2] = lValues[2];
		}
		
		mFileObject->FieldReadEnd ();
	}

	//
	// Read the normals.
	//
	if ( mFileObject->FieldReadBegin ( FIELD_KFBXSHAPE_NORMALS ) )
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

		int lValueCount;
		const double* lValues = mFileObject->FieldReadArrayD(lValueCount);

		int lTotalCount = lValueCount / 3;
		for (int i = 0; i < lTotalCount; ++i, lValues += 3)
		{
			FbxVector4 lVector = lNormals.GetAt(i);
			// drop precision errors
			lVector.Set(0,0,0,0);
			if (!FbxEqual(lValues[0], 0.0)) lVector[0] = lValues[0];
			if (!FbxEqual(lValues[1], 0.0)) lVector[1] = lValues[1];
			if (!FbxEqual(lValues[2], 0.0)) lVector[2] = lValues[2];

			lNormals.SetAt(i, lVector);
		}
		mFileObject->FieldReadEnd ();
	}

	ReadPropertiesAndFlags( &pShape );

	return true;
}

//
// Read texture
//
bool FbxReaderFbx7_Impl::ReadFileTexture(FbxFileTexture& pTexture)
{
    //
    // Read the name
    //
    if ( mFileObject->FieldReadBegin( FIELD_KFBXTEXTURE_TEXTURE_NAME ) )
    {
        FbxString lString = FbxObject::StripPrefix(mFileObject->FieldReadC());
        pTexture.SetName(lString);
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
        pTexture.SetMediaName(lString);
        mFileObject->FieldReadEnd();
    }

    ReadPropertiesAndFlags(&pTexture);

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
bool FbxReaderFbx7_Impl::ReadThumbnail(FbxThumbnail& pThumbnail)
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

                ReadValueArray(lSize, lImagePtr);

                mFileObject->FieldReadEnd();
            }

            lImageRead = true;
        }
    }

    lImageRead &= ReadPropertiesAndFlags(&pThumbnail);

    return lImageRead;
}

//
// Read surface material
//
FbxSurfaceMaterial* FbxReaderFbx7_Impl::ReadSurfaceMaterial(
    const char*             pObjectName,
    const char*             pMaterialType,
    FbxSurfaceMaterial*    pReferencedMaterial
)
{
    FbxSurfaceMaterial* lMaterial = NULL;
    FbxString           lShadingModel(pMaterialType);

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
                lClassId = mManager.FindClass(pMaterialType);
                if( lClassId.IsValid())
                {
                    lMaterial = FbxCast<FbxSurfaceMaterial>( mManager.CreateNewObjectFromClassId(lClassId, pObjectName) );        
                }
                else
                {
                    lMaterial = FbxSurfaceMaterial::Create(&mManager, pObjectName);
                    lMaterial->ShadingModel.Set(lShadingModel);
                }
            }
        }
    }

	// Check in the case the allocation or the cast to FbxSurfaceMaterial failed and bail out
	if (!lMaterial)
		return lMaterial;

    lMaterial->MultiLayer.Set(mFileObject->FieldReadI(FIELD_KFBXMATERIAL_MULTI_LAYER) != 0);

    ReadPropertiesAndFlags(lMaterial);

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
bool FbxReaderFbx7_Impl::ReadLayeredTexture( FbxLayeredTexture& pTex )
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXLAYEREDTEXTURE_VERSION, 100);

    mFileObject->FieldReadBegin(FIELD_KFBXLAYEREDTEXTURE_BLENDMODES);

    int lCount = mFileObject->FieldReadGetCount();
    pTex.mInputData.Resize( lCount );
	FBX_ASSERT(pTex.mInputData.Size() == lCount);
	if (pTex.mInputData.Size() == lCount)
	{
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
	}
    mFileObject->FieldReadEnd();


    if (mFileObject->FieldReadBegin(FIELD_KFBXLAYEREDTEXTURE_ALPHAS))
    {
        int lAlphaCount = mFileObject->FieldReadGetCount();

        FBX_ASSERT(lAlphaCount == pTex.mInputData.GetCount());
		// Prevent index out of bound in case of corrupted file
		if (lAlphaCount > pTex.mInputData.GetCount())
		{
            mStatus.SetCode(FbxStatus::eInvalidParameter, "Invalid parameter while reading layered texture Alphas");
			lAlphaCount = pTex.mInputData.GetCount();
		}

        for( int i = 0; i < lAlphaCount; ++i )
        {
            double lAlpha = mFileObject->FieldReadD();
            lAlpha = FbxMin(FbxMax(lAlpha, 0.0), 1.0);

            pTex.mInputData[i].mAlpha = lAlpha;
        }

        mFileObject->FieldReadEnd();
    }


    return ReadPropertiesAndFlags(&pTex);
}

//
// Read ProceduralTexture
//
bool FbxReaderFbx7_Impl::ReadProceduralTexture( FbxProceduralTexture& pTex )
{
    return ReadPropertiesAndFlags(&pTex);
}


//
// Read FbxImplementation
//
bool FbxReaderFbx7_Impl::ReadImplementation( FbxImplementation& pImplementation )
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXIMPLEMENTATION_VERSION, 100);

    return ReadPropertiesAndFlags(&pImplementation);
}

//
// Read collection
//
bool FbxReaderFbx7_Impl::ReadCollection( FbxCollection& pCollection )
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXIMPLEMENTATION_VERSION, 100);

    return ReadPropertiesAndFlags(&pCollection);
}

bool FbxReaderFbx7_Impl::ReadSelectionNode(FbxSelectionNode& pSelectionNode)
{
    mFileObject->FieldReadI(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_SELECTIONNODE, 100);    

    ReadPropertiesAndFlags(&pSelectionNode);

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

bool FbxReaderFbx7_Impl::ReadSelectionSet(FbxSelectionSet& pSelectionSet)
{
    bool lResult = true;

    mFileObject->FieldReadI(FIELD_KFBXCOLLECTION_COLLECTION, 100);

    // Read the properties
    ReadPropertiesAndFlags(&pSelectionSet);

    int nbNodes = mFileObject->FieldReadI("NbSelectionNodes");

    return lResult;
}
//
// Read document
//
bool FbxReaderFbx7_Impl::ReadDocument(FbxDocument& pSubDocument)
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXDOCUMENT_VERSION, 100);

    return ReadPropertiesAndFlags(&pSubDocument);
}

//
// Read binding operator
//
bool FbxReaderFbx7_Impl::ReadBindingOperator( FbxBindingOperator& pOperator )
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXBINDINGOPERATOR_VERSION, 100);

    bool lSuccess = ReadPropertiesAndFlags(&pOperator);

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
bool FbxReaderFbx7_Impl::ReadBindingTable( FbxBindingTable& pBindingTable )
{
    const int lVersion = mFileObject->FieldReadI(FIELD_KFBXBINDINGTABLE_VERSION, 100);

    bool lSuccess = ReadPropertiesAndFlags(&pBindingTable);

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

    return lSuccess;
}

//
// Read cache
//
bool FbxReaderFbx7_Impl::ReadCache( FbxCache& pCache )
{
    mFileObject->FieldReadI(FIELD_KFBXCACHE_VERSION, 100);

    ReadPropertiesAndFlags(&pCache);

    // Update the absolute path is necessary
    FbxString lRelativeFileName, lAbsoluteFileName;
    pCache.GetCacheFileName(lRelativeFileName, lAbsoluteFileName);

    if (!FbxFileUtils::Exist(lAbsoluteFileName))
    {
        // Try to construct an absolute path from the relative path
        FbxString lFBXDirectory = mFileObject->GetFullPath("");

        if (lFBXDirectory.GetLen() == 0 || FbxPathUtils::IsRelative(lFBXDirectory))
        {
            lFBXDirectory = FbxPathUtils::GetFolderName(FbxPathUtils::Resolve(lFBXDirectory));
        }

        FbxString lTentativePath = lFBXDirectory + FbxString("/") + lRelativeFileName;
        lTentativePath = FbxPathUtils::Clean(lTentativePath);

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
bool FbxReaderFbx7_Impl::ReadVideo(FbxVideo& pVideo)
{
    FbxVideo* lReferencedVideo = FbxCast<FbxVideo>(pVideo.GetReferenceTo());
    if (lReferencedVideo != NULL)
    {
        return ReadPropertiesAndFlags(&pVideo);
    }

    ReadPropertiesAndFlags(&pVideo);

    pVideo.ImageTextureSetMipMap(mFileObject->FieldReadB (FIELD_KFBXVIDEO_USEMIPMAP) );

    FbxString lFileName, lRelativeFileName;
    lFileName = mFileObject->FieldReadC (FIELD_MEDIA_FILENAME);
    lFileName = pVideo.GetFileName();
    lRelativeFileName = mFileObject->FieldReadC (FIELD_MEDIA_RELATIVE_FILENAME);

    pVideo.SetOriginalFormat(true);
    pVideo.SetOriginalFilename(lFileName.Buffer());

    // If this field exist, the media is embedded.
    bool lSkipValidation = true;
    if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true))
    {
        lSkipValidation = false;
        if( mFileObject->FieldReadBegin(FIELD_MEDIA_CONTENT) )
        {
            FbxString lDefaultPath = "";
            FbxString lUserDefinePath = IOS_REF.GetStringProp(IMP_EXTRACT_FOLDER, lDefaultPath);
            const char* lUserDefinePathBuffer = (lUserDefinePath == lDefaultPath) ? NULL : lUserDefinePath.Buffer();
            bool lStatus = mFileObject->FieldReadEmbeddedFile (lFileName, lRelativeFileName, mFileObject->GetMediaDirectory(true, lUserDefinePathBuffer));
            mFileObject->FieldReadEnd();
        }
    }

    pVideo.SetFileName(lFileName);
    pVideo.SetRelativeFileName(lRelativeFileName);

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

//
// Read properties and flags for fbx container
//
bool FbxReaderFbx7_Impl::ReadContainer(FbxContainer& pContainer)
{
    bool lStatus = true;
    mFileObject->FieldReadI(FIELD_KFBXCONTAINER_VERSION, 100);

    // Read the properties
    ReadPropertiesAndFlags(&pContainer);

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
            pContainer.mContainerTemplate->ContainerTemplatePath.Set(lOriginalFileName);

            //Parse the template file to extract the extend template files
            //pContainer.mContainerTemplate->SetTemplatePath(lOriginalFileName.Buffer());
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
// Read GemetryWeightMap
//
bool FbxReaderFbx7_Impl::ReadGeometryWeightedMap(FbxGeometryWeightedMap& pGeometryWeightedMap)
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
bool FbxReaderFbx7_Impl::ReadLink(FbxCluster& pLink)
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
			ReadValueArray(FIELD_KFBXLINK_TRANSFORM, 16, (double*)lMatrix.mData);
			pLink.SetTransformMatrix(*(FbxAMatrix*)&lMatrix);
		}

        //
        // Read the TRANSFORM LINK matrix...
        //
        if ( mFileObject->FieldReadBegin ( FIELD_KFBXLINK_TRANSFORM_LINK ) )
        {
            FbxMatrix lMatrix;

            ReadValueArray(16, (double*) lMatrix.mData);

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

                    ReadValueArray(16, (double*) lMatrix.mData);

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

            ReadValueArray(16, (double*) lMatrix.mData);

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
bool FbxReaderFbx7_Impl::ReadSkin(FbxSkin& pSkin)
{
    int lVersion = mFileObject->FieldReadI(FIELD_KFBXSKIN_VERSION, 100);
    //pSkin.SetMultiLayer(mFileObject->FieldReadI(FIELD_KFBXDEFORMER_MULTI_LAYER) != 0);

    ReadPropertiesAndFlags(&pSkin);

	if (mFileObject->FieldReadBegin(FIELD_KFBXSKIN_DEFORM_ACCURACY))
	{
		pSkin.SetDeformAccuracy(mFileObject->FieldReadD());
		mFileObject->FieldReadEnd();
	}
	
	if (lVersion >= 101)
	{
		//
		// Read the skinning type
		//
		pSkin.SetSkinningType(FbxSkin::eRigid);

		if (mFileObject->FieldReadBegin(FIELD_KFBXSKIN_SKINNINGTYPE))
		{
			FbxString lSkinningTypeStr = mFileObject->FieldReadC();

			if (lSkinningTypeStr.CompareNoCase(TOKEN_KFBXSKIN_LINEAR) == 0)
			{
				pSkin.SetSkinningType(FbxSkin::eLinear);
			}
			else if (lSkinningTypeStr.CompareNoCase(TOKEN_KFBXSKIN_DUALQUATERNION) == 0)
			{
				pSkin.SetSkinningType(FbxSkin::eDualQuaternion);
			}
			else if(lSkinningTypeStr.CompareNoCase(TOKEN_KFBXSKIN_BLEND) == 0)
			{
				pSkin.SetSkinningType(FbxSkin::eBlend);
			}

			mFileObject->FieldReadEnd ();
		}

		if(pSkin.GetSkinningType() == FbxSkin::eBlend)
		{
			//
			// Read the skin INDICES...
			//
			int lPointCount = 0;
			if (mFileObject->FieldReadBegin(FIELD_KFBXSKIN_INDEXES))
			{
				const int* lIndices = mFileObject->FieldReadArrayI(lPointCount);
				pSkin.SetControlPointIWCount(lPointCount);
				memcpy(pSkin.GetControlPointIndices(), lIndices, lPointCount * sizeof(int));
				mFileObject->FieldReadEnd ();
			}

			//
			// Read the BLENDWEIGHTS...
			//
			ReadValueArray(FIELD_KFBXSKIN_BLENDWEIGHTS, lPointCount, pSkin.GetControlPointBlendWeights());
		}
	}
    return true;
}

//
// Read vertex cache deformer
//
bool FbxReaderFbx7_Impl::ReadVertexCacheDeformer(FbxVertexCacheDeformer& pDeformer)
{
    /*int lVersion*/ mFileObject->FieldReadI(FILED_KFBXVERTEXCACHEDEFORMER_VERSION, 100);
    //pDeformer.SetMultiLayer(mFileObject->FieldReadI(FIELD_KFBXDEFORMER_MULTI_LAYER) != 0);

    ReadPropertiesAndFlags(&pDeformer);

    return true;
}

//
// Read Cluster
//
bool FbxReaderFbx7_Impl::ReadCluster(FbxCluster& pCluster)
{
    /*int lVersion*/ mFileObject->FieldReadI(FIELD_KFBXCLUSTER_VERSION, 100);
    //pCluster.SetMultiLayer(mFileObject->FieldReadI(FIELD_KFBXDEFORMER_MULTI_LAYER) != 0);

    ReadPropertiesAndFlags(&pCluster);

    //
    // Read The Link MODE...
    //
    pCluster.SetLinkMode(FbxCluster::eNormalize);

    if (mFileObject->FieldReadBegin(FIELD_KFBXCLUSTER_MODE))
    {
        FbxString lModeStr = mFileObject->FieldReadC();

        if (lModeStr.CompareNoCase(TOKEN_KFBXCLUSTER_ADDITIVE) == 0)
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
    if (mFileObject->FieldReadBegin(FIELD_KFBXCLUSTER_USERDATA))
    {
		FbxString UserDataID = mFileObject->FieldReadC();
		FbxString UserData = mFileObject->FieldReadC();
		pCluster.SetUserData(UserDataID.Buffer(), UserData.Buffer());
        mFileObject->FieldReadEnd ();
    }

    //
    // Read the Link INDICES...
    //
    int lPointCount = 0;
    if (mFileObject->FieldReadBegin(FIELD_KFBXCLUSTER_INDEXES))
    {
        const int* lIndices = mFileObject->FieldReadArrayI(lPointCount);
		pCluster.SetControlPointIWCount(lPointCount);
        memcpy(pCluster.GetControlPointIndices(), lIndices, lPointCount * sizeof(int));
        mFileObject->FieldReadEnd ();
    }

    //
    // Read the Link WEIGHTS...
    //
    ReadValueArray(FIELD_KFBXCLUSTER_WEIGHTS, lPointCount, pCluster.GetControlPointWeights());

    //
    // Read the TRANSFORM matrix...
    //
	FbxMatrix Transform;
	Transform.SetIdentity();
    ReadValueArray(FIELD_KFBXCLUSTER_TRANSFORM, 16, (double*)Transform.mData);

    //
    // Read the TRANSFORM LINK matrix...
    //
	FbxMatrix TransformLink;
    TransformLink.SetIdentity();
    ReadValueArray(FIELD_KFBXLINK_TRANSFORM_LINK, 16, (double*)TransformLink.mData);

    // Make sure it is affine
	Transform = TransformLink * Transform;
	pCluster.SetTransformMatrix(*(FbxAMatrix*)&Transform);
	pCluster.SetTransformLinkMatrix(*(FbxAMatrix*)&TransformLink);

    //
    // Read the ASSOCIATE MODEL...
    //
    if (mFileObject->FieldReadBegin(FIELD_KFBXCLUSTER_ASSOCIATE_MODEL))
    {
        if (mFileObject->FieldReadBlockBegin())
        {
            if (mFileObject->FieldReadBegin(FIELD_KFBXCLUSTER_TRANSFORM))
            {
				FbxMatrix TransformAssociate;
                ReadValueArray(16, (double*)TransformAssociate.mData);

				TransformAssociate = TransformLink * TransformAssociate;
				pCluster.SetTransformAssociateModelMatrix(*(FbxAMatrix*)&TransformAssociate);

                mFileObject->FieldReadEnd ();
            }

            mFileObject->FieldReadBlockEnd ();
        }
        mFileObject->FieldReadEnd ();
    }

    if (mFileObject->FieldReadBegin(FIELD_KFBXCLUSTER_TRANSFORM_PARENT))
    {
		FbxMatrix TransformParent;
        ReadValueArray(16, (double*)TransformParent.mData);
		pCluster.SetTransformParentMatrix(*(FbxAMatrix*)&TransformParent);
        mFileObject->FieldReadEnd ();
    }

    return true;
}

//
// Read blend shape deformer
//
bool FbxReaderFbx7_Impl::ReadBlendShape(FbxBlendShape& pBlendShape)
{
	int lVersion = mFileObject->FieldReadI(FIELD_KFBXBLENDSHAPE_VERSION, 100);
	ReadPropertiesAndFlags(&pBlendShape);

	return true;
}

//
// Read blend shape channels
//
bool FbxReaderFbx7_Impl::ReadBlendShapeChannel(FbxBlendShapeChannel& pBlendShapeChannel)
{
	int lVersion = mFileObject->FieldReadI(FIELD_KFBXBLENDSHAPECHANNEL_VERSION, 100);
	ReadPropertiesAndFlags(&pBlendShapeChannel);

	bool lResult = true;

	//
	// Read the Full Weights...
	//
	int lTargetShapeCount = 0;
	if (mFileObject->FieldReadBegin(FIELD_KFBXBLENDSHAPECHANNEL_FULLWEIGHTS))
	{
		const double* lFullWeights = mFileObject->FieldReadArrayD(lTargetShapeCount);
		pBlendShapeChannel.SetFullWeightsCount(lTargetShapeCount);
		memcpy(pBlendShapeChannel.GetTargetShapeFullWeights(), lFullWeights, lTargetShapeCount * sizeof(double));
		mFileObject->FieldReadEnd ();
	}

	return true;
}

//
// Read gloabl settings
//
bool FbxReaderFbx7_Impl::ReadGlobalSettings(FbxGlobalSettings& pGlobalSettings)
{
    mFileObject->FieldReadI(FIELD_GLOBAL_SETTINGS_VERSION, 1000);

    ReadPropertiesAndFlags(&pGlobalSettings);

    return true;
}


//
// Read constraint
//
bool FbxReaderFbx7_Impl::ReadConstraint(FbxConstraint& pConstraint)
{
    if (IOS_REF.GetBoolProp(IMP_FBX_CONSTRAINT, true))
    {
        ReadPropertiesAndFlags(&pConstraint);

        // patch patch patch !!!
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

                            FbxProperty lPropertyTranslation = pConstraint.FindProperty(lTranslationOffsetName);
                            if (lPropertyTranslation.IsValid()) {
                                FbxVector4 lTranslationVector;
                                lTranslationVector[0] = mFileObject->FieldReadD();
                                lTranslationVector[1] = mFileObject->FieldReadD();
                                lTranslationVector[2] = mFileObject->FieldReadD();
                                FbxDouble4 lDouble4 = FbxDouble4(lTranslationVector[0],lTranslationVector[1], lTranslationVector[2],lTranslationVector[3]);
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
// Convert camera name
//
FbxString FbxReaderFbx7_Impl::ConvertCameraName(FbxString pCameraName)
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
bool FbxReaderFbx7_Impl::ReadProperties(FbxObject *pFbxObject)
{
    // Wd don't support Properties60 anymore, with its 'N' flag for node attributes.
    bool lReadExtendedProperties = mFileObject->FieldReadBegin("Properties70");

    if (lReadExtendedProperties ) {
        if (mFileObject->FieldReadBlockBegin()) {
            int     FieldInstanceCount = mFileObject->FieldGetInstanceCount("P");
            FbxString lPropertyName, lTypeName, lDataTypeName, lFlags;

            pFbxObject->RootProperty.BeginCreateOrFindProperty();

            int index = 0;
            for (int c = 0; c<FieldInstanceCount; c++ ) {
                bool lFieldIsBegin = false;

                lFieldIsBegin   = mFileObject->FieldReadBegin("P", c);

                lPropertyName   = mFileObject->FieldReadS( );    // Name
                lTypeName       = mFileObject->FieldReadS( );    // TypeName
                lDataTypeName   = mFileObject->FieldReadS();
                lFlags          = mFileObject->FieldReadS( );    // Flags : see store for description

                bool                lIsAnimatable   = strchr(lFlags,'A')!=NULL;
                bool                lIsUser         = strchr(lFlags,'U')!=NULL;
                bool                lIsAnimated     = strchr(lFlags,'+')!=NULL;
                bool                lIsHidden       = strchr(lFlags,'H')!=NULL;
                bool                lIsLocked       = strchr(lFlags,'L')!=NULL;
                bool                lIsMuted        = strchr(lFlags,'M')!=NULL;

                FbxPropertyFlags::EFlags lLockedMembers = FbxPropertyFlags::eNone;
                if ( lIsLocked )
                {
                    // Figure out if the whole property is locked or if only members are.
                    const char* lFlagPtr = strchr(lFlags,'L');
                    FBX_ASSERT( lFlagPtr != NULL );

                    lLockedMembers = static_cast< FbxPropertyFlags::EFlags >(
                        FbxGetFlagsFromChar( lFlagPtr[1] ) << FbxPropertyFlags::sLockedMembersBitOffset
                        );
                }

                FbxPropertyFlags::EFlags lMutedMembers = FbxPropertyFlags::eNone;
                if ( lIsMuted )
                {
                    // Figure out if the whole property is muted or if only members are.
                    const char* lFlagPtr = strchr(lFlags,'M');
                    FBX_ASSERT( lFlagPtr != NULL );

                    lMutedMembers = static_cast< FbxPropertyFlags::EFlags >(
                        FbxGetFlagsFromChar( lFlagPtr[1] ) << FbxPropertyFlags::sMutedMembersBitOffset
                        );
                }

                FbxDataType        lDataType;

                // interpretation of the data types
                if( !lDataTypeName.IsEmpty() )
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

                    lDataType = mManager.GetDataTypeFromName(lDataTypeName);
                    if( !lDataType.Valid() ) lDataType = mManager.GetDataTypeFromName(lTypeName);

                    if( !lDataType.Valid() )
                    {
                        FbxDataType lBaseType = mManager.GetDataTypeFromName(lDataTypeName);

                        if( lBaseType.Valid() )
                        {
                            lDataType = mManager.CreateDataType(lDataTypeName, lBaseType.GetType());
                        }
                    }
                }
                else
                {
                    lDataType = mManager.GetDataTypeFromName(lTypeName);
                    if (!lDataType.Valid()) lDataType=mManager.GetDataTypeFromName(lTypeName);
                }

                if( !lDataType.Valid() )
                {
                    FBX_ASSERT_NOW("Unsupported property type!");
                    continue;
                }

                // Check if the property already exist
                // and create a new one if not
                FbxProperty lUserProperty;
                FbxObject*  lPropertyContainer = pFbxObject;

                // Find the property
                lUserProperty = lPropertyContainer->FindPropertyHierarchical(lPropertyName);

                if (!lUserProperty.IsValid())
                {
                    // get the parent first
                    int lIndex = lPropertyName.ReverseFind( *FbxProperty::sHierarchicalSeparator );
                    if( -1 != lIndex )
                    {
                        FbxProperty lParent = lPropertyContainer->FindPropertyHierarchical( lPropertyName.Mid(0, lIndex ) );
                        FBX_ASSERT( lParent.IsValid() );
                        if( lParent.IsValid() ) lUserProperty = FbxProperty::Create(lParent, lDataType, lPropertyName.Mid( lIndex + 1 ), "", false);
                    }
                    else lUserProperty = FbxProperty::Create(lPropertyContainer, lDataType, lPropertyName, "", false);

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

                if( (lUserProperty.GetFlags() & FbxPropertyFlags::eLockedAll) != lLockedMembers )
                {
                    // Make sure only proper flags are set.
                    FBX_ASSERT( ( lLockedMembers & ~FbxPropertyFlags::eLockedAll ) == 0 );

                    // Unset the flags.
                    lUserProperty.ModifyFlag(FbxPropertyFlags::eLockedAll,false);
                    // Set them back to the right values.
                    lUserProperty.ModifyFlag(lLockedMembers,true);
                }

                if( (lUserProperty.GetFlags() & FbxPropertyFlags::eMutedAll) != lMutedMembers )
                {
                    // Make sure only proper flags are set.
                    FBX_ASSERT( ( lMutedMembers & ~FbxPropertyFlags::eMutedAll ) == 0 );

                    // Unset the flags.
                    lUserProperty.ModifyFlag(FbxPropertyFlags::eMutedAll,false);
                    // Set them back to the right values.
                    lUserProperty.ModifyFlag(lMutedMembers,true);
                }

                int lTempEnumVal = 0;
                if ( !lDataType.Is(FbxEventDT) && !lDataType.Is(FbxActionDT) && !lDataType.Is(FbxCompoundDT)) {

                    // Read values based on type
                    switch (lUserProperty.GetPropertyDataType().GetType()) 
					{
					case eFbxChar:		lUserProperty.Set( FbxChar(mFileObject->FieldReadByte()) );     break;
					case eFbxUChar:		lUserProperty.Set( FbxUChar(mFileObject->FieldReadUByte()) );     break;
					case eFbxShort:		lUserProperty.Set( FbxShort(mFileObject->FieldReadShort()) );     break;
					case eFbxUShort:		lUserProperty.Set( FbxUShort(mFileObject->FieldReadUShort()) );     break;
					case eFbxUInt:    lUserProperty.Set( FbxUInt(mFileObject->FieldReadUI()) );     break;
					case eFbxULongLong:   lUserProperty.Set( FbxULongLong(mFileObject->FieldReadULL()) );     break;
					case eFbxHalfFloat:	
						{
							FbxHalfFloat hf(mFileObject->FieldReadF());
							lUserProperty.Set( hf );	
						}
						break;

					case eFbxBool:		lUserProperty.Set( FbxBool(mFileObject->FieldReadB()) );     break;
                    case eFbxInt:		lUserProperty.Set( FbxInt(mFileObject->FieldReadI()) ); break;
					case eFbxLongLong:	lUserProperty.Set( FbxLongLong(mFileObject->FieldReadLL()) );     break;
                    case eFbxFloat:		lUserProperty.Set( FbxFloat(mFileObject->FieldReadF()) );    break;
                    case eFbxDouble:		lUserProperty.Set( FbxDouble(mFileObject->FieldReadD()) );   break;
                    case eFbxDouble2: 
						{
							FbxDouble2 lValue;
							mFileObject->FieldReadDn((double *)&lValue, 2);
							lUserProperty.Set( lValue );
						} 
						break;
                    case eFbxDouble3: 
						{
							FbxDouble3 lValue;
							mFileObject->FieldRead3D((double *)&lValue);
							lUserProperty.Set( lValue );
						} 
						break;
                    case eFbxDouble4:
						{
							FbxDouble4 lValue;
							mFileObject->FieldRead4D((double *)&lValue);
							lUserProperty.Set( lValue );
						} 
						break;
                    case eFbxDouble4x4:
						{
							FbxDouble4x4 lValue;
							mFileObject->FieldRead4D( (double *)&(lValue[0]) );
							mFileObject->FieldRead4D( (double *)&(lValue[1]) );
							mFileObject->FieldRead4D( (double *)&(lValue[2]) );
							mFileObject->FieldRead4D( (double *)&(lValue[3]) );
							lUserProperty.Set( lValue );
						} 
						break;
					case eFbxEnumM:
                    case eFbxEnum:       lTempEnumVal = FbxEnum(mFileObject->FieldReadI()); lUserProperty.Set(lTempEnumVal); break;					
                    case eFbxString:     lUserProperty.Set( FbxString(mFileObject->FieldReadS()) ); break;
                    case eFbxTime:       lUserProperty.Set( FbxTime(mFileObject->FieldReadT()) ); break;
                    case eFbxReference:  break; // used as a port entry to reference object or properties
                    case eFbxBlob:
						{
							FbxBinaryBlobTarget    lBlobTarget;

							if( ReadBinaryData(lBlobTarget) )
							{
								lBlobTarget.AssignToProperty(lUserProperty);
							}
						}
                        break;
                    case eFbxDistance:  
						{
							float value = mFileObject->FieldReadF();
							FbxString unit = mFileObject->FieldReadS();
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
                    }
                }

                // for user properties get the min and max
                if (lIsUser) 
				{
					switch (lUserProperty.GetPropertyDataType().GetType()) 
					{
					default:
					    break;
					case eFbxChar:
						{
						lUserProperty.SetMinLimit( mFileObject->FieldReadByte() );
						lUserProperty.SetMaxLimit( mFileObject->FieldReadByte() );
                        } 
						break;
					case eFbxUChar:
						{
						lUserProperty.SetMinLimit( mFileObject->FieldReadUByte() );
						lUserProperty.SetMaxLimit( mFileObject->FieldReadUByte() );
                        } 
						break;
					case eFbxShort:
						{
						lUserProperty.SetMinLimit( mFileObject->FieldReadShort() );
						lUserProperty.SetMaxLimit( mFileObject->FieldReadShort() );
                        } 
						break;
					case eFbxUShort:
						{
						lUserProperty.SetMinLimit( mFileObject->FieldReadUShort() );
						lUserProperty.SetMaxLimit( mFileObject->FieldReadUShort() );
                        } 
						break;
					case eFbxUInt:
						{
						lUserProperty.SetMinLimit( mFileObject->FieldReadUI() );
						lUserProperty.SetMaxLimit( mFileObject->FieldReadUI() );
                        } 
						break;
					case eFbxULongLong:
						{
						lUserProperty.SetMinLimit( double(mFileObject->FieldReadULL()) );
						lUserProperty.SetMaxLimit( double(mFileObject->FieldReadULL()) );
                        } 
						break;
                    case eFbxInt:
                        {
                        lUserProperty.SetMinLimit( mFileObject->FieldReadI() );
                        lUserProperty.SetMaxLimit( mFileObject->FieldReadI() );
                        }
                        break;
					case eFbxHalfFloat:

					case eFbxBool:
                    case eFbxFloat:   
					case eFbxDouble:   
						{
                        lUserProperty.SetMinLimit( mFileObject->FieldReadD() );
                        lUserProperty.SetMaxLimit( mFileObject->FieldReadD() );
						} 
						break;
					case eFbxEnumM:
                    case eFbxEnum: 
						{ // enum
							char* EnumList = const_cast<char*>( mFileObject->FieldReadS());    // Values
							if (strcmp(EnumList,"")!=0) 
							{
								char* next_token=NULL;
								char* lItem = FBXSDK_strtok( EnumList, "~", &next_token);
								while( lItem != NULL)
								{
									lUserProperty.AddEnumValue(lItem);
									lItem = FBXSDK_strtok( NULL, "~", &next_token);
								}
							}
							lUserProperty.Set(lTempEnumVal);
						} 
						break;
                    } // switch
                }

                if (lFieldIsBegin) mFileObject->FieldReadEnd();
            }

            pFbxObject->RootProperty.EndCreateOrFindProperty();

            mFileObject->FieldReadBlockEnd();
        }

        mFileObject->FieldReadEnd();
    }

    return true;
}

//
// Read object properties and flags
//
bool FbxReaderFbx7_Impl::ReadPropertiesAndFlags(FbxObject *pObject)
{
    if( mProgress && !mProgressPause )
    {
        mProgress->Update(1.0f, pObject->GetName());
    }
    return ReadProperties(pObject);
}

// ****************************************************************************************
// Connections
// ****************************************************************************************

//
// Read connection section
//
bool FbxReaderFbx7_Impl::ReadConnectionSection(FbxDocument* pDocument)
{
    if (mFileObject->FieldReadBegin("Connections"))
    {
        if (mFileObject->FieldReadBlockBegin())
        {
            while (mFileObject->FieldReadBegin("C"))
            {
                char Type[32];
                FbxProperty SrcP,DstP;
                FbxObject* Src = NULL;
                FbxObject* Dst = NULL;
                FbxObject* Object = NULL;

                FBXSDK_strncpy(Type, 32, mFileObject->FieldReadC(), 31);
                if (strcmp(Type,"OO")==0) {
                    Src = GetObjectFromId(mFileObject->FieldReadLL());
                    Dst = GetObjectFromId(mFileObject->FieldReadLL());
                } else if (strcmp(Type,"OD")==0) {
                    Src = GetObjectFromId(mFileObject->FieldReadLL());
                    Dst = GetObjectFromId(mFileObject->FieldReadLL());
                } else if (strcmp(Type,"PO")==0) {
                    Object = GetObjectFromId(mFileObject->FieldReadLL());
                    if (Object) {
                        FbxString PropertyName = mFileObject->FieldReadC();
                        SrcP = Object->FindPropertyHierarchical(PropertyName);
                        if (SrcP.IsValid()) {
                            Src  = Object;
                        }
                    }
                    Dst = GetObjectFromId(mFileObject->FieldReadLL());
                } else if (strcmp(Type,"OP")==0) {
                    Src = GetObjectFromId(mFileObject->FieldReadLL());
                    Object = GetObjectFromId(mFileObject->FieldReadLL());
                    if (Object) {
                        FbxString PropertyName = mFileObject->FieldReadC();
                        DstP = Object->FindPropertyHierarchical(PropertyName);
                        if (DstP.IsValid()) {
                            Dst  = Object;
                        }
                    }
                } else if (strcmp(Type,"PP")==0) {
                    //Source Property
                    Object = GetObjectFromId(mFileObject->FieldReadLL());
                    if( Object ) {
                        SrcP = Object->FindPropertyHierarchical(mFileObject->FieldReadC());
                        if (SrcP.IsValid()) {
                            Src  = Object;
                        }
                    }

                    //Destination Property
                    Object = GetObjectFromId(mFileObject->FieldReadLL());
                    if( Object ) {
                        DstP = Object->FindPropertyHierarchical(mFileObject->FieldReadC());
                        if (DstP.IsValid()) {
                            Dst  = Object;
                        }
                    }
                } else if( strcmp(Type, "EP") == 0 ) {
                    //Source Object (Entity)
                    Src = pDocument;

                    //Destination Property
                    Object = GetObjectFromId(mFileObject->FieldReadLL());
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
                        } else {
                            if (Dst->Is<FbxLayeredTexture>() && Src->Is<FbxTexture>()) {
                                Dst->ConnectSrcObject(Src);
                            }
                            if (Dst->Is<FbxNode>() && Src->Is<FbxSurfaceMaterial>()) {
                                Dst->ConnectSrcObject(Src);
                            }
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
// Read document animation
//
bool FbxReaderFbx7_Impl::ReadDocumentAnimation(FbxDocument* pDocument)
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
					lScene->SetTakeInfo(*lTakeInfo);

                mFileObject->FieldReadEnd();
            }
            mFileObject->FieldReadBlockEnd();
        }
        mFileObject->FieldReadEnd();
    }

	return lResult;
}

//
// Read AnimStack
//
bool FbxReaderFbx7_Impl::ReadAnimStack(FbxAnimStack& pAnimStack)
{
    return ReadPropertiesAndFlags(&pAnimStack);
}

//
// Read AnimLayer
//
bool FbxReaderFbx7_Impl::ReadAnimLayer(FbxAnimLayer& pAnimLayer)
{
    return ReadPropertiesAndFlags(&pAnimLayer);
}

//
// ReadAnimCurveNode
//
bool FbxReaderFbx7_Impl::ReadCurveNode(FbxAnimCurveNode& pAnimCurveNode)
{
    return ReadPropertiesAndFlags(&pAnimCurveNode);
}

//
// ReadAnimCurve
//
bool FbxReaderFbx7_Impl::ReadCurve(FbxAnimCurve& pAnimCurve)
{
    bool ret = ReadPropertiesAndFlags(&pAnimCurve);
    if (ret)
        ret = pAnimCurve.Retrieve(mFileObject);

    return ret;
}

//
// ReadAudioLayer
//
bool FbxReaderFbx7_Impl::ReadAudioLayer(FbxAudioLayer& pAudioLayer)
{
    return ReadPropertiesAndFlags(&pAudioLayer);
}

//
// ReadAudio
//
bool FbxReaderFbx7_Impl::ReadAudio(FbxAudio& pAudio)
{
	FbxAudio* lReferencedAudio = FbxCast<FbxAudio>(pAudio.GetReferenceTo());
	if (lReferencedAudio != NULL)
	{
		return ReadPropertiesAndFlags(&pAudio);
	}

	ReadPropertiesAndFlags(&pAudio);

	FbxString lFileName = pAudio.GetFileName();
	FbxString lRelativeFileName = pAudio.GetRelativeFileName();

    pAudio.SetOriginalFormat(true);
    pAudio.SetOriginalFilename(lFileName.Buffer());

    // If this field exist, the media is embedded.
	bool lStatus = true;
    bool lSkipValidation = true;
    if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true))
    {
        lSkipValidation = false;
        if( mFileObject->FieldReadBegin(FIELD_MEDIA_CONTENT) )
        {
            FbxString lDefaultPath = "";
            FbxString lUserDefinePath = IOS_REF.GetStringProp(IMP_EXTRACT_FOLDER, lDefaultPath);
            const char* lUserDefinePathBuffer = (lUserDefinePath == lDefaultPath) ? NULL : lUserDefinePath.Buffer();
            lStatus = mFileObject->FieldReadEmbeddedFile (lFileName, lRelativeFileName, mFileObject->GetMediaDirectory(true, lUserDefinePathBuffer));
            mFileObject->FieldReadEnd();
        }
    }

    pAudio.SetFileName(lFileName);
    pAudio.SetRelativeFileName(lRelativeFileName);

    if (lSkipValidation == false)
    {
	    // Check if the "absolute" path of the audio exist
	    // If the "absolute" path of the audio is not found BUT the "relative" path is found
	    // replace the "absolute" path of the audio, then if we later write this scene in a file, the "absolute" path exist.
	    // This can occur when a FBX file and "relative" audio are moved.
	    if( FbxFileUtils::Exist(  pAudio.GetFileName() ) == false)
	    {
		    FbxString lNewAbsolutePath = mFileObject->GetFullFilePath( pAudio.GetRelativeFileName() );
		    lNewAbsolutePath = FbxPathUtils::Clean( lNewAbsolutePath );
		    if( FbxFileUtils::Exist( lNewAbsolutePath ) )
		    {
			    // Set with a valid "absolute" path
			    pAudio.SetFileName( lNewAbsolutePath.Buffer() );
		    }
	    }
    }

    return !lFileName.IsEmpty();
}


//
// ReadReference
//
bool FbxReaderFbx7_Impl::ReadReference(FbxSceneReference& pReference)
{
    return ReadPropertiesAndFlags(&pReference);
}

//
// Get timeshift from node animation
//
bool FbxReaderFbx7_Impl::TimeShiftNodeAnimation(FbxScene& pScene, FbxTakeInfo* pTakeInfo)
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
        FbxTimeSpan lTimeInterval(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);

        lCount = pScene.GetMemberCount<FbxNode>();

        for (i = 0; i < lCount; i++)
        {
            pScene.GetMember<FbxNode>(i)->GetAnimationInterval(lTimeInterval, lAnimStack);
        }

        lTimeShift = pTakeInfo->mImportOffset - lTimeInterval.GetStart();
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
            }
        }

        pTakeInfo->mLocalTimeSpan.SetStart(pTakeInfo->mLocalTimeSpan.GetStart() + lTimeShift);
        pTakeInfo->mLocalTimeSpan.SetStop(pTakeInfo->mLocalTimeSpan.GetStop() + lTimeShift);
    }

    return true;
}

#if 0
/****************************************************************************************************************
//
// THIS SECTION IS LEFT FOR REFERENCE. A MAJOR RE-THINKING IS REQUIRED FOR VERSION 7
//
//
//
// Merge layer with timewarp
//
void FbxReaderFbx7_Impl::MergeLayerAndTimeWarp(KFCurveNode* pCurveNode, KFCurveNode* pTimeWarpNode /* = NULL /)
{
    bool lFilterCurve = false;
    KFCurveNode* lLayerNode = pCurveNode->LayerGet();
    KFCurveNode* lTimeWarpNode = pCurveNode->TimeWarpGet();

    // Local time warp node overrides parent time warp node but it should not happen.
    if (!lTimeWarpNode)
    {
        lTimeWarpNode = pTimeWarpNode;
    }

    if( lLayerNode )
    {
        MergeTimeWarpWithLayer(pCurveNode, lTimeWarpNode);
        lFilterCurve = true;
    }
    else
    {
        if (pTimeWarpNode)
        {
            MergeTimeWarpWithoutLayer(pCurveNode, lTimeWarpNode);
            lFilterCurve = true;
        }

        int i, lCount = pCurveNode->GetCount();

        for (i = 0; i < lCount; i++)
        {
            MergeLayerAndTimeWarp(pCurveNode->Get(i), lTimeWarpNode);
        }
    }

    if (lFilterCurve)
    {
        //
        //  Apply rotation filter
        //
        KFCURVE_ApplyRotationFilter( pCurveNode );

        //
        //  Constant Key Reducing
        //
        if (KFCURVE_GetUseConstantKeyReducer())
        {
			FbxAnimCurveFilterConstantKeyReducer lFilter;
            lFilter.Apply( *pCurveNode );
        }
    }
}

void FbxReaderFbx7::MergeLayerAndTimeWarp(FbxObject* pObj)
{
    // Collect the KFCurveNode of all the properties
    pObj->RootProperty.BeginCreateOrFindProperty();
    FbxProperty p = pObj->GetFirstProperty();  
    while (p.IsValid())
    {
        FbxAnimCurveNode* acn = p.GetCurveNode(mAnimLayer, false);
        if (acn) 
        {
            KFCurveNode* lCurveNode = acn->GetKFCurveNode();
            KFCurveNode* lLayerNode = lCurveNode->LayerGet();
            KFCurveNode* lTimeWarpNode = lCurveNode->TimeWarpGet();
            bool lFilterCurve = false;

            if( lLayerNode )
            {
                MergeTimeWarpWithLayer(lCurveNode, lTimeWarpNode);
                lFilterCurve = true;
            }

            if (lFilterCurve)
            {
                //
                //  Apply rotation filter
                //
                KFCURVE_ApplyRotationFilter( pCurveNode );

                //
                //  Constant Key Reducing
                //
		        if (KFCURVE_GetUseConstantKeyReducer())
		        {
			        FbxAnimCurveFilterConstantKeyReducer lFilter;
			        lFilter.Apply(*acn);
		        }
            }
        }
        p = pObj->GetNextProperty(p);
    }
    pObj->RootProperty.EndCreateOrFindProperty();
}

//
// Merge timewarp with layer
//
void FbxReaderFbx7_Impl::MergeTimeWarpWithLayer(KFCurveNode* pCurveNode, KFCurveNode* pTimeWarpNode)
{
    FbxTime lStart = FBXSDK_TIME_INFINITE;
    FbxTime lStop = FBXSDK_TIME_MINUS_INFINITE;

    if (pTimeWarpNode)
    {
        // Keys before and after a time warp's animation interval are constant.
        pTimeWarpNode->GetAnimationInterval(lStart, lStop);
    }
    else
    {
        pCurveNode->GetAnimationInterval(lStart, lStop);
    }

    KFCurve_PlotInLayers( pCurveNode,
                          NULL,
                          0,
                          eKFCURVE_PLOT_DestinationCurveReplaceByResult,
                          lStart,
                          lStop,
                          KFCURVE_GetPlotPeriod(),
                          NULL,
                          NULL,
                          true );
}


// This function is almost an exact copy from function
// void KtFCurveEditor::MergeNodeTimeWarp(HIKFCurveNode  pNode, HIKFCurveNode  pTWNode )
void FbxReaderFbx7_Impl::MergeTimeWarpWithoutLayer(KFCurveNode* pCurveNode, KFCurveNode* pTimeWarpNode)
{
    if( pCurveNode->FCurveGet() )
    {
        KFCurve* lFCurve = pCurveNode->FCurveGet();
        KFCurve* lTWFCurve = pTimeWarpNode->FCurveGet();
        KFCurve* lNewFCurve = KFCurveCreate();
        KFCurveKey lKey;
        int lIndex = 0;

        FbxTime     lStartTime = FBXSDK_TIME_INFINITE;
        FbxTime     lStopTime = FBXSDK_TIME_MINUS_INFINITE;
        FbxTime     lCurrentTime;
        FbxTime     lSamplePeriod;
        FbxTime     lTWTime;
        double   lTWTimeDouble;
        double   lValue;
        int      lTWIndex = 0;

        lNewFCurve->SetColor(lFCurve->GetColor());

        if (lFCurve->KeyGetCount() > 1 && lTWFCurve->KeyGetCount() > 1)
        {
            // Keys before and after a time warp's animation interval are constant.
            pTimeWarpNode->GetAnimationInterval(lStartTime, lStopTime);

            // Add sample every plot period time
            lSamplePeriod = KFCURVE_GetPlotPeriod();
            lCurrentTime = lStartTime;
            while (lCurrentTime < lStopTime)
            {
                pTimeWarpNode->CandidateEvaluate( &lTWTimeDouble, lCurrentTime, &lTWIndex );

                lTWTime.SetSecondDouble( lTWTimeDouble );

                pCurveNode->CandidateEvaluate( &lValue, lTWTime, &lIndex );

                lKey.Set( lCurrentTime, lValue, FbxAnimCurveDef::eInterpolationCubic, FbxAnimCurveDef::eTangentAuto);

                lNewFCurve->KeyAdd(lCurrentTime, lKey);
                lCurrentTime += lSamplePeriod;
            }

            // Copy resulting curve
            lFCurve->CopyFrom(*lNewFCurve);
        }

        FbxDelete(lNewFCurve);
    }

    pCurveNode->TimeWarpSet(NULL);
}
*/
#endif

//
// Rebuild trim regions
//
void FbxReaderFbx7_Impl::RebuildTrimRegions(FbxScene& pScene) const
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
// Rebuild Target Shape
//
void FbxReaderFbx7_Impl::RebuildTargetShapes(FbxScene& pScene) const
{
	int shapeCount = pScene.GetSrcObjectCount<FbxShape>();
	if (shapeCount == 0) return;

	bool needMesh2WorkMeshCopy = true;
    int  lastIterTotalCount = -1;
    
	// persistent data
	FbxArray<int> lVerticesIds;
	FbxMesh::ControlPointToVerticesMap lCtrlPtToVtces;
	// re-use the FbxMesh::ControlPointToVerticesMap structure
	// even if the content is going to be control points instead of vertices
	FbxMesh::ControlPointToVerticesMap lCtrlPtToAdjCtrlPt;
	FbxMesh* lWorkMesh = FbxMesh::Create(pScene.GetFbxManager(), "tempWork");

	for (int lShapeIndex = 0; lShapeIndex < shapeCount; lShapeIndex++)
    {
        FbxShape* lShape = pScene.GetSrcObject<FbxShape>(lShapeIndex);
        if (lShape == NULL)
        {
            FBX_ASSERT_NOW("null shape");
            continue;
        }
                                         
        FbxBlendShapeChannel* lChannel = lShape->GetBlendShapeChannel();
        if (lChannel == NULL)
        {
            FBX_ASSERT_NOW("orphan blendshape target");
            continue;
        }

        FbxBlendShape* lBlendShape = lChannel->GetBlendShapeDeformer();
        if (lBlendShape == NULL)
        {
            FBX_ASSERT_NOW("orphan blendshape channel");
            continue;
        }

        FbxGeometry* lGeometry = lBlendShape->GetGeometry();
        if (lGeometry == NULL)
        {
            FBX_ASSERT_NOW("orphan blendshape deformer");
            continue;
        }

        // FbxShape only contains the geometry info of control points that are different from base geometry. 
        // To create shape object with full geometry info, we need to combine it with base geometry.
        // 1. Keep the offset values aside;
        // 2. Refresh the FbxShape with the geometry info of base geometry;
        // 3. Add the offset values to get the deformed target geometry.
        int lChangedPointCount = lShape->GetControlPointIndicesCount();
		FbxArray<FbxVector4> lWorkControlPoints;
		FbxArray<int> lIndices; 
		FbxMap<int, int> lCtrlPtToIndex; // map control point to index in lIndices (size == lChangedPointCount)

		// 1.
        for(int lIndex = 0; lIndex<lChangedPointCount; ++lIndex)
        {
            lWorkControlPoints.Add(lShape->GetControlPointAt(lIndex));
			int lCtrlPtIndex = lShape->GetControlPointIndices()[lIndex];
            lIndices.Add(lCtrlPtIndex);
			lCtrlPtToIndex.Insert(lCtrlPtIndex, lIndex);
        }

        // 2. Set the shape's control points as the base geometry's control points
        int lTotalCount = lGeometry->GetControlPointsCount();
        if (lastIterTotalCount != lTotalCount)
		{
			// shape changed from last iteration, reset our data...
			lastIterTotalCount = lTotalCount;
			needMesh2WorkMeshCopy = true;
		}
        
        lShape->InitControlPoints(lTotalCount);
        lShape->mControlPoints = lGeometry->mControlPoints;
	
        // 3. Add the offset to the points that are deformed.
        for (int i = 0; i < lChangedPointCount; ++i)
        {
            FbxVector4 lVector = lShape->GetControlPointAt(lIndices[i]);
            FbxVector4 lOffset = lWorkControlPoints[i];
            lVector[0] += lOffset[0];
            lVector[1] += lOffset[1];
            lVector[2] += lOffset[2];
            lShape->SetControlPointAt(lVector, lIndices[i]);
        }

		// Do similar job for the normals
        if (lGeometry->GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            // Only process normals of the shape on layer 0.
            FbxLayer*              lLayer;
            FbxLayerElementNormal* lLayerElementNormal;

            lLayer = lShape->GetLayer(0);
            if (!lLayer)
            {
                lLayer = lShape->GetLayer(lShape->CreateLayer());
				FBX_ASSERT(lLayer != NULL);
            }

            lLayerElementNormal = lLayer->GetNormals();
            if (!lLayerElementNormal)
            {
                lLayerElementNormal = FbxLayerElementNormal::Create(lShape, "");
                lLayer->SetNormals(lLayerElementNormal);
            }

			FbxMesh* lMesh = (FbxMesh*) lGeometry;
			FbxLayer* lSrcLayer = lMesh->GetLayer(0);
			FbxLayerElementNormal* lSrcLayerElementNormal = (lSrcLayer) ? lSrcLayer->GetNormals() : NULL;

			// reset shape layer element to same mapping as base			
			bool byCtrlPts = true;
			FbxLayerElement::EMappingMode lMapping = FbxLayerElement::eByControlPoint;
			if (lSrcLayerElementNormal) 
			{
				lMapping = lSrcLayerElementNormal->GetMappingMode();
				byCtrlPts = (lMapping == FbxLayerElement::eByControlPoint);				
			}

			lLayerElementNormal->SetMappingMode(lMapping);
			lLayerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);
            FbxLayerElementArrayTemplate<FbxVector4>& lShapeNormals = lLayerElementNormal->GetDirectArray(); // Normals on the shape

			// if all the deltas == FbxZeroVector4, we will use the work normals only, else
			// the shape absolute normal = base + delta
			bool lNoDeltaDefined = true;
			FbxArray<FbxVector4> lDeltaN;
			lShapeNormals.CopyTo(lDeltaN); // normals offsets
			for (int i = 0, c = lDeltaN.GetCount(); lNoDeltaDefined == true && i < c; i++)
				lNoDeltaDefined = FbxEqual(lDeltaN[i], FbxZeroVector4);

			if (needMesh2WorkMeshCopy)
			{				
				// copy mesh definition only once
				lWorkMesh->Copy(*lMesh);			
				needMesh2WorkMeshCopy = false;
			}

			lWorkMesh->mControlPoints = lShape->mControlPoints;  // set the points to those of the shape
			lWorkMesh->GenerateNormals(true, byCtrlPts);		 // force recompute of the normals

			// Normals on the base geometry (or fallback to workmesh computed normals)
            FbxLayerElementArrayTemplate<FbxVector4>& lBaseNormals = (lSrcLayerElementNormal) ? lSrcLayerElementNormal->GetDirectArray() : lWorkMesh->GetElementNormal()->GetDirectArray(); 
			FbxLayerElementArrayTemplate<FbxVector4>& lWNormals = lWorkMesh->GetElementNormal()->GetDirectArray(); // computed normals on the target
			FBX_ASSERT(lWorkMesh->mControlPoints.GetCount() == lTotalCount);

			// make sure we (re)fill the maps if the control points count is different from previous iteration
			if (lCtrlPtToVtces.GetCount() != lTotalCount)
			{
				lCtrlPtToVtces.Clear();
				lCtrlPtToAdjCtrlPt.Clear();

				// create required mapping tables...
				lWorkMesh->ComputeControlPointToVerticesMap(lCtrlPtToVtces);
				FBX_ASSERT(lCtrlPtToVtces.GetCount() == lTotalCount);

				if (lCtrlPtToAdjCtrlPt.Init(lTotalCount))
				{
					lWorkMesh->BeginGetMeshEdgeVertices();
					for (int e = 0, ec = lWorkMesh->GetMeshEdgeCount(); e < ec; e++)
					{
						int p1,p2;
						lWorkMesh->GetMeshEdgeVertices(e, p1, p2);
						lCtrlPtToAdjCtrlPt[p1]->Add(p2);
						lCtrlPtToAdjCtrlPt[p2]->Add(p1);
					}
					lWorkMesh->EndGetMeshEdgeVertices();
				}
			}

			// easy & quick case: normals by control point, we can use the calculated normals for all the non deformed
			// points and the base + delta for the deformed points (just to ensure that we are using the normals deltas stored
			// in the shape
			if (byCtrlPts)
			{
				lShapeNormals = lWNormals;
				if (lNoDeltaDefined == false)
				{
					for (int i = 0; i < lChangedPointCount; ++i)
					{
						FbxVector4 bn = lBaseNormals.GetAt(lIndices[i]);
						FbxVector4 n  = bn + lDeltaN[i];
						n.Normalize();
						lShapeNormals.SetAt(lIndices[i], n);
					}
				}
			}
			else
			{
				bool lConformed = lWorkMesh->ConformNormalsTo(lMesh);  // if normals are not by control point, we conform them to the base

				// expand lIndices with the control points connected with an edge by the deformed ones
				for (int p = 0, c = lIndices.GetCount(); p < c; p++)
				{
					int lCtrlPt = lIndices[p];
					FBX_ASSERT(lCtrlPt < lCtrlPtToAdjCtrlPt.GetCount());
					if (lCtrlPt < lCtrlPtToAdjCtrlPt.GetCount())
					{
						int lCount = lCtrlPtToAdjCtrlPt[lCtrlPt]->GetCount();
						for (int i = 0; i < lCount; i++)
						{
							int pt = lCtrlPtToAdjCtrlPt[lCtrlPt]->GetAt(i);
							if (lIndices.Find(pt) == -1)
								lIndices.Add(pt);
						}
					}
				}

				// now that we have all the control points we collect the corresponding vertices indices
				for (int i = 0, c = lIndices.GetCount(); i < c; i++)
				{
					int lCtrlPt = lIndices[i];
					FBX_ASSERT(lCtrlPt < lCtrlPtToVtces.GetCount());
					if (lCtrlPt < lCtrlPtToVtces.GetCount())
					{
						int lCount = lCtrlPtToVtces[lCtrlPt]->GetCount();
						for (int v = 0; v < lCount; v++)
							lVerticesIds.Add(lCtrlPtToVtces[lCtrlPt]->GetAt(v));
					}
				}

				int bnc = lBaseNormals.GetCount();
				int vtc = lMesh->mPolygonVertices.GetCount();
				FBX_ASSERT(bnc == vtc);

				// replace the stored delta normals on the shape with base absolute normals
				lShapeNormals = lBaseNormals;

				if (!lConformed) 
				{
					// we were unable to conform computed shape normals with base mesh
					// We cannot really recover so let's use the base normals and continue on next shape
					// ... unless we have some deltas, in wich case we want to use them
					vtc = lVerticesIds.GetCount();
					for (int i = 0; lNoDeltaDefined == false && i < vtc; i++)
					{
						int vtxId = lVerticesIds[i];
						int ctrlPt = lMesh->mPolygonVertices[vtxId]; 
						FbxMap<int, int>::RecordType* lRec = lCtrlPtToIndex.Find(ctrlPt);
						if (lRec)
						{
							int indx = lRec->GetValue();

							// n = base + delta
							FbxVector4 d = lDeltaN.GetAt(indx);
							FbxVector4 bn = lBaseNormals.GetAt(vtxId);
							FbxVector4 n = bn + d;
							n.Normalize();
							lShapeNormals.SetAt(vtxId, n);							
						}
					}
				}
				else
				{
					int wnc = lWNormals.GetCount();
					FBX_ASSERT(wnc == bnc);

					vtc = lVerticesIds.GetCount();
					// now, update with the work mormals. 
					for (int i = 0; i < vtc; i++)
					{
						int vtxId = lVerticesIds[i];
						FbxVector4 wn = lWNormals.GetAt(vtxId);
						FbxVector4 bn = lBaseNormals.GetAt(vtxId);

						// work normal != base 
						if (!FbxEqual(wn, bn)) 
						{
							 // always start with the computed target shape normal
							lShapeNormals.SetAt(vtxId, wn);

							int ctrlPt = lMesh->mPolygonVertices[vtxId]; 
							FbxMap<int, int>::RecordType* lRec = lCtrlPtToIndex.Find(ctrlPt);
							if (!lRec)
							{
								// The computed normal != base normal on a vertex that is not deformed 
								// by the target shape.Expand the shape control point index array to include 
								// this control point
								lShape->AddControlPointIndex(ctrlPt);
								lCtrlPtToIndex.Insert(ctrlPt, -1);
							}
							else
							{
								int indx = lRec->GetValue();
								if (indx == -1) continue; // we already processed this record above (if !lRec)
								FBX_ASSERT(indx >= 0 && indx < lChangedPointCount);
							
								// the visited vertex is one of the original deformed control points.
								// we can check the saved normal vector in the shape
								if (lNoDeltaDefined == false)
								{
									// n = base + delta
									FbxVector4 d = lDeltaN.GetAt(indx);
									if (!FbxEqual(d, FbxZeroVector4))
									{
										FbxVector4 n = bn + d;
										n.Normalize();
										lShapeNormals.SetAt(vtxId, n);
									}
								}
							}
						}
					} // for all vertices
				} // else (!lConformed) 
			} // else (byCtrlPts)
        } // if mesh

		lVerticesIds.Clear();
		lCtrlPtToIndex.Clear();
		lWorkControlPoints.Clear();
        lIndices.Clear();

        // Convert the shape deform property to the DeformPercent property of FbxBlendShapeChannel.
        // To make sure FBX 7 file before 7200 with shape can still be imported properly with animation.
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
            lChannel->DeformPercent.CopyValue(lProp);

             // loop through all the stacks & layers so we copy everything.
            int nbStacks = pScene.GetMemberCount<FbxAnimStack>();
            for (int i = 0; i < nbStacks; i++)
            {
                FbxAnimStack* lAnimStack = pScene.GetMember<FbxAnimStack>(i);
                if(lAnimStack)
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
                                lShapeCNNew = lChannel->DeformPercent.GetCurveNode(lAnimLayer, true);
                                if (lShapeCNNew)
                                {
                                    FBX_ASSERT(lShapeCN->GetChannelsCount() == lShapeCNNew->GetChannelsCount());
                                    unsigned int nbc = (unsigned int)lShapeCN->GetChannelsCount();
                                    for (unsigned int c = 0; c < nbc; c++)
									{
                                        lShapeCNNew->SetChannelValue<float>(c, lShapeCN->GetChannelValue<float>(c, 0.0f));
										while (lShapeCN->GetCurveCount(c))
										{
											FbxAnimCurve* lShapeCurve = lShapeCN->GetCurve(c);
											if (lShapeCurve)
											{
												lShapeCN->DisconnectFromChannel(lShapeCurve, c);
												lShapeCNNew->ConnectToChannel(lShapeCurve, c);
											}
										}
									}
									lShapeCN->Destroy();
                                }
                            }
                        }// If lAnimLayer
                    }
                }// If lAnimStack
            }

			// destroy the property. It is not required in memory (and if we keep it when we save to disk, we can break the import
			// into MoBu
			lProp.Destroy();
        }// If shape deform property exist

    }  // for each shape in scene

	lWorkMesh->Destroy(); // destroy temporary mesh
}

//
// Rebuild layered texture alphas
//
void FbxReaderFbx7_Impl::RebuildLayeredTextureAlphas(FbxScene& pScene) const
{
    int lMajor, lMinor, lRevision;
    mImporter.GetFileVersion(lMajor, lMinor, lRevision);
    if (lMajor >= 7 && lMinor >= 2)
    {
        // no need to rebuild alphas for FBX version 2012 (7.2) and later
        return;
    }

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

//
// Read Binary data
//
bool FbxReaderFbx7_Impl::ReadBinaryData(FbxBinaryTarget& pDest)
{
    int lDataLength = mFileObject->FieldReadI();
    if( !pDest.Initialize(lDataLength) )
    {
        mStatus.SetCode(FbxStatus::eFailure, "Error decoding binary data chunk. Cannot write data.");
		return false;
    }

    bool lResult = false;
    if (mFileObject->FieldReadBlockBegin())
    {
        // Block may be empty -- we don't expect more than 1,
        // but it won't hurt to be forgiving.
        int lChunkCount = mFileObject->FieldGetInstanceCount("BinaryData");
	    lResult = true;

        for( int i = 0; i < lChunkCount && (lDataLength > 0) && lResult; ++i )
        {
            if (mFileObject->FieldReadBegin("BinaryData"))
			{
				for( int lValueCount = mFileObject->FieldReadGetCount();
						(lValueCount > 0) && (lDataLength > 0) && lResult; --lValueCount )
				{
					int lChunkSize;
					void* lChunkData = mFileObject->FieldReadR(&lChunkSize);

					if( lChunkData && lChunkSize > 0)
					{
						FBX_ASSERT( lChunkSize <= lDataLength );

						// We may not read everything if this occurs,
						// but we won't crash... (right away)
						lChunkSize = FbxMin(lChunkSize, lDataLength);

						if( !pDest.AppendData(reinterpret_cast<const char*>(lChunkData), lChunkSize) )
						{
							lResult = false;
						}
						else
						{
							lDataLength  -= lChunkSize;
						}
					} 
					else
					{
						// File is most likely invalid - error out? Note that this does not appear to stop the
						// decoding of the file.
						lResult = false;
					}

					if (lResult == false)
						mStatus.SetCode(FbxStatus::eFailure, "Error decoding binary data chunk. The file may be corrupted.");
				}

				mFileObject->FieldReadEnd();
			}
        }

		mFileObject->FieldReadBlockEnd();
    }

    return lResult;
}

//
// Read embedded files
//
bool FbxReaderFbx7_Impl::ReadEmbeddedFiles(FbxDocument* pDocument)
{
    FBX_ASSERT( pDocument );

    if (!mFileObject->FieldReadBegin(FIELD_EMBEDDED_FILES))
    {
        // Not all files will have embedded files;
        return true;
    }

    if (IOS_REF.GetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true) == false)
    {
        return true;
    }

    if (mFileObject->FieldReadBlockBegin())
    {
        FbxString lMediaFolder;   // Create the folder as late as possible.

        while( !mStatus.Error() && mFileObject->FieldReadBegin(FIELD_EMBEDDED_FILE) )
        {
            if( lMediaFolder.IsEmpty() )
            {
                FbxString lDefaultPath = "";
                FbxString lUserDefinePath = IOS_REF.GetStringProp(IMP_EXTRACT_FOLDER, lDefaultPath);
                const char* lUserDefinePathBuffer = (lUserDefinePath == lDefaultPath) ? NULL : lUserDefinePath.Buffer();
                lMediaFolder = mFileObject->GetMediaDirectory(true, lUserDefinePathBuffer);

                if( lMediaFolder.IsEmpty() )
                {
                    mFileObject->FieldReadEnd();

                    mStatus.SetCode(FbxStatus::eFailure, "Unable to create .fbm folder to extract embedded files");
                    break;
                }
            }

            if (mFileObject->FieldReadBlockBegin())
            {
                FbxString lFileName = mFileObject->FieldReadC(FIELD_EMBEDDED_FILENAME);

                // Writer is not allowed to put absolute paths here.
                FBX_ASSERT(FbxPathUtils::IsRelative(lFileName));

                if (mFileObject->FieldReadBegin(FIELD_MEDIA_CONTENT))
                {
                    FbxString lTargetFileName = FbxPathUtils::Bind(lMediaFolder, lFileName);

                    FbxBinaryFileTarget lFileWriter(lTargetFileName);
                    if (!ReadBinaryData(lFileWriter))
                    {
                        remove(lTargetFileName);
                    }

                    mFileObject->FieldReadEnd();
                }

                if (mFileObject->FieldReadBegin(FIELD_EMBEDDED_CONSUMERS))
                {
                    ReadAndAdjustEmbeddedConsumers(lFileName);

                    mFileObject->FieldReadEnd();
                }
                mFileObject->FieldReadBlockEnd();
            }
            mFileObject->FieldReadEnd();
        }

        mFileObject->FieldReadBlockEnd();
    }

	return mStatus.GetCode() == FbxStatus::eSuccess;
}

//
// Read and adjust embedded consumers
//
void FbxReaderFbx7_Impl::ReadAndAdjustEmbeddedConsumers(const FbxString& pFileName)
{
    if (!mFileObject->FieldReadBlockBegin())
		return;

    while( mFileObject->FieldReadBegin(FIELD_EMBEDDED_CONSUMER) )
    {
        FbxLongLong lObjectId = mFileObject->FieldReadLL();
        FbxString   lPropName = mFileObject->FieldReadC();
        int       lPropIndex= mFileObject->FieldReadI();

        FbxObject* lObject = GetObjectFromId(lObjectId);

        if( lObject )
        {
            FbxProperty lProp = lObject->FindPropertyHierarchical(lPropName);

            if( lProp.IsValid() )
            {
                FbxString lPropValue = mManager.GetXRefManager().GetUrl(lProp, lPropIndex);

                if( lPropValue != pFileName )
                {
                    FbxString lNewPropertyValue = ReplaceStringToken(lProp.Get<FbxString>(), lPropIndex, pFileName);
                    lProp.Set(lNewPropertyValue);
                }
            }
            else
            {
                FBX_ASSERT_NOW("Missing property on consumer object.");
            }
        }
        else
        {
            // Maybe we just didn't read it because of some options?
            // Anyway, assert just in case.
            FBX_ASSERT_MSG(lObject, "Embedded file referenced by a missing consumer object.");
        }
        mFileObject->FieldReadEnd();      
    }
    mFileObject->FieldReadBlockEnd();
}

//
// Order information by type
//
void FbxReaderFbx7_Impl::OrderTypeInfoSection(FbxObjectTypeInfoList& pObjectContent)
{
    // Order shouldn't really matter, but unfortunately it does.
    // Until we can make the reader 'push' dangling references as it's reading, and
    // have it complete when it gets what it's missing, we need to put the read
    // back into some sort of order.
    // 
    // This is similar to what the fbx6 writer does, but we also move the node attributes
    // first since nodes may have node attributes that in turn are clone refs, and thus
    // must be first.

    // from kbxwritefbx6.cxx:
    /*
        pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MODEL, 0);
        pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MATERIAL, 1);
        pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TEXTURE, 2);
        pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_VIDEO, 3);
    
        pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONSTRAINT, -5);
        pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TIMELINE, -4);
        pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TIMELINE_TRACK, -3);
        pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CLIP, -2);
        pDefinitions.MoveDefinition(FIELD_OBJECT_DEFINITION_OBJECT_TYPE_FOLDER, -1);
    */

    FbxString lMovedToFront[] =
    {
        FIELD_OBJECT_DEFINITION_OBJECT_TYPE_VIDEO,
        FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TEXTURE,
        FIELD_OBJECT_DEFINITION_OBJECT_TYPE_PROCEDURAL_TEXTURE,
        FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MATERIAL,
        FIELD_OBJECT_DEFINITION_OBJECT_TYPE_MODEL,
        FIELD_OBJECT_DEFINITION_OBJECT_TYPE_NODE_ATTRIBUTE,
        ""
    };

    for( FbxString* lType = lMovedToFront; !lType->IsEmpty(); ++lType )
    {
		size_t lIndex = pObjectContent.Find(*lType);
        if( lIndex != -1 )
        {
			pObjectContent.Remove(lIndex);
			pObjectContent.Insert(0, *lType);
        }
    }

    FbxString lPushedToBack[] =
    {
        FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CONSTRAINT, 
        FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TIMELINE, 
        FIELD_OBJECT_DEFINITION_OBJECT_TYPE_TIMELINE_TRACK, 
        FIELD_OBJECT_DEFINITION_OBJECT_TYPE_CLIP, 
        FIELD_OBJECT_DEFINITION_OBJECT_TYPE_FOLDER, 
        ""  
    };

    for( FbxString* lType = lPushedToBack; !lType->IsEmpty(); ++lType )
    {
		size_t lIndex = pObjectContent.Find(*lType);
        if( lIndex != -1 )
        {
			pObjectContent.Remove(lIndex);
			pObjectContent.PushBack(*lType);
        }
    }
}

#include <fbxsdk/fbxsdk_nsend.h>
