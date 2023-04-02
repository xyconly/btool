/*************************************************
File name:  co_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ����Э�������̳߳ػ���,��������ظ�����
*************************************************/
#pragma once
#include <set>
#include <unordered_map>
#include "submodule/libgo/libgo/coroutine.h"
#include "atomic_switch.hpp"
#include "rwmutex.hpp"

namespace BTool
{
    /*************************************************
    Description:    �ṩ������ͬ��������������ִ�е��̳߳�
    1, ÿ�����Զ�����ͬʱ��Ӷ������;
    2, �кܶ�����Ժͺܶ������;
    3, ÿ��������ӵ��������������ִ��,����ͬһʱ�̲�����ͬʱִ��һ���û�����������;
    4, ʵʱ��:ֻҪ�̳߳��߳��п��е�,��ô�ύ������������ִ��;����������̵߳������ʡ�
    5. �ṩ����չ�������̳߳��������ܡ�
    *************************************************/
    template<typename TPropType, bool NO_LOCK = true>
    class CoroSerialTaskPool {
    public:
        typedef std::function<void()> TaskItem;

    protected:
        struct SerialTask {
            bool        exit_;  // �Ƿ��˳�
            TaskItem    task_;
        };
        // Э����ͣ״̬��־
        typedef co::co_chan<SerialTask> GoTaskChanType;
        typedef co::co_chan<nullptr_t>  GoRunFlagChanType;

    public:
        // ��Ҫ��ǰԤ������ֵ����
        // max_single_task_count: ÿ����������������,��������ֵ�ᵼ������
        CoroSerialTaskPool(size_t max_single_task_count = 100000)
            : m_scheduler(co::Scheduler::Create())
            , m_max_single_task_count(max_single_task_count)
        {
        }

        CoroSerialTaskPool(const std::set<TPropType>& props, size_t max_single_task_count = 100000)
            : m_scheduler(co::Scheduler::Create())
            , m_max_single_task_count(max_single_task_count)
        {
            init_props(props);
        }
        
        ~CoroSerialTaskPool() {
            stop();
        }

        // �������Լ��ϣ�����ִ��stop(true)ǿ�Ƶȴ���ǰ����ִ�����, Ȼ����������,�����¿���
        // void reset_props(size_t max_single_task_count = 0, size_t min_thread_num = 0, size_t max_thread_num = 0) {
        //     stop(true);
        //     m_max_single_task_count = max_single_task_count;
        //     start(min_thread_num, max_thread_num);
        // }

        // props: ���Զ���
        void init_props(const TPropType& prop) {
            if constexpr (NO_LOCK) {
                start_prop(prop);
            }
            else {
                writeLock locker(m_mtx);
                start_prop(prop);
            }
        }

        void init_props(const std::set<TPropType>& props) {
            if constexpr (NO_LOCK) {
                for(auto& prop : props) {
                    start_prop(prop);
                }
            }
            else {
                writeLock locker(m_mtx);
                for(auto& prop : props) {
                    start_prop(prop);
                }
            }
        }

        // ����Э�̳�
        // min_thread_num: ������С�߳���,0��ʾϵͳCPU����
        // max_thread_num: ��������߳���,<= ��С�߳���ʱ, ��ʾ�̶��߳���
        void start(size_t min_thread_num = 0, size_t max_thread_num = 0) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;

            if (min_thread_num == 0) {
                min_thread_num = 1;
            }
            if (max_thread_num == 0) {
                max_thread_num = std::thread::hardware_concurrency();
            }
            max_thread_num = (std::max)(min_thread_num, max_thread_num);

            //if(co_opt.stack_size < m_go_tasks.size())
            //    co_opt.stack_size = m_go_tasks.size();

            m_scheduler->goStart(min_thread_num, max_thread_num);
        }

        // ������������,ע��,�������Զ�������ʱ,���������
        template<typename TType, typename TFunction>
        bool add_task(TType&& prop, TFunction&& item) {
            if (!m_atomic_switch.has_started())
                return false;

            SerialTask task{ false, std::forward<TFunction>(item) };

            if constexpr (NO_LOCK) {
                auto iter = m_go_tasks.find(std::forward<TType>(prop));
                if(iter == m_go_tasks.end())
                    return false;

                iter->second << task;
            }
            else {
                writeLock locker(m_mtx);
                auto& go_ch = start_prop(std::forward<TType>(prop));
                go_ch << task;
            }

            return true;
        }

