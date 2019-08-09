/****************************************************************************************
 
   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <fbxsdk/scene/animation/fbxanimcurve.h>
#include <fbxsdk/fileio/collada/fbxcolladautils.h>
#include <fbxsdk/fileio/collada/fbxcolladaanimationelement.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

AnimationElement::AnimationElement()
{
    mKeyCount = 0;
    mInputArray = NULL;
    mOutputArray = NULL;
    mOutputStride = 0;
    mInterpolationArray = NULL;
    mInterpolationStride = 0;
    mInTangentArray = NULL;
    mInTangentStride = 0;
    mOutTangentArray = NULL;
    mOutTangentStride = 0;
}

AnimationElement::~AnimationElement()
{
    FbxDeleteArray(mInputArray);
    FbxDeleteArray(mOutputArray);
    FbxDeleteArray(mInterpolationArray);
    FbxDeleteArray(mInTangentArray);
    FbxDeleteArray(mOutTangentArray);
}

int AnimationElement::GetChannelCount() const
{
    return mOutputStride;
}

bool AnimationElement::FromCOLLADA(xmlNode * pElement, const SourceElementMapType & pSourceElements)
{
    if (pElement)
        SetXMLElement(pElement);

    xmlNode * lElement = GetXMLElement();
    if (!lElement)
        return true;

    xmlNode * lChannelElement = DAE_FindChildElementByTag(lElement, COLLADA_CHANNEL_STRUCTURE);
    FBX_ASSERT(lChannelElement);
    const FbxString lSamplerID = DAE_GetIDFromSourceAttribute(lChannelElement);
    xmlNode * lSamplerElement = DAE_FindChildElementByAttribute(lElement, COLLADA_ID_PROPERTY, lSamplerID);
    FBX_ASSERT(lSamplerElement);

    xmlNode * lInputElement = DAE_GetSourceWithSemantic(lSamplerElement, COLLADA_INPUT_SEMANTIC, pSourceElements);
    xmlNode * lOutputElement = DAE_GetSourceWithSemantic(lSamplerElement, COLLADA_OUTPUT_SEMANTIC, pSourceElements);
    xmlNode * lInterpolationElement = DAE_GetSourceWithSemantic(lSamplerElement, COLLADA_INTERPOLATION_SEMANTIC, pSourceElements);
    xmlNode * lInTangentElement = DAE_GetSourceWithSemantic(lSamplerElement, COLLADA_IN_TANGENT_SEMANTIC, pSourceElements);
    xmlNode * lOutTangentElement = DAE_GetSourceWithSemantic(lSamplerElement, COLLADA_OUT_TANGENT_SEMANTIC, pSourceElements);

    FBX_ASSERT(lInputElement && lOutputElement);

    if (lInputElement && lOutputElement)
    {
        {
            SourceElementContentAccessor<double> lInputAccessor(lInputElement);
            SourceElementContentAccessor<double> lOutputAccessor(lOutputElement);
            mKeyCount = lInputAccessor.mCount;
            mOutputStride = lOutputAccessor.mStride;
            FBX_ASSERT(lInputAccessor.mStride == 1);
            FBX_ASSERT(lOutputAccessor.mCount == mKeyCount);
			if (mKeyCount > 0)
			{
				mInputArray = FbxNewArray<double>(mKeyCount);
				if (lInputAccessor.GetArray(mInputArray, mKeyCount) != mKeyCount)
					return false;
			}

			int lSize = lOutputAccessor.mCount * mOutputStride;
			if (lSize > 0)
			{
				mOutputArray = FbxNewArray<double>(lSize);
				if (lOutputAccessor.GetArray(mOutputArray, lSize) != lSize)
					return false;
			}
        }

        if (lInterpolationElement)
        {
            SourceElementContentAccessor<FbxString> lInterpolationAccessor(lInterpolationElement);
            FBX_ASSERT(mKeyCount == lInterpolationAccessor.mCount);
            mInterpolationStride = lInterpolationAccessor.mStride;
            FBX_ASSERT(mInterpolationStride == 1 || mInterpolationStride == mOutputStride);
			int lSize = mKeyCount * mInterpolationStride;
			if (lSize > 0)
			{
				mInterpolationArray = FbxNewArray<FbxString>(lSize);
				if (lInterpolationAccessor.GetArray(mInterpolationArray, lSize) != lSize)
					return false;
			}
        }

        if (lInTangentElement)
        {
            SourceElementContentAccessor<double> lInTangentAccessor(lInTangentElement);
            mInTangentStride = lInTangentAccessor.mStride;
            FBX_ASSERT(lInTangentAccessor.mCount == mKeyCount);
			int lSize = lInTangentAccessor.mCount * mInTangentStride;
			if (lSize > 0)
			{
				mInTangentArray = FbxNewArray<double>(lSize);
				if (lInTangentAccessor.GetArray(mInTangentArray, lSize) != lSize)
					return false;
			}
        }

        if (lOutTangentElement)
        {
            SourceElementContentAccessor<double> lOutTangentAccessor(lOutTangentElement);
            mOutTangentStride = lOutTangentAccessor.mStride;
            FBX_ASSERT(lOutTangentAccessor.mCount == mKeyCount);
			int lSize = lOutTangentAccessor.mCount * mOutTangentStride;
			if (lSize > 0)
			{
				mOutTangentArray = FbxNewArray<double>(lSize);
				if (lOutTangentAccessor.GetArray(mOutTangentArray, lSize) != lSize)
					return false;
			}
        }
    }

	return true;
}

bool AnimationElement::FromFBX(const FbxAnimCurve* pCurve, double pUnitConversion)
{
    mKeyCount = pCurve->KeyGetCount();
    mOutputStride = mInterpolationStride = 1;

    bool lHasCubic = false;
    mInputArray = FbxNewArray<double>(mKeyCount);
    mOutputArray = FbxNewArray<double>(mKeyCount);
    mInterpolationArray = FbxNewArray<FbxString>(mKeyCount);
    for (int lKeyIndex = 0; lKeyIndex < mKeyCount; lKeyIndex++)
    {
        mInputArray[lKeyIndex] = pCurve->KeyGetTime(lKeyIndex).GetSecondDouble();
        mOutputArray[lKeyIndex] = pCurve->KeyGetValue(lKeyIndex) / pUnitConversion;
        
        // Key interpolations
		FbxAnimCurveDef::EInterpolationType lInterp = pCurve->KeyGetInterpolation(lKeyIndex);
		if( lInterp == FbxAnimCurveDef::eInterpolationConstant )
        {
            mInterpolationArray[lKeyIndex] = COLLADA_INTERPOLATION_TYPE_STEP;
        }
        else if (lInterp == FbxAnimCurveDef::eInterpolationLinear)
        {
            // The interpretation of "LINEAR" is different in COLLADAMaya than in FBX.
            // For now, keep to LINEAR, but this will result in changes in the fcurves.
            mInterpolationArray[lKeyIndex] = COLLADA_INTERPOLATION_TYPE_LINEAR;
        }
        else if (lInterp == FbxAnimCurveDef::eInterpolationCubic)
        {
            // Whatever the tangent mode, Break, User, Auto_Break, Auto or TCB, 
            // the curve interpolation type is set to BEZIER.
            mInterpolationArray[lKeyIndex] = COLLADA_INTERPOLATION_TYPE_BEZIER;
            lHasCubic = true;
        }
        else
        {
            FBX_ASSERT_NOW("Unexpected interpolation type");
        }
    }

    // If any key has cubic interpolation, export tangents
    if (lHasCubic)
    {
        const int lCount = mKeyCount * 2;
        mInTangentArray = FbxNewArray<double>(lCount);
        mOutTangentArray = FbxNewArray<double>(lCount);
        mInTangentArray[0] = mInTangentArray[1] = 0;
        mOutTangentArray[lCount - 2] = mOutTangentArray[lCount - 1] = 0;
        for (int lKeyIndex = 0; lKeyIndex < mKeyCount; lKeyIndex++)
        {
            if (lKeyIndex == 0) // The first key has no left tangent
            {
                mInTangentArray[lKeyIndex * 2] = 0;
                mInTangentArray[lKeyIndex * 2 + 1] = 0;
            }
            else
            {
                const double lDeltaTime = mInputArray[lKeyIndex] -
                    mInputArray[lKeyIndex - 1];
                const double lLeftWeight = pCurve->KeyGetLeftTangentWeight(lKeyIndex);
                const double lLeftDerivative = (const_cast<FbxAnimCurve*>(pCurve))
                    ->KeyGetLeftDerivative(lKeyIndex) / pUnitConversion;
                mInTangentArray[lKeyIndex * 2] = mInputArray[lKeyIndex] -
                    lDeltaTime * lLeftWeight;
                mInTangentArray[lKeyIndex * 2 + 1] = mOutputArray[lKeyIndex] -
                    lLeftDerivative * lDeltaTime * lLeftWeight;
            }

            if (lKeyIndex == mKeyCount - 1) // The last key has no right tangent
            {
                mOutTangentArray[lKeyIndex * 2] = 0;
                mOutTangentArray[lKeyIndex * 2 + 1] = 0;
            }
            else
            {
                const double lDeltaTime = mInputArray[lKeyIndex + 1] -
                    mInputArray[lKeyIndex];
                const double lRightWeight = pCurve->KeyGetRightTangentWeight(lKeyIndex);
                const double lRightDerivative = (const_cast<FbxAnimCurve*>(pCurve))
                    ->KeyGetRightDerivative(lKeyIndex) / pUnitConversion;;
                mOutTangentArray[lKeyIndex * 2] = mInputArray[lKeyIndex] +
                    lDeltaTime * lRightWeight;
                mOutTangentArray[lKeyIndex * 2 + 1] = mOutputArray[lKeyIndex] +
                    lRightDerivative * lDeltaTime * lRightWeight;
            }
        }
    }
	return true;
}

bool AnimationElement::ToFBX(FbxAnimCurve * pFBXCurve,
                             int pChannelIndex,
                             double pUnitConversion /* = 1.0 */) const
{
    // CopyToNode should be used for matrix animation
    FBX_ASSERT(mOutputStride != MATRIX_STRIDE);
	if (mOutputStride == MATRIX_STRIDE) return false;

    FBX_ASSERT(pFBXCurve && mKeyCount > 0);
    FBX_ASSERT(mInputArray && mOutputArray);
    if (!pFBXCurve || mKeyCount == 0 || !mInputArray || !mOutputArray)
        return false;

	bool ret = true;
    pFBXCurve->KeyModifyBegin();

    pFBXCurve->ResizeKeyBuffer(mKeyCount);
    int lInTangentOffset = mInTangentStride == 1 ? 0 : mInTangentStride / mOutputStride * pChannelIndex;
    int lOutTangentOffset = mOutTangentStride == 1 ? 0 : mOutTangentStride / mOutputStride * pChannelIndex;
    for (int lKeyIndex = 0; lKeyIndex < mKeyCount; ++lKeyIndex)
    {
        FbxTime lTime;
        float lValue = static_cast<float>(mOutputArray[lKeyIndex * mOutputStride +
            pChannelIndex] * pUnitConversion);
        lTime.SetSecondDouble(mInputArray[lKeyIndex]);

        FbxString lInterpolationStr = COLLADA_INTERPOLATION_TYPE_LINEAR;
        if (mInterpolationArray)
            lInterpolationStr = mInterpolationArray[lKeyIndex];
        if (lInterpolationStr == COLLADA_INTERPOLATION_TYPE_LINEAR) 
        {
            pFBXCurve->KeySet(lKeyIndex, lTime, lValue, FbxAnimCurveDef::eInterpolationLinear);
        }
        else if (lInterpolationStr == COLLADA_INTERPOLATION_TYPE_STEP)
        {
            pFBXCurve->KeySet(lKeyIndex, lTime, lValue, FbxAnimCurveDef::eInterpolationConstant);
            pFBXCurve->KeySetConstantMode(lKeyIndex, FbxAnimCurveDef::eConstantStandard);
        }
        else if (lInterpolationStr == COLLADA_INTERPOLATION_TYPE_BEZIER)
        {
            if (mInTangentArray || mOutTangentArray)
            {
                double lRightSlope = 0;
                double lNextLeftSlope = 0;
                double lRightWeight = FbxAnimCurveDef::sDEFAULT_WEIGHT;
                double lNextLeftWeight = FbxAnimCurveDef::sDEFAULT_WEIGHT;
                FbxAnimCurveDef::EWeightedMode lTangentWeightMode = FbxAnimCurveDef::eWeightedNone;

                // In FBX, the interpolation control the out tangent of this key and the in tangent of next key.
                // But in COLLADA, the interpolation control the in and out tangent of this key.
                // If the next interpolation type is linear, convert the left tangent of linear to cubic.
                if (lKeyIndex < mKeyCount - 1 && mInterpolationArray[lKeyIndex + 1] ==
                    COLLADA_INTERPOLATION_TYPE_LINEAR)
                {
                    const double lKeyInterval = mInputArray[lKeyIndex + 1] - mInputArray[lKeyIndex];
                    const double lValueInterval = mOutputArray[(lKeyIndex + 1) *
                        mOutputStride + pChannelIndex] - mOutputArray[lKeyIndex * mOutputStride + pChannelIndex];
                    lNextLeftSlope = lValueInterval / lKeyInterval;
                }
                else if (mInTangentArray && lKeyIndex < mKeyCount - 1)
                {
                    const double lKeyInterval = mInputArray[lKeyIndex + 1] - mInputArray[lKeyIndex];
                    if (mInTangentStride == 1 || mInTangentStride == mOutputStride) // 1D Tangent, just the value offset and use the default time offset.
                    {
                        lNextLeftSlope = mInTangentArray[(lKeyIndex + 1) * mInTangentStride + lInTangentOffset] *
                            pUnitConversion / (lKeyInterval * FbxAnimCurveDef::sDEFAULT_WEIGHT);
                    }
                    else // standard 2D Tangent
                    {
                        double lValueDelta = (mOutputArray[(lKeyIndex + 1) * mOutputStride + pChannelIndex] -
                            mInTangentArray[(lKeyIndex + 1) * mInTangentStride + lInTangentOffset + 1]) * pUnitConversion;
                        double lTimeDelta = mInputArray[(lKeyIndex + 1)] -
                            mInTangentArray[(lKeyIndex + 1) * mInTangentStride + lInTangentOffset];
                        if (fabs(lTimeDelta) < DBL_EPSILON)
                        {
                            lNextLeftSlope = 0;
                            lNextLeftWeight = 0;
                        }
                        else
                        {
                            lNextLeftSlope = lValueDelta / lTimeDelta;
                            lNextLeftWeight = lTimeDelta / lKeyInterval;
                        }
                    }
                    lTangentWeightMode = (FbxAnimCurveDef::EWeightedMode)(lTangentWeightMode | FbxAnimCurveDef::eWeightedNextLeft);
                }

                if (mOutTangentArray && lKeyIndex < mKeyCount - 1)
                {
                    const double lKeyInterval = mInputArray[lKeyIndex + 1] - mInputArray[lKeyIndex];
                    if (mOutTangentStride == 1 || mOutTangentStride == mOutputStride) // 1D Tangent, just the value offset and use the default time offset.
                    {
                        lRightSlope = mOutTangentArray[lKeyIndex * mOutputStride + pChannelIndex]  * pUnitConversion / (lKeyInterval * FbxAnimCurveDef::sDEFAULT_WEIGHT);
                    }
                    else // standard 2D Tangent
                    {
                        double lValueDelta = (mOutTangentArray[lKeyIndex * mOutTangentStride + lOutTangentOffset + 1]
                        - mOutputArray[lKeyIndex * mOutputStride + pChannelIndex])  * pUnitConversion;
                        double lTimeDelta = mOutTangentArray[lKeyIndex * mOutTangentStride + lOutTangentOffset]
                        - mInputArray[lKeyIndex];
                        if (fabs(lTimeDelta) < DBL_EPSILON)
                        {
                            lRightSlope = 0;
                            lRightWeight = 0;
                        }
                        else
                        {
                            lRightSlope = lValueDelta / lTimeDelta;
                            lRightWeight = lTimeDelta / lKeyInterval;
                        }
                    }
                    lTangentWeightMode = (FbxAnimCurveDef::EWeightedMode)(lTangentWeightMode | FbxAnimCurveDef::eWeightedRight);
                }

                pFBXCurve->KeySet(lKeyIndex, lTime, lValue, FbxAnimCurveDef::eInterpolationCubic,
                    FbxAnimCurveDef::eTangentUser, (float)lRightSlope, (float)lNextLeftSlope, lTangentWeightMode,
                    (float)lRightWeight, (float)lNextLeftWeight);
            }
            else
            {
                pFBXCurve->KeySet(lKeyIndex, lTime, lValue, FbxAnimCurveDef::eInterpolationCubic,
                    FbxAnimCurveDef::eTangentAuto);
            }
        }
        else if (lInterpolationStr == COLLADA_INTERPOLATION_TYPE_HERMITE)
        {
            if (mInTangentArray || mOutTangentArray)
            {
                double lRightSlope = 0;
                double lNextLeftSlope = 0;
                double lRightWeight = FbxAnimCurveDef::sDEFAULT_WEIGHT;
                double lNextLeftWeight = FbxAnimCurveDef::sDEFAULT_WEIGHT;
                FbxAnimCurveDef::EWeightedMode lTangentWeightMode = FbxAnimCurveDef::eWeightedNone;

                if (mInTangentArray && lKeyIndex < mKeyCount - 1)
                {
                    FBX_ASSERT(mInTangentStride == mOutputStride);
					if (mInTangentStride != mOutputStride)
						ret = false;

                    const double lKeyInterval = mInputArray[lKeyIndex + 1] - mInputArray[lKeyIndex];
                    const double lValueInterval = mOutputArray[(lKeyIndex + 1) * mOutputStride + pChannelIndex] - mOutputArray[lKeyIndex * mOutputStride + pChannelIndex];
                    lNextLeftSlope = lValueInterval * mInTangentArray[(lKeyIndex + 1) * mInTangentStride + lInTangentOffset] / lKeyInterval;
                    lTangentWeightMode = (FbxAnimCurveDef::EWeightedMode)(lTangentWeightMode | FbxAnimCurveDef::eWeightedNextLeft);
                }
                if (mOutTangentArray && lKeyIndex < mKeyCount - 1)
                {
                    FBX_ASSERT(mOutTangentStride == mOutputStride);
					if (mOutTangentStride != mOutputStride)
						ret = false;

                    const double lKeyInterval = mInputArray[lKeyIndex + 1] - mInputArray[lKeyIndex];
                    const double lValueInterval = mOutputArray[(lKeyIndex + 1) * mOutputStride + pChannelIndex] - mOutputArray[lKeyIndex * mOutputStride + pChannelIndex];
                    lRightSlope = lValueInterval * mOutTangentArray[lKeyIndex * mOutputStride + pChannelIndex] / lKeyInterval;
                    lTangentWeightMode = (FbxAnimCurveDef::EWeightedMode)(lTangentWeightMode | FbxAnimCurveDef::eWeightedRight);
                }

                pFBXCurve->KeySet(lKeyIndex, lTime, lValue, FbxAnimCurveDef::eInterpolationCubic,
                    FbxAnimCurveDef::eTangentUser, (float)lRightSlope, (float)lNextLeftSlope, lTangentWeightMode,
                    (float)lRightWeight, (float)lNextLeftWeight);
            }
            else
            {
                pFBXCurve->KeySet(lKeyIndex, lTime, lValue, FbxAnimCurveDef::eInterpolationCubic,
                    FbxAnimCurveDef::eTangentAuto);
            }
        }
        else
        {
            // Interpolation type not supported (HERMITE, CARDINAL & BSPLINE).
            const FbxString msg = FbxString("This interpolation type is not supported: ") + lInterpolationStr +
                " It will be interpreted as BEZIER.";
            FBX_ASSERT_NOW(msg);
			ret = false;
        }
    }

    pFBXCurve->KeyModifyEnd();
	return ret;
}

