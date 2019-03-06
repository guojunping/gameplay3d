// ==============================================================
//	This file is part of Glest Shared Library (www.glest.org)
//
//	Copyright (C) 2001-2008 Martiño Figueroa
//
//	You can redistribute this code and/or modify it under 
//	the terms of the GNU General Public License as published 
//	by the Free Software Foundation; either version 2 of the 
//	License, or (at your option) any later version
// ==============================================================

#ifndef _UTIL_CONVERSION_H_
#define _UTIL_CONVERSION_H_

#include <string>


using std::string;

namespace gameplay
{
		bool strToBool(const char *s);
		int strToInt(const char *s);
		unsigned int strToUInt(const char *s);
		float strToFloat(const char *s);

		bool strToBool(const char *s, bool *b);
		bool strToInt(const char *s, int *i);
		bool strToUInt(const char *s, unsigned int *i);
		bool strToFloat(const char *s, float *f);

		string boolToStr(bool b);
		string uIntToStr(const unsigned long long  value);
		string intToStr(const long long value);
		string intToHex(int i);
		string floatToStr(float f, int precsion = 2);
		string doubleToStr(double f, int precsion = 2);

		bool IsNumeric(const char *p, bool  allowNegative = true);

		string formatNumber(unsigned long long f);

		double getTimeDuationMinutes(int frames, int updateFps);
		string getTimeDuationString(int frames, int updateFps);

		int UTF82Unicode(const char* utf8Buf, wchar_t *pUniBuf, int utf8Leng);
		std::wstring Utf8ToUnicode(const std::string& source_str);
		std::wstring Utf8ToUnicode(const char* source_str);
		std::string UnicodeToUtf8(const std::wstring& source_str);
		int Unicode2UTF8(unsigned wchar, char *utf8);

}//end namespace

#endif