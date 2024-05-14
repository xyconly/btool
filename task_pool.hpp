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
#include "submodule/oneTBB/include/tbb/concurrent_hash_map.h"
#include "safe_thread.hpp"
#include "task_queue.hpp"

namespace BTool {
    /*************************************************
                   �����̳߳ػ���
    *************************************************/
    template<typename TCrtpImpl, typename TQueueType>
    class TaskPoolBase
    {
        enum {
            TP_MAX_THREAD = 2000,   // ����߳���
        };
        // noncopyable
        TaskPoolBase(const TaskPoolBase&) = delete;
        TaskPoolBase& operator=(const TaskPoolBase&) = delete;

    protected:
        TaskPoolBase(size_t max_task_count = 0) : m_task_queue(max_task_count), m_cur_thread_ver(0) {}
        virtual ~TaskPoolBase() { stop(); }

        static bool BindToCore(int coreId) {
            cpu_set_t cpuSet;
            CPU_ZERO(&cpuSet);
            CPU_SET(coreId, &cpuSet);

            if (sched_setaffinity(0,sizeof(cpuSet), &cpuSet) == -1) {
                return false;
            }
            else {
                return true;
            }
        }

    public:
        // �����̳߳�
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        void start(size_t thread_num = std::thread::hardware_concurrency(), bool is_bind_core = false, int start_core_index = 0) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;

