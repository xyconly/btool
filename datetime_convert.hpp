/******************************************************************************
File name:  datetime_convert.hpp
Author:     AChar
Purpose: ʱ��ת����,���ڸ���ʱ��ת��,�ȶ�
Note:    Ŀǰ�ı����ͽ�֧�ֱ�׼����ʱ��(DTK_Local)
*****************************************************************************/
#pragma once

#include <stdarg.h>
#include <codecvt>
#include <chrono>
#ifdef __linux
#include <string.h>  // for memcpy
#include <stdexcept> // for std::domain_error
#endif

namespace BTool {

    class DateTimeConvert {
        // ���ڷ�JAVA��չʱʹ��
        //         static std::string DateTimeConvert::wtb[32] = {
        //         "am", "pm",
        //         "monday", "tuesday", "wednesday", "thursday", "friday",
        //         "saturday", "sunday",
        //         "january", "february", "march", "april", "may", "june",
        //         "july", "august", "september", "october", "november", "december",
        //         "gmt", "ut", "utc", "est", "edt", "cst", "cdt",
        //         "mst", "mdt", "pst", "pdt"
        //         };
        //
        //         static int DateTimeConvert::ttb[32] = {
        //         14, 1, 0, 0, 0, 0, 0, 0, 0,
        //         2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
        //         10000 + 0, 10000 + 0, 10000 + 0,    // GMT/UT/UTC
        //         10000 + 5 * 60, 10000 + 4 * 60,     // EST/EDT
        //         10000 + 6 * 60, 10000 + 5 * 60,     // CST/CDT
        //         10000 + 7 * 60, 10000 + 6 * 60,     // MST/MDT
        //         10000 + 8 * 60, 10000 + 7 * 60      // PST/PDT
        //         };

    public:
        // ����
        enum DayOfWeek: short {
            UNDEFINE = -1,
            SUNDAY = 0, // ������
            MONDAY,     // ����һ
            TUESDAY,    // ���ڶ�
            WEDNESDAY,  // ������
            THURSDAY,   // ������
            FRIDAY,     // ������
            SATURDAY,   // ������
        };

        // ʱ������
        enum DateTimeKind: unsigned char {
            DTK_Local = 0, // ����ʱ��,��:2018-07-07 15:20:00.000.000
            DTK_KLine,     // K�߱���ʱ��,��:20180707 152000 000
            DTK_CST,       // UTC+8 , ��:Thu Jul 22 23:58:32 CST 2018
            DTK_UTC,       // �����׼ʱ��, ��:20180707T152000Z/20180707T152000+08(����Z��ʾ�Ǳ�׼ʱ��,"+08"��ʾ������)
            DTK_GMT,       // ��������ʱ��, ��:Thu Jul 22 23:58:32 'GMT' 2018
            DKT_UnixStamp, // unixʱ���,��1970-01-01����������
        };

        // ����ʱ���ֶ�
        enum DateTimeStyle: unsigned char {
            DTS_Invlid = 0x00,
            DTS_Year = 0x80,  // ��	1000 0000
            DTS_Month = 0x40, // ��	0100 0000
            DTS_Day = 0x20,   // ��	0010 0000
            DTS_Hour = 0x10,  // ʱ	0001 0000
            DTS_Min = 0x08,   // ��	0000 1000
            DTS_Sec = 0x04,   // ��	0000 0100
            DTS_mSec = 0x02,  // ����	0000 0010
            DTS_uSec = 0x01,  // ΢��	0000 0001

            DTS_YMD = DTS_Year | DTS_Month | DTS_Day,    // ������
            DTS_HMS = DTS_Hour | DTS_Min | DTS_Sec,      // ʱ����
            DTS_YMDHMS = DTS_YMD | DTS_HMS,              // ������ʱ����
            DTS_YMDHMS_M = DTS_YMD | DTS_HMS | DTS_mSec, // ������ʱ�������
            DTS_ALL = 0xFF,
        };

