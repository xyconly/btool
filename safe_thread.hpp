/*************************************************
File name:  safe_thread.hpp
Author:     AChar
Version:
Date:
Description:    提供安全的线程操作
1, 可复制对象,从而加入容器中;
2, 避免使用new等自维护的内存模块时,由于线程已执行完毕而自动释放时,导致重复析构的问题;
3, 非安全退出时,执行std::thread本身的terminate();可通过set_terminate(func_cbk)捕获
*************************************************/
#pragma once

#include <thread>

namespace BTool
{
    // 安全线程
    class SafeThread
    {
    public:
        // 安全线程
        // safe_exit: 是否确保安全退出, 若为false则再退出时调用terminate()
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

        // 禁止拷贝
        SafeThread(const SafeThread&) = delete;
        SafeThread& operator=(const SafeThread&) = delete;

        static unsigned int hardware_concurrency() noexcept {
            return std::thread::hardware_concurrency();
        }

        void set_safe_flag(bool safe_exit) noexcept {
            m_safe_exit = safe_exit;
        }

        // 开启任务,如果原先任务已存在且未结束则返回false
        template<typename _Callable, typename... _Args>
        bool start(_Callable&& _Fx, _Args&&... _args)
        {
            if (m_thread.joinable())
                return false;
            m_thread = std::thread(std::forward<_Callable>(_Fx), std::forward<_Args>(_args)...);
            return true;
        }

        // 开启任务,如果原先任务已存在且未结束则等待结束后才会开启,存在阻塞,谨慎使用
        template<typename _Callable, typename... _Args>
        void restart(_Callable&& _Fx, _Args&&... _args)
        {
            if (m_thread.joinable())
                m_thread.join();
            m_thread = std::thread(std::forward<_Callable>(_Fx), std::forward<_Args>(_args)...);
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
        // 是否安全退出
        bool                m_safe_exit;
        // 执行线程
        std::thread         m_thread;
    };
}