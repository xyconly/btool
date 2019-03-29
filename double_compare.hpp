/******************************************************************************
File name:  double_compare.hpp
Author:     AChar
Purpose: ��ȫ�ȶ�double����
*****************************************************************************/
#pragma once
#include <codecvt>
#include <algorithm>

namespace BTool
{
    class SafeDouble
    {
    public:
        // �Ƚϴ�С
        // mixSpace: ����С��λ����,Ĭ��Ϊ����8λС��
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

        // �������� ���� base > 0
        // base: �������뾫��
        // mixSpace: ����С��λ����,Ĭ��Ϊ����8λС��
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

        // �ܷ�����
        // src: ����
        // base: ��ĸ
        // mixSpace: ����С��λ����,Ĭ��Ϊ����8λС��
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