        DateTimeConvert()
            : m_style(DTS_Invlid)
            , m_day_of_week(UNDEFINE)
            , m_isvalid(false)
            , m_year(0)
            , m_month(1)
            , m_day(1)
            , m_hour(0)
            , m_minute(0)
            , m_second(0)
            , m_millsecond(0)
            , m_microsecond(0)
        {
        }

        // ���ַ���ʽת��,��ʽ�ο�DateTimeKind
        // Ŀǰ��֧��DTK_Local�Լ�DTK_KLine
        explicit DateTimeConvert(const char* dt, DateTimeStyle style = DTS_ALL, DateTimeKind kind = DTK_Local)
            : m_style(style)
            , m_day_of_week(UNDEFINE)
            , m_isvalid(style != DTS_Invlid)
        {
            char tmp[5] = { 0 };
            int offset(0);

            if (style & DTS_Year) {
                memcpy(tmp, dt, 4);
                if (kind == DTK_Local)
                    offset += 5;
                else if (kind == DTK_KLine)
                    offset += 4;

                m_year = atoi(tmp);
                memset(tmp, 0, 5);
            }
            if (style & DTS_Month) {
                memcpy(tmp, dt + offset, 2);
                if (kind == DTK_Local)
                    offset += 3;
                else if (kind == DTK_KLine)
                    offset += 2;

                m_month = atoi(tmp);
                memset(tmp, 0, 5);
            }
            if (style & DTS_Day) {
                memcpy(tmp, dt + offset, 2);
                offset += 3;
                m_day = atoi(tmp);
                memset(tmp, 0, 5);
            }
            if (style & DTS_Hour) {
                memcpy(tmp, dt + offset, 2);
                if (kind == DTK_Local)
                    offset += 3;
                else if (kind == DTK_KLine)
                    offset += 2;
                m_hour = atoi(tmp);
                memset(tmp, 0, 5);
            }
            if (style & DTS_Min) {
                memcpy(tmp, dt + offset, 2);
                if (kind == DTK_Local)
                    offset += 3;
                else if (kind == DTK_KLine)
                    offset += 2;
                m_minute = atoi(tmp);
                memset(tmp, 0, 5);
            }
            if (style & DTS_Sec) {
                memcpy(tmp, dt + offset, 2);
                if (kind == DTK_Local)
                    offset += 3;
                else if (kind == DTK_KLine)
                    offset += 3;
                m_second = atoi(tmp);
                memset(tmp, 0, 5);
            }
            if (style & DTS_mSec) {
                memcpy(tmp, dt + offset, 3);
                if (kind == DTK_Local)
                    offset += 4;
                else if (kind == DTK_KLine)
                    offset += 4;
                m_millsecond = atoi(tmp);
                memset(tmp, 0, 5);
            }
            if (style & DTS_uSec) {
                memcpy(tmp, dt + offset, 3);
                m_microsecond = atoi(tmp);
                memset(tmp, 0, 5);
            }
            
            m_isvalid &= m_year >= 1900 && m_month > 0 && m_day > 0 && m_hour >= 0 && m_minute >= 0 && m_second >= 0;
        }

        // ��������ʱ�����˳��,����DateTimeStyle���и�ֵ,����Ϊint��
        explicit DateTimeConvert(DateTimeStyle style, int date_time, ...)
            : m_style(style)
            , m_day_of_week(UNDEFINE)
            , m_isvalid(style != DTS_Invlid)
        {
            int cur_param = date_time;

            va_list args;
            va_start(args, date_time);

            if (style & DTS_Year) {
                m_year = cur_param;
                cur_param = va_arg(args, int);
            }
            if (style & DTS_Month) {
                m_month = cur_param;
                cur_param = va_arg(args, int);
            }
            if (style & DTS_Day) {
                m_day = cur_param;
                cur_param = va_arg(args, int);
            }
            if (style & DTS_Hour) {
                m_hour = cur_param;
                cur_param = va_arg(args, int);
            }
            if (style & DTS_Min) {
                m_minute = cur_param;
                cur_param = va_arg(args, int);
            }
            if (style & DTS_Sec) {
                m_second = cur_param;
                cur_param = va_arg(args, int);
            }
            if (style & DTS_mSec) {
                m_millsecond = cur_param;
                cur_param = va_arg(args, int);
            }
            if (style & DTS_uSec) {
                m_microsecond = cur_param;
            }

            va_end(args);
            
            m_isvalid &= m_year >= 1900 && m_month > 0 && m_day > 0 && m_hour >= 0 && m_minute >= 0 && m_second >= 0;
        }