            m_task_queue.start();
            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num, is_bind_core, start_core_index);
        }

        // ��ֹ�̳߳�
        // ע��˴����������ȴ�task�Ļص��߳̽���,����task�Ļص��߳��в��ɵ��øú���
        // bwait: �Ƿ�ǿ�Ƶȴ���ǰ���ж���ִ����Ϻ�Ž���
        // ��ȫֹͣ�󷽿����¿���
        void stop(bool bwait = false) {
            if (!m_atomic_switch.stop())
                return;

            m_task_queue.stop(bwait);

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
            m_task_queue.clear();
        }

        // �ȴ���������ִ�����
        void wait() {
            m_task_queue.wait();
        }

        // �����̳߳ظ���,ÿ����һ���߳�ʱ�����һ��ָ����ڴ�����(�߳���Դ���Զ��ͷ�),ִ��stop��������������������������
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        // ע��:���뿪���̳߳غ󷽿���Ч
        void reset_thread_num(size_t thread_num = std::thread::hardware_concurrency(), bool is_bind_core = false, int start_core_index = 0) {
            if (!m_atomic_switch.has_started())
                return;

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num, is_bind_core, start_core_index);
        }

    protected:
        // crtpʵ��
        void pop_task_inner() {
            static_cast<TCrtpImpl*>(this)->pop_task_inner_impl();
        }
        void pop_task_inner_impl() {
            m_task_queue.pop_task();
        }

    private:
        // �����߳�
        void create_thread(size_t thread_num, bool is_bind_core, int start_core_index) {
            int core_num = std::thread::hardware_concurrency();
            if (thread_num == 0) {
                thread_num = core_num;
            }
            size_t cur_thread_ver = ++m_cur_thread_ver;
            thread_num = thread_num < TP_MAX_THREAD ? thread_num : TP_MAX_THREAD;

            for (size_t i = 0; i < thread_num; i++) {
                if (is_bind_core) {
                    if (start_core_index < core_num) {
                        m_cur_threads.push_back(new SafeThread(std::bind(&TaskPoolBase::thread_fun, this, cur_thread_ver, true, start_core_index++)));
                        continue;
                    }
                }
                
                m_cur_threads.push_back(new SafeThread(std::bind(&TaskPoolBase::thread_fun, this, cur_thread_ver, false,  0)));
            }
        }

        // �̳߳��߳�
        void thread_fun(size_t thread_ver, bool is_bind_core, int core_index) {
            if (is_bind_core)
                BindToCore(core_index);
 
            while (true) {
                if (m_atomic_switch.has_stoped() && m_task_queue.empty()) {
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
        TQueueType                  m_task_queue;

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
        : public TaskPoolBase<ParallelTaskPool, ParallelTaskQueue>
    {
        friend class TaskPoolBase<ParallelTaskPool, ParallelTaskQueue>;
    public:
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        ParallelTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<ParallelTaskPool, ParallelTaskQueue>(max_task_count)
        { }

        virtual ~ParallelTaskPool() {}

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template<typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<TFunction>(func));
        }
    };

    class NoBlockingParallelTaskPool
        : public TaskPoolBase<NoBlockingParallelTaskPool, NoBlockingParallelTaskQueue>
    {
        friend class TaskPoolBase<NoBlockingParallelTaskPool, NoBlockingParallelTaskQueue>;
    public:
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        NoBlockingParallelTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<NoBlockingParallelTaskPool, NoBlockingParallelTaskQueue>(max_task_count)
        { }

        virtual ~NoBlockingParallelTaskPool() {}

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template<typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<TFunction>(func));
        }
    };

    // ����������/������ģʽ
    class SingleThreadParallelTaskPool
        : public TaskPoolBase<SingleThreadParallelTaskPool, SingleThreadParallelTaskQueue>
    {
        friend class TaskPoolBase<SingleThreadParallelTaskPool, SingleThreadParallelTaskQueue>;
    public:
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        SingleThreadParallelTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<SingleThreadParallelTaskPool, SingleThreadParallelTaskQueue>(max_task_count)
        {
        }

        virtual ~SingleThreadParallelTaskPool() {}

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template<typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<TFunction>(func));
        }
    };


    /*************************************************
    Description:    ר����CTP,�ṩ��������ִ�е��̳߳�
    1, ��ͬʱ��Ӷ������;
    2, �����������Ⱥ�ִ��˳��,�����ܻ�ͬʱ����;
    4, ʵʱ��:ֻҪ�̳߳��߳��п��е�,��ô�ύ������������ִ��;����������̵߳������ʡ�
    5. �ṩ����չ�������̳߳��������ܡ�
    6. ÿ��POPʱ����ʱ1S
    *************************************************/
    class ParallelWaitTaskPool
        : public TaskPoolBase<ParallelWaitTaskPool, ParallelTaskQueue>
    {
        friend class TaskPoolBase<ParallelWaitTaskPool, ParallelTaskQueue>;
    public:
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        ParallelWaitTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<ParallelWaitTaskPool, ParallelTaskQueue>(max_task_count)
            , m_sleep_millseconds(1100)
        {}

        ~ParallelWaitTaskPool() {}

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template<typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<TFunction>(func));
        }

        // ���ü��ʱ��
        void set_sleep_milliseconds(long long millseconds) {
            m_sleep_millseconds = millseconds;
        }

    protected:
        void pop_task_inner_impl() {
            TaskPoolBase<ParallelWaitTaskPool, ParallelTaskQueue>::pop_task_inner_impl();
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
        : public TaskPoolBase<LastTaskPool<TPropType>, LastTaskQueue<TPropType>>
    {
        friend class TaskPoolBase<LastTaskPool<TPropType>, LastTaskQueue<TPropType>>;
    public:
        // ������ͬ��������ִ������״̬���̳߳�
        // max_task_count: ����������,��������������������;0���ʾ������
        LastTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<LastTaskPool<TPropType>, LastTaskQueue<TPropType>>(max_task_count)
        {
        }

        ~LastTaskPool() {}

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template<typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func));
        }

        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            this->m_task_queue.remove_prop(std::forward<AsTPropType>(prop));
        }
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
        : public TaskPoolBase<SerialTaskPool<TPropType>, SerialTaskQueue<TPropType>>
    {
        friend class TaskPoolBase<SerialTaskPool<TPropType>, SerialTaskQueue<TPropType>>;
    public:
        // ������ͬ��������������ִ�е��̳߳�
        // max_task_count: ����������,��������������������;0���ʾ������
        SerialTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<SerialTaskPool<TPropType>, SerialTaskQueue<TPropType>>(max_task_count)
        {
        }

        ~SerialTaskPool() {}

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template<typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;
            return this->m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func));
        }

        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            this->m_task_queue.remove_prop(std::forward<AsTPropType>(prop));
        }
    };

    /*************************************************
    Description:    �ṩ������ͬ��������������ִ�е��̳߳�
                    �����ǽ��������Լ򵥾��ȷ������ڲ����߳��̳߳�
                    ���������ܴ����ĺ����, ����ĳ���̳߳����ɶ������߳̿���
                    ��һ��add_task�󲻿�ɾ������, ����һ��ȷ���̺߳ź󲻵ø���
                    ������������������������Ծ��ȵĳ���
    1, ÿ�����Զ�����ͬʱ��Ӷ������;
    2, �кܶ�����Ժͺܶ������;
    3, ÿ��������ӵ��������������ִ��,����ͬһʱ�̲�����ͬʱִ��һ���û�����������;
    4, ʵʱ��:���̳߳��޷�ȷ����ͬ���Լ��ʵʱ�Լ�������, ����ֻ�ǰ�����������ת
    5. �ṩ����չ�������̳߳��������ܡ�
    *************************************************/
    template<typename TPropType>
    class RotateSerialTaskPool
    {
        enum {
            TP_MAX_THREAD = 2000,   // ����߳���
        };

        using hash_type = typename tbb::concurrent_hash_map<TPropType, size_t>;

    public:
        // ע��: max_task_count:��ʾ���̳߳��ڵ����������, �������̳߳ز�ͬ
        RotateSerialTaskPool(size_t max_task_count = 0) : m_max_task_count(max_task_count), m_next_task_pool_index(-1) {}
        ~RotateSerialTaskPool() { stop(); }

    public:
        // �����̳߳�
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        void start(size_t thread_num = std::thread::hardware_concurrency(), bool is_bind_core = false, int start_core_index = 0) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start())
                return;
            writeLock locker(m_mtx);
            create_thread(thread_num);
            for (auto& item : m_task_pools) {
                item->start(1, is_bind_core, start_core_index++);
            }
        }

        // ��ֹ�̳߳�
        // ע��˴����������ȴ�task�Ļص��߳̽���,����task�Ļص��߳��в��ɵ��øú���
        // bwait: �Ƿ�ǿ�Ƶȴ���ǰ���ж���ִ����Ϻ�Ž���
        // ��ȫֹͣ�󷽿����¿���
        void stop(bool bwait = false) {
            if (!m_atomic_switch.stop())
                return;

            writeLock locker(m_mtx);
            if (bwait) {
                std::vector<SafeThread> tmp_threads(m_task_pools.size());
                for (size_t i = 0;i < m_task_pools.size(); ++i) {
                    tmp_threads[i].start([this, i] {
                        m_task_pools[i]->stop(true);
                    });
                }
                for (auto& item : tmp_threads) {
                    if (item.joinable()) item.join();
                }
            }

            // ɾ������
            for (auto& item : m_task_pools) {
                delete item;
                item = nullptr;
            }

            m_task_pools.clear();
            m_next_task_pool_index.store(-1);
            m_atomic_switch.reset();
        }

        // ע��˴���������, Ϊ����stop��ͬ��
        void clear() {
            readLock locker(m_mtx);
            for (auto& item : m_task_pools) {
                item->clear();
            }
        }
        
        // �ȴ���������ִ�����, Ϊ����stop��ͬ��
        void wait() {
            typename hash_type::accessor ac;
            readLock locker(m_mtx);
            for (auto& item : m_task_pools) {
                item->wait();
            }
        }

        // �����̳߳ظ���,ÿ����һ���߳�ʱ�����һ��ָ����ڴ�����(�߳���Դ���Զ��ͷ�),ִ��stop��������������������������
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        // ע��:���뿪���̳߳غ󷽿���Ч
        void reset_thread_num(size_t thread_num = std::thread::hardware_concurrency()) {
            stop(true);
            start(thread_num);
        }

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template<typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            readLock locker(m_mtx); // Ϊ����stop��ͬ��
            if (UNLIKELY(!this->m_atomic_switch.has_started()))
                return false;

            size_t index = 0;
            {
                typename hash_type::accessor ac;
                bool ok = m_prop_index.insert(ac, prop);
                if (ok) {
                    index = ac->second = ++m_next_task_pool_index % m_task_pools.size();
                }
                else {
                    index = ac->second;
                }
            }
            return m_task_pools[index]->add_task(std::forward<TFunction>(func));
        }

    private:
        // �����߳�
        void create_thread(size_t thread_num, bool is_bind_core = false, int start_core_index = 0) {
            if (thread_num == 0) {
                thread_num = std::thread::hardware_concurrency();
            }
            thread_num = thread_num < TP_MAX_THREAD ? thread_num : TP_MAX_THREAD;
            for (size_t i = 0; i < thread_num; i++) {
                m_task_pools.emplace_back(new ParallelTaskPool(m_max_task_count));
            }
        }

    protected:
        // ���̳߳��ڵ����������
        size_t                              m_max_task_count;
        // ԭ����ͣ��־
        AtomicSwitch                        m_atomic_switch;
        // ���ݰ�ȫ��
        rwMutex                             m_mtx;
        // �̶߳���
        std::vector<ParallelTaskPool*>      m_task_pools;
        // ��һ���������Ե���������±�
        std::atomic<size_t>                 m_next_task_pool_index;
        // ���Զ�Ӧ�����±�
        hash_type                           m_prop_index;
    };

}