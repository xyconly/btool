/******************************************************************************
File name:  value_convert.hpp
Author:     AChar
Purpose: 用于各类数据类型间安全转换,防止精度丢失或转换失败等意外情况
*****************************************************************************/
#pragma once
#include <boost/lexical_cast.hpp>

namespace BTool
{
    class ValueConvert
    {
    public:
        template<typename Source, typename Target>
        inline static bool SafeConvert(const Source& value, Target& rslt)
        {
            try
            {
                rslt = boost::lexical_cast<Target>(value);
                return true;
            }
            catch (boost::bad_lexical_cast&)
            {
                return false;
            }
        }

        template<typename Target>
        inline static bool SafeConvert(const char* chars, Target& rslt) {
            return SafeConvert(chars, sizeof(rslt), rslt);
        }

        template<typename Target>
        inline static bool SafeConvert(const char* chars, size_t count, Target& rslt) {
            size_t cmp_count = strlen(chars);
            try {
                rslt = boost::lexical_cast<Target>(chars, count > cmp_count ? cmp_count : count);
                return true;
            }
            catch (boost::bad_lexical_cast&) {
                return false;
            }
        }

        template<typename Target>
        inline static bool SafeConvert(const unsigned char* chars, Target& rslt) {
            return SafeConvert(chars, sizeof(rslt), rslt);
        }

        template<typename Target>
        inline static bool SafeConvert(const unsigned char* chars, size_t count, Target& rslt) {
            try {
                rslt = boost::lexical_cast<Target>(chars, count);
                return true;
            }
            catch (boost::bad_lexical_cast&) {
                return false;
            }
        }

        template<typename Target>
        inline static bool SafeConvert(const signed char* chars, Target& rslt) {
            return SafeConvert(chars, sizeof(rslt), rslt);
        }

        template<typename Target>
        inline static bool SafeConvert(const signed char* chars, size_t count, Target& rslt) {
            try {
                rslt = boost::lexical_cast<Target>(chars, count);
                return true;
            }
            catch (boost::bad_lexical_cast&) {
                return false;
            }
        }

        template<typename Target>
        inline static bool SafeConvert(const wchar_t* chars, Target& rslt) {
            return SafeConvert(chars, sizeof(rslt), rslt);
        }

        template<typename Target>
        inline static bool SafeConvert(const wchar_t* chars, size_t count, Target& rslt) {
            try {
                rslt = boost::lexical_cast<Target>(chars, count);
                return true;
            }
            catch (boost::bad_lexical_cast&) {
                return false;
            }
        }

        template<typename Target>
        inline static bool SafeConvert(const char16_t* chars, Target& rslt) {
            return SafeConvert(chars, sizeof(rslt), rslt);
        }

        template<typename Target>
        inline static bool SafeConvert(const char16_t* chars, size_t count, Target& rslt) {
            try {
                rslt = boost::lexical_cast<Target>(chars, count);
                return true;
            }
            catch (boost::bad_lexical_cast&) {
                return false;
            }
        }

        template<typename Target>
        inline static bool SafeConvert(const char32_t* chars, Target& rslt) {
            return SafeConvert(chars, sizeof(rslt), rslt);
        }

        template<typename Target>
        inline static bool SafeConvert(const char32_t* chars, size_t count, Target& rslt) {
            try {
                rslt = boost::lexical_cast<Target>(chars, count);
                return true;
            }
            catch (boost::bad_lexical_cast&) {
                return false;
            }
        }

    };
}