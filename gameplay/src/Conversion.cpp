#include "Conversion.h"
#include "Base.h"
#include <sstream>
#include <iostream>
#include <clocale>
#ifdef WIN32
#include <Windows.h>
#else
#include <iconv.h>
#endif

using namespace std;
namespace gameplay
{
	const int strSize = 256;

	bool strToBool(const char *s) {
		if (strcmp(s, "0") == 0 || strcmp(s, "false") == 0) {
			return false;
		}
		if (strcmp(s, "1") == 0 || strcmp(s, "true") == 0) {
			return true;
		}
		throw std::runtime_error("Error converting string to bool, expected 0 or 1, found: [" + string(s) + "]");
	}

	int strToInt(const char *s) {
		char *endChar;
		//setlocale(LC_NUMERIC, "C");
		int intValue = strtol(s, &endChar, 10);
		if (*endChar != '\0') {
			throw std::runtime_error("Error converting from string to int, found: [" + string(s) + "]");
		}
		return intValue;
	}

	unsigned int strToUInt(const char *s) {
		char *endChar;
		//setlocale(LC_NUMERIC, "C");
		unsigned int intValue = strtoul(s, &endChar, 10);
		if (*endChar != '\0') {
			throw std::runtime_error("Error converting from string to uint, found: [" + string(s) + "]");
		}
		return intValue;
	}


	float strToFloat(const char *s) {
		char *endChar;
		//setlocale(LC_NUMERIC, "C");
		float floatValue = static_cast<float>(strtod(s, &endChar));
		if (*endChar != '\0') {
			throw std::runtime_error("Error converting from string to float, found: [" + string(s) + "]");
		}
		return floatValue;
	}

	bool strToBool(const char *s, bool *b) {
		if (strcmp(s, "0") == 0 || strcmp(s, "false") == 0) {
			*b = false;
			return true;
		}

		if (strcmp(s, "1") == 0 || strcmp(s, "true") == 0) {
			*b = true;
			return true;
		}

		return false;
	}

	bool strToInt(const char *s, int *i) {
		char *endChar;

		//setlocale(LC_NUMERIC, "C");
		*i = strtol(s, &endChar, 10);

		if (*endChar != '\0') {
			return false;
		}
		return true;
	}

	bool strToUInt(const char *s, unsigned int *i) {
		char *endChar;

		//setlocale(LC_NUMERIC, "C");
		*i = strtoul(s, &endChar, 10);

		if (*endChar != '\0') {
			return false;
		}
		return true;
	}

	bool strToFloat(const char *s, float *f) {
		char *endChar;
		//setlocale(LC_NUMERIC, "C");
		*f = static_cast<float>(strtod(s, &endChar));

		if (*endChar != '\0') {
			return false;
		}
		return true;
	}

	string boolToStr(bool b) {
		if (b) {
			return "1";
		}
		else {
			return "0";
		}
	}

	string intToStr(const long long value) {
		char str[strSize] = "";
		snprintf(str, strSize - 1, "%lld", (long long int)value);
		return (str[0] != '\0' ? str : "");
	}

	string uIntToStr(const unsigned long long value) {
		char str[strSize] = "";
		snprintf(str, strSize - 1, "%llu", (long long unsigned int)value);
		return (str[0] != '\0' ? str : "");
	}

	string intToHex(int i) {
		char str[strSize] = "";
		snprintf(str, strSize - 1, "%x", i);
		return (str[0] != '\0' ? str : "");
	}

	string floatToStr(float f, int precsion) {
		//setlocale(LC_NUMERIC, "C");
		char str[strSize] = "";
		snprintf(str, strSize - 1, "%.*f", precsion, f);
		return (str[0] != '\0' ? str : "");
	}

	string doubleToStr(double d, int precsion) {
		//setlocale(LC_NUMERIC, "C");
		char str[strSize] = "";
		snprintf(str, strSize - 1, "%.*f", precsion, d);
		return (str[0] != '\0' ? str : "");
	}

	bool IsNumeric(const char *p, bool  allowNegative) {
		if (p == NULL) {
			return false;
		}
		if (strcmp(p, "-") == 0) {
			return false;
		}
		int index = 0;
		for (; *p; p++) {
			if (*p < '0' || *p > '9') {
				if (allowNegative == false || (*p != '-' && index == 0)) {
					return false;
				}
			}
			index++;
		}
		return true;
	}

	class Comma : public numpunct<char>// own facet class
	{
	protected:
		//char do_thousands_sep() const { return ','; }// use the comma
		//string do_grouping() const { return "\3"; }//group 3 digits
	};
	string formatNumber(unsigned long long f) {

		locale myloc(locale(),    // C++ default locale
			new Comma);// Own numeric facet

		ostringstream out;
		out.imbue(myloc);
		out << f;
		return out.str();
	}