bool AnimationElement::ToFBX(FbxNode * pFBXNode, FbxAnimLayer * pAnimLayer,
                             double pUnitConversion /* = 1.0 */) const
{
    FBX_ASSERT(mOutputStride == MATRIX_STRIDE);
	if (mOutputStride != MATRIX_STRIDE)
		return false;

	if (mKeyCount == 0)
        // FbxAnimCurves cannot exist if they are empty (0 keys) so let's bail out
        // now to make sure we don't create them for nothing.
		return true;

    FbxAnimCurve* lCurves[9];
    lCurves[0] = pFBXNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
    lCurves[1] = pFBXNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
    lCurves[2] = pFBXNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
    lCurves[3] = pFBXNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
    lCurves[4] = pFBXNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
    lCurves[5] = pFBXNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
    lCurves[6] = pFBXNode->LclScaling.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
    lCurves[7] = pFBXNode->LclScaling.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
    lCurves[8] = pFBXNode->LclScaling.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

    for (int lChannelIndex = 0; lChannelIndex < 9; ++lChannelIndex)
    {
        lCurves[lChannelIndex]->KeyModifyBegin();
        lCurves[lChannelIndex]->ResizeKeyBuffer(mKeyCount);
    }

    for (int lKeyIndex = 0; lKeyIndex < mKeyCount; ++lKeyIndex)
    {
        FbxAMatrix lTransformMatrix;
        for (int lIndex = 0; lIndex < MATRIX_STRIDE; ++lIndex)
            ((double *)lTransformMatrix)[lIndex] = mOutputArray[MATRIX_STRIDE * lKeyIndex + lIndex];
        lTransformMatrix = lTransformMatrix.Transpose();
        const FbxVector4 lTranslation = lTransformMatrix.GetT();
        const FbxVector4 lRotation = lTransformMatrix.GetR();
        const FbxVector4 lScaling = lTransformMatrix.GetS();

        FbxTime lTime;
        lTime.SetSecondDouble(mInputArray[lKeyIndex]);

        lCurves[0]->KeySet(lKeyIndex, lTime, (float)lTranslation[0], FbxAnimCurveDef::eInterpolationLinear);
        lCurves[1]->KeySet(lKeyIndex, lTime, (float)lTranslation[1], FbxAnimCurveDef::eInterpolationLinear);
        lCurves[2]->KeySet(lKeyIndex, lTime, (float)lTranslation[2], FbxAnimCurveDef::eInterpolationLinear);
        lCurves[3]->KeySet(lKeyIndex, lTime, (float)lRotation[0], FbxAnimCurveDef::eInterpolationLinear);
        lCurves[4]->KeySet(lKeyIndex, lTime, (float)lRotation[1], FbxAnimCurveDef::eInterpolationLinear);
        lCurves[5]->KeySet(lKeyIndex, lTime, (float)lRotation[2], FbxAnimCurveDef::eInterpolationLinear);
        lCurves[6]->KeySet(lKeyIndex, lTime, (float)(lScaling[0] * pUnitConversion), FbxAnimCurveDef::eInterpolationLinear);
        lCurves[7]->KeySet(lKeyIndex, lTime, (float)(lScaling[1] * pUnitConversion), FbxAnimCurveDef::eInterpolationLinear);
        lCurves[8]->KeySet(lKeyIndex, lTime, (float)(lScaling[2] * pUnitConversion), FbxAnimCurveDef::eInterpolationLinear);
    }

    for (int lChannelIndex = 0; lChannelIndex < 9; ++lChannelIndex)
    {
        lCurves[lChannelIndex]->KeyModifyEnd();
    }

	return true;
}

