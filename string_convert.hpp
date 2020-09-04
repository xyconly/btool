/******************************************************************************
File name:  string_convert.hpp
Author:     AChar
Purpose: 字符类,用于各类字符转换
*****************************************************************************/
#pragma once

#ifdef __linux
# include <string.h>
# include <iconv.h>
#else
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
#endif

#include <algorithm>
#include <iterator>
#include <cctype>
#include <stdlib.h>

namespace BTool
{
    class StringConvert
    {
#pragma region 内部类型定义
        class AsciiToWideChar {
            AsciiToWideChar(void) = delete;
            AsciiToWideChar(AsciiToWideChar&) = delete;
            AsciiToWideChar operator = (AsciiToWideChar&) = delete;
        public:
            AsciiToWideChar(const char* s) : s_(convert(s)) {}
            ~AsciiToWideChar(void) {
                if (s_) delete[] s_;
            }

            wchar_t* wchar_rep(void) {
                return s_;
            }

        private:
            static wchar_t* convert(const char* str) {
                if (str == nullptr)
                    return nullptr;

# if defined (WIN32)
                UINT cp = GetACP();
                int len = ::MultiByteToWideChar(cp, 0, str, -1, 0, 0);
                wchar_t* wstr = new wchar_t[len];
                ::MultiByteToWideChar(cp, 0, str, -1, wstr, len);
# else
                const char* const encTo = "unicode";
                const char* const encFrom = "gb2312";

                iconv_t cd = iconv_open(encTo, encFrom);
                size_t srclen = strlen(str) + 1;
                size_t unilen = srclen * 2;
                wchar_t *wstr = new wchar_t[unilen];
                wchar_t *pOut = wstr;
                size_t ret = iconv(cd, (char **) &str, &srclen, (char **) &pOut, &unilen);
                if(ret == -1){
                    delete[] wstr;
                    wstr = nullptr;
                }
#endif
                return wstr;
            }

        private:
            wchar_t* s_;
        };

        class WideCharToAscii {
            WideCharToAscii(void) = delete;
            WideCharToAscii(WideCharToAscii&) = delete;
            WideCharToAscii operator= (WideCharToAscii&) = delete;
        public:
            WideCharToAscii(const wchar_t* s) : s_(convert(s)) {}
            ~WideCharToAscii(void) {
                if(s_) delete[] s_;
            }

            char* ascii_rep(void) {
                return s_;
            }

        private:
            static char* convert(const wchar_t* wstr) {
                if (wstr == nullptr)
                    return nullptr;

# if defined (WIN32)
                UINT cp = GetACP();
                int len = ::WideCharToMultiByte(cp, 0, wstr, -1, 0, 0, 0, 0);
                char* str = new char[len];
                ::WideCharToMultiByte(cp, 0, wstr, -1, str, len, 0, 0);
# else
                const char* const encTo = "gb2312";
                const char* const encFrom = "unicode";

                iconv_t cd = iconv_open(encTo, encFrom);
                size_t wsrclen = wcslen(wstr) + 1;
                size_t asciilen = wsrclen * 2;
                char *str = new char[asciilen];
                char *pOut = str;
                size_t ret = iconv(cd, (char **) &wstr, &wsrclen, (char **) &pOut, &asciilen);
                if(ret == -1){
                    delete[] str;
                    str = nullptr;
                }
# endif
                return str;
            }

        private:
            char* s_;
        };

        class WideCharToUTF8 {
            WideCharToUTF8(void) = delete;
            WideCharToUTF8(WideCharToUTF8&) = delete;
            WideCharToUTF8& operator= (WideCharToUTF8&) = delete;
        public:
            WideCharToUTF8(const wchar_t* wstr)
                : utf8_(convert(wstr))
            {
            }
            ~WideCharToUTF8(void) {
                if (utf8_) delete[] utf8_;
            }
            char* utf8_rep(void) {
                return utf8_;
            }
        private:
            static char* convert(const wchar_t* wstr) {
                if (wstr == nullptr)
                    return nullptr;
#if defined (WIN32)
                int const textlen = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL) + 1;
                char* str = new char[textlen] {0};
                WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, textlen, NULL, NULL);
                return str;
#else
                const char* const encTo = "utf-8";
                const char* const encFrom = "unicode";

