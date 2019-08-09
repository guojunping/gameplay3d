/****************************************************************************************
 
   Copyright (C) 2017 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/fileio/collada/fbxcolladautils.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

enum TransformItemUsage
{
    eUSAGE_TRANSLATION,
    eUSAGE_ROTATION_OFFSET,
    eUSAGE_ROTATION_PIVOT,
    eUSAGE_PRE_ROTATION,
    eUSAGE_ROTATION,
    eUSAGE_POST_ROTATION,
    eUSAGE_ROTATION_PIVOT_INVERSE,
    eUSAGE_SCALING_OFFSET,
    eUSAGE_SCALING_PIVOT,
    eUSAGE_SCALING,
    eUSAGE_SCALING_PIVOT_INVERSE,
    eUSAGE_UNKNOWN,
};

static short USAGE_MAXIMUM[eUSAGE_UNKNOWN] = {2, 1, 1, 3, 3, 3, 1, 1, 1, 1, 1};

const ColladaLayerTraits ColladaLayerTraits::GetLayerTraits(const FbxString & pLabel)
{
    if (pLabel == COLLADA_NORMAL_INPUT)
        return ColladaLayerTraits(FbxLayerElement::eNormal, 3);
    else if (pLabel == COLLADA_COLOR_INPUT)
        return ColladaLayerTraits(FbxLayerElement::eVertexColor, 3);
    else if (pLabel == COLLADA_MAPPING_INPUT)
        return ColladaLayerTraits(FbxLayerElement::eUV, 2);
    else if (pLabel == COLLADA_TEXCOORD_INPUT)
        return ColladaLayerTraits(FbxLayerElement::eUV, 2);
    else if (pLabel == COLLADA_TEXTANGENT_INPUT)
        return ColladaLayerTraits(FbxLayerElement::eTangent, 3);
    else if (pLabel == COLLADA_TEXBINORMAL_INPUT)
        return ColladaLayerTraits(FbxLayerElement::eBiNormal, 3);
    else
    {
        FBX_ASSERT_NOW("Unknown property for polygons");
        return ColladaLayerTraits();
    }
}

#define INITIALIZE_BUFFER\
    FbxAutoFreePtr<char> lStr((char*)FbxMalloc(lGuessStringLen));\
    if( !lStr )\
    {\
        FBX_ASSERT_MSG(false, "Insufficient memory is available!");\
        return;\
    }\
    FBXSDK_strcpy(lStr, lGuessStringLen, "\n");

#define CHECK_REALLOCATE(size)\
    if( (size) >= lGuessStringLen )\
    {\
        lGuessStringLen = 2*(size);\
        lStr.Reset((char*)FbxRealloc(lStr.Release(), lGuessStringLen));\
        if( !lStr )\
        {\
            FBX_ASSERT_MSG(false,"Insufficient memory is available!");\
            return;\
        }\
    }

FbxString arrayOfType(FbxString typeProperty);
xmlNode* createChildArray(xmlNode* parent, const char* typeProperty, const char* content, const char* arrayId, int count);

void DAE_AddFlow(xmlNode* node, DAE_Flow flow)
//
// Description:
//     Add "flow" attribute to a <parameter>
//
{
    switch(flow)
    {
        case kCOLLADAFlowIn:        xmlNewProp(node, XML_STR COLLADA_FLOW_PROPERTY, XML_STR COLLADA_IN_FLOW);break;
        case kCOLLADAFlowOut:        xmlNewProp(node, XML_STR COLLADA_FLOW_PROPERTY, XML_STR COLLADA_OUT_FLOW);break;
        case kCOLLADAFlowInOut:        xmlNewProp(node, XML_STR COLLADA_FLOW_PROPERTY, XML_STR COLLADA_INOUT_FLOW);break;
    }
}

xmlNode* DAE_AddParameter(xmlNode* parentXmlNode, const char* name, const FbxColor& color, DAE_Flow flow)
//
// Description:
//     Add <parameter> of type "double3"
//
{
    FbxString lStr = FbxString(color.mRed) + " ";
    lStr += FbxString(color.mGreen) + FbxString(" ") + FbxString(color.mBlue);
    xmlNode* paramNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR lStr.Buffer());
    xmlNewProp(paramNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR name);
    xmlNewProp(paramNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR COLLADA_FLOAT3_TYPE);
    DAE_AddFlow(paramNode, flow);
    return paramNode;
}

xmlNode* DAE_AddParameter(xmlNode* parentXmlNode, const char* name, const FbxDouble3& color)
//
// Description:
//     Add <parameter> of type "double3"
//     For COLLADA 1.4,
//
{
    FbxString lStr = FbxString(color[0]) + " ";
    lStr += FbxString(" ") + FbxString(color[1]);
    lStr += FbxString(" ") + FbxString(color[2]);
    lStr += FbxString(" ") + FbxString(1.);
    xmlNode* paramNode = xmlNewChild(parentXmlNode, NULL, XML_STR name, NULL);
    xmlNode* valueNode = xmlNewChild(paramNode, NULL, XML_STR COLLADA_FXSTD_COLOR_ELEMENT, XML_STR lStr.Buffer());
    xmlNewProp(valueNode, XML_STR COLLADA_SUBID_PROPERTY, XML_STR name);
    return paramNode;
}

xmlNode* DAE_AddParameter(xmlNode* parentXmlNode, const char* name, const FbxColor& color)
//
// Description:
//     Add <parameter> of type "double3"
//     For COLLADA 1.4,
//
{
    FbxString lStr = FbxString(color.mRed) + " ";
    lStr += FbxString(" ") + FbxString(color.mGreen);
    lStr += FbxString(" ") + FbxString(color.mBlue);
    lStr += FbxString(" ") + FbxString(color.mAlpha);
    xmlNode* paramNode = xmlNewChild(parentXmlNode, NULL, XML_STR name, NULL);
    xmlNode* valueNode = xmlNewChild(paramNode, NULL, XML_STR COLLADA_FXSTD_COLOR_ELEMENT, XML_STR lStr.Buffer());
    xmlNewProp(valueNode, XML_STR COLLADA_SUBID_PROPERTY, XML_STR name);
    return paramNode;
}

xmlNode* DAE_AddParameter(xmlNode* parentXmlNode, const char* name, const FbxVector4& vector, DAE_Flow flow)
//
// Description:
//     Add <parameter> of type "float3"
//
{
    FbxString lStr = FbxString(vector.mData[0]) + " ";
    lStr += FbxString(vector.mData[1]) + " ";
    lStr += FbxString(vector.mData[2]);
    xmlNode* paramNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR lStr.Buffer());

    xmlNewProp(paramNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR name);
    xmlNewProp(paramNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR COLLADA_FLOAT3_TYPE);
    DAE_AddFlow(paramNode, flow);
    return paramNode;
}
xmlNode* DAE_AddParameter(xmlNode* parentXmlNode, const char* name, const FbxVector4& vector)
//
// Description:
//     Add <parameter> of type "float3"
//     For COLLADA 1.4,
//
{
    FbxString lStr = FbxString(vector.mData[0]) + " ";
    lStr += FbxString(vector.mData[1]) + " ";
    lStr += FbxString(vector.mData[2]);
    xmlNode* paramNode = xmlNewChild(parentXmlNode, NULL, XML_STR name, NULL);
    xmlNode* valueNode = xmlNewChild(paramNode, NULL, XML_STR COLLADA_FLOAT3_TYPE, XML_STR lStr.Buffer());
    xmlNewProp(valueNode, XML_STR COLLADA_SUBID_PROPERTY, XML_STR name);
    return paramNode;
}

xmlNode* DAE_AddParameter(xmlNode* parentXmlNode, const char* name, double value, DAE_Flow flow)
{
    FbxString lStr = FbxString(value);
    xmlNode* paramNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR lStr.Buffer());
    xmlNewProp(paramNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR name);
    xmlNewProp(paramNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "float");
    DAE_AddFlow(paramNode, flow);
    return paramNode;
}
xmlNode* DAE_AddParameter(xmlNode* parentXmlNode, const char* name, double value)
{
    // For COLLADA 1.4
    FbxString lStr = FbxString(value);
    xmlNode* paramNode = xmlNewChild(parentXmlNode, NULL, XML_STR name, NULL);
    xmlNode* valueNode = xmlNewChild(paramNode, NULL, XML_STR "float", XML_STR lStr.Buffer());
    xmlNewProp(valueNode, XML_STR COLLADA_SUBID_PROPERTY, XML_STR name);
    return paramNode;
}


xmlNode* DAE_AddParameter(xmlNode* parentXmlNode, const char* name, bool value, DAE_Flow flow)
{
    xmlNode* paramNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, 
        XML_STR (value ? COLLADA_TRUE_KEYWORD : COLLADA_FALSE_KEYWORD));
    xmlNewProp(paramNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR name);
    xmlNewProp(paramNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "bool");
    DAE_AddFlow(paramNode, flow);
    return paramNode;
}
xmlNode* DAE_AddParameter(xmlNode* parentXmlNode, const char* name, bool value)
{
    // For COLLADA 1.4
    xmlNode* paramNode = xmlNewChild(parentXmlNode, NULL, XML_STR name, 
        XML_STR (value ? COLLADA_TRUE_KEYWORD : COLLADA_FALSE_KEYWORD));
    xmlNewProp(paramNode, XML_STR COLLADA_SUBID_PROPERTY, XML_STR name);
    xmlNewChild(paramNode, NULL, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "bool");
    return paramNode;
}


xmlNode* DAE_AddParameter(xmlNode* parentXmlNode, const char* name, const char* type, const char* value, DAE_Flow flow)
{
    xmlNode* paramNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR value);
    xmlNewProp(paramNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR name);
    if (type != NULL)
        xmlNewProp(paramNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR type);
    DAE_AddFlow(paramNode, flow);
    return paramNode;
}
xmlNode* DAE_AddParameter(xmlNode* parentXmlNode, const char* name, const char* type, const char* value)
{
    // For COLLADA 1.4
    xmlNode* paramNode = xmlNewChild(parentXmlNode, NULL, XML_STR name, XML_STR value);
    xmlNewProp(paramNode, XML_STR COLLADA_SUBID_PROPERTY, XML_STR name);
    if (type != NULL)
        xmlNode* valueNode = xmlNewChild(paramNode, NULL, XML_STR type, XML_STR value);
    return paramNode;
}

xmlNode* DAE_AddTechnique(xmlNode* parentXmlNode, const char* technique)
{
    xmlNode* techniqueNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_TECHNIQUE_STRUCTURE, NULL);
    xmlNewProp(techniqueNode, XML_STR COLLADA_PROFILE_PROPERTY, XML_STR technique);
    return techniqueNode;
}


void DAE_AddInput(xmlNode* parentXmlNode, const char* semantic, const char* source, int idx)
//
// Description:
//     Adds: <input semantic="semantic" source="#source" />
//
{
    xmlNode* inputNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_INPUT_STRUCTURE, NULL);
    xmlNewProp(inputNode, XML_STR COLLADA_SEMANTIC_PROPERTY, XML_STR semantic);

    if (idx != -1)
    {
        FbxString lStr = FbxString(idx);
        xmlNewProp(inputNode, XML_STR COLLADA_IDX_PROPERTY, XML_STR lStr.Buffer());
    }

    if (source)
    {
        FbxString lStr = FbxString("#") + source;
        xmlNewProp(inputNode, XML_STR COLLADA_SOURCE_PROPERTY, XML_STR lStr.Buffer());
    }
}
void DAE_AddInput14(xmlNode* parentXmlNode, const char* semantic, const char* source, int offset, int set)
//
// Description:
//     Adds: <input semantic="semantic" source="#source" />
//
{
    xmlNode* inputNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_INPUT_STRUCTURE, NULL);
    xmlNewProp(inputNode, XML_STR COLLADA_SEMANTIC_PROPERTY, XML_STR semantic);

    if (offset != -1)
    {
        FbxString lStr = FbxString(offset);
        xmlNewProp(inputNode, XML_STR COLLADA_OFFSET_PROPERTY, XML_STR lStr.Buffer());
    }

    if (set != -1)
    {
        FbxString lStr = FbxString(set);
        xmlNewProp(inputNode, XML_STR COLLADA_SET_PROPERTY, XML_STR lStr.Buffer());
    }

    if (source)
    {
        FbxString lStr = FbxString("#") + source;
        xmlNewProp(inputNode, XML_STR COLLADA_SOURCE_PROPERTY, XML_STR lStr.Buffer());
    }
}

const FbxString DAE_GetIDFromUrlAttribute(xmlNode* instance, FbxString& externalRef)
{
    externalRef = "";
    const FbxString lURL = DAE_GetElementAttributeValue(instance, COLLADA_URL_PROPERTY);
    if (!lURL.IsEmpty())
    {
        const int lPoundSignIndex = lURL.Find('#');
        if (lPoundSignIndex != -1)
        {
            externalRef = lURL.Left(lPoundSignIndex);
            return lURL.Mid(lPoundSignIndex + 1);
        }
    }
    return FbxString();
}

const FbxString DAE_GetIDFromSourceAttribute(xmlNode* instance)
{
    const FbxString lSource = DAE_GetElementAttributeValue(instance, COLLADA_SOURCE_PROPERTY);
    if (!lSource.IsEmpty())
    {
        const int lPoundSignIndex = lSource.Find('#');
        if (lPoundSignIndex != -1)
            return lSource.Mid(lPoundSignIndex + 1);
    }
    return FbxString();
}

const FbxString DAE_GetIDFromTargetAttribute(xmlNode* instance)
{
    const FbxString lTarget = DAE_GetElementAttributeValue(instance, COLLADA_TARGET_PROPERTY);
    if (!lTarget.IsEmpty())
    {
        const int lPoundSignIndex = lTarget.Find('#');
        if (lPoundSignIndex != -1)
            return lTarget.Mid(lPoundSignIndex + 1);
        else
            return lTarget;
    }
    return FbxString();
}

const FbxString DAE_GetElementAttributeValue(xmlNode * pElement, const char * pAttributeName)
{
    FbxString lReturn;
    DAE_GetElementAttributeValue(pElement, pAttributeName, lReturn);
    return lReturn;
}

bool DAE_CompareAttributeValue(xmlNode * pElement, const char * pAttributeName, const char * pValue)
{
    if (!pElement || !pAttributeName)
        return false;

	xmlChar* lPropertyValue = xmlGetProp(pElement, (const xmlChar *)pAttributeName);
    if (lPropertyValue)
    {
		bool ret = strcmp((const char *)lPropertyValue, pValue) == 0;
		xmlFree(lPropertyValue);
		return ret;
    }
    return false;
}

void DAE_ExportArray(xmlNode* parentXmlNode, const char* id, FbxArray<FbxVector4>& arr)
//
// Description:
//     Exports an array of FbxVector4
//
{
    //Pre-calculate the string size 
    FbxString lUnitDouble(0.0);
    size_t lGuessStringLen = (lUnitDouble.GetLen() + 4) * arr.GetCount() * 3 + 2;
    INITIALIZE_BUFFER
    size_t lLen = 1;
    FbxString lTmp0,lTmp1,lTmp2;

    for (int i = 0; i < arr.GetCount(); i++)
    {
        FbxVector4 lV = arr.GetAt(i);
        lTmp0 = lV[0];lTmp1 = lV[1];lTmp2 = lV[2]; 
        size_t lTmp0Size = lTmp0.GetLen();
        size_t lTmp1Size = lTmp1.GetLen();
        size_t lTmp2Size = lTmp2.GetLen();
        size_t lNeedsize = lTmp0Size + lTmp1Size + lTmp2Size + 3;
        CHECK_REALLOCATE(lLen + lNeedsize)
        
        memcpy(&lStr[lLen], lTmp0.Buffer(),lTmp0Size); lLen += lTmp0Size;
        lStr[lLen] = ' ';                              lLen ++;
        memcpy(&lStr[lLen], lTmp1.Buffer(),lTmp1Size); lLen += lTmp1Size;
        lStr[lLen] = ' ';                              lLen ++;
        memcpy(&lStr[lLen], lTmp2.Buffer(),lTmp2Size); lLen += lTmp2Size;
        lStr[lLen] = '\n';                             lLen ++;        
    }
    lStr[lLen] = '\0';

    // Create the typed array node.
    //
    xmlNode* arrayNode = createChildArray(parentXmlNode, COLLADA_FLOAT_TYPE, lStr, id, (int)(arr.GetCount()*3));
}

void DAE_ExportArray(xmlNode* parentXmlNode, const char* id, FbxArray<FbxVector2>& arr)
//
// Description:
//     Exports an array of FbxVector2
//
{
    //Pre-calculate the string size 
    FbxString lUnitDouble(0.0);
    size_t lGuessStringLen = (lUnitDouble.GetLen() + 4) * arr.GetCount() * 2 + 2;
    INITIALIZE_BUFFER
    size_t lLen = 1;
    FbxString lTmp0,lTmp1;

    for (int i = 0; i < arr.GetCount(); i++)
    {
        FbxVector2 lV = arr.GetAt(i);
        lTmp0 = lV[0];lTmp1 = lV[1];
        size_t lTmp0Size = lTmp0.GetLen();
        size_t lTmp1Size = lTmp1.GetLen();
        size_t lNeedsize = lTmp0Size + lTmp1Size + 2;
        CHECK_REALLOCATE(lLen + lNeedsize)
        
        memcpy(&lStr[lLen], lTmp0.Buffer(),lTmp0Size); lLen += lTmp0Size;
        lStr[lLen] = ' ';                              lLen ++;
        memcpy(&lStr[lLen], lTmp1.Buffer(),lTmp1Size); lLen += lTmp1Size;
        lStr[lLen] = '\n';                             lLen ++; 
    }
    lStr[lLen] = '\0';

    // Create the typed array node.
    //
    xmlNode* arrayNode = createChildArray(parentXmlNode, COLLADA_FLOAT_TYPE, lStr, id, (int)(arr.GetCount()* 2));
}

void DAE_ExportArray(xmlNode* parentXmlNode, const char* id, FbxArray<double>& arr)
//
// Description:
//     Exports a single double array
//     Change lines every 16 element.
//
{
    //Pre-calculate the string size 
    FbxString lUnitDouble(0.0);
    size_t lGuessStringLen = (lUnitDouble.GetLen() + 4) * arr.GetCount() + 2;
    INITIALIZE_BUFFER
    size_t lLen = 1;
    FbxString lTmp;
    
    for (int i = 0; i < arr.GetCount(); i++) 
    {
        lTmp = arr.GetAt(i);
        size_t lTmpSize = lTmp.GetLen();
        size_t lNeedsize = lTmpSize + 1;
        CHECK_REALLOCATE(lLen + lNeedsize)
        
        if (i % 16 == 0) 
            lStr[lLen] = '\n';
        else if (i > 0) 
           lStr[lLen] = ' ';
        lLen ++;        
        memcpy(&lStr[lLen], lTmp.Buffer(),lTmpSize);  lLen += lTmpSize;
    }
    lStr[lLen] = '\0';

    // Create the typed array node.
    //
    xmlNode* arrayNode = createChildArray(parentXmlNode, COLLADA_FLOAT_TYPE, lStr, id, arr.GetCount());
}

void DAE_ExportArray(xmlNode* parentXmlNode, const char* id, FbxArray<FbxColor>& colors)
//
// Description:
//     Exports a single color array
//
{
    // Convert the array to a long string
    //
    int length = colors.GetCount();
    
    //Pre-calculate the string size 
    FbxString lUnitDouble(0.0);
    size_t lGuessStringLen = (lUnitDouble.GetLen() + 4) * colors.GetCount() * 4 + 2;
    INITIALIZE_BUFFER
    size_t lLen = 1;
    FbxString lTmp0,lTmp1,lTmp2,lTmp3;
    
    for (int i = 0; i < length; i++)
    {
        lTmp0 = colors[i].mRed;  lTmp1 = colors[i].mGreen;
        lTmp2 = colors[i].mBlue; lTmp3 = colors[i].mAlpha;
        size_t lTmp0Size = lTmp0.GetLen();
        size_t lTmp1Size = lTmp1.GetLen();
        size_t lTmp2Size = lTmp2.GetLen();
        size_t lTmp3Size = lTmp3.GetLen();
        size_t lNeedsize = lTmp0Size + lTmp1Size + lTmp2Size + lTmp3Size + 4;
        CHECK_REALLOCATE(lLen + lNeedsize)
        
        memcpy(&lStr[lLen], lTmp0.Buffer(),lTmp0Size); lLen += lTmp0Size;
        lStr[lLen] = ' ';                              lLen ++;
        memcpy(&lStr[lLen], lTmp1.Buffer(),lTmp1Size); lLen += lTmp1Size;
        lStr[lLen] = ' ';                              lLen ++;
        memcpy(&lStr[lLen], lTmp2.Buffer(),lTmp2Size); lLen += lTmp2Size;
        lStr[lLen] = ' ';                              lLen ++;
        memcpy(&lStr[lLen], lTmp3.Buffer(),lTmp3Size); lLen += lTmp3Size;
        lStr[lLen] = '\n';                             lLen ++;
    }
    lStr[lLen] = '\0';

    // <array name="name-array" type="double" count="xx">
    //
    xmlNode* arrayNode = createChildArray(parentXmlNode, COLLADA_FLOAT_TYPE, lStr, id, length *  4);
}

void DAE_ExportArray(xmlNode* parentXmlNode, const char* id, FbxStringList& arr)
{
    //Pre-calculate the string size 
    size_t lGuessStringLen = 2;    
    for (int c = 0; c < arr.GetCount(); ++c)
    {
        lGuessStringLen += arr[c].GetLen() + 1;
    }
    INITIALIZE_BUFFER
    
    size_t lLen = 1;
    size_t lLineLength = 0;
    for (int i = 0; i < arr.GetCount(); ++i)
    {
        size_t iSize = arr[i].GetLen();
        lLineLength += iSize;
        if (lLineLength > 70) 
        {
            lStr[lLen] = '\n'; lLineLength = 0;
        }
        else if (lLineLength > 0) 
            lStr[lLen] = ' ';
        lLen++;
        memcpy(&lStr[lLen],arr[i].Buffer(),iSize);
        lLen += iSize;
    }
    lStr[lLen] = '\0';

    // <array name="name-array" type="double" count="xx">
    // 
    xmlNode* arrayNode = createChildArray(parentXmlNode, COLLADA_NAME_TYPE, lStr, id, (int)(arr.GetCount()));
}

void DAE_ExportSourceArray(xmlNode* sourceNode, const char* id, FbxArray<FbxColor>& arr)
{
    // To avoid additional memory copies creating the strings "<id>-array"
    // and "#<id>-array" for the accessor, we'll just create the latter,
    // and point past the '#' to get the former - dirty, but faster
    //
    FbxString idStr("#");
    idStr += id;
    idStr += "-array"; 

    // <array name="name-array" type="double" count="xx"> 
    //
    DAE_ExportArray(sourceNode, idStr.Buffer() + 1, arr);

    // accessor
    xmlNode* techniqueNode = DAE_AddTechnique(sourceNode, COLLADA_GENERIC_TECHNIQUE);
    xmlNode* accessorNode = xmlNewChild(techniqueNode, NULL, XML_STR COLLADA_ACCESSOR_STRUCTURE, XML_STR NULL);
    xmlNewProp(accessorNode, XML_STR COLLADA_SOURCE_STRUCTURE, XML_STR idStr.Buffer());
    
    FbxString lLength = arr.GetCount();
    xmlNewProp(accessorNode, XML_STR "count", XML_STR lLength.Buffer());
    xmlNewProp(accessorNode, XML_STR "stride", XML_STR "4");    

    xmlNode* fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "R");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "double");
    DAE_AddFlow(fieldNode, kCOLLADAFlowOut);

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "G");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "double");
    DAE_AddFlow(fieldNode, kCOLLADAFlowOut);

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "B");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "double");
    DAE_AddFlow(fieldNode, kCOLLADAFlowOut);

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "A");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "double");
    DAE_AddFlow(fieldNode, kCOLLADAFlowOut);
}

xmlNode* DAE_ExportSource14(xmlNode* parentXmlNode, const char* id, FbxArray<FbxVector4>& arr)
{
    // <source>
    //
    xmlNode* sourceNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_SOURCE_STRUCTURE, NULL);
    
    if (id != NULL)
        xmlNewProp(sourceNode, XML_STR COLLADA_ID_PROPERTY, XML_STR id);

    // To avoid additional memory copies creating the strings "<id>-array"
    // and "#<id>-array" for the accessor, we'll just create the latter,
    // and point past the '#' to get the former - dirty, but faster
    //
    FbxString str("#");
    str += id;
    str += "-array"; 
    
    // <array>
    //
    DAE_ExportArray(sourceNode, str.Buffer() + 1, arr);

    // accessor
    //
    DAE_AddXYZAccessor14(sourceNode, COLLADA_GENERIC_TECHNIQUE, id, str.Buffer(), arr.GetCount());

    return sourceNode;
}

xmlNode* DAE_ExportSource14(xmlNode* parentXmlNode, const char* id, FbxArray<FbxVector2>& arr)
{
    // <source>
    //
    xmlNode* sourceNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_SOURCE_STRUCTURE, NULL);
    
    if (id != NULL)
        xmlNewProp(sourceNode, XML_STR COLLADA_ID_PROPERTY, XML_STR id);

    // To avoid additional memory copies creating the strings "<id>-array"
    // and "#<id>-array" for the accessor, we'll just create the latter,
    // and point past the '#' to get the former - dirty, but faster
    //
    FbxString str("#");
    str += id;
    str += "-array"; 
    
    // <array>
    //
    DAE_ExportArray(sourceNode, str.Buffer() + 1, arr);

    // accessor
    //
    DAE_AddSTAccessor14(sourceNode, COLLADA_GENERIC_TECHNIQUE, id, str.Buffer(), arr.GetCount());

    return sourceNode;
}


xmlNode* DAE_ExportSource14(xmlNode* parentXmlNode, const char* id, FbxArray<FbxColor>& arr)
//
// Description:
//     Exports a single color array
//
{
    // <source>
    //
    xmlNode* sourceNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_SOURCE_STRUCTURE, NULL);
    xmlNewProp(sourceNode, XML_STR COLLADA_ID_PROPERTY, XML_STR id);

    DAE_ExportSourceArray14(sourceNode, id, arr);

    return sourceNode;
}

void DAE_ExportSourceArray14(xmlNode* sourceNode, const char* id, FbxArray<FbxColor>& arr)
{
    // To avoid additional memory copies creating the strings "<id>-array"
    // and "#<id>-array" for the accessor, we'll just create the latter,
    // and point past the '#' to get the former - dirty, but faster
    //
    FbxString idStr("#");
    idStr += id;
    idStr += "-array"; 

    // <array name="name-array" type="double" count="xx"> 
    //
    DAE_ExportArray(sourceNode, idStr.Buffer() + 1, arr);

    // accessor
    xmlNode* techniqueNode = xmlNewChild(sourceNode, NULL, XML_STR COLLADA_TECHNIQUE_COMMON_ELEMENT, NULL);
    xmlNode* accessorNode = xmlNewChild(techniqueNode, NULL, XML_STR COLLADA_ACCESSOR_STRUCTURE, XML_STR NULL);
    xmlNewProp(accessorNode, XML_STR COLLADA_SOURCE_STRUCTURE, XML_STR idStr.Buffer());
    
    FbxString lLength = arr.GetCount();
    xmlNewProp(accessorNode, XML_STR "count", XML_STR lLength.Buffer());
    xmlNewProp(accessorNode, XML_STR "stride", XML_STR "4");    

    xmlNode* fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "R");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "double");

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "G");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "double");

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "B");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "double");

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "A");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "double");
}

xmlNode* DAE_ExportSource14(xmlNode* parentXmlNode, const char* id, FbxArray<FbxAMatrix>& arr)
//
// Description:
//     Exports transformation matrices
//
{
    // <source>
    //
    xmlNode* sourceNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_SOURCE_STRUCTURE, NULL);
    xmlNewProp(sourceNode, XML_STR COLLADA_ID_PROPERTY, XML_STR id);

    // Make an array of doubles with the matrices array
    FbxArray<double> arr_doubles;
    arr_doubles.Resize(arr.GetCount() * 16);
    for (int k = 0; k < arr.GetCount(); k++) {
        FbxAMatrix lM = arr[k];
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                //FbxAMatrix is column-major matrix, need to transfer to row-major for writing Collada document.
                arr_doubles.SetAt((16*k + 4*j + i),lM.mData[i][j]); 
            }
        }
    }

    FbxString arrayIdStr("#");
    arrayIdStr += id;
    arrayIdStr += "-array"; 
    DAE_ExportArray(sourceNode, arrayIdStr.Buffer() + 1, arr_doubles);
    
    // Accessor.
    //
    FbxString accessorIdStr(id);
    accessorIdStr += "-accessor"; 
    xmlNode* techniqueNode = xmlNewChild(sourceNode, NULL, XML_STR COLLADA_TECHNIQUE_COMMON_ELEMENT, NULL);
    DAE_ExportAccessor14(techniqueNode, accessorIdStr.Buffer(), arrayIdStr.Buffer(), arr.GetCount(), 16, NULL, COLLADA_FXCMN_FLOAT4X4_ELEMENT);

    return sourceNode;
}

xmlNode* DAE_ExportSource14(xmlNode* parentXmlNode, const char* id, FbxArray<FbxMatrix>& arr)
//
// Description:
//     Exports transformation matrices
//
{
    // <source>
    //
    xmlNode* sourceNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_SOURCE_STRUCTURE, NULL);
    xmlNewProp(sourceNode, XML_STR COLLADA_ID_PROPERTY, XML_STR id);

    // Make an array of doubles with the matrices array
    FbxArray<double> arr_doubles;
    arr_doubles.Resize(arr.GetCount()*16);
    for (int k = 0; k < arr.GetCount(); k++) {
        FbxMatrix lM = arr[k];
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                arr_doubles.SetAt((16*k + 4*i + j), lM.mData[i][j]);
            }
        }
    }

    FbxString arrayIdStr("#");
    arrayIdStr += id;
    arrayIdStr += "-array"; 
    DAE_ExportArray(sourceNode, arrayIdStr.Buffer() + 1, arr_doubles);
    
    // Accessor.
    //
    FbxString accessorIdStr(id);
    accessorIdStr += "-accessor"; 
    xmlNode* techniqueNode = xmlNewChild(sourceNode, NULL, XML_STR COLLADA_TECHNIQUE_COMMON_ELEMENT, NULL);
    DAE_ExportAccessor14(techniqueNode, accessorIdStr.Buffer(), arrayIdStr.Buffer(), arr.GetCount(), 16, NULL, COLLADA_FXCMN_FLOAT4X4_ELEMENT);

    return sourceNode;
}


xmlNode* DAE_ExportSource14(xmlNode* parentXmlNode, const char* id, FbxStringList& accessorParams, FbxArray<double>& arr, bool isCommonProfile)
{
    xmlNode* sourceNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_SOURCE_STRUCTURE, NULL);
    xmlNewProp(sourceNode, XML_STR COLLADA_ID_PROPERTY, XML_STR id);

    // Source Array
    FbxString arrayIdStr("#");
    arrayIdStr += id;
    arrayIdStr += "-array"; 
    DAE_ExportArray(sourceNode, arrayIdStr.Buffer() + 1, arr);

    // Source Accessor
    FbxString accessorIdStr(id);
    accessorIdStr += "-accessor"; 
    int stride = accessorParams.GetCount();
    xmlNode *techniqueNode;
    if (isCommonProfile) 
        techniqueNode = xmlNewChild(sourceNode, NULL, XML_STR COLLADA_TECHNIQUE_COMMON_ELEMENT, NULL);
    else
        techniqueNode = DAE_AddTechnique(sourceNode, COLLADA_MAYA_PROFILE);
    xmlNode* accessorNode;
    if (stride > 0)
        for (int i = 0; i < stride; i++)
            accessorNode = DAE_ExportAccessor14(techniqueNode, accessorIdStr.Buffer(), arrayIdStr.Buffer(), arr.GetCount() / stride, stride, accessorParams.GetStringAt(i), COLLADA_FLOAT_TYPE);
    else
        accessorNode = DAE_ExportAccessor14(techniqueNode, accessorIdStr.Buffer(), arrayIdStr.Buffer(), arr.GetCount(), 1, NULL, COLLADA_FLOAT_TYPE);

    return sourceNode;
}

xmlNode* DAE_ExportSource14(xmlNode* parentXmlNode, const char* id, const char* accessorParam, FbxArray<double>& arr, int stride, const char* type, bool isCommonProfile)
//
// Description:
//     Exports a single double array
//
{
    // <source>
    //
    xmlNode* sourceNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_SOURCE_STRUCTURE, NULL);
    xmlNewProp(sourceNode, XML_STR COLLADA_ID_PROPERTY, XML_STR id);

    // To avoid additional memory copies creating the strings "<id>-array"
    // and "#<id>-array" for the accessor, we'll just create the latter,
    // and point past the '#' to get the former - dirty, but faster
    //
    // <array name="name-array" type="double" count="xx">
    //
    FbxString arrayIdStr("#");
    arrayIdStr += id;
    arrayIdStr += "-array"; 
    DAE_ExportArray(sourceNode, arrayIdStr.Buffer() + 1, arr);

    // Accessor.
    //
    FbxString accessorIdStr("#");
    accessorIdStr += id;
    accessorIdStr += "-accessor"; 
    xmlNode* techniqueNode = DAE_AddTechnique(sourceNode, isCommonProfile ? COLLADA_TECHNIQUE_COMMON_ELEMENT : COLLADA_MAYA_PROFILE);
    DAE_ExportAccessor(techniqueNode, accessorIdStr.Buffer() + 1, arrayIdStr.Buffer(), arr.GetCount() / stride, stride, accessorParam, type);

    return sourceNode;
}

xmlNode* DAE_ExportSource14(xmlNode* parentXmlNode, const char* id, FbxStringList& arr, const char* type, bool isCommonProfile)
{
    FbxString arrayIdStr = FbxString("#") + id + "-array";
    FbxString accessorIdStr = FbxString("#") + id + "-accessor"; 

    // <source>
    //
    xmlNode* sourceNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_SOURCE_STRUCTURE, NULL);
    xmlNewProp(sourceNode, XML_STR COLLADA_ID_PROPERTY, XML_STR id);

    // <array name="name-array" type="double" count="xx">
    //
    DAE_ExportArray(sourceNode, arrayIdStr.Buffer() + 1, arr);

    // Accessor.
    //
    xmlNode* techniqueNode;
    if (isCommonProfile) 
        techniqueNode = xmlNewChild(sourceNode, NULL, XML_STR COLLADA_TECHNIQUE_COMMON_ELEMENT, NULL);
    else
        techniqueNode = DAE_AddTechnique(sourceNode, COLLADA_MAYA_PROFILE);
    DAE_ExportAccessor14(techniqueNode, accessorIdStr.Buffer() + 1, arrayIdStr.Buffer(), arr.GetCount(), 1, NULL, type);

    return sourceNode;
}

xmlNode* DAE_ExportAccessor(xmlNode* parentXmlNode, const char* id, const char* arrayRef, int count, int stride, const char* name, const char* type)
{
    xmlNode* accessor = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_ACCESSOR_STRUCTURE, NULL);
    xmlNewProp(accessor, XML_STR COLLADA_SOURCE_PROPERTY, XML_STR arrayRef);

    FbxString lCount(count);
    xmlNewProp(accessor, XML_STR COLLADA_COUNT_PROPERTY, XML_STR lCount.Buffer());
    if (stride != 1)
    {
        FbxString lStride(stride);
        xmlNewProp(accessor, XML_STR COLLADA_STRIDE_PROPERTY, XML_STR lStride.Buffer());
    }

    xmlNode* param = xmlNewChild(accessor, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, NULL);
    if ( name && strlen( name ) ) {
        xmlNewProp(param, XML_STR COLLADA_NAME_PROPERTY, XML_STR name);
    }
    xmlNewProp(param, XML_STR COLLADA_TYPE_PROPERTY, XML_STR type);
    DAE_AddFlow(param, kCOLLADAFlowOut);

    return accessor;
}

xmlNode* DAE_ExportAccessor14(xmlNode* parentXmlNode, const char* id, const char* arrayRef, int count, int stride, const char* name, const char* type)
{
    xmlNode* accessor = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_ACCESSOR_STRUCTURE, NULL);
    xmlNewProp(accessor, XML_STR COLLADA_SOURCE_PROPERTY, XML_STR arrayRef);

    FbxString lCount(count);
    xmlNewProp(accessor, XML_STR COLLADA_COUNT_PROPERTY, XML_STR lCount.Buffer());
    if (stride != 1)
    {
        FbxString lStride(stride);
        xmlNewProp(accessor, XML_STR COLLADA_STRIDE_PROPERTY, XML_STR lStride.Buffer());
    }

    xmlNode* param = xmlNewChild(accessor, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, NULL);
    if ( name && strlen( name ) ) {
        xmlNewProp(param, XML_STR COLLADA_NAME_PROPERTY, XML_STR name);
    }
    xmlNewProp(param, XML_STR COLLADA_TYPE_PROPERTY, XML_STR type);

    return accessor;
}

void DAE_AddXYZAccessor(xmlNode* parentXmlNode, const char* profile, const char* arrayName, const char* arrayRef, int count)
//    <technique profile=COLLADA_PROFILE_PROPERTY>
//            <accessor source="#name-array" count="x" stride="3" />
//    </technique>
//
{
    xmlNode* techniqueNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_TECHNIQUE_STRUCTURE, XML_STR NULL);
    xmlNewProp(techniqueNode, XML_STR COLLADA_PROFILE_PROPERTY, XML_STR profile);
    xmlNode* accessorNode = xmlNewChild(techniqueNode, NULL, XML_STR COLLADA_ACCESSOR_STRUCTURE, XML_STR NULL);
    xmlNewProp(accessorNode, XML_STR COLLADA_SOURCE_STRUCTURE, XML_STR arrayRef);
    FbxString lStr = FbxString(count);
    xmlNewProp(accessorNode, XML_STR "count", XML_STR lStr.Buffer());
    xmlNewProp(accessorNode, XML_STR "stride", XML_STR "3");    

    xmlNode* fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "X");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "float");
    DAE_AddFlow(fieldNode, kCOLLADAFlowOut);

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "Y");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "float");
    DAE_AddFlow(fieldNode, kCOLLADAFlowOut);

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "Z");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "float");
    DAE_AddFlow(fieldNode, kCOLLADAFlowOut);
}
void DAE_AddXYZAccessor14(xmlNode* parentXmlNode, const char* profile, const char* arrayName, const char* arrayRef, int count)
//    <technique_common>
//            <accessor source="#name-array" count="x" stride="3" />
//    </technique_common>
//
{
    xmlNode* techniqueNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_TECHNIQUE_COMMON_ELEMENT, XML_STR NULL);
    xmlNode* accessorNode = xmlNewChild(techniqueNode, NULL, XML_STR COLLADA_ACCESSOR_STRUCTURE, XML_STR NULL);
    xmlNewProp(accessorNode, XML_STR COLLADA_SOURCE_STRUCTURE, XML_STR arrayRef);
    FbxString lStr = FbxString(count);
    xmlNewProp(accessorNode, XML_STR "count", XML_STR lStr.Buffer());
    xmlNewProp(accessorNode, XML_STR "stride", XML_STR "3");    

    xmlNode* fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "X");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "float");

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "Y");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "float");

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "Z");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "float");
}


void DAE_AddSTAccessor(xmlNode* parentXmlNode, const char* profile, const char* arrayName, const char* arrayRef, int count)
//    <technique profile=COLLADA_PROFILE_PROPERTY>
//            <accessor source="#name-array" count="x" stride="2" />
//    </technique>
//
{
    xmlNode* techniqueNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_TECHNIQUE_STRUCTURE, XML_STR NULL);
    xmlNewProp(techniqueNode, XML_STR COLLADA_PROFILE_PROPERTY, XML_STR profile);
    xmlNode* accessorNode = xmlNewChild(techniqueNode, NULL, XML_STR COLLADA_ACCESSOR_STRUCTURE, XML_STR NULL);
    xmlNewProp(accessorNode, XML_STR COLLADA_SOURCE_STRUCTURE, XML_STR arrayRef);
    FbxString lStr = FbxString(count);
    xmlNewProp(accessorNode, XML_STR "count", XML_STR lStr.Buffer());
    xmlNewProp(accessorNode, XML_STR "stride", XML_STR "2");    

    xmlNode* fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "S");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "float");
    DAE_AddFlow(fieldNode, kCOLLADAFlowOut);

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "T");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "float");
    DAE_AddFlow(fieldNode, kCOLLADAFlowOut);
}
void DAE_AddSTAccessor14(xmlNode* parentXmlNode, const char* profile, const char* arrayName, const char* arrayRef, int count)
//    <technique profile=COLLADA_PROFILE_PROPERTY>
//            <accessor source="#name-array" count="x" stride="2" />
//    </technique>
//
{
    xmlNode* techniqueNode = xmlNewChild(parentXmlNode, NULL, XML_STR COLLADA_TECHNIQUE_COMMON_ELEMENT, XML_STR NULL);
    xmlNode* accessorNode = xmlNewChild(techniqueNode, NULL, XML_STR COLLADA_ACCESSOR_STRUCTURE, XML_STR NULL);
    xmlNewProp(accessorNode, XML_STR COLLADA_SOURCE_STRUCTURE, XML_STR arrayRef);
    FbxString lStr = FbxString(count);
    xmlNewProp(accessorNode, XML_STR "count", XML_STR lStr.Buffer());
    xmlNewProp(accessorNode, XML_STR "stride", XML_STR "2");    

    xmlNode* fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "S");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "float");

    fieldNode = xmlNewChild(accessorNode, NULL, XML_STR COLLADA_PARAMETER_STRUCTURE, XML_STR NULL);
    xmlNewProp(fieldNode, XML_STR COLLADA_NAME_PROPERTY, XML_STR "T");
    xmlNewProp(fieldNode, XML_STR COLLADA_TYPE_PROPERTY, XML_STR "float");
}

// Description:
//     Return the typed array name for a given type.
//
FbxString arrayOfType(FbxString typeProperty)
{
    if (typeProperty == COLLADA_FLOAT_TYPE) return FbxString(COLLADA_FLOAT_ARRAY_STRUCTURE);
    else if (typeProperty == COLLADA_NAME_TYPE) return FbxString(COLLADA_NAME_ARRAY_STRUCTURE);
    else if (typeProperty == COLLADA_INT_TYPE) return FbxString(COLLADA_INT_ARRAY_STRUCTURE);
    else if (typeProperty == COLLADA_BOOL_TYPE) return FbxString(COLLADA_BOOL_ARRAY_STRUCTURE);
    else if (typeProperty == COLLADA_IDREF_TYPE) return FbxString(COLLADA_IDREF_ARRAY_STRUCTURE);
    else return FbxString("");
}

// Description:
//        Create a child node for the given 'parent'. The child
// node is an array (either weakly or strongly-typed, depending
// on the value of CExportOptions::exportWeaklyTypedArrays),
// of type 'typeProperty' (e.g. "double"), specified 'content', 
// with an id property set to 'arrayId'. The number of elements
// within the array is specified in 'count'.
//
//        If the 'typeProperty' is not recognized, the function
// defaults to a weakly-typed array.
//
xmlNode* createChildArray(xmlNode* parent,  
    const char* typeProperty, const char* content,
    const char* arrayId, int count)
{
    FbxString typedArray = arrayOfType(typeProperty);

    xmlNode* node = NULL;

    if (typedArray != NULL) {
        node = xmlNewChild(parent, NULL, XML_STR typedArray.Buffer(), XML_STR content);
    }
    else
    {
        node = xmlNewChild(parent, NULL, XML_STR COLLADA_ARRAY_STRUCTURE, XML_STR content);
        xmlNewProp(node, XML_STR COLLADA_TYPE_PROPERTY, XML_STR typeProperty);
    }

    xmlNewProp(node, XML_STR COLLADA_ID_PROPERTY, XML_STR arrayId);

    // Set the count.
    FbxString lStr = FbxString(count);
    xmlNewProp(node, XML_STR COLLADA_COUNT_PROPERTY, XML_STR lStr.Buffer());

    return node;
}

FbxString matrixToString(const FbxAMatrix& mx)
{

    FbxString lStr;
    lStr += FbxString(mx.mData[0][0]) + ' ';
    lStr += FbxString(mx.mData[1][0]) + ' ';
    lStr += FbxString(mx.mData[2][0]) + ' ';
    lStr += FbxString(mx.mData[3][0]) + ' ';
    lStr += FbxString(mx.mData[0][1]) + ' ';
    lStr += FbxString(mx.mData[1][1]) + ' ';
    lStr += FbxString(mx.mData[2][1]) + ' ';
    lStr += FbxString(mx.mData[3][1]) + ' ';
    lStr += FbxString(mx.mData[0][2]) + ' ';
    lStr += FbxString(mx.mData[1][2]) + ' ';
    lStr += FbxString(mx.mData[2][2]) + ' ';
    lStr += FbxString(mx.mData[3][2]) + ' ';
    lStr += FbxString(mx.mData[0][3]) + ' ';
    lStr += FbxString(mx.mData[1][3]) + ' ';
    lStr += FbxString(mx.mData[2][3]) + ' ';
    lStr += FbxString(mx.mData[3][3]) + '\n';
    return lStr;
}

xmlNode* DAE_FindChildElementByAttribute(xmlNode* pParentElement, const char * pAttributeName,
                                         const char * pAttributeValue, const char * pDefaultAttributeValue)
{
    if (pParentElement != NULL)
    {
        xmlNode* kids = pParentElement->children;
        while (kids != NULL)
        {
            if (kids->type == XML_ELEMENT_NODE)
            {
                xmlChar* lPropertyValue = xmlGetProp(kids, (const xmlChar *)pAttributeName);
                bool lSuccess = false;
                if (lPropertyValue)
                {
                    lSuccess = strcmp(reinterpret_cast<const char *>(lPropertyValue), pAttributeValue) == 0;
					xmlFree(lPropertyValue);
                }
                else
                {
                    lSuccess = strcmp(pDefaultAttributeValue, pAttributeValue) == 0;
                }
                if (lSuccess)
                    return kids;
            }
            kids = kids->next;
        }
    }
    return NULL;
}

xmlNode* DAE_FindChildElementByTag(xmlNode* pParentElement, const char * pTag, xmlNode* pFindFrom)
{
    if (pParentElement)
    {
        xmlNode* lChild = pParentElement->children;
        if (pFindFrom)
            lChild = pFindFrom->next;
        while (lChild)
        {
            if (lChild->type == XML_ELEMENT_NODE)
            {
                if (strcmp((const char *)(lChild->name), pTag) == 0)
                    return lChild;
            }
            lChild = lChild->next;
        }
    }
    return NULL;
}

//
// Find a child by type
//
void findChildrenByType(xmlNode* lParentElement, const FbxSet<FbxString>& lTypes, CNodeList& lChildrenElements)
{
    if (lParentElement != NULL)
    {
        xmlNode* lChildElement = lParentElement->children;
        while (lChildElement != NULL)
        {
            if( lChildElement->type == XML_ELEMENT_NODE && lTypes.Find((const char*)lChildElement->name) )
                lChildrenElements.Add(lChildElement);
            lChildElement = lChildElement->next;
        }
    }
}

// Get a list of all the children node of a given type
//
void findChildrenByType(xmlNode* parent, const char * type, CNodeList& children)
{
    if (parent != NULL)
    {
        for (xmlNode* kid = parent->children; kid != NULL; kid = kid->next)
        {
            if (kid->type == XML_ELEMENT_NODE && strcmp((char*)kid->name, type) == 0)
            {
                children.Add(kid);
            }
        }
    }
}

xmlNode* getSourceAccessor(xmlNode* sourceNode)
{
    xmlNode* techniqueNode = DAE_FindChildElementByTag(sourceNode, COLLADA_TECHNIQUE_COMMON_ELEMENT);
    xmlNode* accessorNode = DAE_FindChildElementByTag(techniqueNode, COLLADA_ACCESSOR_STRUCTURE);
    return accessorNode;
}

xmlNode* getTechniqueNode(xmlNode* parent, const char * profile)
{
    if (parent != NULL)
    {
        xmlNode* technique = parent->children;
        while (technique != NULL)
        {
            if (technique->type == XML_ELEMENT_NODE &&
                strcmp((const char*)technique->name, COLLADA_TECHNIQUE_STRUCTURE) == 0 &&
                strcmp(profile, DAE_GetElementAttributeValue(technique, COLLADA_PROFILE_PROPERTY)) == 0)
            {
                return technique;
            }
            technique = technique->next;
        }
    }
    return NULL;
}

void DAE_SetName(FbxObject * pObject, const FbxString & pName, const FbxString & pID)
{
    if (pName.IsEmpty())
        pObject->SetName(pID);
    else
        pObject->SetName(pName);

    if (pID.IsEmpty() == false)
    {
        FbxProperty lIDProperty = FbxProperty::Create(pObject, FbxStringDT, COLLADA_ID_PROPERTY_NAME);
        lIDProperty.Set(pID);
    }
}

bool DAE_CheckCompatibility(xmlNode * pNodeElement)
{
    short lUsageCount[eUSAGE_UNKNOWN] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    TransformItemUsage lLastUsage = eUSAGE_TRANSLATION;

    for (xmlNode* lChildElement = pNodeElement->children; lChildElement != NULL; lChildElement = lChildElement->next)
    {
        if (lChildElement->type != XML_ELEMENT_NODE)
            continue;

        const FbxString lTag = (const char*)lChildElement->name;
        const FbxString lSid = DAE_GetElementAttributeValue(lChildElement, COLLADA_SUBID_PROPERTY);
        TransformItemUsage lUsage = lLastUsage;

        if (lTag == COLLADA_PERSPECTIVE_STRUCTURE)
            continue;
        else if (lTag == COLLADA_MATRIX_STRUCTURE)
            lUsage = eUSAGE_UNKNOWN;
        else if (lTag == COLLADA_ROTATE_STRUCTURE)
        {
            if (lSid == COLLADA_PRE_ROTATION_X || lSid == COLLADA_PRE_ROTATION_Y || lSid == COLLADA_PRE_ROTATION_Z)
                lUsage = eUSAGE_PRE_ROTATION;
            else if (lSid == COLLADA_POST_ROTATION_X || lSid == COLLADA_POST_ROTATION_Y || lSid == COLLADA_POST_ROTATION_Z)
                lUsage = eUSAGE_POST_ROTATION;
            else if (lSid == COLLADA_ROTATE_AXIS_X || lSid == COLLADA_ROTATE_AXIS_Y || lSid == COLLADA_ROTATE_AXIS_Z)
                lUsage = eUSAGE_POST_ROTATION;
            else if (lSid == COLLADA_ROTATE_X || lSid == COLLADA_ROTATE_Y || lSid == COLLADA_ROTATE_Z || 
                lSid == COLLADA_ROT_X || lSid == COLLADA_ROT_Y || lSid == COLLADA_ROT_Z ||
                lSid == COLLADA_ROTATION_X || lSid == COLLADA_ROTATION_Y || lSid == COLLADA_ROTATION_Z || 
                lSid == COLLADA_ROTATIONX || lSid == COLLADA_ROTATIONY || lSid == COLLADA_ROTATIONZ ||lSid.IsEmpty())
                lUsage = eUSAGE_ROTATION;
            else
                lUsage = eUSAGE_UNKNOWN;
        }
        else if (lTag == COLLADA_SCALE_STRUCTURE)
            lUsage = eUSAGE_SCALING;
        else if (lTag == COLLADA_SKEW_STRUCTURE)
            continue;
        else if (lTag == COLLADA_TRANSLATE_STRUCTURE)
        {
            if (lSid == COLLADA_ROTATE_PIVOT)
                lUsage = eUSAGE_ROTATION_PIVOT;
            else if (lSid == COLLADA_SCALE_PIVOT)
                lUsage = eUSAGE_SCALING_PIVOT;
            else if (lSid == COLLADA_ROTATE_PIVOT_OFFSET)
                lUsage = eUSAGE_ROTATION_OFFSET;
            else if (lSid == COLLADA_SCALE_PIVOT_OFFSET)
                lUsage = eUSAGE_SCALING_OFFSET;
            else if (lSid == COLLADA_ROTATE_PIVOT_INVERSE)
                lUsage = eUSAGE_ROTATION_PIVOT_INVERSE;
            else if (lSid == COLLADA_SCALE_PIVOT_INVERSE)
                lUsage = eUSAGE_SCALING_PIVOT_INVERSE;
            else if (lSid == COLLADA_TRANSLATE_ORIGIN || lSid == COLLADA_TRANSLATE_STRUCTURE ||
                     lSid == COLLADA_TRANSLATION_STRUCTURE || lSid == COLLADA_TRANSLATE_LOCATION || lSid.IsEmpty())
                lUsage = eUSAGE_TRANSLATION;
            else
                lUsage = eUSAGE_UNKNOWN;
        }
        else
            continue;

        // If the sid or the tag is unknown, or the order is not compatible with FBX
        if (lUsage == eUSAGE_UNKNOWN || lUsage < lLastUsage)
            return false;
        lUsageCount[lUsage]++;
    }

    // At most three times for each rotation unit, and at most one time for each translation and scaling unit
    for (int lUsageIndex = 0; lUsageIndex < eUSAGE_UNKNOWN; ++lUsageIndex)
    {
        if (lUsageCount[lUsageIndex] > USAGE_MAXIMUM[lUsageIndex])
            return false;
    }

    return true;
}

void DAE_AddNotificationError(const FbxManager * pSdkManger, const FbxString & pErrorMessage)
{
    FbxUserNotification * lUserNotification = pSdkManger->GetUserNotification();
    if ( lUserNotification )
    {
        const FbxString lError = "ERROR: " + pErrorMessage;
        lUserNotification->AddDetail( FbxUserNotification::eFileIONotify, lError );
    }
}

void DAE_AddNotificationWarning(const FbxManager * pSdkManger, const FbxString & pWarningMessage)
{
    FbxUserNotification * lUserNotification = pSdkManger->GetUserNotification();
    if ( lUserNotification )
    {
        const FbxString lWarning = "Warning: " + pWarningMessage;
        lUserNotification->AddDetail( FbxUserNotification::eFileIONotify, lWarning );
    }
}

xmlNode * DAE_GetSourceWithSemantic(xmlNode * pConsumerElement, const char * pSemantic,
                                    const SourceElementMapType & pSourceElements)
{
    const FbxString lConsumerID = DAE_GetElementAttributeValue(pConsumerElement, COLLADA_ID_PROPERTY);
    xmlNode * lInputElement = DAE_FindChildElementByAttribute(pConsumerElement, COLLADA_SEMANTIC_PROPERTY, pSemantic);
    if( !lInputElement ) return NULL;

    const FbxString lSourceID = DAE_GetIDFromSourceAttribute(lInputElement);
    const SourceElementMapType::RecordType* iter = pSourceElements.Find(lSourceID);
    return iter ? iter->GetValue() : NULL;
}

void DAE_GetElementTag(xmlNode * pElement, FbxString & pTag)
{
    if (pElement != NULL)
    {
        pTag = (const char *)(pElement->name);
    }
}

const FbxSystemUnit DAE_ImportUnit(xmlNode * pUnitElement)
{
    double lConversionFactor = 1;
    DAE_GetElementAttributeValue(pUnitElement, COLLADA_METER_PROPERTY, lConversionFactor);
    FBX_ASSERT(lConversionFactor > 0.0001);
    return FbxSystemUnit(lConversionFactor * 100); // FBX is in centimeter and COLLADA in meter
}

void IncreaseLclTranslationAnimation(FbxNode * pNode, FbxDouble3 & pOffset)
{
    const int lAnimStackCount = pNode->GetScene()->GetMemberCount<FbxAnimStack>();
    for (int lAnimStackIndex = 0; lAnimStackIndex < lAnimStackCount; ++lAnimStackIndex)
    {
        FbxAnimStack * lAnimStack = pNode->GetScene()->GetMember<FbxAnimStack>(lAnimStackIndex);
        const int lAnimLayerCount = lAnimStack->GetMemberCount<FbxAnimLayer>();
        for (int lAnimLayerIndex = 0; lAnimLayerIndex < lAnimLayerCount; ++lAnimLayerIndex)
        {
            FbxAnimLayer * lAnimLayer = lAnimStack->GetMember<FbxAnimLayer>(lAnimLayerIndex);
            const char * lChannelNames[] = {FBXSDK_CURVENODE_COMPONENT_X, FBXSDK_CURVENODE_COMPONENT_Y, FBXSDK_CURVENODE_COMPONENT_Z};
            for (int lChannelIndex = 0; lChannelIndex < 3; ++lChannelIndex)
            {
                FbxAnimCurve * lCurve = pNode->LclTranslation.GetCurve(lAnimLayer,
                    lChannelNames[lChannelIndex], false);
                if (lCurve)
                {
                    const int lKeyCount = lCurve->KeyGetCount();
                    for (int lKeyIndex = 0; lKeyIndex < lKeyCount; ++lKeyIndex)
                    {
                        lCurve->KeyIncValue(lKeyIndex, (float)pOffset[lChannelIndex]);
                    }
                }
            }
        }
    }
}

void RecursiveSearchElement(xmlNode * pBaseElement, const char * pTag, FbxArray<xmlNode*> & pResult)
{
    for (xmlNode* lChild = pBaseElement->children; lChild != NULL; lChild = lChild->next)
    {
        if (strcmp(pTag, (const char *)lChild->name) == 0)
            pResult.Add(lChild);
        
        RecursiveSearchElement(lChild, pTag, pResult);
    }
}


FbxRenamingStrategyCollada::FbxRenamingStrategyCollada() :
    FbxRenamingStrategyBase(':')
{
}
FbxRenamingStrategyCollada::~FbxRenamingStrategyCollada()
{
    CleanUp();
}

void FbxRenamingStrategyCollada::CleanUp()
{
    FbxRenamingStrategyBase::CleanUp();
    //nothing else to clean up...
}

bool FbxRenamingStrategyCollada::DecodeScene(FbxScene* pScene)
{
    if (!pScene)
        return true;

    bool lRet = true;
    for (int i = 0, count = pScene->GetSrcObjectCount(); i < count; i++)
    {
        FbxObject* lObj = pScene->GetSrcObject(i);
        FbxString  lNameWithoutNameSpacePrefix = lObj->GetNameWithoutNameSpacePrefix();
        FbxNameHandler name(lNameWithoutNameSpacePrefix.Buffer());
        lRet |= this->DecodeString(name);
        lObj->SetNameSpace(name.GetNameSpace());
        lObj->SetName(name.GetCurrentName());
    }
    return lRet;
}
bool FbxRenamingStrategyCollada::EncodeScene(FbxScene* pScene)
{
    if (!pScene)
        return true;

    bool lRet = true;
    for (int i = 0, count = pScene->GetSrcObjectCount(); i < count; i++)
    {
        FbxObject* lObj = pScene->GetSrcObject(i);
        FbxString  lNameWithoutNameSpacePrefix = lObj->GetNameWithoutNameSpacePrefix();
        FbxNameHandler name(lNameWithoutNameSpacePrefix.Buffer());
        lRet |= this->EncodeString(name, false);
        lObj->SetName(name.GetCurrentName());
        lObj->SetNameSpace(name.GetNameSpace());
    }
    return lRet;
}
bool FbxRenamingStrategyCollada::DecodeString(FbxNameHandler& pName)
{
    bool lCode = false;
    FbxString lName = pName.GetCurrentName();

    FbxRenamingStrategyUtils::DecodeNonAlpha(lName);
    pName.SetCurrentName(lName.Buffer());

    lCode = FbxRenamingStrategyUtils::DecodeDuplicate(lName);
    lCode |= FbxRenamingStrategyUtils::DecodeCaseInsensitive(lName);
    if (lCode)
    {
        pName.SetCurrentName(lName.Buffer());
    }

    return lCode;
}
bool FbxRenamingStrategyCollada::EncodeString(FbxNameHandler& pName, bool pIsPropertyName)
{
    FbxString lNewName;
    bool lCode = false;
    bool lDuplicateFound = false;
    bool lFirstCharMustBeAlphaOnly = true;
    FbxString lPermittedChars("-_.");

    mStringNameArray.SetCaseSensitive(true);
    lNewName = pName.GetCurrentName();

    FbxRenamingStrategyUtils::EncodeNonAlpha(lNewName, lFirstCharMustBeAlphaOnly, lPermittedChars);
    pName.SetCurrentName(lNewName.Buffer());

    //Here we search in the previous strings if we have a duplicate    
    NameCell* lSensitiveReference = NULL;
    if (pIsPropertyName)
        lSensitiveReference = (NameCell*)mStringNameArray.Get(const_cast<char*>(pName.GetCurrentName()));
    else {
        FbxString lCurrentNameString = pName.GetCurrentName();
        FbxString lFullName = pName.GetParentName() + lCurrentNameString;
        lSensitiveReference = (NameCell*)mStringNameArray.Get(const_cast<char*>(lFullName.Buffer()));
    }

    if (lSensitiveReference)
        lDuplicateFound = true;

    if (lDuplicateFound)
    {
        lSensitiveReference->mInstanceCount++;
        if (pIsPropertyName)
            lNewName = lSensitiveReference->mName;
        lCode = FbxRenamingStrategyUtils::EncodeDuplicate(lNewName, lSensitiveReference->mInstanceCount);
        pName.SetCurrentName(lNewName.Buffer());
    }

    lNewName = pName.GetCurrentName();
    int nsPos = lNewName.ReverseFind(mNamespaceSymbol);
    if (nsPos > -1)
    {
        pName.SetNameSpace(lNewName.Left(nsPos).Buffer());
    }

    if (pIsPropertyName) {
        NameCell *namecell = FbxNew< NameCell  >(pName.GetCurrentName());
        mStringNameArray.Add(const_cast<char*>(pName.GetCurrentName()), (FbxHandle)namecell);
    }
    else {
        FbxString lNewCurrentNameString = pName.GetCurrentName();
        FbxString lNewFullName = pName.GetParentName() + lNewCurrentNameString;
        NameCell *namecell = FbxNew< NameCell  >(lNewFullName.Buffer());
        mStringNameArray.Add(const_cast<char*>(lNewFullName.Buffer()), (FbxHandle)namecell);
    }

    return lCode;
}


#include <fbxsdk/fbxsdk_nsend.h>

