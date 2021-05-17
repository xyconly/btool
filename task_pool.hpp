/*************************************************
File name:  task_pool.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ���������̳߳ػ���,��������ظ�����
*************************************************/
#pragma once
#include <mutex>
#include <vector>
#include <boost/noncopyable.hpp>
#include "safe_thread.hpp"
#include "task_queue.hpp"

namespace BTool
{
    /*************************************************
                   �����̳߳ػ���
    *************************************************/
    class TaskPoolBase
    {
        enum {
            TP_MAX_THREAD = 2000,   // ����߳���
        };

    public:
        TaskPoolBase() : m_cur_thread_ver(0), m_task_queue(nullptr) {}
        virtual ~TaskPoolBase() { }

    protected:
        void set_queue_ptr(TaskQueueBaseVirtual* task_queue) {
            m_task_queue = task_queue;
        }

        virtual void pop_task_inner() = 0;

    public:
        // �����̳߳�
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        void start(size_t thread_num = std::thread::hardware_concurrency()) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;

            m_task_queue->start();
            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num);
        }

        // ��ֹ�̳߳�
        // ע��˴����������ȴ�task�Ļص��߳̽���,����task�Ļص��߳��в��ɵ��øú���
        // bwait: �Ƿ�ǿ�Ƶȴ���ǰ���ж���ִ����Ϻ�Ž���
        // ��ȫֹͣ�󷽿����¿���
        void stop(bool bwait = false) {
            if (!m_atomic_switch.stop())
                return;

            m_task_queue->stop(bwait);

            std::vector<SafeThread*> tmp_threads;
            {
                std::lock_guard<std::mutex> lck(m_threads_mtx);
                tmp_threads.swap(m_cur_threads);
            }

            for (auto& thread : tmp_threads) {
                delete thread;
                thread = nullptr;
            }

            tmp_threads.clear();
            m_atomic_switch.reset();
        }

        // ����������,��������
        void clear() {
            m_task_queue->clear();
        }

        // �����̳߳ظ���,ÿ����һ���߳�ʱ�����һ��ָ����ڴ�����(�߳���Դ���Զ��ͷ�),ִ��stop��������������������������
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        // ע��:���뿪���̳߳غ󷽿���Ч
        void reset_thread_num(size_t thread_num = std::thread::hardware_concurrency()) {
            if (!m_atomic_switch.has_started())
                return;

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num);
        }

    private:
        // �����߳�
        void create_thread(size_t thread_num) {
            if (thread_num == 0) {
                thread_num = std::thread::hardware_concurrency();
            }
            size_t cur_thread_ver = ++m_cur_thread_ver;
            thread_num = thread_num < TP_MAX_THREAD ? thread_num : TP_MAX_THREAD;
            for (size_t i = 0; i < thread_num; i++) {
                m_cur_threads.push_back(new SafeThread(std::bind(&TaskPoolBase::thread_fun, this, cur_thread_ver)));
            }
        }

        // �̳߳��߳�
        void thread_fun(size_t thread_ver) {
            while (true) {
                if (m_atomic_switch.has_stoped() && m_task_queue->empty()) {
                    break;
                }

                if (thread_ver < m_cur_thread_ver.load())
                    break;

                pop_task_inner();
            }
        }

    protected:
        // ԭ����ͣ��־
        AtomicSwitch                m_atomic_switch;
        // ����ָ��,��ͬ�̳߳�ֻ���滻��ͬ����ָ�뼴��
        TaskQueueBaseVirtual*       m_task_queue;

        std::mutex                  m_threads_mtx;
        // �̶߳���
        std::vector<SafeThread*>    m_cur_threads;
        // ��ǰ�����̰߳汾��,ÿ�����������߳���ʱ,���������ֵ
        std::atomic<size_t>         m_cur_thread_ver;
    };

    /*************************************************
    Description:    �ṩ��������ִ�е��̳߳�
    1, ��ͬʱ��Ӷ������;
    2, �����������Ⱥ�ִ��˳��,�����ܻ�ͬʱ����;
    4, ʵʱ��:ֻҪ�̳߳��߳��п��е�,��ô�ύ������������ִ��;����������̵߳������ʡ�
    5. �ṩ����չ�������̳߳��������ܡ�
    *************************************************/
    class ParallelTaskPool
        : public TaskPoolBase
        , private boost::noncopyable
    {
    public:
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        ParallelTaskPool(size_t max_task_count = 0) : m_task_queue(max_task_count) {
            set_queue_ptr(&m_task_queue);
        }

        virtual ~ParallelTaskPool() {
            stop();
        }

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template<typename TFunction>
        bool add_task(TFunction&& func) {
            if (!m_atomic_switch.has_started())
                return false;
            return m_task_queue.add_task(std::forward<TFunction>(func));
        }

    protected:
        virtual void pop_task_inner() override {
            m_task_queue.pop_task();
        }

    protected:
        TaskQueue               m_task_queue;
    };

    /*************************************************
    Description:    ר����CTP,�ṩ��������ִ�е��̳߳�
    1, ��ͬʱ��Ӷ������;
    2, �����������Ⱥ�ִ��˳��,�����ܻ�ͬʱ����;
    4, ʵʱ��:ֻҪ�̳߳��߳��п��е�,��ô�ύ������������ִ��;����������̵߳������ʡ�
    5. �ṩ����չ�������̳߳��������ܡ�
    6. ÿ��POPʱ����ʱ1S
    *************************************************/
    class ParallelWaitTaskPool : public ParallelTaskPool
    {
    public:
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        ParallelWaitTaskPool(size_t max_task_count = 0)
            : ParallelTaskPool(max_task_count)
            , m_sleep_millseconds(1100)
        {}

        ~ParallelWaitTaskPool() {}

        // ���ü��ʱ��
        void set_sleep_milliseconds(long long millseconds) {
            m_sleep_millseconds = millseconds;
        }

    protected:
        void pop_task_inner() override {
            ParallelTaskPool::pop_task_inner();
            std::this_thread::sleep_for(std::chrono::milliseconds(m_sleep_millseconds));
        }

    private:
        long long m_sleep_millseconds;
    };

    /*************************************************
    Description:    �ṩ������ͬ��������ִ������״̬���̳߳�
    1, ÿ�����Զ�����ͬʱ��Ӷ������;
    2, �кܶ�����Ժͺܶ������;
    3, ÿ��������ӵ��������������ִ��,����ͬһʱ�̲�����ͬʱִ��һ���û�����������;
    4, ʵʱ��:ֻҪ�̳߳��߳��п��е�,��ô�ύ������������ִ��;����������̵߳������ʡ�
    5. �ṩ����չ�������̳߳��������ܡ�
    *************************************************/
    template<typename TPropType>
    class LastTaskPool
        : public TaskPoolBase
        , private boost::noncopyable
    {
    public:
        // ������ͬ��������ִ������״̬���̳߳�
        // max_task_count: ����������,��������������������;0���ʾ������
        LastTaskPool(size_t max_task_count = 0)
            : m_task_queue(max_task_count)
        {
            set_queue_ptr(&m_task_queue);
        }

        ~LastTaskPool() {
            stop();
        }

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template<typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            if (!m_atomic_switch.has_started())
                return false;
            return m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func));
        }

        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            m_task_queue.remove_prop(std::forward<AsTPropType>(prop));
        }

    protected:
        void pop_task_inner() override {
            m_task_queue.pop_task();
        }

    private:
        // ��ִ���������
        LastTaskQueue<TPropType>        m_task_queue;
    };

    /*************************************************
    Description:    �ṩ������ͬ��������������ִ�е��̳߳�
    1, ÿ�����Զ�����ͬʱ��Ӷ������;
    2, �кܶ�����Ժͺܶ������;
    3, ÿ��������ӵ��������������ִ��,����ͬһʱ�̲�����ͬʱִ��һ���û�����������;
    4, ʵʱ��:ֻҪ�̳߳��߳��п��е�,��ô�ύ������������ִ��;����������̵߳������ʡ�
    5. �ṩ����չ�������̳߳��������ܡ�
    *************************************************/
    template<typename TPropType>
    class SerialTaskPool
        : public TaskPoolBase
        , private boost::noncopyable
    {
    public:
        // ������ͬ��������������ִ�е��̳߳�
        // max_task_count: ����������,��������������������;0���ʾ������
        SerialTaskPool(size_t max_task_count = 0) : m_task_queue(max_task_count) {
            set_queue_ptr(&m_task_queue);
        }

        ~SerialTaskPool() {
            stop();
        }

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template<typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            if (!m_atomic_switch.has_started())
                return false;
            return m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func));
        }

        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            m_task_queue.remove_prop(std::forward<AsTPropType>(prop));
        }

    protected:
        void pop_task_inner() override {
            m_task_queue.pop_task();
        }

    private:
        // ��ִ���������
        SerialTaskQueue<TPropType>          m_task_queue;
    };

}