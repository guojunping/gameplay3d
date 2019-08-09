/****************************************************************************************
 
   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/fileio/collada/fbxcolladanamespace.h>
#include <fbxsdk/fileio/collada/fbxcolladautils.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

void FbxColladaNamespace::Push(xmlNode *pElement)
{
    xmlNode * lNewParamElement = DAE_FindChildElementByTag(pElement, COLLADA_FXCMN_NEWPARAM_ELEMENT);
    int lNewParamCount = 0;
    while (lNewParamElement)
    {
        mParamDefinition.Add(lNewParamElement);
        ++lNewParamCount;
        lNewParamElement = DAE_FindChildElementByTag(pElement, COLLADA_FXCMN_NEWPARAM_ELEMENT, lNewParamElement);
    }
    mParamDefinitionCount.Add(lNewParamCount);

    xmlNode * lSetParamElement = DAE_FindChildElementByTag(pElement, COLLADA_FXCMN_SETPARAM_ELEMENT);
    int lSetParamCount = 0;
    while (lSetParamElement)
    {
        mParamModification.Add(lSetParamElement);
        ++lSetParamCount;
        lSetParamElement = DAE_FindChildElementByTag(pElement, COLLADA_FXCMN_SETPARAM_ELEMENT, lSetParamElement);
    }
    mParamModificationCount.Add(lSetParamCount);
}

void FbxColladaNamespace::Pop()
{
    {
        FBX_ASSERT(mParamDefinitionCount.GetCount());
        int lCount = mParamDefinitionCount.RemoveLast();
        FBX_ASSERT(mParamDefinition.GetCount() >= lCount);
        for (int lIndex = 0; lIndex < lCount; ++lIndex)
            mParamDefinition.RemoveLast();
    }

    {
        FBX_ASSERT(mParamModificationCount.GetCount());
        int lCount = mParamModificationCount.RemoveLast();
        FBX_ASSERT(mParamModification.GetCount() >= lCount);
        for (int lIndex = 0; lIndex < lCount; ++lIndex)
            mParamModification.RemoveLast();
    }
}

xmlNode * FbxColladaNamespace::FindParamDefinition(const char * pSID) const
{
    const int lCount = mParamDefinition.GetCount();
    for (int lIndex = lCount - 1; lIndex >= 0; --lIndex)
    {
        xmlNode * lElement = mParamDefinition[lIndex];
        if (DAE_CompareAttributeValue(lElement, COLLADA_SUBID_PROPERTY, pSID))
            return lElement;
    }
    return NULL;
}

xmlNode * FbxColladaNamespace::FindParamModification(const char * pSID) const
{
    const int lCount = mParamModification.GetCount();
    for (int lIndex = lCount - 1; lIndex >= 0; --lIndex)
    {
        xmlNode * lElement = mParamModification[lIndex];
        if (DAE_CompareAttributeValue(lElement, COLLADA_REF_PROPERTY, pSID))
            return lElement;
    }
    return NULL;
}

int FbxColladaNamespace::GetParamModificationCount() const
{
    return mParamModification.GetCount();
}

xmlNode * FbxColladaNamespace::GetParamModification(int pIndex) const
{
    return mParamModification[pIndex];
}

#include <fbxsdk/fbxsdk_nsend.h>
