/*************************************************
File name:  instance.hpp
Author:     AChar
Version:
Date:
Purpose: ��ʵ����������,�ṩ�����㷨��instance��unInstance��ȡ����
Note:    �ⲿʹ��ʱֱ��ʹ��instance()��ȡ,��Ҫ��instance()����ʱ�洢,�ǽ��ᵼ���޷���������
*************************************************/

#pragma once

#include <memory>
#include <mutex>

namespace BTool
{
    // ��ʵ����������
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