        explicit DateTimeConvert(std::time_t dt, DateTimeStyle style = DTS_YMDHMS)
            : m_style(style)
            , m_isvalid(style != DTS_Invlid)
        {
#ifdef __linux
            std::tm tp;
            localtime_r(&dt, &tp);
#else
            std::tm tp;
            localtime_s(&tp, &dt);
#endif
            if (style & DTS_Year) {
                m_year = tp.tm_year + 1900;
            }
            if (style & DTS_Month) {
                m_month = tp.tm_mon + 1;
            }
            if (style & DTS_Day) {
                m_day = tp.tm_mday;
            }
            if (style & DTS_Hour) {
                m_hour = tp.tm_hour;
            }
            if (style & DTS_Min) {
                m_minute = tp.tm_min;
            }
            if (style & DTS_Sec) {
                m_second = tp.tm_sec;
            }
            m_day_of_week = (DayOfWeek)tp.tm_wday;
            m_isvalid &= m_day_of_week != UNDEFINE && m_year >= 1900 && m_month > 0 && m_day > 0 && m_hour >= 0 && m_minute >= 0 && m_second >= 0;
        }

        explicit DateTimeConvert(const std::tm& dt, DateTimeStyle style = DTS_YMDHMS)
            : m_style(style)
            , m_day_of_week((DayOfWeek)dt.tm_wday)
            , m_isvalid(style != DTS_Invlid)
        {
            if (style & DTS_Year) {
                m_year = dt.tm_year + 1900;
            }
            if (style & DTS_Month) {
                m_month = dt.tm_mon + 1;
            }
            if (style & DTS_Day) {
                m_day = dt.tm_mday;
            }
            if (style & DTS_Hour) {
                m_hour = dt.tm_hour;
            }
            if (style & DTS_Min) {
                m_minute = dt.tm_min;
            }
            if (style & DTS_Sec) {
                m_second = dt.tm_sec;
            }
            m_isvalid &= m_day_of_week != UNDEFINE && m_year >= 1900 && m_month > 0 && m_day > 0 && m_hour >= 0 && m_minute >= 0 && m_second >= 0;
        }

        explicit DateTimeConvert(const std::chrono::system_clock::time_point& dt, DateTimeStyle style = DTS_ALL)
            : m_style(style), m_isvalid(style != DTS_Invlid)
        {
            std::time_t time = std::chrono::system_clock::to_time_t(dt);
#ifdef __linux
            std::tm tp;
            localtime_r(&time, &tp);
#else
            std::tm tp;
            localtime_s(&tp, &time);
#endif
            if (style & DTS_Year) {
                m_year = tp.tm_year + 1900;
            }
            if (style & DTS_Month) {
                m_month = tp.tm_mon + 1;
            }
            if (style & DTS_Day) {
                m_day = tp.tm_mday;
            }
            if (style & DTS_Hour) {
                m_hour = tp.tm_hour;
            }
            if (style & DTS_Min) {
                m_minute = tp.tm_min;
            }
            if (style & DTS_Sec) {
                m_second = tp.tm_sec;
            }
            if (style & DTS_mSec) {
                m_millsecond = std::chrono::duration_cast<std::chrono::milliseconds>(dt.time_since_epoch()).count() % 1000;
            }
            if (style & DTS_uSec) {
                m_microsecond = std::chrono::duration_cast<std::chrono::microseconds>(dt.time_since_epoch()).count() % 1000;
            }
            m_day_of_week = (DayOfWeek)tp.tm_wday;
            
            m_isvalid &= m_day_of_week != UNDEFINE && m_year >= 1900 && m_month > 0 && m_day > 0 && m_hour >= 0 && m_minute >= 0 && m_second >= 0;
        }

