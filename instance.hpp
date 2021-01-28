/*************************************************
File name:  instance.hpp
Author:     AChar
Version:
Date:
Purpose: 单实例公共基类,提供懒惰算法的instance及unInstance获取方法
Note:    外部使用时直接使用instance()获取
         volatile对于VS可有可无,加不加都不会倍优化,但是在gcc下会被优化掉
*************************************************/

#pragma once

#include <mutex>

namespace BTool
{
    // 单实例公共基类
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
