/*************************************************
File name:  concurrent_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    提供无锁任务线程池基类,避免外界重复创建
Note:      特别注意!!!!
           该队列每个属性均创建独立线程运行!!!!
           故而不能用于属性过多的情况, 避免出现远大于cpu核心数的情况!!!
*************************************************/
#pragma once
#include <stdexcept>
#include <map>
#include <vector>
#include <set>
#include <functional>
#include "submodule/concurrentqueue/concurrentqueue.h"
#include "atomic_switch.hpp"
#include "safe_thread.hpp"

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
    template<typename TPropType>
    class ConcurrentSerialTaskPool {
    public:
        typedef std::function<void()> TaskItem;
    public:
        ConcurrentSerialTaskPool() {}
        ConcurrentSerialTaskPool(const std::set<TPropType>& props) {
            init_props(props);
        }

        void init_props(const std::set<TPropType>& props) {
            for (auto& item : props) {
                m_queues[item];
            }
        }

        // 开启线程池
        // ignore:仅为适配其他pool
        void start(size_t ignore = 0) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;

            // 线程过多反而影响性能, 默认最大10倍CPU核心数
            static int max_thread_num = 10 * std::thread::hardware_concurrency();

            if (m_queues.size() > max_thread_num)
                throw std::runtime_error("prop size too mach!");

            for (auto& item : m_queues) {
                m_consumers.emplace_back(
                    new BTool::SafeThread([&, que = &item.second]() {
                    TaskItem task;
                    while (true) {
                        if (que->try_dequeue(task)) {
                            task();
                        }
                        else if (m_atomic_switch.has_stoped() && que->size_approx() == 0) {
                            break;
                        }

                        if (!m_atomic_switch.has_init()) {
                            moodycamel::ConcurrentQueue<TaskItem> tmp;
                            que->swap(tmp);
                            break;
                        }
                    }
                })
                );

            }
        }

        template<typename Type>
        bool add_task(Type&& prop, const TaskItem& task) {
            if (!m_atomic_switch.has_started())
                return false;

            auto iter = m_queues.find(std::forward<Type>(prop));
            if(iter == m_queues.end()) {
                return false;
            }
            iter->second.enqueue(task);
            return true;
        }

        void stop(bool bwait = false) {
            if (!m_atomic_switch.stop())
                return;

            if (!bwait) {
                m_atomic_switch.reset();
            }

            for (auto& item : m_consumers) {
                delete item;
            }
            m_consumers.clear();
        }

    private:
        // 防重入启停开关
        BTool::AtomicSwitch                                         m_atomic_switch;
        std::vector<BTool::SafeThread*>                             m_consumers;
        std::map<TPropType, moodycamel::ConcurrentQueue<TaskItem>>  m_queues;
    };
}