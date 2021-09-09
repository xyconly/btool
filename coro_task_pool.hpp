/*************************************************
File name:  co_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    提供各类协程任务线程池基类,避免外界重复创建
*************************************************/
#pragma once
#include <unordered_map>
#include "libgo/coroutine.h"
#include "atomic_switch.hpp"

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
    template<typename TType>
    class CoroSerialTaskPool {

    public:
        typedef std::function<void()> TaskItem;

    protected:
        struct SerialTask {
            bool        exit_;  // 是否退出
            TaskItem    task_;
        };

    public:
        // 需要提前预设属性值集合
        // props: 属性队列
        // max_task_count: 每个属性最大任务个数,超过该数值会导致阻塞
        CoroSerialTaskPool(const std::set<TType>& props, size_t max_task_count = 10000)
            : m_scheduler(nullptr)
        {
            set_props(props, max_task_count);
        }
        ~CoroSerialTaskPool() {
            stop();
        }

        // 重置属性集合，首先执行stop(true)强制等待当前队列执行完毕, 然后重置属性,再重新开启
        void reset_props(const std::set<TType>& props, size_t max_task_count = 10000, size_t min_thread_num = 0, size_t max_thread_num = 0) {
            stop(true);
            set_props(props, max_task_count);
            start(min_thread_num, max_thread_num);
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
            max_thread_num = (std::max)(min_thread_num, max_thread_num);
            m_scheduler = co::Scheduler::Create();

            //if(co_opt.stack_size < m_go_tasks.size())
            //    co_opt.stack_size = m_go_tasks.size();

            for (auto& item : m_go_tasks) {
                go co_scheduler(m_scheduler)[this, prop = item.first, go_ch = item.second]{
                    this->go_func(prop, m_go_tasks[prop], m_go_child_task_exited[prop]);
                };
            }
            m_scheduler->goStart(min_thread_num, max_thread_num);
        }

        // 新增属性任务,注意,当该属性队列已满时,会存在阻塞
        template<typename Type>
        bool add_task(Type&& prop, const TaskItem& item) {
            if (!m_atomic_switch.has_started())
                return false;

            SerialTask task{ false, item };
            m_go_tasks[std::forward<Type>(prop)] << task;
            return true;
        }

        // 终止线程池
        // 注意此处可能阻塞等待task的回调线程结束,故在task的回调线程中不可调用该函数
        // bwait: 是否强制等待当前所有队列执行完毕后才结束
        // 完全停止后方可重新开启
        void stop(bool bwait = false) {
            if (!m_atomic_switch.stop())
                return;

            if (bwait) {
                SerialTask task{ true, nullptr };
                for (auto& item : m_go_tasks) {
                    item.second << task;
                }
            }
            else {
                for (auto& item : m_go_tasks)
                    item.second.Close();
            }

            for (auto& item : m_go_child_task_exited) {
                item.second >> nullptr_t();
            }

            m_atomic_switch.reset();
        }

    private:
        void set_props(const std::set<TType>& props, size_t max_task_count) {
            bool large_count = props.size() >= 20000;
            for (auto& item : props) {
                m_go_child_task_exited.emplace(item, co::co_chan<nullptr_t>(max_task_count, large_count));
                m_go_tasks.emplace(item, co::co_chan<SerialTask>(max_task_count, large_count));
            }
        }

        void go_func(const TType& prop, co::co_chan<SerialTask>& go_ch, co::co_chan<nullptr_t>& exit_ch) {
            for (;;) {
                SerialTask task{ true, nullptr };
                go_ch >> task;

                if (task.exit_) {
                    break;
                }

                if (!task.task_)
                    continue;

                task.task_();
            }
            exit_ch << nullptr_t();
            return;
        }

    private:
        // 防重入启停开关
        BTool::AtomicSwitch                                     m_atomic_switch;
        // 统一调度器
        co::Scheduler* m_scheduler;

        // 当前各属性待执行任务
        std::unordered_map<TType, co::co_chan<SerialTask>>      m_go_tasks;
        // 协程子任务退出标志
        std::unordered_map<TType, co::co_chan<nullptr_t>>       m_go_child_task_exited;
    };
}