	double getTimeDuationMinutes(int frames, int updateFps) {
		int framesleft = frames;
		double hours = (int)((int)frames / (float)updateFps / 3600.0f);
		framesleft = framesleft - hours * 3600 * updateFps;
		double minutes = (int)((int)framesleft / (float)updateFps / 60.0f);
		framesleft = framesleft - minutes * 60 * updateFps;
		double seconds = (int)((int)framesleft / (float)updateFps);

		double result = (hours * 60.0) + minutes;
		if (seconds > 0) {
			result += seconds / 60.0;
		}
		return result;
	}

	string getTimeDuationString(int frames, int updateFps) {
		int framesleft = frames;
		int hours = (int)((int)frames / (float)updateFps / 3600.0f);
		framesleft = framesleft - hours * 3600 * updateFps;
		int minutes = (int)((int)framesleft / (float)updateFps / 60.0f);
		framesleft = framesleft - minutes * 60 * updateFps;
		int seconds = (int)((int)framesleft / (float)updateFps);
		//framesleft=framesleft-seconds*GameConstants::updateFps;

		string hourstr = intToStr(hours);
		if (hours < 10) {
			hourstr = "0" + hourstr;
		}

		string minutestr = intToStr(minutes);
		if (minutes < 10) {
			minutestr = "0" + minutestr;
		}

		string secondstr = intToStr(seconds);
		if (seconds < 10) {
			secondstr = "0" + secondstr;
		}

		return hourstr + ":" + minutestr + ":" + secondstr;
	}

	static int UTF82UnicodeOne(const char* utf8, wchar_t& wch)
	{
		//首字符的Ascii码大于0xC0才需要向后判断，否则，就肯定是单个ANSI字符了
		unsigned char firstCh = utf8[0];
		if (firstCh >= 0xC0)
		{
			//根据首字符的高位判断这是几个字母的UTF8编码
			int afters, code;
			if ((firstCh & 0xE0) == 0xC0)
			{
				afters = 2;
				code = firstCh & 0x1F;
			}
			else if ((firstCh & 0xF0) == 0xE0)
			{
				afters = 3;
				code = firstCh & 0xF;
			}
			else if ((firstCh & 0xF8) == 0xF0)
			{
				afters = 4;
				code = firstCh & 0x7;
			}
			else if ((firstCh & 0xFC) == 0xF8)
			{
				afters = 5;
				code = firstCh & 0x3;
			}
			else if ((firstCh & 0xFE) == 0xFC)
			{
				afters = 6;
				code = firstCh & 0x1;
			}
			else
			{
				wch = firstCh;
				return 1;
			}
			//知道了字节数量之后，还需要向后检查一下，如果检查失败，就简单的认为此UTF8编码有问题，或者不是UTF8编码，于是当成一个ANSI来返回处理

			for (int k = 1; k < afters; ++k)
			{
				if ((utf8[k] & 0xC0) != 0x80)
				{
					//判断失败，不符合UTF8编码的规则，直接当成一个ANSI字符返回
					wch = firstCh;
					return 1;
				}

				code <<= 6;
				code |= (unsigned char)utf8[k] & 0x3F;
			}

			wch = code;
			return afters;
		}
		else
		{
			wch = firstCh;
		}
		return 1;
	}
	//参数1是UTF8编码的字符串
	//参数2是输出的UCS-2的Unicode字符串
	//参数3是参数1字符串的长度
	//使用的时候需要注意参数2所指向的内存块足够用。其实安全的办法是判断一下pUniBuf是否为NULL，如果为NULL则只统计输出长度不写pUniBuf，这样
	//通过两次函数调用就可以计算出实际所需要的Unicode缓存输出长度。当然，更简单的思路是：无论如何转换，UTF8的字符数量不可能比Unicode少，所
	//以可以简单的按照sizeof(wchar_t) * utf8Leng来分配pUniBuf的内存……
	int UTF82Unicode(const char* utf8Buf, wchar_t *pUniBuf, int utf8Leng)
	{
		int i = 0, count = 0;
		while (i < utf8Leng)
		{
			i += UTF82UnicodeOne(utf8Buf + i, pUniBuf[count]);
			count++;
		}
		return count;
	}


	static int utf82wchar(const char* in, size_t in_len, wchar_t* out, size_t out_max)
	{
#ifdef WIN32
		return ::MultiByteToWideChar(CP_UTF8, 0, in, in_len, out, out_max);
#else
		size_t result;
		iconv_t env;
		env = iconv_open("WCHAR_T", "UTF-8");
		if (env == (iconv_t)-1)
		{
			int err = errno;
			GP_WARN("iconv_open err:%s,%d", strerror(errno), err);
			return -1;
		}
		result = iconv(env, (char**)&in, (size_t*)&in_len, (char**)&out, (size_t*)&out_max);
		iconv_close(env);
		if (result == (size_t)-1) {
			int err = errno;
			GP_WARN("iconv_open err:%s,%d", strerror(errno), err);
			return -1;
		}
		return (int)result;
#endif
	}

