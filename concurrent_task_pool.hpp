/*************************************************
File name:  concurrent_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ���������̳߳ػ���,��������ظ�����
Note:      �ر�ע��!!!!
           �ö���ÿ�����Ծ����������߳�����!!!!
           �ʶ������������Թ�������, �������Զ����cpu�����������!!!
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
    Description:    �ṩ������ͬ��������������ִ�е��̳߳�
    1, ÿ�����Զ�����ͬʱ��Ӷ������;
    2, �кܶ�����Ժͺܶ������;
    3, ÿ��������ӵ��������������ִ��,����ͬһʱ�̲�����ͬʱִ��һ���û�����������;
    4, ʵʱ��:ֻҪ�̳߳��߳��п��е�,��ô�ύ������������ִ��;����������̵߳������ʡ�
    5. �ṩ����չ�������̳߳��������ܡ�
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

        // �����̳߳�
        // ignore:��Ϊ��������pool
        void start(size_t ignore = 0) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;

            // �̹߳��෴��Ӱ������, Ĭ�����10��CPU������
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
        // ��������ͣ����
        BTool::AtomicSwitch                                         m_atomic_switch;
        std::vector<BTool::SafeThread*>                             m_consumers;
        std::map<TPropType, moodycamel::ConcurrentQueue<TaskItem>>  m_queues;
    };
}