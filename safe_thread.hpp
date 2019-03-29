/*************************************************
File name:  safe_thread.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ��ȫ���̲߳���
1, �ɸ��ƶ���,�Ӷ�����������;
2, ����ʹ��new����ά�����ڴ�ģ��ʱ,�����߳���ִ����϶��Զ��ͷ�ʱ,�����ظ�����������;
3, �ǰ�ȫ�˳�ʱ,ִ��std::thread�����terminate();��ͨ��set_terminate(func_cbk)����
*************************************************/
#pragma once

#include <thread>

namespace BTool
{
    // ��ȫ�߳�
    class SafeThread
    {
    public:
        // ��ȫ�߳�
        // safe_exit: �Ƿ�ȷ����ȫ�˳�, ��Ϊfalse�����˳�ʱ����terminate()
        SafeThread(bool safe_exit = true) noexcept
            : m_safe_exit(safe_exit)
        {
        }

        template<typename _Callable, typename... _Args>
        explicit SafeThread(_Callable&& _Fx, _Args&&... _args)
            : m_safe_exit(true)
        {
            m_thread = std::thread(std::forward<_Callable>(_Fx), std::forward<_Args>(_args)...);
        }

        virtual ~SafeThread() noexcept {
            stop();
        }

        // ��ֹ����
        SafeThread(const SafeThread&) = delete;
        SafeThread& operator=(const SafeThread&) = delete;

        static unsigned int hardware_concurrency() noexcept {
            return std::thread::hardware_concurrency();
        }

        void set_safe_flag(bool safe_exit) noexcept {
            m_safe_exit = safe_exit;
        }

        template<typename _Callable, typename... _Args>
        bool start(_Callable&& _Fx, _Args&&... _args)
        {
            if (m_thread.joinable())
                return false;
            m_thread = std::thread(std::forward<_Callable>(_Fx), std::forward<_Args>(_args)...);
            return true;
        }

        void stop() {
            if (m_safe_exit && m_thread.joinable())
                m_thread.join();
        }

        bool joinable() noexcept {
            return m_thread.joinable();
        }

        void join() {
            if (m_thread.joinable())
                m_thread.join();
        }

        void detach() {
            if (m_thread.joinable())
                m_thread.detach();
        }

        void swap(std::thread& _Other) noexcept {
            m_thread.swap(_Other);
        }

        void swap(SafeThread& _Other) noexcept {
            m_thread.swap(_Other.m_thread);
        }

        void swap(SafeThread&& _Other) noexcept {
            m_thread.swap(_Other.m_thread);
        }

        std::thread::id get_id() const noexcept {
            return m_thread.get_id();
        }
        std::thread::native_handle_type native_handle() {
            return m_thread.native_handle();
        }

    private:
        // �Ƿ�ȫ�˳�
        bool                m_safe_exit;
        // ִ���߳�
        std::thread         m_thread;
    };
}