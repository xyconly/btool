/*************************************************
File name:  co_task_pool.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ����Э�������̳߳ػ���,��������ظ�����
*************************************************/
#pragma once
#include <unordered_map>
#include "libgo/coroutine.h"
#include "atomic_switch.hpp"

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
    template<typename TType>
    class CoroSerialTaskPool {

    public:
        typedef std::function<void()> TaskItem;

    protected:
        struct SerialTask {
            bool        exit_;  // �Ƿ��˳�
            TaskItem    task_;
        };

    public:
        // ��Ҫ��ǰԤ������ֵ����
        // props: ���Զ���
        // max_task_count: ÿ����������������,��������ֵ�ᵼ������
        CoroSerialTaskPool(const std::set<TType>& props, size_t max_task_count = 10000)
            : m_scheduler(nullptr)
        {
            set_props(props, max_task_count);
        }
        ~CoroSerialTaskPool() {
            stop();
        }

        // �������Լ��ϣ�����ִ��stop(true)ǿ�Ƶȴ���ǰ����ִ�����, Ȼ����������,�����¿���
        void reset_props(const std::set<TType>& props, size_t max_task_count = 10000, size_t min_thread_num = 0, size_t max_thread_num = 0) {
            stop(true);
            set_props(props, max_task_count);
            start(min_thread_num, max_thread_num);
        }

        // ����Э�̳�
        // min_thread_num: ������С�߳���,0��ʾϵͳCPU����
        // max_thread_num: ��������߳���,<= ��С�߳���ʱ, ��ʾ�̶��߳���
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

        // ������������,ע��,�������Զ�������ʱ,���������
        template<typename Type>
        bool add_task(Type&& prop, const TaskItem& item) {
            if (!m_atomic_switch.has_started())
                return false;

            SerialTask task{ false, item };
            m_go_tasks[std::forward<Type>(prop)] << task;
            return true;
        }

        // ��ֹ�̳߳�
        // ע��˴����������ȴ�task�Ļص��߳̽���,����task�Ļص��߳��в��ɵ��øú���
        // bwait: �Ƿ�ǿ�Ƶȴ���ǰ���ж���ִ����Ϻ�Ž���
        // ��ȫֹͣ�󷽿����¿���
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
        // ��������ͣ����
        BTool::AtomicSwitch                                     m_atomic_switch;
        // ͳһ������
        co::Scheduler* m_scheduler;

        // ��ǰ�����Դ�ִ������
        std::unordered_map<TType, co::co_chan<SerialTask>>      m_go_tasks;
        // Э���������˳���־
        std::unordered_map<TType, co::co_chan<nullptr_t>>       m_go_child_task_exited;
    };
}