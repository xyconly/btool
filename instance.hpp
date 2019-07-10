/*************************************************
File name:  instance.hpp
Author:     AChar
Version:
Date:
Purpose: 单实例公共基类,提供懒惰算法的instance及unInstance获取方法
Note:    外部使用时直接使用instance()获取,不要将instance()做临时存储,那将会导致无法正常析构
*************************************************/

#pragma once

#include <memory>
#include <mutex>

namespace BTool
{
    // 单实例公共基类
    template<class _Ty>
    class instance_base
    {
    public:
        static std::shared_ptr<_Ty> instance()
        {
            if (!_pInstance)
            {
                std::lock_guard<std::recursive_mutex> lock(_mtx_pres_);
                if (!_pInstance)
                {
                    _pInstance = std::make_shared<_Ty>();
                }
            }
            return _pInstance;
        }

        static void unInstance()
        {
            if (_pInstance)
            {
                std::lock_guard<std::recursive_mutex> lock(_mtx_pres_);
                if (_pInstance)
                {
                    _pInstance.reset();
                }
            }
        }

    protected:
        instance_base() {}
        ~instance_base() {}
    private:
        static std::shared_ptr<_Ty>  _pInstance; // Instance
        static std::recursive_mutex  _mtx_pres_; // Instance Lock, Achieve thread-safety
    };

    template<class _Ty>
    std::shared_ptr<_Ty> instance_base<_Ty>::_pInstance = nullptr;
    template<class _Ty>
    std::recursive_mutex instance_base<_Ty>::_mtx_pres_;
}
