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
#ifdef __USE_TBB__
# include "submodule/oneTBB/include/tbb/concurrent_hash_map.h"
#endif
#include "comm_function_os.hpp"
#include "safe_thread.hpp"
#include "task_queue.hpp"


namespace BTool {
    /*************************************************
                 �����̳߳ػ���
    *************************************************/
    template <typename TCrtpImpl, typename TQueueType>
    class TaskPoolBase {
        enum {
            TP_MAX_THREAD = 2000,  // ����߳���
        };
        // noncopyable
        TaskPoolBase(const TaskPoolBase&) = delete;
        TaskPoolBase& operator=(const TaskPoolBase&) = delete;

    protected:
        TaskPoolBase(size_t max_task_count = 0) : m_task_queue(max_task_count), m_cur_thread_ver(0) {}
        virtual ~TaskPoolBase() { stop(); }

    public:
        // �����̳߳�
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        void start(size_t thread_num = std::thread::hardware_concurrency(), bool is_bind_core = false, int start_core_index = 1) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start()) return;

            m_task_queue.start();
            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num, is_bind_core, start_core_index);
        }

        // ��ֹ�̳߳�
        // ע��˴����������ȴ�task�Ļص��߳̽���,����task�Ļص��߳��в��ɵ��øú���
        // bwait: �Ƿ�ǿ�Ƶȴ���ǰ���ж���ִ����Ϻ�Ž���
        // ��ȫֹͣ�󷽿����¿���
        void stop(bool bwait = false) {
            if (!m_atomic_switch.stop()) return;

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
        void clear() { m_task_queue.clear(); }

        // �ȴ���������ִ�����
        void wait() { m_task_queue.wait(); }

        // �����̳߳ظ���,ÿ����һ���߳�ʱ�����һ��ָ����ڴ�����(�߳���Դ���Զ��ͷ�),ִ��stop��������������������������
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        // ע��:���뿪���̳߳غ󷽿���Ч
        void reset_thread_num(size_t thread_num = std::thread::hardware_concurrency(), bool is_bind_core = false, int start_core_index = 1) {
            if (!m_atomic_switch.has_started()) return;

            std::lock_guard<std::mutex> lck(m_threads_mtx);
            create_thread(thread_num, is_bind_core, start_core_index);
        }

    protected:
        // crtpʵ��
        void pop_task_inner_impl() { m_task_queue.pop_task(); }

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

                m_cur_threads.push_back(new SafeThread(std::bind(&TaskPoolBase::thread_fun, this, cur_thread_ver, false, 0)));
            }
        }

        // �̳߳��߳�
        void thread_fun(size_t thread_ver, bool is_bind_core, int core_index) {
            if (is_bind_core) CommonOS::BindCore(core_index);

            while (true) {
                if (m_atomic_switch.has_stoped() && m_task_queue.empty()) {
                    break;
                }

                if (thread_ver < m_cur_thread_ver.load()) break;

                static_cast<TCrtpImpl*>(this)->pop_task_inner_impl();
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
    class ParallelTaskPool : public TaskPoolBase<ParallelTaskPool, ParallelTaskQueue> {
        friend class TaskPoolBase<ParallelTaskPool, ParallelTaskQueue>;

    public:
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        ParallelTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<ParallelTaskPool, ParallelTaskQueue>(max_task_count)
        {}

        virtual ~ParallelTaskPool() {}

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template <typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started())) return false;
            return this->m_task_queue.add_task(std::forward<TFunction>(func));
        }
    };

    class NoBlockingParallelTaskPool : public TaskPoolBase<NoBlockingParallelTaskPool, NoBlockingParallelTaskQueue> {
        friend class TaskPoolBase<NoBlockingParallelTaskPool, NoBlockingParallelTaskQueue>;

    public:
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        NoBlockingParallelTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<NoBlockingParallelTaskPool, NoBlockingParallelTaskQueue>(max_task_count)
        {}

        virtual ~NoBlockingParallelTaskPool() {}

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template <typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started())) return false;
            return this->m_task_queue.add_task(std::forward<TFunction>(func));
        }
    };

    // ����������/������ģʽ
    class SingleThreadParallelTaskPool : public TaskPoolBase<SingleThreadParallelTaskPool, SingleThreadParallelTaskQueue> {
        friend class TaskPoolBase<SingleThreadParallelTaskPool, SingleThreadParallelTaskQueue>;

    public:
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        SingleThreadParallelTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<SingleThreadParallelTaskPool, SingleThreadParallelTaskQueue>(max_task_count)
        {}

        virtual ~SingleThreadParallelTaskPool() {}

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template <typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started())) return false;
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
    class ParallelWaitTaskPool : public TaskPoolBase<ParallelWaitTaskPool, ParallelTaskQueue> {
        friend class TaskPoolBase<ParallelWaitTaskPool, ParallelTaskQueue>;

    public:
        // ������������˳��������ִ�е��̳߳�
        // max_task_count: ������񻺴����,��������������������;0���ʾ������
        ParallelWaitTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<ParallelWaitTaskPool, ParallelTaskQueue>(max_task_count), m_sleep_millseconds(1100)
        {}

        ~ParallelWaitTaskPool() {}

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task([param1, param2=...]{...})
        // add_task(std::bind(&func, param1, param2))
        template <typename TFunction>
        bool add_task(TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started())) return false;
            return this->m_task_queue.add_task(std::forward<TFunction>(func));
        }

        // ���ü��ʱ��
        void set_sleep_milliseconds(long long millseconds) { m_sleep_millseconds = millseconds; }

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
    template <typename TPropType>
    class LastTaskPool : public TaskPoolBase<LastTaskPool<TPropType>, LastTaskQueue<TPropType>> {
        friend class TaskPoolBase<LastTaskPool<TPropType>, LastTaskQueue<TPropType>>;

    public:
        // ������ͬ��������ִ������״̬���̳߳�
        // max_task_count: ����������,��������������������;0���ʾ������
        LastTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<LastTaskPool<TPropType>, LastTaskQueue<TPropType>>(max_task_count)
        {}

        ~LastTaskPool() {}

        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template <typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started())) return false;
            return this->m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func));
        }

        template <typename AsTPropType>
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
    template <typename TPropType, typename TTaskQueueType = SerialTaskQueue<TPropType>>
    class SerialTaskPool : public TaskPoolBase<SerialTaskPool<TPropType, TTaskQueueType>, TTaskQueueType> {
        friend class TaskPoolBase<SerialTaskPool<TPropType, TTaskQueueType>, TTaskQueueType>;

    public:
        // ������ͬ��������������ִ�е��̳߳�
        // max_task_count: ����������,��������������������;0���ʾ������
        SerialTaskPool(size_t max_task_count = 0)
            : TaskPoolBase<SerialTaskPool<TPropType, TTaskQueueType>, TTaskQueueType>(max_task_count)
        {}

        ~SerialTaskPool() {}

        void reset_props(const std::vector<TPropType>& props) {
            if constexpr (TTaskQueueType::NEED_SET_PROP::value) {
                // ֻ�е��ײ���������Ҫ����ʱ�ŵ���
                this->m_task_queue.reset_props(props);
            }
        }
        // �����������,�������������ʱ��������
        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        // add_task(prop, [param1, param2=...]{...})
        // add_task(prop, std::bind(&func, param1, param2))
        template <typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            if (UNLIKELY(!this->m_atomic_switch.has_started())) return false;
            return this->m_task_queue.add_task(std::forward<AsTPropType>(prop), std::forward<TFunction>(func));
        }

        template <typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            this->m_task_queue.remove_prop(std::forward<AsTPropType>(prop));
        }
    };

    /*************************************************
    Description:    ���������������ͬ��������������ִ�е��̳߳�
    1, ÿ�����Զ�����ͬʱ��Ӷ������;
    2, �кܶ�����Ժͺܶ������;
    3, ÿ��������ӵ��������������ִ��;
    4, ʵʱ��:�������ȷ���������;
    5, ����ʱ������̶�����, ���ɸ���, ����ɾ��
    *************************************************/
    template <typename TPropType, typename TTaskQueueType = LockFreeTaskQueue>
    class ConditionRotateSerialTaskPool {
        struct alignas(64) ProcessingFlag {
            std::atomic<bool> flag{false};
        };

    public:
        explicit ConditionRotateSerialTaskPool(const std::vector<TPropType>& props, size_t thread_num = std::thread::hardware_concurrency(),
                                            bool is_bind_core = false, int start_core_index = 1)
            : m_thread_num(thread_num) {
            if (m_thread_num == 0) {
                m_thread_num = std::thread::hardware_concurrency();
            }
            m_thread_num = std::min(props.size(), m_thread_num);

            m_queues.resize(m_thread_num);

            for (size_t i = 0; i < props.size(); ++i) {
                size_t idx = props[i] % m_thread_num;
                m_prop_index[props[i]] = idx;
            }

            m_threads.reserve(m_thread_num);
            for (size_t tid = 0; tid < m_thread_num; ++tid) {
                m_threads.emplace_back([this, is_bind_core, start_core_index, tid] {
                    if (is_bind_core) {
                        CommonOS::BindCore(start_core_index + tid);
                    }
                    thread_worker(tid);
                });
            }
        }

        ~ConditionRotateSerialTaskPool() { stop(); }

        void stop(bool bWait = false) {
            {
                std::lock_guard<std::mutex> lk(m_ready_mtx);
                m_stopping.flag.store(true, std::memory_order_release);
                if (!bWait) {
                    for (auto& q : m_queues) q.clear();
                }
            }
            m_ready_cv.notify_all();
            for (auto& th : m_threads) {
                if (th.joinable()) th.join();
            }
            m_threads.clear();
        }

        template <typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            auto it = m_prop_index.find(prop);
            if (it == m_prop_index.end()) return false;

            m_queues[it->second].add_task(std::forward<TFunction>(func));
            m_ready_cv.notify_one();
            return true;
        }

    private:
        void thread_worker(size_t tid) {
            auto& cur_que = m_queues[tid];
            for (;;) {
                typename TTaskQueueType::Task cur_item;
                {
                    bool is_stopping = false;
                    std::unique_lock<std::mutex> lk(m_ready_mtx);
                    m_ready_cv.wait(lk, [this, &cur_que, &cur_item, &is_stopping] {
                        is_stopping = m_stopping.flag.load(std::memory_order_relaxed);
                        return cur_que.pop_task(cur_item) || is_stopping;
                    });

                    if (is_stopping && !cur_item) return;
                }

                while (cur_item) {
                    cur_item();
                    cur_que.pop_task(cur_item);
                }

                if (m_stopping.flag.load(std::memory_order_relaxed)) return;
            }
        }

    private:
        size_t                      m_thread_num;
        std::vector<std::thread>    m_threads;

        std::unordered_map<TPropType, size_t> m_prop_index;
        std::vector<TTaskQueueType> m_queues;

        std::mutex                  m_ready_mtx;
        std::condition_variable     m_ready_cv;

        ProcessingFlag              m_stopping;
    };

    /*************************************************
    Description:    �����������ͬ��������������ִ�е��̳߳�
    1, ÿ�����Զ�����ͬʱ��Ӷ������;
    2, �кܶ�����Ժͺܶ������;
    3, ÿ��������ӵ��������������ִ��;
    4, ʵʱ��:�������ȷ���������;
    5, ����ʱ������̶�����, ���ɸ���, ����ɾ��
    *************************************************/
    template <typename TPropType, typename TTaskQueueType = LockFreeTaskQueue>
    class LockFreeRotateSerialTaskPool {
        struct alignas(64) ProcessingFlag {
            std::atomic<bool> flag{false};
        };

    public:
        explicit LockFreeRotateSerialTaskPool(const std::vector<TPropType>& props, size_t thread_num = std::thread::hardware_concurrency(), bool is_bind_core = false, int start_core_index = 1)
            : m_thread_num(thread_num) {
            if (m_thread_num == 0) {
                m_thread_num = std::thread::hardware_concurrency();
            }
            m_thread_num = std::min(props.size(), m_thread_num);

            m_queues.resize(m_thread_num);

            for (size_t i = 0; i < props.size(); ++i) {
                size_t idx = props[i] % m_thread_num;
                m_prop_index[props[i]] = idx;
            }

            m_threads.reserve(m_thread_num);
            for (size_t tid = 0; tid < m_thread_num; ++tid) {
                m_threads.emplace_back([this, is_bind_core, start_core_index, tid] {
                    if (is_bind_core) {
                        CommonOS::BindCore(start_core_index + tid);
                    }
                    thread_worker(tid);
                });
            }
        }

        ~LockFreeRotateSerialTaskPool() { stop(true); }

        // ע��bWaitΪfalseʱ���̰߳�ȫ
        void stop(bool bWait = false) {
            m_stopping.flag.store(true, std::memory_order_release);
            if (!bWait) {
                for (auto& q : m_queues) q.clear();
            }
            for (auto& th : m_threads) {
                if (th.joinable()) th.join();
            }
            m_threads.clear();
        }

        template <typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            auto it = m_prop_index.find(prop);
            if (it == m_prop_index.end()) return false;
            if (UNLIKELY(m_stopping.flag.load(std::memory_order_relaxed))) return false;

            m_queues[it->second].add_task(std::forward<TFunction>(func));
            return true;
        }

    private:
        void thread_worker(size_t tid) {
            auto& cur_que = m_queues[tid];
            for (;;) {
                deal_all(cur_que);
                if (UNLIKELY(m_stopping.flag.load(std::memory_order_relaxed))) {
                    deal_all(cur_que);
                    return;
                }
            }
        }

        void deal_all(TTaskQueueType& cur_que) {
            auto cur_item = cur_que.pop_task();
            while (cur_item) {
                cur_item();
                cur_que.pop_task(cur_item);
            }
        }

    private:
            size_t                                  m_thread_num;
            std::vector<std::thread>                m_threads;

            std::unordered_map<TPropType, size_t>   m_prop_index;
            std::vector<TTaskQueueType>             m_queues;

            std::mutex                              m_ready_mtx;
            std::condition_variable                 m_ready_cv;

            ProcessingFlag                          m_stopping;
    };

