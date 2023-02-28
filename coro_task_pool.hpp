/*************************************************
File name:  co_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    提供各类协程任务线程池基类,避免外界重复创建
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
    Description:    提供具有相同属性任务串行有序执行的线程池
    1, 每个属性都可以同时添加多个任务;
    2, 有很多的属性和很多的任务;
    3, 每个属性添加的任务必须有序串行执行,即在同一时刻不能有同时执行一个用户的两个任务;
    4, 实时性:只要线程池线程有空闲的,那么提交任务后必须立即执行;尽可能提高线程的利用率。
    5. 提供可扩展或缩容线程池数量功能。
    *************************************************/
    template<typename TType, bool NO_LOCK = true>
    class CoroSerialTaskPool {
    public:
        typedef std::function<void()> TaskItem;

    protected:
        struct SerialTask {
            bool        exit_;  // 是否退出
            TaskItem    task_;
        };
        // 协程启停状态标志
        typedef co::co_chan<SerialTask> GoTaskChanType;
        typedef co::co_chan<nullptr_t>  GoRunFlagChanType;

    public:
        // 需要提前预设属性值集合
        // max_single_task_count: 每个属性最大任务个数,超过该数值会导致阻塞
        CoroSerialTaskPool(size_t max_single_task_count = 10000)
            : m_scheduler(co::Scheduler::Create())
            , m_max_single_task_count(max_single_task_count)
        {
        }

        CoroSerialTaskPool(const std::set<TType>& props, size_t max_single_task_count = 10000)
            : m_scheduler(co::Scheduler::Create())
            , m_max_single_task_count(max_single_task_count)
        {
            init_props(props);
        }
        
        ~CoroSerialTaskPool() {
            stop();
        }

        // 重置属性集合，首先执行stop(true)强制等待当前队列执行完毕, 然后重置属性,再重新开启
        // void reset_props(size_t max_single_task_count = 0, size_t min_thread_num = 0, size_t max_thread_num = 0) {
        //     stop(true);
        //     m_max_single_task_count = max_single_task_count;
        //     start(min_thread_num, max_thread_num);
        // }

        // props: 属性队列
        void init_props(const TType& prop) {
            if constexpr (NO_LOCK) {
                start_prop(prop);
            }
            else {
                writeLock locker(m_mtx);
                start_prop(prop);
            }
        }
        void init_props(const std::set<TType>& props) {
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

        // 开启协程池
        // min_thread_num: 开启最小线程数,0表示系统CPU核数
        // max_thread_num: 开启最大线程数,<= 最小线程数时, 表示固定线程数
        void start(size_t min_thread_num = 0, size_t max_thread_num = 0) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;

            if (min_thread_num == 0) {
                min_thread_num = std::thread::hardware_concurrency();
            }
            if (max_thread_num == 0) {
                max_thread_num = std::thread::hardware_concurrency();
            }
            max_thread_num = (std::max)(min_thread_num, max_thread_num);

            //if(co_opt.stack_size < m_go_tasks.size())
            //    co_opt.stack_size = m_go_tasks.size();

            m_scheduler->goStart(min_thread_num, max_thread_num);
        }

        // 新增属性任务,注意,当该属性队列已满时,会存在阻塞
        template<typename Type>
        bool add_task(Type&& prop, const TaskItem& item) {
            if (!m_atomic_switch.has_started())
                return false;

            SerialTask task{ false, item };

            if constexpr (NO_LOCK) {
                auto iter = m_go_tasks.find(std::forward<Type>(prop));
                if(iter == m_go_tasks.end())
                    return false;

                iter->second << task;
            }
            else {
                writeLock locker(m_mtx);
                auto& go_ch = start_prop(std::forward<Type>(prop));
                go_ch << task;
            }

            return true;
        }

        // 终止线程池
        // 注意此处可能阻塞等待task的回调线程结束,故在task的回调线程中不可调用该函数
        // 强制等待当前所有队列执行完毕后才结束
        // 完全停止后方可重新开启
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
        void go_func(const TType& prop, GoTaskChanType& go_ch, GoRunFlagChanType& exit_ch) {
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

        template<typename Type>
        GoTaskChanType& start_prop(Type&& prop) {
            auto [go_iter, go_ok] = m_go_tasks.try_emplace(prop, GoTaskChanType(m_max_single_task_count));
            if (go_ok) {
                auto [exit_iter, exit_ok] = m_go_task_exited.try_emplace(prop, GoRunFlagChanType(1));
                if (!exit_ok) {
                    throw std::runtime_error("exited prop try_emplace error!");
                }
                go co_scheduler(m_scheduler) [this, prop = std::forward<Type>(prop), go_ch = std::ref(go_iter->second), exit_ch = std::ref(exit_iter->second)] {
                    this->go_func(prop, go_ch, exit_ch);
                };
            }
            return go_iter->second;
        }

    private:
        // 防重入启停开关
        BTool::AtomicSwitch                                     m_atomic_switch;
        // 统一调度器
        co::Scheduler*                                          m_scheduler;
        // 单属性任务最大队列个数
        size_t                                                  m_max_single_task_count;

        // 数据安全锁
        rwMutex                                                 m_mtx;
        // 当前各属性待执行任务
        std::unordered_map<TType, GoTaskChanType>               m_go_tasks;
        // 协程子任务退出标志
        std::unordered_map<TType, GoRunFlagChanType>            m_go_task_exited;
    };
}