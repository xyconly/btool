/******************************************************************************
File name:  double_compare.hpp
Author:     AChar
Purpose: 安全比对double数据
*****************************************************************************/
#pragma once
#include <codecvt>
#include <algorithm>

namespace BTool
{
    class SafeDouble
    {
    public:
        // 比较大小
        // mixSpace: 保留小数位精度,默认为保留8位小数
        // left < right ; return -1;
        // left = right ; return 0;
        // left > right ; return 1;
        static int Compare(double left, double right, uint8_t mixSpace = 8) {
            double delta = left - right;
            unsigned long long saveSpace = pow(10, mixSpace);

            if (delta * saveSpace <= -1)
                return -1;

            if (delta * saveSpace >= 1)
                return 1;

            return 0;
        }

        // 四舍五入 其中 base > 0
        // base: 四舍五入精度
        // mixSpace: 保留小数位精度,默认为保留8位小数
        static double Round(double src, double base, uint8_t mixSpace = 8) {
            unsigned long long saveSpace = pow(10, mixSpace);
            if (base * saveSpace > -1 && base * saveSpace < 1)
                return src;
            if (src == 0)
                return 0.0;
            if (src > 0)
                return ((long long)((src + base / 2) / base)) * base;
            else
                return ((long long)((src - base / 2) / base)) * base;

        }

        // 能否被整除
        // src: 分子
        // base: 分母
        // mixSpace: 保留小数位精度,默认为保留8位小数
        static bool CanBeDivided(double src, double base, uint8_t mixSpace = 8) {
            unsigned long long saveSpace = pow(10, mixSpace);
            if (base * saveSpace > -1 && base * saveSpace < 1)
                return false;

            double theta = src / base;
            double delta = theta - (long long)(theta);
            if ((delta * saveSpace > -1 && delta * saveSpace < 1) || ((delta - 1) * saveSpace > -1) || ((delta + 1) * saveSpace < 1))
                return true;

            return false;
        }

    };
}