    public:
        bool operator==(const DateTimeConvert& dt) const {
            if (get_style() != dt.get_style())
                return false;

            if (m_style & DTS_Year) {
                if (dt.year() != year())
                    return false;
            }

            if (m_style & DTS_Month) {
                if (dt.month() != month())
                    return false;
            }

            if (m_style & DTS_Day) {
                if (dt.day() != day())
                    return false;
            }

            if (m_style & DTS_Hour) {
                if (dt.hour() != hour())
                    return false;
            }

            if (m_style & DTS_Min) {
                if (dt.minute() != minute())
                    return false;
            }

            if (m_style & DTS_Sec) {
                if (dt.second() != second())
                    return false;
            }

            if (m_style & DTS_mSec) {
                if (dt.millsecond() != millsecond())
                    return false;
            }

            if (m_style & DTS_uSec) {
                if (dt.microsecond() != microsecond())
                    return false;
            }

            return true;
        }

        bool operator<(const DateTimeConvert& dt) const {
            if (get_style() != dt.get_style())
                return get_style() < dt.get_style();

            if (m_style & DTS_Year) {
                if (year() > dt.year())
                    return false;

                if (year() < dt.year())
                    return true;
            }

            if (m_style & DTS_Month) {
                if (month() > dt.month())
                    return false;

                if (month() < dt.month())
                    return true;
            }

            if (m_style & DTS_Day) {
                if (day() > dt.day())
                    return false;

                if (day() < dt.day())
                    return true;
            }

            if (m_style & DTS_Hour) {
                if (hour() > dt.hour())
                    return false;

                if (hour() < dt.hour())
                    return true;
            }

            if (m_style & DTS_Min) {
                if (minute() > dt.minute())
                    return false;

                if (minute() < dt.minute())
                    return true;
            }

            if (m_style & DTS_Sec) {
                if (second() > dt.second())
                    return false;

                if (second() < dt.second())
                    return true;
            }

            if (m_style & DTS_mSec) {
                if (millsecond() > dt.millsecond())
                    return false;

                if (millsecond() < dt.millsecond())
                    return true;
            }

            if (m_style & DTS_uSec) {
                if (microsecond() > dt.microsecond())
                    return false;

                if (microsecond() < dt.microsecond())
                    return true;
            }

            return false;
        }

        bool operator<=(const DateTimeConvert& dt) const {
            return operator==(dt) || operator<(dt);
        }

        bool operator>(const DateTimeConvert& dt) const {
            if (get_style() != dt.get_style())
                return get_style() > dt.get_style();

            if (m_style & DTS_Year) {
                if (year() < dt.year())
                    return false;

                if (year() > dt.year())
                    return true;
            }

            if (m_style & DTS_Month) {
                if (month() < dt.month())
                    return false;

                if (month() > dt.month())
                    return true;
            }

            if (m_style & DTS_Day) {
                if (day() < dt.day())
                    return false;

                if (day() > dt.day())
                    return true;
            }

            if (m_style & DTS_Hour) {
                if (hour() < dt.hour())
                    return false;

                if (hour() > dt.hour())
                    return true;
            }

            if (m_style & DTS_Min) {
                if (minute() < dt.minute())
                    return false;

                if (minute() > dt.minute())
                    return true;
            }

            if (m_style & DTS_Sec) {
                if (second() < dt.second())
                    return false;

                if (second() > dt.second())
                    return true;
            }

            if (m_style & DTS_mSec) {
                if (millsecond() < dt.millsecond())
                    return false;

                if (millsecond() > dt.millsecond())
                    return true;
            }

            if (m_style & DTS_uSec) {
                if (microsecond() < dt.microsecond())
                    return false;

                if (microsecond() > dt.microsecond())
                    return true;
            }

            return false;
        }

        bool operator>=(const DateTimeConvert& dt) const {
            return operator==(dt) || operator>(dt);
        }

