/****************************************************************************************
 
   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

#include <fbxsdk/fbxsdk_def.h>

#include <cstdlib>
#include <cmath>

#include <fbxsdk/fileio/collada/fbxcolladaiostream.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

template <> bool FromString(int * pDest, const char * pSourceBegin, const char ** pSourceEnd)
{
    int negative;
    int num_digits;
    const char * lEnd = pSourceBegin;
    if (lEnd == NULL)
        return false;

    // Skip leading whitespace
    while (isspace((int)*lEnd)) lEnd++;

    // Handle optional sign
    negative = 0;
    switch (*lEnd)
    {
    case '-': negative = 1; // Fall through to increment position
    case '+': lEnd++;
    }

    *pDest = 0;
    num_digits = 0;

    // Process string of digits
    while (isdigit((int)*lEnd))
    {
        *pDest = *pDest * 10 + (*lEnd - '0');
        lEnd++;
        num_digits++;
    }

    if (pSourceEnd)
        *pSourceEnd = lEnd;

    if (num_digits == 0)
    {
        *pDest = 0;
        return false;
    }

    // Correct for sign
    if (negative) *pDest = -*pDest;

    return true;
}

template <>
bool FromString(double * pDest, const char * pSourceBegin, const char ** pSourceEnd)
{
    int exponent;
    int negative;
    double p10;
    int n;
    int num_digits;
    int num_decimals;
    const char * lEnd = pSourceBegin;

    if (lEnd == NULL)
        return false;

    // Skip leading whitespace
    while (isspace((int)*lEnd)) lEnd++;

    // Handle optional sign
    negative = 0;
    switch (*lEnd)
    {
    case '-': negative = 1; // Fall through to increment position
    case '+': lEnd++;
    }

    *pDest = 0.;
    exponent = 0;
    num_digits = 0;
    num_decimals = 0;

    // Process string of digits
    while (isdigit((int)*lEnd))
    {
        *pDest = *pDest * 10. + (*lEnd - '0');
        lEnd++;
        num_digits++;
    }

    // Process decimal part
    if (*lEnd == '.') 
    {
        lEnd++;

        while (isdigit((int)*lEnd))
        {
            *pDest = *pDest * 10. + (*lEnd - '0');
            lEnd++;
            num_digits++;
            num_decimals++;
        }

        exponent -= num_decimals;
    }

    if (num_digits == 0)
    {
        *pDest = 0.0;
        if (pSourceEnd)
            *pSourceEnd = lEnd;
        return false;
    }

    // Correct for sign
    if (negative) *pDest = -*pDest;

    // Process an exponent string
    if (*lEnd == 'e' || *lEnd == 'E') 
    {
        // Handle optional sign
        negative = 0;
        switch(*++lEnd) 
        {   
        case '-': negative = 1;   // Fall through to increment pos
        case '+': lEnd++;
        }

        // Process string of digits
        n = 0;
        while (isdigit((int)*lEnd)) 
        {   
            n = n * 10 + (*lEnd - '0');
            lEnd++;
        }

        if (negative) 
            exponent -= n;
        else
            exponent += n;
    }

    if (exponent < DBL_MIN_EXP  || exponent > DBL_MAX_EXP)
    {
        *pDest = HUGE_VAL;
        if (pSourceEnd)
            *pSourceEnd = lEnd;
        return false;
    }

    // Scale the result
    p10 = 10.;
    n = exponent;
    if (n < 0) n = -n;
    while (n) 
    {
        if (n & 1) 
        {
            if (exponent < 0)
                *pDest /= p10;
            else
                *pDest *= p10;
        }
        n >>= 1;
        p10 *= p10;
    }

    if (pSourceEnd)
        *pSourceEnd = lEnd;

    return true;
}

template <>
bool FromString(FbxString * pDest, const char * pSourceBegin, const char ** pSourceEnd)
{
    const char * lStart = pSourceBegin;
    if (lStart == NULL)
        return false;

    while (*lStart && isspace((int)*lStart))
        ++lStart;

    const char * lEnd = lStart;
    while (*lEnd && !isspace((int)*lEnd))
        ++lEnd;

    if (pSourceEnd)
        *pSourceEnd = lEnd;

    if (lEnd == lStart)
        return false;

    *pDest = FbxString(lStart, lEnd - lStart);
    return true;
}

template <>
bool FromString(FbxDouble2 * pDest, const char * pSourceBegin, const char ** pSourceEnd)
{
    const char * lBegin = pSourceBegin;
    if (lBegin == NULL)
        return false;

    for (int lIndex = 0; lIndex < 2; ++lIndex)
    {
        if (FromString(&(*pDest)[lIndex], lBegin, &lBegin) == false)
        {
            if (pSourceEnd)
                *pSourceEnd = lBegin;
            return false;
        }
    }

    if (pSourceEnd)
        *pSourceEnd = lBegin;
    return true;
}

template <>
bool FromString(FbxDouble3 * pDest, const char * pSourceBegin, const char ** pSourceEnd)
{
    const char * lBegin = pSourceBegin;
    if (lBegin == NULL)
        return false;

    for (int lIndex = 0; lIndex < 3; ++lIndex)
    {
        if (FromString(&(*pDest)[lIndex], lBegin, &lBegin) == false)
        {
            if (pSourceEnd)
                *pSourceEnd = lBegin;
            return false;
        }
    }

    if (pSourceEnd)
        *pSourceEnd = lBegin;
    return true;
}

template <>
bool FromString(FbxDouble4 * pDest, const char * pSourceBegin, const char ** pSourceEnd)
{
    const char * lBegin = pSourceBegin;
    if (lBegin == NULL)
        return false;

    for (int lIndex = 0; lIndex < 4; ++lIndex)
    {
        if (FromString(&(*pDest)[lIndex], lBegin, &lBegin) == false)
        {
            if (pSourceEnd)
                *pSourceEnd = lBegin;
            return false;
        }
    }

    if (pSourceEnd)
        *pSourceEnd = lBegin;
    return true;
}

template <>
bool FromString(FbxVector4 * pDest, const char * pSourceBegin, const char ** pSourceEnd)
{
    const char * lBegin = pSourceBegin;
    if (lBegin == NULL)
        return false;

    for (int lIndex = 0; lIndex < 3; ++lIndex)
    {
        if (FromString(&(*pDest)[lIndex], lBegin, &lBegin) == false)
        {
            if (pSourceEnd)
                *pSourceEnd = lBegin;
            return false;
        }
    }

    if (pSourceEnd)
        *pSourceEnd = lBegin;
    (*pDest)[3] = 1;
    return true;
}

template <>
bool FromString(FbxAMatrix * pDest, const char * pSourceBegin, const char ** pSourceEnd)
{
    const char * lBegin = pSourceBegin;
    if (lBegin == NULL)
        return false;

    for (int lColumnIndex = 0; lColumnIndex < 4; ++lColumnIndex)
    {
        for (int lRowIndex = 0; lRowIndex < 4; ++lRowIndex)
        {
            if (FromString(&(*pDest)[lRowIndex][lColumnIndex], lBegin, &lBegin) == false)
            {
                if (pSourceEnd)
                    *pSourceEnd = lBegin;
                return false;
            }
        }
    }

    if (pSourceEnd)
        *pSourceEnd = lBegin;
    return true;
}

template <>
bool FromString(FbxColor * pDest, const char * pSourceBegin, const char ** pSourceEnd)
{
    const char * lBegin = pSourceBegin;
    if (lBegin == NULL)
        return false;

    for (int lIndex = 0; lIndex < 3; ++lIndex)
    {
        if (FromString(&(*pDest)[lIndex], lBegin, &lBegin) == false)
        {
            if (pSourceEnd)
                *pSourceEnd = lBegin;
            return false;
        }
    }

    if (pSourceEnd)
        *pSourceEnd = lBegin;
    return true;
}

//----------------------------------------------------------------------------//

template <>
const FbxString ToString(const FbxVector4 & pValue)
{
    return FbxString(pValue[0]) + FbxString(pValue[1]) + 
        FbxString(pValue[2]) + FbxString(pValue[3]);
}

//----------------------------------------------------------------------------//

template <>
const FbxString ToString(const FbxAMatrix & pValue)
{
    return ToString(pValue.GetColumn(0)) + ToString(pValue.GetColumn(1)) +
        ToString(pValue.GetColumn(2)) + ToString(pValue.GetColumn(3));
}

//----------------------------------------------------------------------------//

const FbxString DecodePercentEncoding(const FbxString & pEncodedString)
{
    int pos = 0, next;
    FbxString result;
	size_t esLen = pEncodedString.GetLen();

    while ((next = pEncodedString.Find('%', pos)) != -1)
    {
        result += pEncodedString.Mid(pos, next - pos);
        pos = next;
        if ((int)esLen - next < 3)
            return FbxString();
        char hex[3] = { pEncodedString[next + 1], pEncodedString[next + 2], '\0' };
        char* end_ptr;
        result += (char) std::strtol(hex, &end_ptr, 16);
        if(*end_ptr)
            return FbxString();
        pos = next + 3;
    }

    result += pEncodedString.Mid(pos);
    return result;
}

#include <fbxsdk/fbxsdk_nsend.h>
