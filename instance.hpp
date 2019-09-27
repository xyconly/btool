/*************************************************
File name:  instance.hpp
Author:     AChar
Version:
Date:
Purpose: ��ʵ����������,�ṩ�����㷨��instance��unInstance��ȡ����
Note:    �ⲿʹ��ʱֱ��ʹ��instance()��ȡ
         volatile����VS���п���,�Ӳ��Ӷ����ᱶ�Ż�,������gcc�»ᱻ�Ż���
*************************************************/

#pragma once

#include <mutex>

namespace BTool
{
    // ��ʵ����������
    template<class _Ty>
    class instance_base {
    public:
        static _Ty* instance() {
            if (!m_pInstance) {
                std::lock_guard<std::mutex> lock(m_mtx_pres);
                if (!m_pInstance) {
                    m_pInstance = new _Ty();
                }
            }
            return m_pInstance;
        }

        static void unInstance() {
            if (m_pInstance) {
                std::lock_guard<std::mutex> lock(m_mtx_pres);
                if (m_pInstance) {
                    delete m_pInstance;
                    m_pInstance = nullptr;
                }
            }
        }

    protected:
        instance_base() {}
        ~instance_base() {}

    private:
        static _Ty* volatile m_pInstance; // Instance
        static std::mutex    m_mtx_pres;  // Instance Lock, Achieve thread-safety
    };

    template<class _Ty>
    _Ty* volatile instance_base<_Ty>::m_pInstance = nullptr;
    template<class _Ty>
    std::mutex instance_base<_Ty>::m_mtx_pres;
}