        long long operator-(const DateTimeConvert& rightDt) const {
            if (!m_isvalid || !rightDt.m_isvalid || m_style != rightDt.m_style) {
                throw std::runtime_error("DateTimeConvert is invalid, or style not equal!");
                return -1;
            }

            if ((m_style & DTS_YMD) == DTS_YMD) {
                return to_time_t() * 1000 * 1000 + m_millsecond * 1000 + m_microsecond - rightDt.to_time_t() * 1000 * 1000 + rightDt.m_millsecond * 1000 + rightDt.m_microsecond;
            }
            else if ((m_style & DTS_HMS) == DTS_HMS) {
                long long lefttm = m_hour * 60 * 60 + m_minute * 60 + m_second;
                long long righttm = rightDt.m_hour * 60 * 60 + rightDt.m_minute * 60 + rightDt.m_second;
                return lefttm * 1000 * 1000 + m_millsecond * 1000 + m_microsecond - righttm * 1000 * 1000 + rightDt.m_millsecond * 1000 + rightDt.m_microsecond;
            }
        }

    public:
        // ��ȡ��ǰʱ��
        static DateTimeConvert GetCurrentSystemTime(DateTimeStyle style = DTS_ALL) {
            if (style & (DTS_mSec | DTS_uSec)) {
                std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
                DateTimeConvert rslt(std::chrono::system_clock::to_time_t(tp), style);
                rslt.m_millsecond = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count() % 1000;
                rslt.m_microsecond = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count() % 1000;
                return rslt;
            }
            return DateTimeConvert(time(NULL), style);
        }

        // ��ȡ����ָ�����ڼ��������
        // ��Ч����-1
        // leftDt��style����rightDtһ��,�Ұ���DTS_YMD,������Ч
        static int GetDateSpace(const DateTimeConvert& leftDt, const DateTimeConvert& rightDt) {
            if (!leftDt.isvalid() || !rightDt.isvalid() || leftDt.m_style != rightDt.m_style || (leftDt.m_style & DTS_YMD) != DTS_YMD)
                return -1;

            std::time_t lefttm = leftDt.to_time_t();
            std::time_t righttm = rightDt.to_time_t();

            long long value = abs(lefttm - righttm);
            int day = static_cast<int>(value / (60 * 60 * 24));
            return day;
        }

        // ��ȡ����ָ��ʱ����������
        // ��Ч����-1
        // leftDt��style����rightDtһ��,�Ұ���DTS_HMS,������Ч
        static long long GetSecondSpace(const DateTimeConvert& leftDt, const DateTimeConvert& rightDt) {
            return abs(leftDt - rightDt) / 1000 / 1000;
        }

        // ��ȡ����ָ��ʱ�����ĺ�����
        // ��Ч����-1
        // leftDt��style����rightDtһ��,�Ұ���DTS_HMS,������Ч
        static long long GetMillSecondSpace(const DateTimeConvert& leftDt, const DateTimeConvert& rightDt) {
            return abs(leftDt - rightDt) / 1000;
        }

        // ��ȡ����ָ��ʱ������΢����
        // ��Ч����-1
        // leftDt��style����rightDtһ��,�Ұ���DTS_HMS,������Ч
        static long long GetMicroSecondSpace(const DateTimeConvert& leftDt, const DateTimeConvert& rightDt) {
            return abs(leftDt - rightDt);
        }

        // ��ȡ����+���Ư������
        // dt�������DTS_YMD,������Ч
        static DateTimeConvert GetAddDate(const DateTimeConvert& dt, int days) {
            return dt.get_add_date(days);
        }

        // ��ȡ����+���Ư������
        // dt�������DTS_YMDHMS,������Ч
        static DateTimeConvert GetAddSecond(const DateTimeConvert& dt, long long secs) {
            return dt.get_add_second(secs);
        }

    public:
        // ��ȡ����ʱ���ֶ�
        DateTimeStyle get_style() const {
            return m_style;
        }

        // ����ʱ����
        void set_time(int hour, int minute, int second) {
            m_hour = hour;
            m_minute = minute;
            m_second = second;
        }

        // ��������
        // ��Ч�򲻰���DTS_YMDʱ,�ޱ仯
        void add_date(int days) {
            if (!isvalid() || (m_style & DTS_YMD) != DTS_YMD)
                return;

            *this = get_add_date(days);
            m_day_of_week = UNDEFINE;
        }