                iconv_t cd = iconv_open(encTo, encFrom);
                size_t wsrclen = wcslen(wstr) + 1;
                size_t utf8len = wsrclen * 2;
                char *str = new char[utf8len];
                char *pOut = str;
                size_t ret = iconv(cd, (char **) &wstr, &wsrclen, (char **) &pOut, &utf8len);
                if(ret == -1){
                    delete[] str;
                    str = nullptr;
                }
                return str;
#endif
            }
        private:
            char* utf8_;
        };

        class AnsiiToUTF8 {
            AnsiiToUTF8(void) = delete;
            AnsiiToUTF8(AnsiiToUTF8&) = delete;
            AnsiiToUTF8& operator= (AnsiiToUTF8&) = delete;
        public:
            AnsiiToUTF8(const char* s)
#if defined (WIN32)
                : s_(new WideCharToUTF8(AsciiToWideChar(s).wchar_rep()))
#else
                : utf8_(convert(s))
#endif
            {
            }
            ~AnsiiToUTF8(void) {
#if defined (WIN32)
                delete s_;
#else
                if (utf8_) delete[] utf8_;
#endif
            }
            char* utf8_rep(void) {
#if defined (WIN32)
                return s_->utf8_rep();
#else
                return utf8_;
#endif
            }

#ifndef WIN32
        private:
            static char* convert(const char* str) {
                if (str == nullptr)
                    return nullptr;

                const char* const encTo = "utf-8";
                const char* const encFrom = "gb2312";

                iconv_t cd = iconv_open(encTo, encFrom);
                size_t srclen = strlen(str) + 1;
                size_t utf8len = srclen * 2;
                char *utf8 = new char[utf8len];
                char *pOut = utf8;
                size_t ret = iconv(cd, (char **) &str, &srclen, &pOut, &utf8len);
                if(ret == -1){
                    delete[] utf8;
                    utf8 = nullptr;
                }

                iconv_close(cd);
                return utf8;
            }
#endif
        private:
# if defined (WIN32)
            WideCharToUTF8* s_;
#else
            char* utf8_;
#endif
        };

        class UTF8ToWideChar {
            UTF8ToWideChar(void) = delete;
            UTF8ToWideChar(UTF8ToWideChar&) = delete;
            UTF8ToWideChar& operator= (UTF8ToWideChar&) = delete;
        public:
            UTF8ToWideChar(const char* utf8)
                : s_(convert(utf8))
            {
            }
            ~UTF8ToWideChar(void) {
                if (s_) delete[] s_;
            }

            wchar_t* wchar_rep(void) { return s_; }

        private:
            static wchar_t* convert(const char* utf8) {
                if (utf8 == nullptr)
                    return nullptr;
#if defined (WIN32)
                int const textlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0) + 1;
                wchar_t* result = new wchar_t[textlen]{ 0 };
                MultiByteToWideChar(CP_UTF8, 0, utf8, -1, (LPWSTR)result, textlen);
                return result;
#else
                throw std::logic_error("system does not support!");
#endif
            }
        private:
            wchar_t* s_;
        };

        class UTF8ToAscii {
            UTF8ToAscii(void) = delete;
            UTF8ToAscii(UTF8ToAscii&) = delete;
            UTF8ToAscii& operator= (UTF8ToAscii&) = delete;
        public:
            UTF8ToAscii(const char* utf8)
#if defined (WIN32)
            : s_(new WideCharToAscii(UTF8ToWideChar(utf8).wchar_rep()))
#else
            : ascii_(convert(utf8))
#endif
            {
            }
            ~UTF8ToAscii(void) {
#if defined (WIN32)
                delete s_;
#else
                if (ascii_) delete[] ascii_;
#endif
            }

            char* ascii_rep(void) {
#if defined (WIN32)
                return s_->ascii_rep();
#else
                return ascii_;
#endif
            }

#ifndef WIN32
        private:
            static char* convert(const char* utf8) {
                if (utf8 == nullptr)
                    return nullptr;

                const char* const encFrom = "utf-8";
                const char* const encTo = "gb2312";

                iconv_t cd = iconv_open(encTo, encFrom);
                size_t srclen = strlen(utf8) + 1;
                size_t asciilen = srclen * 2;
                char *ascii = new char[asciilen];
                char *pOut = ascii;
                size_t ret = iconv(cd, (char **) &utf8, &srclen, &pOut, &asciilen);
                if(ret == -1){
                    delete[] ascii;
                    ascii = nullptr;
                }

                iconv_close(cd);
                return ascii;
            }
#endif
        private:
# if defined (WIN32)
            WideCharToAscii* s_;
#else
            char* ascii_;
#endif
        };