        // ��ֹ�̳߳�
        // ע��˴����������ȴ�task�Ļص��߳̽���,����task�Ļص��߳��в��ɵ��øú���
        // ǿ�Ƶȴ���ǰ���ж���ִ����Ϻ�Ž���
        // ��ȫֹͣ�󷽿����¿���
        void stop() {
            if (!m_atomic_switch.stop())
                return;

            auto stop_impl = [&] () {
                SerialTask task{ true, nullptr };
                for (auto& item : m_go_tasks) {
                    item.second << task;
                    m_go_task_exited[item.first] >> nullptr_t();
                }
            };
            
            if constexpr (NO_LOCK) {
                stop_impl();
            }
            else {
                writeLock locker(m_mtx);
                stop_impl();
            }

            // if(m_scheduler) {
            //     delete m_scheduler;
            //     m_scheduler = nullptr;
            // }
            m_atomic_switch.reset();
        }

    private:
        void go_func(const TPropType& prop, GoTaskChanType& go_ch, GoRunFlagChanType& exit_ch) {
            for (;;) {
                SerialTask task{ true, nullptr };
                go_ch.pop(task);
                if (task.exit_) {
                    break;
                }
                if(task.task_)
                    task.task_();
            }
            exit_ch << nullptr_t();
            return;
        }

        template<typename TType>
        GoTaskChanType& start_prop(TType&& prop) {
            auto [go_iter, go_ok] = m_go_tasks.try_emplace(prop, GoTaskChanType(m_max_single_task_count));
            if (go_ok) {
                auto [exit_iter, exit_ok] = m_go_task_exited.try_emplace(prop, GoRunFlagChanType(1));
                if (!exit_ok) {
                    throw std::runtime_error("exited prop try_emplace error!");
                }
                go co_scheduler(m_scheduler) [this, prop = std::forward<TType>(prop), go_ch = std::ref(go_iter->second), exit_ch = std::ref(exit_iter->second)] {
                    this->go_func(prop, go_ch, exit_ch);
                };
            }
            return go_iter->second;
        }

    private:
        // ��������ͣ����
        BTool::AtomicSwitch                                     m_atomic_switch;
        // ͳһ������
        co::Scheduler*                                          m_scheduler;
        // ���������������и���
        size_t                                                  m_max_single_task_count;

        // ���ݰ�ȫ��
        rwMutex                                                 m_mtx;
        // ��ǰ�����Դ�ִ������
        std::unordered_map<TPropType, GoTaskChanType>           m_go_tasks;
        // Э���������˳���־
        std::unordered_map<TPropType, GoRunFlagChanType>        m_go_task_exited;
    };

    template<template<typename TPropType> typename TSerialTaskPool, typename TPropType, bool NO_LOCK = true>
    class CoroSerialTaskPoolWithThreadPool : public CoroSerialTaskPool<TPropType, NO_LOCK> {
    public:
        CoroSerialTaskPoolWithThreadPool(size_t max_single_task_count = 100000)
            : CoroSerialTaskPool<TPropType, NO_LOCK>(max_single_task_count)
        {
        }

        CoroSerialTaskPoolWithThreadPool(const std::set<TPropType>& props, size_t max_single_task_count = 100000)
            : CoroSerialTaskPool<TPropType, NO_LOCK>(props, max_single_task_count)
        {
        }

        ~CoroSerialTaskPoolWithThreadPool(){
        }

        void start(size_t thread_pool_num = std::thread::hardware_concurrency(), size_t min_coro_thread_num = 0, size_t max_coro_thread_num = 0) {
            CoroSerialTaskPool<TPropType, NO_LOCK>::start(min_coro_thread_num, max_coro_thread_num);
            std::unique_lock<co_rwmutex> lock(m_smtx);
            m_task_pool.start(thread_pool_num);
        }

        void stop(bool bwait = false) {
            CoroSerialTaskPool<TPropType, NO_LOCK>::stop();
            std::unique_lock<co_rwmutex> lock(m_smtx);
            m_task_pool.stop(bwait);
        }
        void clear_threadpool() {
            std::unique_lock<co_rwmutex> lock(m_smtx);
            m_task_pool.clear();
        }
        void wait_threadpool() {
            std::unique_lock<co_rwmutex> lock(m_smtx);
            m_task_pool.wait();
        }

        // ע��co_rwmutex��Э���ڲ���Ч
        template<typename TType, typename TFunction>
        bool push_threadpool(TType&& prop, TFunction&& item) {
            std::unique_lock<co_rwmutex> lock(m_smtx);
            return m_task_pool.add_task(std::forward<TType>(prop), std::forward<TFunction>(item));
        }

    private:
        TSerialTaskPool<TPropType>          m_task_pool;
        co_rwmutex                          m_smtx;
    };

}