        // ��������
        // ��Ч�򲻰���DTS_YMDHMSʱ,�ޱ仯
        void add_second(long long secs) {
            if (!isvalid() || (m_style & DTS_YMDHMS) != DTS_YMDHMS)
                return;
            *this = get_add_second(secs);
            m_day_of_week = UNDEFINE;
        }

        // ����Ư��������ʱ��
        // style�������DTS_YMD,������Ч
        DateTimeConvert get_add_date(int days) const {
            if (!isvalid() || (m_style & DTS_YMD) != DTS_YMD)
                return DateTimeConvert();

            if ((m_style & DTS_YMDHMS) == DTS_YMDHMS)
                return get_add_second(days * 24 * 60 * 60);

            return DateTimeConvert(DTS_YMDHMS, year(), month(), day(), 0, 0, 0).get_add_second(days * 24 * 60 * 60);
        }

        // ����Ư��������ʱ��
        // style�������DTS_YMDHMS,������Ч
        DateTimeConvert get_add_second(long long secs) const {
            if (!isvalid() || (m_style & DTS_YMDHMS) != DTS_YMDHMS)
                return DateTimeConvert();

            std::time_t ltime = to_time_t();

            ltime = ltime + secs;

#ifdef __linux
            std::tm tmcur;
            localtime_r(&ltime, &tmcur);
#else
            std::tm tmcur;
            localtime_s(&tmcur, &ltime);
#endif
            DateTimeConvert rslt(tmcur, m_style);
            if (m_style & DTS_mSec) {
                rslt.m_millsecond = m_millsecond;
            }

            if (m_style & DTS_uSec) {
                rslt.m_microsecond = m_microsecond;
            }

            return rslt;
        }

        // ����Ư�ƺ�������ʱ��
        // style�������DTS_YMDHMS,������Ч
        DateTimeConvert get_add_millsecond(long long millsecs) const {
            if (!isvalid() || (m_style & DTS_YMDHMS) != DTS_YMDHMS)
                return DateTimeConvert();

            std::time_t ltime = to_time_t();

            int res_millsec = millsecs % 1000 + m_millsecond;
            // int new_millsec = res_millsec % 1000;
            long long secs(res_millsec / 1000 + millsecs / 1000);
            ltime = ltime + secs;

#ifdef __linux
            std::tm tmcur;
            localtime_r(&ltime, &tmcur);
#else
            std::tm tmcur;
            localtime_s(&tmcur, &ltime);
#endif
            DateTimeConvert rslt(tmcur, m_style);
            if (m_style & DTS_mSec) {
                rslt.m_millsecond = res_millsec;
            }

            if (m_style & DTS_uSec) {
                rslt.m_microsecond = m_microsecond;
            }

            return rslt;
        }

        std::string to_local_string(DateTimeStyle style = DTS_YMDHMS) const {
            char szDate[11] = { 0 };
            std::string szDateTimeSpcae;
            char szTime[9] = { 0 };
            std::string szTimemSecSpcae;
            char szmSec[4] = { 0 };
            std::string szmSecuSecSpcae;
            char szuSec[4] = { 0 };

            if (style & DTS_YMD) {
                snprintf(szDate, sizeof(szDate),
                    "%04d-%02d-%02d", year(), month(), day());

                if (style & DTS_HMS)
                    szDateTimeSpcae = " ";
            }

            if (style & DTS_HMS) {
                snprintf(szTime, sizeof(szTime),
                    "%02d:%02d:%02d", hour(), minute(), second());

                if (style & DTS_mSec)
                    szTimemSecSpcae = ".";
            }

            if (style & DTS_mSec) {
                snprintf(szmSec, sizeof(szmSec),
                    "%03d", millsecond());

                if (style & DTS_uSec)
                    szmSecuSecSpcae = ".";
            }

            if (style & DTS_uSec) {
                snprintf(szuSec, sizeof(szuSec),
                    "%03d", microsecond());
            }

            std::string rslt = szDate;
            rslt += szDateTimeSpcae;
            rslt += szTime;
            rslt += szTimemSecSpcae;
            rslt += szmSec;
            rslt += szmSecuSecSpcae;
            rslt += szuSec;

            return rslt;
        }