#pragma endregion


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
            char* rtmp = myToLow(rvalue);
            int rslt = strcmp(ltmp, rtmp);
            delete[] ltmp;
            delete[] rtmp;
            return rslt;
        }

#pragma endregion

#pragma region 字符转换
    public:
        static std::string AnsiiToUtf8(const char* chr) {
            if (!chr || strlen(chr) == 0)
                return "";
            return std::string(AnsiiToUTF8(chr).utf8_rep());
        }
        static std::string WCharToUtf8(const wchar_t* wchr) {
            if (!wchr || wcslen(wchr) == 0)
                return "";
            return std::string(WideCharToUTF8(wchr).utf8_rep());
        }
        static std::string Utf8ToAnsii(const char* utf8) {
            if (!utf8 || strlen(utf8) == 0)
                return "";
            return std::string(UTF8ToAscii(utf8).ascii_rep());
        }
        static std::wstring Utf8ToWString(const char* utf8) {
            if (!utf8 || strlen(utf8) == 0)
                return L"";
            return std::wstring(UTF8ToWideChar(utf8).wchar_rep());
        }

//         static std::string CharToUtf16(const char* chr) {
//             return WCharToUtf16(CharToWString(chr).c_str());
//         }
//         static std::string WCharToUtf16(const wchar_t* wchr) {
//             return std::wstring_convert<std::codecvt_utf16<wchar_t>>{}.to_bytes(wchr);
//         }
//         static std::wstring Utf16ToWString(const char* utf16) {
//             return std::wstring_convert<std::codecvt_utf16<wchar_t>>{}.from_bytes(utf16);
//         }
//         static std::wstring Utf8ToUtf16(const char* utf8) {
//             return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(utf8);
//         }
//         static std::string Utf16ToUtf8(const wchar_t* utf16) {
//             return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(utf16);
//         }

        static std::string WCharToString(const wchar_t* wchr) {
            if (!wchr || wcslen(wchr) == 0)
                return "";
            return std::string(WideCharToAscii(wchr).ascii_rep());
        }
        static std::wstring CharToWChar(const char* chr) {
            if (!chr || strlen(chr) == 0)
                return L"";
            return std::wstring(AsciiToWideChar(chr).wchar_rep());
        }
#pragma endregion

#pragma region 字符转换安全模式
    public:
        static std::string AnsiiToUtf8_Safe(const char* chr) try {
            return AnsiiToUtf8(chr);
        }
        catch (...) {
            return "";
        }
        static std::string WCharToUtf8_Safe(const wchar_t* wchr) try {
            return WCharToUtf8(wchr);
        }
        catch (...) {
            return "";
        }
        static std::string Utf8ToAnsii_Safe(const char* utf8) try {
            return Utf8ToAnsii(utf8);
        }
        catch (...) {
            return "";
        }
        static std::wstring Utf8ToWString_Safe(const char* utf8) try {
            return Utf8ToWString(utf8);
        }
        catch (...) {
            return L"";
        }
        static std::string WCharToString_Safe(const wchar_t* wchr) try {
            return WCharToString(wchr);
        }
        catch (...) {
            return "";
        }
        static std::wstring CharToWChar_Safe(const char* chr) try {
            return CharToWChar(chr);
        }
        catch (...) {
            return L"";
        }
#pragma endregion

    };
}