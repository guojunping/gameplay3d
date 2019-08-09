/****************************************************************************************
 
   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/fileio/collada/fbxcolladautils.h>
#include <fbxsdk/fileio/collada/fbxcolladaelement.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

ElementContentAccessor::ElementContentAccessor()
: mContent(NULL), mPointer(NULL)
{
}

ElementContentAccessor::ElementContentAccessor(xmlNode * pElement)
: mContent(NULL), mPointer(NULL)
{
    mContent = xmlNodeGetContent(pElement);
    mPointer = (const char *)mContent;
}

ElementContentAccessor::~ElementContentAccessor()
{
    xmlFree(mContent);
}

const char* ElementBase::smID_PROPERTY_NAME = "COLLADA_ID";

ElementBase::ElementBase()
: mXMLElement(NULL), mID(NULL), mUnit(NULL)
{
}

ElementBase::~ElementBase()
{
    FbxDelete(mID);
    FbxDelete(mUnit);
}

const FbxString & ElementBase::GetID() const
{
    if (!mID)
    {
        mID = FbxNew<FbxString>();
        DAE_GetElementAttributeValue(mXMLElement, COLLADA_ID_PROPERTY, *mID);
    }

    return *mID;
}

const FbxSystemUnit * ElementBase::GetUnit() const
{
    if (!mUnit)
    {
        xmlNode * lAssetElement = DAE_FindChildElementByTag(mXMLElement,
            COLLADA_ASSET_STRUCTURE);
        if (lAssetElement)
        {
            xmlNode * lUnitElement = DAE_FindChildElementByTag(lAssetElement,
                COLLADA_UNIT_STRUCTURE);
            if (lUnitElement)
            {
                double lConversionFactor = 1;
                DAE_GetElementAttributeValue(lUnitElement, COLLADA_METER_PROPERTY, lConversionFactor);
                // FBX is in centimeter and COLLADA in meter
                mUnit = FbxNew<FbxSystemUnit>(lConversionFactor * 100);
            }
        }
    }

    return mUnit;
}

#include <fbxsdk/fbxsdk_nsend.h>
