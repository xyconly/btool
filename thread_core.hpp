/************************************************************************
File name:  thread_core.hpp
Author:	    AChar
Purpose:    可控制启停的异步线程
Note:       不提供强杀线程的方法,不允许在回调线程中终止线程操作
/************************************************************************/
#pragma once

#include <assert.h>
#include <thread>
#include "rwmutex.hpp"
#include "safe_thread.hpp"
#include "atomic_switch.hpp"

namespace BTool
{
    class thread_core
    {
        typedef std::function<void()> Function;
    public:
        thread_core()
            : m_func(nullptr)
            , m_thread(nullptr)
        {
        }

        thread_core(Function&& func)
            : m_func(func)
            , m_thread(nullptr)
        {
        }

        ~thread_core() {
            stop();
        }

        // 重新设置线程回调函数
        void set_thread_fun(const Function& func) {
            m_func = func;
        }
        void set_thread_fun(Function&& func) {
            m_func = std::move(func);
        }

        // 启动
        bool start() {
            if (!m_atomic_switch.start())
                return true;

            m_thread = new SafeThread(&thread_core::thread_fun, this);
            return true;
        }

        // 终止, 不可在回调的线程动力中终止线程操作
        bool stop() {
            if (!m_atomic_switch.stop())
                return true;

            if (m_thread && m_thread->joinable())
                m_thread->join();

            m_atomic_switch.store_start_flag(false);
            return true;
        }

        // 是否已启动
        bool is_running() const {
            return m_atomic_switch.has_started();
        }

        // 是否已终止
        bool is_stoped() const {
            return m_atomic_switch.has_stoped();
        }

    protected:
        void thread_fun() {
            m_func();
        }

    protected:
        Function		    m_func;         // 回调函数
        AtomicSwitch        m_atomic_switch;// 原子启停标志
        SafeThread*         m_thread;       // 安全线程
    };
}