bool AnimationElement::ToCOLLADA(xmlNode * pAnimationLibrary,
                                 const char * pNodeID,
                                 const char * pAttributeSID)
{
    xmlNode* lAnimationElement = DAE_AddChildElement(pAnimationLibrary, COLLADA_ANIMATION_STRUCTURE);

    const FbxString lSamplerID = FbxString(pNodeID) + "-" + pAttributeSID;
    const FbxString lInputID = lSamplerID + "-input";
    const FbxString lOutputID = lSamplerID + "-output";
    const FbxString lInterpolationID = lSamplerID + "-interpolation";
    const FbxString lInTangentID = lSamplerID + "-intan";
    const FbxString lOutTangentID = lSamplerID + "-outtan";

    AddSourceElement(lAnimationElement, lInputID, mInputArray, mKeyCount);
    AddSourceElement(lAnimationElement, lOutputID, mOutputArray, mKeyCount);
    AddSourceElement(lAnimationElement, lInterpolationID, mInterpolationArray, mKeyCount);
    if (mInTangentArray)
        AddSourceElement(lAnimationElement, lInTangentID, mInTangentArray, mKeyCount, 2);
    if (mOutTangentArray)
        AddSourceElement(lAnimationElement, lOutTangentID, mOutTangentArray, mKeyCount, 2);

    xmlNode * lSamlerElement = DAE_AddChildElement(lAnimationElement, COLLADA_SAMPLER_STRUCTURE);
    DAE_AddAttribute(lSamlerElement, COLLADA_ID_PROPERTY, lSamplerID);
    xmlNode * lInputElement = DAE_AddChildElement(lSamlerElement, COLLADA_INPUT_STRUCTURE);
    DAE_AddAttribute(lInputElement, COLLADA_SEMANTIC_PROPERTY, COLLADA_INPUT_SEMANTIC);
    DAE_AddAttribute(lInputElement, COLLADA_SOURCE_PROPERTY, URL(lInputID));
    lInputElement = DAE_AddChildElement(lSamlerElement, COLLADA_INPUT_STRUCTURE);
    DAE_AddAttribute(lInputElement, COLLADA_SEMANTIC_PROPERTY, COLLADA_OUTPUT_SEMANTIC);
    DAE_AddAttribute(lInputElement, COLLADA_SOURCE_PROPERTY, URL(lOutputID));
    lInputElement = DAE_AddChildElement(lSamlerElement, COLLADA_INPUT_STRUCTURE);
    DAE_AddAttribute(lInputElement, COLLADA_SEMANTIC_PROPERTY, COLLADA_INTERPOLATION_SEMANTIC);
    DAE_AddAttribute(lInputElement, COLLADA_SOURCE_PROPERTY, URL(lInterpolationID));
    if (mInTangentArray)
    {
        lInputElement = DAE_AddChildElement(lSamlerElement, COLLADA_INPUT_STRUCTURE);
        DAE_AddAttribute(lInputElement, COLLADA_SEMANTIC_PROPERTY, COLLADA_IN_TANGENT_SEMANTIC);
        DAE_AddAttribute(lInputElement, COLLADA_SOURCE_PROPERTY, URL(lInTangentID));
    }
    if (mOutTangentArray)
    {
        lInputElement = DAE_AddChildElement(lSamlerElement, COLLADA_INPUT_STRUCTURE);
        DAE_AddAttribute(lInputElement, COLLADA_SEMANTIC_PROPERTY, COLLADA_OUT_TANGENT_SEMANTIC);
        DAE_AddAttribute(lInputElement, COLLADA_SOURCE_PROPERTY, URL(lOutTangentID));
    }

    FbxString lChannelName = FbxString(pNodeID) + "/" + pAttributeSID;
    xmlNode * lChannelElement = DAE_AddChildElement(lAnimationElement, COLLADA_CHANNEL_STRUCTURE);
    DAE_AddAttribute(lChannelElement, COLLADA_SOURCE_PROPERTY, URL(lSamplerID));
    DAE_AddAttribute(lChannelElement, COLLADA_TARGET_PROPERTY, lChannelName);

	return true;
}

#include <fbxsdk/fbxsdk_nsend.h>
