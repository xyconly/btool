/******************************************************************************
File name:  string_convert.hpp
Author:     AChar
Purpose: 字符类,用于各类字符转换
*****************************************************************************/
#pragma once

#ifdef __linux
#include <string.h>
#include <locale>
#endif
#include <codecvt>
#include <algorithm>

namespace BTool
{
    class StringConvert
    {
        class chs_codecvt : public std::codecvt_byname<wchar_t, char, std::mbstate_t>
        {
        public:
#ifdef __linux
            chs_codecvt() : codecvt_byname("zh_CN.GB2312") { }
#else
            chs_codecvt() : codecvt_byname("chs") { }
#endif
        };

#pragma region 字符校验
//         static bool isNumber(const std::string& value) {
//             return 
//         }
//         static bool isNumber(const char* value) {
// 
//         }

#pragma endregion

#pragma region 字符比对
    public:
        // 检测大小写
        // lvalue < rvalue:返回-1; lvalue = rvalue:返回0; lvalue > rvalue:返回1
        static int compare(const std::string& lvalue, const std::string& rvalue) {
            return lvalue.compare(rvalue);
        }
        static int compare(const char* lvalue, const char* rvalue) {
            return strcmp(lvalue, rvalue);
        }

        // 不检测大小写,忽略大小写差异
        // lvalue < rvalue:返回-1; lvalue = rvalue:返回0; lvalue > rvalue:返回1
        static int compareNonCase(const std::string& lvalue, const std::string& rvalue) {
            std::string ltmp = lvalue;
            std::transform(lvalue.begin(), lvalue.end(), ltmp.begin(), ::tolower);
            std::string rtmp = rvalue;
            std::transform(rvalue.begin(), rvalue.end(), rtmp.begin(), ::tolower);
            return ltmp.compare(rtmp);
        }
        static int compareNonCase(const char* lvalue, const char* rvalue) {
            auto myToLow = [](const char* chr)->char* {
                size_t len = strlen(chr);
                char* rslt = new char[len + 1];
                rslt[len] = '\0';
                for (size_t i = 0; i < len; i++) {
                    if (chr[i] >= 'A' && chr[i] <= 'Z') {
                        rslt[i] = ::tolower(chr[i]);
                    }
                    else {
                        rslt[i] = chr[i];
                    }
                }
                return rslt;
            };
            char* ltmp = myToLow(lvalue);
            char* rtmp = myToLow(lvalue);
            int rslt = strcmp(lvalue, rvalue);
            delete[] ltmp;
            delete[] rtmp;
            return rslt;
        }

#pragma endregion

#pragma region 字符转换
    public:
        static std::string CharToUtf8(const char* chr) {
            return WCharToUtf8(CharToWString(chr).c_str());
        }
        static std::string WCharToUtf8(const wchar_t* wchr) {
            return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(wchr);
        }
        static std::string Utf8ToString(const char* utf8) {
            return WCharToString(Utf8ToWString(utf8).c_str());
        }
        static std::wstring Utf8ToWString(const char* utf8) {
            return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(utf8);
        }

        static std::string CharToUtf16(const char* chr) {
            return WCharToUtf16(CharToWString(chr).c_str());
        }
        static std::string WCharToUtf16(const wchar_t* wchr) {
            return std::wstring_convert<std::codecvt_utf16<wchar_t>>{}.to_bytes(wchr);
        }
        static std::wstring Utf16ToWString(const char* utf16) {
            return std::wstring_convert<std::codecvt_utf16<wchar_t>>{}.from_bytes(utf16);
        }
        static std::wstring Utf8ToUtf16(const char* utf8) {
            return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(utf8);
        }
        static std::string Utf16ToUtf8(const wchar_t* utf16) {
            return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(utf16);
        }

        static std::string WCharToString(const wchar_t* wchr) {
            return std::wstring_convert<chs_codecvt>{}.to_bytes(wchr);
        }
        static std::wstring CharToWString(const char* chr) {
            return std::wstring_convert<chs_codecvt>{}.from_bytes(chr);
        }
#pragma endregion

    };
}