/************************************************************************
File name:  thread_core.hpp
Author:	    AChar
Purpose:    �ɿ�����ͣ���첽�߳�
Note:       ���ṩǿɱ�̵߳ķ���,�������ڻص��߳�����ֹ�̲߳���
************************************************************************/
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
            m_atomic_switch.init();
        }

        thread_core(Function&& func)
            : m_func(func)
            , m_thread(nullptr)
        {
            m_atomic_switch.init();
        }

        ~thread_core() {
            stop();
        }

        // ���������̻߳ص�����
        void set_thread_fun(const Function& func) {
            m_func = func;
        }
        void set_thread_fun(Function&& func) {
            m_func = std::move(func);
        }

        // ����
        void start() {
            if (!m_atomic_switch.start())
                return;

            m_thread = new SafeThread(&thread_core::thread_fun, this);
            return;
        }

        // ��ֹ, �����ڻص����̶߳�������ֹ�̲߳���
        void stop() {
            if (!m_atomic_switch.stop())
                return;

            if (m_thread && m_thread->joinable())
                m_thread->join();

            m_atomic_switch.reset();
            return;
        }

        // �Ƿ�������
        bool is_running() const {
            return m_atomic_switch.has_started();
        }

        // �Ƿ�����ֹ
        bool is_stoped() const {
            return m_atomic_switch.has_stoped();
        }

    protected:
        void thread_fun() {
            m_func();
        }

    protected:
        Function		    m_func;         // �ص�����
        AtomicSwitch        m_atomic_switch;// ԭ����ͣ��־
        SafeThread*         m_thread;       // ��ȫ�߳�
    };
}