	std::wstring Utf8ToUnicode(const std::string& source_str)
	{
		if (source_str.empty())
		{
			return std::wstring();
		}
		size_t len = source_str.length();
		std::wstring ret(len, 0);
		//int size = utf82wchar(&source_str[0], source_str.length(), &ret[0], len*sizeof(wchar_t));
		int size = UTF82Unicode(&source_str[0], &ret[0], len);
		ret.resize(size);
		return ret;
	}

	std::wstring Utf8ToUnicode(const char* source_str)
	{
		if (source_str[0] == 0x0)
		{
			return std::wstring();
		}
		size_t len = strlen(source_str);
		std::wstring ret(len, 0);
		//int size = utf82wchar(&source_str[0], source_str.length(), &ret[0], len*sizeof(wchar_t));
		int size = UTF82Unicode(&source_str[0], &ret[0], len);
		ret.resize(size);
		return ret;
	}

	static inline int wchar2utf8(const wchar_t* in, int in_len, char* out, int out_max)
	{
#ifdef WIN32
		BOOL use_def_char;
		use_def_char = FALSE;
		return ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, in, in_len / sizeof(wchar_t), out, out_max, "?", &use_def_char);
#else
		size_t result;
		iconv_t env;
		env = iconv_open("UTF-8", "WCHAR_T");
		if (env == (iconv_t)-1)
		{
			int err = errno;
			GP_WARN("iconv_open err:%s,%d", strerror(errno), err);
			return -1;
		}
		result = iconv(env, (char**)&in, (size_t*)&in_len, (char**)&out, (size_t*)&out_max);
		iconv_close(env);
		if (result == (size_t)-1) {
			int err = errno;
			GP_WARN("iconv_open err:%s,%d", strerror(errno), err);
			return -1;
		}
		return (int)result;
#endif
	}


	int Unicode2UTF8(unsigned wchar, char *utf8)
	{
		int len = 0;
		if (wchar < 0xC0)
		{
			utf8[len++] = (char)wchar;
		}
		else if (wchar < 0x800)
		{
			utf8[len++] = 0xc0 | (wchar >> 6);
			utf8[len++] = 0x80 | (wchar & 0x3f);
		}
		else if (wchar < 0x10000)
		{
			utf8[len++] = 0xe0 | (wchar >> 12);
			utf8[len++] = 0x80 | ((wchar >> 6) & 0x3f);
			utf8[len++] = 0x80 | (wchar & 0x3f);
		}
		else if (wchar < 0x200000)
		{
			utf8[len++] = 0xf0 | ((int)wchar >> 18);
			utf8[len++] = 0x80 | ((wchar >> 12) & 0x3f);
			utf8[len++] = 0x80 | ((wchar >> 6) & 0x3f);
			utf8[len++] = 0x80 | (wchar & 0x3f);
		}
		else if (wchar < 0x4000000)
		{
			utf8[len++] = 0xf8 | ((int)wchar >> 24);
			utf8[len++] = 0x80 | ((wchar >> 18) & 0x3f);
			utf8[len++] = 0x80 | ((wchar >> 12) & 0x3f);
			utf8[len++] = 0x80 | ((wchar >> 6) & 0x3f);
			utf8[len++] = 0x80 | (wchar & 0x3f);
		}
		else if (wchar < 0x80000000)
		{
			utf8[len++] = 0xfc | ((int)wchar >> 30);
			utf8[len++] = 0x80 | ((wchar >> 24) & 0x3f);
			utf8[len++] = 0x80 | ((wchar >> 18) & 0x3f);
			utf8[len++] = 0x80 | ((wchar >> 12) & 0x3f);
			utf8[len++] = 0x80 | ((wchar >> 6) & 0x3f);
			utf8[len++] = 0x80 | (wchar & 0x3f);
		}

		return len;
	}

	static int Unicode2UTF8(const wchar_t * pUniBuf, const char* utf8Buf, int uniLeng)
	{
		int i = 0, count = 0;
		while (i < uniLeng)
		{
			count += Unicode2UTF8(pUniBuf[i++], (char *)&utf8Buf[count]);
		}
		return count;
	}

	std::string UnicodeToUtf8(const std::wstring& source_str)
	{
		if (source_str.empty())
		{
			return std::string();
		}

		size_t len = (source_str.length() + 1) * 3;
		std::string ret(len, 0);
		//len = wchar2utf8(&source_str[0], source_str.length() * sizeof(wchar_t), &ret[0], len);
		len = Unicode2UTF8(&source_str[0], &ret[0], source_str.length());
		ret.resize(len);
		return ret;
	}
}