#ifdef __USE_TBB__
    // class ParallelTaskPool
    // {
    // public:
    //     explicit ParallelTaskPool(size_t max_task_count = 0)
    //         : m_max_task_count(max_task_count), m_stopped(false) {}

    //     ~ParallelTaskPool() {
    //         stop(true);
    //     }

    //     void start(size_t thread_num = 1, bool is_bind_core = false, int start_core_index = 1) {
    //         if (thread_num == 0) thread_num = 1;
    //         for (size_t i = 0; i < thread_num; ++i) {
    //             m_thread.start([this] { run(); });
    //         }
    //     }

    //     void stop(bool wait = false) {
    //         {
    //             std::lock_guard<std::mutex> lock(m_mtx);
    //             m_stopped = true;
    //             m_cv.notify_all();
    //         }
    //         if (wait) {
    //             m_thread.join();
    //         }
    //     }

    //     template<typename TFunction>
    //     bool add_task(TFunction&& func) {
    //         {
    //             std::unique_lock<std::mutex> lock(m_mtx);
    //             if (m_max_task_count > 0 && m_tasks.size() >= m_max_task_count) {
    //                 m_cv.wait(lock, [this] { return m_tasks.size() < m_max_task_count; });
    //             }
    //             if (m_stopped) return false;
    //             m_tasks.emplace(std::forward<TFunction>(func));
    //         }
    //         m_cv.notify_one();
    //         return true;
    //     }

    //     void clear() {
    //         std::lock_guard<std::mutex> lock(m_mtx);
    //         std::queue<std::function<void()>> empty;
    //         std::swap(m_tasks, empty);
    //     }

    //     void wait() {
    //         std::unique_lock<std::mutex> lock(m_mtx);
    //         m_cv.wait(lock, [this] { return m_tasks.empty(); });
    //     }

    // private:
    //     void run() {
    //         while (true) {
    //             std::function<void()> task;
    //             {
    //                 std::unique_lock<std::mutex> lock(m_mtx);
    //                 m_cv.wait(lock, [this] { return m_stopped || !m_tasks.empty(); });

    //                 if (m_stopped && m_tasks.empty())
    //                     break;

    //                 task = std::move(m_tasks.front());
    //                 m_tasks.pop();
    //                 if (m_max_task_count > 0) {
    //                     m_cv.notify_one();
    //                 }
    //             }
    //             if (task) {
    //                 task();
    //             }
    //         }
    //     }

    // private:
    //     size_t m_max_task_count;
    //     std::queue<std::function<void()>> m_tasks;
    //     std::mutex m_mtx;
    //     std::condition_variable m_cv;
    //     bool m_stopped;
    //     SafeThread m_thread;
    // };
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
    template <typename TPropType>
    class RotateSerialTaskPool {
        enum {
            TP_MAX_THREAD = 2000,
        };

        using hash_type = typename tbb::concurrent_hash_map<TPropType, size_t>;

    public:
        // ע��: max_task_count:��ʾ���̳߳��ڵ����������, �������̳߳ز�ͬ
        RotateSerialTaskPool(size_t max_task_count = 0) : m_max_task_count(max_task_count), m_next_thread_index(0) {}

        ~RotateSerialTaskPool() { stop(); }

        // �����̳߳�
        // thread_num: �����߳���,���ΪSTP_MAX_THREAD���߳�,0��ʾϵͳCPU����
        void start(size_t thread_num = std::thread::hardware_concurrency(), bool is_bind_core = false, int start_core_index = 1) {
            if (!m_atomic_switch.init() || !m_atomic_switch.start()) return;
            writeLock locker(m_mtx);
            create_threads(thread_num);
            for (auto& pool : m_task_pools) {
                pool->start(1, is_bind_core, start_core_index++);
            }
        }

        // ��ֹ�̳߳�
        // ע��˴����������ȴ�task�Ļص��߳̽���,����task�Ļص��߳��в��ɵ��øú���
        // bwait: �Ƿ�ǿ�Ƶȴ���ǰ���ж���ִ����Ϻ�Ž���
        // ��ȫֹͣ�󷽿����¿���
        void stop(bool bwait = false) {
            if (!m_atomic_switch.stop()) return;

            writeLock locker(m_mtx);
            if (bwait) {
                std::vector<SafeThread> tmp_threads(m_task_pools.size());
                for (size_t i = 0; i < m_task_pools.size(); ++i) {
                    tmp_threads[i].start([this, i] { m_task_pools[i]->stop(true); });
                }
                for (auto& item : tmp_threads) {
                    if (item.joinable()) item.join();
                }
            }
            for (auto& item : m_task_pools) {
                delete item;
                item = nullptr;
            }
            m_task_pools.clear();
            m_prop_index.clear();
            m_next_thread_index.store(0);
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
        template <typename AsTPropType, typename TFunction>
        bool add_task(AsTPropType&& prop, TFunction&& func) {
            readLock locker(m_mtx);  // Ϊ����stop��ͬ��
            if (UNLIKELY(!this->m_atomic_switch.has_started())) return false;

            size_t index = get_thread_index(std::forward<AsTPropType>(prop));
            return m_task_pools[index]->add_task(std::forward<TFunction>(func));
        }

    private:
        template <typename AsTPropType>
        size_t get_thread_index(AsTPropType&& prop) {
            typename hash_type::accessor ac;
            bool inserted = m_prop_index.insert(ac, prop);
            if (inserted) {
                // �����ԣ���ѯ����һ���̳߳�
                ac->second = m_next_thread_index.fetch_add(1) % m_task_pools.size();
            }
            return ac->second;
        }

        // �����߳�
        void create_threads(size_t thread_num) {
            if (thread_num == 0) {
                thread_num = std::thread::hardware_concurrency();
            }
            thread_num = std::min(thread_num, (size_t)TP_MAX_THREAD);
            for (size_t i = 0; i < thread_num; ++i) {
                m_task_pools.emplace_back(new ParallelTaskPool(m_max_task_count));
            }
        }

    private:
        // ���̳߳��ڵ����������
        size_t                              m_max_task_count;
        // ԭ����ͣ��־
        AtomicSwitch                        m_atomic_switch;
        // ���ݰ�ȫ��
        rwMutex                             m_mtx;
        // �̶߳���
        std::vector<ParallelTaskPool*>      m_task_pools;
        // ��һ���������Ե���������±�
        std::atomic<size_t>                 m_next_thread_index;
        // ���Զ�Ӧ�����±�
        hash_type                           m_prop_index;
    };
#endif

}