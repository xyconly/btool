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
    template<class _Ty>
    class instance_base {
    public:
        static _Ty* instance() {
            static _Ty s_instance;
            return &s_instance;
        }
    };
    
    // ��ʵ����������
    template<class _Ty>
    class lazy_instance_base {
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
        lazy_instance_base() {}
        ~lazy_instance_base() {}

    private:
        static _Ty* volatile m_pInstance; // Instance
        static std::mutex    m_mtx_pres;  // Instance Lock, Achieve thread-safety
    };

    template<class _Ty>
    _Ty* volatile lazy_instance_base<_Ty>::m_pInstance = nullptr;
    template<class _Ty>
    std::mutex lazy_instance_base<_Ty>::m_mtx_pres;
}
