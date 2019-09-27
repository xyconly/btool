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
    class TaskPoolVirtual
    {
        enum {
            TP_MAX_THREAD = 2000,   // ����߳���
        };

    public:
        TaskPoolVirtual() : m_cur_thread_ver(0) {
            m_atomic_switch.init();
        }
        virtual ~TaskPoolVirtual() {}

    public:
        // �Ƿ�������
        bool has_start() const {
            return m_atomic_switch.has_started();
        }

        // �Ƿ�����ֹ
        bool has_stop() const {
            return m_atomic_switch.has_stoped();
        }

        // �����̳߳�
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        void start(size_t thread_num = std::thread::hardware_concurrency()) {
            if (!m_atomic_switch.start())
                return;

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num);
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

        // ��ֹ�̳߳�
        // ע��˴����������ȴ�task�Ļص��߳̽���,����task�Ļص��߳��в��ɵ��øú���
        // ��ȫֹͣ�󷽿����¿���
        void stop() {
            if (!m_atomic_switch.stop())
                return;

            stop_inner();

            std::vector<SafeThread*> tmp_threads;
            {
                std::lock_guard<std::mutex> lck(m_threads_mtx);
                tmp_threads.swap(m_cur_thread);
            }
            for (auto& thread : tmp_threads) {
                delete thread;
                thread = nullptr;
            }
            tmp_threads.clear();
            m_atomic_switch.store_start_flag(false);
        }

    protected:
        // �ڲ�ִ��stop����
        virtual void stop_inner() = 0;
        // �ڲ�ִ��pop�������,���޿�pop����ʱ������
        virtual void pop_task_inner() = 0;

    private:
        // �����߳�
        void create_thread(size_t thread_num) {
            if (thread_num == 0) {
                thread_num = std::thread::hardware_concurrency();
            }
            m_cur_thread_ver++;
            thread_num = thread_num < TP_MAX_THREAD ? thread_num : TP_MAX_THREAD;
            for (size_t i = 0; i < thread_num; i++) {
                SafeThread* thd = new SafeThread(std::bind(&TaskPoolVirtual::thread_fun, this, m_cur_thread_ver));
                m_cur_thread.push_back(thd);
            }
        }

        // �̳߳��߳�
        void thread_fun(size_t thread_ver) {
            while (true) {
                if (m_atomic_switch.has_stoped())
                    break;

                {
                    std::lock_guard<std::mutex> lck(m_threads_mtx);
                    if (thread_ver < m_cur_thread_ver)
                        break;
                }

                pop_task_inner();
            }
        }

    private:
        // ԭ����ͣ��־
        AtomicSwitch                m_atomic_switch;

        // �̶߳�����
        std::mutex                  m_threads_mtx;
        // �̶߳���
        std::vector<SafeThread*>    m_cur_thread;
        // ��ǰ�����̰߳汾��,ÿ�����������߳���ʱ,���������ֵ
        size_t                      m_cur_thread_ver;

    };

    /*************************************************
    Description:    �ṩ��������ִ�е��̳߳�
    1, ��ͬʱ��Ӷ������;
    2, �����������Ⱥ�ִ��˳��,�����ܻ�ͬʱ����;
    4, ʵʱ��:ֻҪ�̳߳��߳��п��е�,��ô�ύ������������ִ��;����������̵߳������ʡ�
    5. �ṩ����չ�������̳߳��������ܡ�
    *************************************************/
    class ParallelTaskPool
        : public TaskPoolVirtual
        , private boost::noncopyable
    {
    public:
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        ParallelTaskPool(size_t max_task_count = 0)
            : m_task_queue(max_task_count)
        {}

        virtual ~ParallelTaskPool() {
            clear();
            stop();
        }

        // ����������
        void clear() {
            m_task_queue.clear();
        }

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template<typename Function, typename... Args>
        bool add_task(Function&& func, Args&&... args) {
            if (!has_start())
                return false;

            return m_task_queue.add_task(std::forward<Function>(func), std::forward<Args>(args)...);
        }

    protected:
        void stop_inner() override final {
            m_task_queue.stop();
        }

        virtual void pop_task_inner() override {
            m_task_queue.pop_task();
        }

    protected:
        TupleTaskQueue           m_task_queue;
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
            , m_sleep_millseconds(1000)
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
        : public TaskPoolVirtual
        , private boost::noncopyable
    {
        typedef std::shared_ptr<PropTaskVirtual<TPropType>> PropTaskPtr;

    public:
        // ������ͬ��������ִ������״̬���̳߳�
        // max_task_count: ����������,��������������������;0���ʾ������
        LastTaskPool(size_t max_task_count = 0)
            : m_task_queue(max_task_count)
        {}

        ~LastTaskPool() {
            clear();
            stop();
        }

        void clear() {
            m_task_queue.clear();
        }

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template<typename AsTPropType, typename TFunction, typename... Args>
        bool add_task(AsTPropType&& prop, TFunction&& func, Args&&... args) {
            if (!has_start())
                return false;
            return m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func), std::forward<Args>(args)...);
        }

        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            m_task_queue.remove_prop(std::forward<AsTPropType>(prop));
        }

    protected:
        void stop_inner() override final {
            m_task_queue.stop();
        }

        void pop_task_inner() override {
            m_task_queue.pop_task();
        }

    private:
        // ��ִ���������
        LastTupleTaskQueue<TPropType> m_task_queue;
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
        : public TaskPoolVirtual
        , private boost::noncopyable
    {
    public:
        // ������ͬ��������������ִ�е��̳߳�
        // max_task_count: ����������,��������������������;0���ʾ������
        SerialTaskPool(size_t max_task_count = 0)
            : m_task_queue(max_task_count)
        {
        }

        ~SerialTaskPool() {
            clear();
            stop();
        }

        // ����������
        void clear() {
            m_task_queue.clear();
        }

        // �����������,�������������ʱ��������
       // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template<typename AsTPropType, typename TFunction, typename... Args>
        bool add_task(AsTPropType&& prop, TFunction&& func, Args&&... args) {
            if (!has_start())
                return false;

            return m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func), std::forward<Args>(args)...);
        }

        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            m_task_queue.remove_prop(std::forward<AsTPropType>(prop));
        }

    protected:
        void stop_inner() override final {
            m_task_queue.stop();
        }

        void pop_task_inner() override {
            m_task_queue.pop_task();
        }

    private:
        // ��ִ���������
        SerialTupleTaskQueue<TPropType>     m_task_queue;
    };

}