        std::string to_kline_string(DateTimeStyle style = DTS_YMDHMS) {
            char szDate[9] = { 0 };
            std::string szDateTimeSpcae;
            char szTime[7] = { 0 };
            std::string szTimemSecSpcae;
            char szmSec[4] = { 0 };
            std::string szmSecuSecSpcae;
            char szuSec[4] = { 0 };

            if (style & DTS_YMD) {
                snprintf(szDate, sizeof(szDate),
                    "%04d%02d%02d", year(), month(), day());
                if (style & DTS_HMS)
                    szDateTimeSpcae = " ";
            }

            if (style & DTS_HMS) {
                snprintf(szTime, sizeof(szTime),
                    "%02d%02d%02d", hour(), minute(), second());
                if (style & DTS_mSec)
                    szTimemSecSpcae = " ";
            }

            if (style & DTS_mSec) {
                snprintf(szmSec, sizeof(szmSec),
                    "%03d", millsecond());
                if (style & DTS_uSec)
                    szmSecuSecSpcae = " ";
            }

            if (style & DTS_uSec) {
                snprintf(szuSec, sizeof(szuSec),
                    "%03d", microsecond());
            }

            std::string rslt = szDate;
            rslt += szDateTimeSpcae;
            rslt += szTime;
            rslt += szTimemSecSpcae;
            rslt += szmSec;
            rslt += szmSecuSecSpcae;
            rslt += szuSec;
            return rslt;
        }

        // ���ص�ǰʱ��
        std::tm to_tm() const {
            struct tm tmcur;
            tmcur.tm_year = m_year - 1900;
            tmcur.tm_mon = m_month - 1;
            tmcur.tm_mday = m_day;
            tmcur.tm_hour = m_hour;
            tmcur.tm_min = m_minute;
            tmcur.tm_sec = m_second;
            tmcur.tm_wday = m_day_of_week;
            tmcur.tm_isdst = -1;
            return tmcur;
        }

        // ���ص�ǰʱ��
        // ��Ч����-1
        // style�������DTS_YMDHMS,������Ч
        std::time_t to_time_t() const {
            if (!isvalid() && (m_style & DTS_YMDHMS) != DTS_YMDHMS)
                return -1;
            std::tm tp = to_tm();
            return std::mktime(&tp);
        }

        // ���ص�ǰ����
        // ��ʽ:yyyyMMdd
        // ��Ч����-1
        // style�������DTS_YMD,������Ч
        int to_int_date() const {
            if (!isvalid() || (m_style & DTS_YMD) != DTS_YMD)
                return -1;
            return m_year * 10000 + m_month * 100 + m_day;
        }

        // ���ص�ǰʱ��
        // ��ʽ:hhmmss
        // ��Ч����-1
        // style�������DTS_HMS,������Ч
        int to_int_time() const {
            if (!isvalid() || (m_style & DTS_HMS) != DTS_HMS)
                return -1;
            return m_hour * 10000 + m_minute * 100 + m_second;
        }

        // ���ص�ǰʱ��
        // ��Ч�׳��쳣
        // style�������DTS_YMDHMS,������Ч
        std::chrono::system_clock::time_point to_system_time_point() const {
            if (!isvalid() || (m_style & DTS_YMDHMS) != DTS_YMDHMS) {
                throw std::runtime_error("Invalid time, time_point cannot be made");
                return std::chrono::system_clock::time_point();
            }

            auto rslt = std::chrono::system_clock::from_time_t(to_time_t());
            if (m_style & DTS_mSec) {
                rslt += std::chrono::milliseconds(m_millsecond);
            }
            if (m_style & DTS_uSec) {
                rslt += std::chrono::microseconds(m_microsecond);
            }
            return rslt;
        }

        /********* Ŀǰ��ʵ�ֱ���ʱ�� *****
        // ����UTCʱ��
        // ��Ч����-1
        std::string to_utc_string() const;

        // ����GMTʱ��
        // ��ʽ:d MMM yyyy HH:mm:ss 'GMT'
        // ��Ч���ؿ�
        std::string to_gmt_string() const;

        // ����unixʱ���
        // ��Ч����-1
        long to_unix_stamp() const;
        ***********************************/

