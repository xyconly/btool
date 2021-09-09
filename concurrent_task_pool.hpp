/*************************************************
File name:  concurrent_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ���������̳߳ػ���,��������ظ�����
Note:      �ر�ע��!!!!
           �ö���ÿ�����Ծ����������߳�����!!!!
*************************************************/
#pragma once
#include <map>
#include <vector>
#include <set>
#include <functional>
#include "concurrentqueue/concurrentqueue.h"
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
        ConcurrentSerialTaskPool(const std::set<TPropType>& props)
        {
            for (auto& item : props) {
                m_queues[item];
            }
        }

        // �����̳߳�
        // ignore:��Ϊ��������pool
        void start(size_t ignore = 0) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;

            if (m_queues.size() > 100)
                throw std::exception("prop size too mach...");

            for (auto& item : m_queues) {
                m_consumers.emplace_back(
                    new BTool::SafeThread([&, prop = item.first]() {
                    TaskItem task;
                    while (true) {
                        if (m_queues[prop].try_dequeue(task)) {
                            task();
                        }
                        else if (m_atomic_switch.has_stoped() && m_atomic_switch.has_init())
                            return;

                        if (!m_atomic_switch.has_init()) {
                            moodycamel::ConcurrentQueue<TaskItem> tmp;
                            m_queues[prop].swap(tmp);
                            return;
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

            m_queues[std::forward<Type>(prop)].enqueue(task);
            return false;
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