        // ��ȡ���ڼ��ֶ�
        DayOfWeek day_of_week() const {
            if (!isvalid())
                return UNDEFINE;

            if (m_day_of_week == UNDEFINE && (m_style & DTS_YMD) == DTS_YMD)
                m_day_of_week = caculate_weekday(m_year, m_month, m_day);

            return m_day_of_week;
        }

        // �ж��Ƿ���Ч
        bool isvalid() const {
            return m_isvalid;
        }

        // �ж��Ƿ���˫����
        bool isweekday() const {
            if (!isvalid())
                return false;

            if (m_day_of_week == UNDEFINE && (m_style & DTS_YMD) == DTS_YMD)
                m_day_of_week = caculate_weekday(m_year, m_month, m_day);

            return m_day_of_week == SUNDAY || m_day_of_week == SATURDAY;
        }

        // �Ƿ�������
        bool isleapyear() const {
            return (m_year % 4) == 0 && ((m_year % 100) != 0 || (m_year % 400) == 0);
        }

        // ���������
        // ��Ч����-1
        int days_of_year() const {
            return isleapyear() ? 366 : 365;
        }

        // ���µ�����
        // ��Ч����-1
        int days_of_month() const {
            if (m_month < 1)
                return -1;

            static int daysOfMonthTable[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

            if (m_month == 2 && isleapyear())
                return 29;
            else
                return daysOfMonthTable[m_month - 1];
        }

        // ��ȡ���
        // ��Ч����-1
        int year() const {
            if (!isvalid())
                return -1;
            return m_year;
        }

        // ��ȡ�·�
        // ��Ч����-1
        int month() const {
            if (!isvalid())
                return -1;
            return m_month;
        }

        // ��ȡ�շ�,um....
        // ��Ч����-1
        int day() const {
            if (!isvalid())
                return -1;
            return m_day;
        }

        // ��ȡСʱ
        // ��Ч����-1
        int hour() const {
            if (!isvalid())
                return -1;
            return m_hour;
        }

        // ��ȡ����
        // ��Ч����-1
        int minute() const {
            if (!isvalid())
                return -1;
            return m_minute;
        }

        // ��ȡ����
        // ��Ч����-1
        int second() const {
            if (!isvalid())
                return -1;
            return m_second;
        }

        // ��ȡ������
        // ��Ч����-1
        int millsecond() const {
            if (!isvalid())
                return -1;
            return m_millsecond;
        }

        // ��ȡ΢����
        // ��Ч����-1
        int microsecond() const {
            if (!isvalid())
                return -1;
            return m_microsecond;
        }

    private:
        // ��ķ����ɭ���㹫ʽ
        static DayOfWeek caculate_weekday(int y, int m, int d) {
            if (m == 1 || m == 2) {// ��һ�ºͶ��»������һ���ʮ���º�������
                m += 12;
                y--;
            }
            int Week = (d + 2 * m + 3 * (m + 1) / 5 + y + y / 4 - y / 100 + y / 400) % 7;
            switch (Week) {
            case 0:
                return MONDAY;
                break;
            case 1:
                return TUESDAY;
                break;
            case 2:
                return WEDNESDAY;
                break;
            case 3:
                return THURSDAY;
                break;
            case 4:
                return FRIDAY;
                break;
            case 5:
                return SATURDAY;
                break;
            case 6:
                return SUNDAY;
                break;
            default:
                return UNDEFINE;
                break;
            }
            return UNDEFINE;
        }

    private:
        // ����ʱ���ֶ�
        DateTimeStyle       m_style;

        mutable DayOfWeek   m_day_of_week;

        bool                m_isvalid;
        int                 m_year = 0;
        int                 m_month = 1;
        int                 m_day = 1;
        int                 m_hour = 0;
        int                 m_minute = 0;
        int                 m_second = 0;
        int                 m_millsecond = 0;
        int                 m_microsecond = 0;
    };
}