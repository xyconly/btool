/*************************************************
File name:  task_queue.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ�����������,��������ظ�����
                �಻���ü̳е���ʽ,����Ķ�����,�ɼӿ��ٶ�

Note:  condition_variableʹ��ע��:�ڽ���waitʱ������
    1.ִ���ж�,Ϊtrue���˳�
    2.�ͷ�������(�ź���)����
    3.����notify,������
    Ȼ���ظ�1-3����,ֱ���ﵽ�����������˳�,ע���ʱ����Ϊ1������,��δ�ͷ���

Note2:  queue���Լ�������, ����ͨ��swap�ȷ�ʽ�����ͷ�ռ���ڴ�
        map/set�����ڲ�������, �޷�ͨ��swap��������ȥ��ʵ�ͷ�ʵ���ڴ�
            һ��Ҫ�ͷŵĻ�, ������������Ϻ����malloc_trim(0);
*************************************************/
#pragma once
#include <assert.h>
#include <stdlib.h>

#include <condition_variable>
#include <functional>
#include <list>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "atomic_switch.hpp"
#include "comm_function_os.hpp"
#include "fast_function.hpp"
#include "object_pool.hpp"
#include "rwmutex.hpp"
#include "submodule/concurrentqueue/concurrentqueue.h"
#ifdef __USE_TBB__
#include "submodule/oneTBB/include/tbb/concurrent_hash_map.h"
#endif

// #include "concurrentqueue/blockingconcurrentqueue.h"

namespace BTool {
    /*************************************************
    Description:�ṩ���ں�����FIFO�������
    *************************************************/
    class TaskQueue {
    public:
        typedef std::function<void()> TaskItem;
        // class TaskItem {
        // public:
        //     template <typename TFunc>
        //     TaskItem(TFunc&& func) : m_func(reinterpret_cast<void*>(new TFunc(std::forward<TFunc>(func)))),
        //                                 m_invoke([](void* f) { (*reinterpret_cast<TFunc*>(f))(); }),
        //                                 m_destroy([](void* f) { delete reinterpret_cast<TFunc*>(f); }) {}
        //     ~TaskItem() { m_destroy(m_func); }
        //     void operator()() { m_invoke(m_func); }
        // private:
        //     void* m_func;
        //     void (*m_invoke)(void*);
        //     void (*m_destroy)(void*);
        // };
    public:
        TaskQueue(size_t max_task_count = 0)
            : m_bstop(false)
            , m_max_task_count(max_task_count)
        {}

        virtual ~TaskQueue() {
            stop();
        }

        bool empty() const {
            std::lock_guard<std::mutex> locker(m_mtx);
            return !not_empty();
        }

        void clear() {
            std::lock_guard<std::mutex> locker(m_mtx);
            clear_inner();
        }

        void start() {
            // ��λ����ֹ��־��
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            std::lock_guard<std::mutex> locker(m_mtx);
            // �Ƿ�����ֹ�ж�
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }

            if (bwait) {
                m_cv_not_full.notify_all();
                m_cv_not_empty.notify_all();
                return;
            }
            clear_inner();
        }

        void wait() {
            while (!empty()) std::this_thread::yield();
        }

        template <typename AsTFunction>
        bool add_task(AsTFunction&& func) {
            if (UNLIKELY(m_bstop.load())) return false;

            {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });

                if (UNLIKELY(m_bstop.load())) return false;

                m_queue.push(std::forward<AsTFunction>(func));
            }
            m_cv_not_empty.notify_one();
            return true;
        }

        void pop_task() {
            TaskItem pop_task(nullptr);
            {
                std::unique_lock<std::mutex> locker(m_mtx);
                if (LIKELY(!m_bstop.load())) {
                    m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
                }

                if (UNLIKELY(!not_empty())) return;

                pop_task = std::move(m_queue.front());
                m_queue.pop();
                // queue���������ͷ��ѿ��ٿռ�,���������ʱ���ͷ�һ��,��ʵ�ʻ�����,���Լ�����Ϊlist������queue����ռ�õ�����,����ᵼ�����ܵ���΢�½�,����ʵ���������
                if (m_queue.empty()) {
                    std::queue<TaskItem> empty;
                    m_queue.swap(empty);
                }
            }
            m_cv_not_full.notify_one();

            if (pop_task) pop_task();
        }

        bool full() const {
            std::lock_guard<std::mutex> locker(m_mtx);
            return !not_full();
        }

        size_t size() const {
            std::lock_guard<std::mutex> locker(m_mtx);
            return m_queue.size();
        }

    protected:
        void clear_inner() {
            std::queue<TaskItem> empty;
            m_queue.swap(empty);
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }
        // �Ƿ���δ��״̬
        bool not_full() const { return m_max_task_count == 0 || m_queue.size() < m_max_task_count; }

        // �Ƿ��ڿ�״̬
        bool not_empty() const { return !m_queue.empty(); }

    protected:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>           m_bstop;
        // ���ݰ�ȫ��
        mutable std::mutex          m_mtx;

        // �ܴ�ִ���������,�������еĴ�ִ������
        std::queue<TaskItem>        m_queue;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                      m_max_task_count;

        // ��Ϊ�յ���������
        std::condition_variable     m_cv_not_empty;
        // û��������������
        std::condition_variable     m_cv_not_full;
    };

    /*************************************************
    Description:�ṩ���������Լ��ź�����FIFO�������
    *************************************************/
    class ParallelTaskQueue {
    public:
        typedef std::function<void()> TaskItem;
        using NEED_SET_PROP = std::false_type;

    public:
        ParallelTaskQueue(size_t max_task_count = 0)
            : m_bstop(false)
            , m_max_task_count(max_task_count)
        {}

        virtual ~ParallelTaskQueue() { stop(); }

        inline bool empty() const { return !not_empty(); }

        void clear() {
            std::lock_guard<std::mutex> locker(m_mtx);
            clear_inner();
        }

        void start() {
            // ��λ����ֹ��־��
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            std::lock_guard<std::mutex> locker(m_mtx);
            // �Ƿ�����ֹ�ж�
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }

            if (bwait) {
                m_cv_not_full.notify_all();
                m_cv_not_empty.notify_all();
                return;
            }

            clear_inner();
        }

        void wait() {
            while (!empty()) std::this_thread::yield();
        }

        template <typename AsTFunction>
        bool add_task(AsTFunction&& func) {
            if (UNLIKELY(m_bstop.load())) return false;

            {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });
            }

            if (UNLIKELY(m_bstop.load())) return false;
            m_queue.enqueue(std::forward<AsTFunction>(func));
            m_cv_not_empty.notify_one();
            return true;
        }

        void pop_task() {
            if (LIKELY(!m_bstop.load())) {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
            }

            if (UNLIKELY(empty())) return;

            TaskItem pop_task(nullptr);
            if (m_queue.try_dequeue_non_interleaved(pop_task)) {
                m_cv_not_full.notify_one();
                if (pop_task) pop_task();
            } else {
                m_cv_not_full.notify_one();
            }
        }

        inline bool full() const { return !not_full(); }

        inline size_t size() const { return m_queue.size_approx(); }

    protected:
        void clear_inner() {
            moodycamel::ConcurrentQueue<TaskItem> empty;
            m_queue.swap(empty);

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }
        // �Ƿ���δ��״̬
        inline bool not_full() const { return m_max_task_count == 0 || size() < m_max_task_count; }

        // �Ƿ��ڿ�״̬
        inline bool not_empty() const { return size() != 0; }

    protected:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>           m_bstop;
        // ���ݰ�ȫ��
        mutable std::mutex          m_mtx;

        // �ܴ�ִ���������,�������еĴ�ִ������
        moodycamel::ConcurrentQueue<TaskItem>       m_queue;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                      m_max_task_count;

        // ��Ϊ�յ���������
        std::condition_variable     m_cv_not_empty;
        // û��������������
        std::condition_variable     m_cv_not_full;
    };

    class SingleThreadParallelTaskQueue {
    public:
        typedef std::function<void()> TaskItem;
        using NEED_SET_PROP = std::false_type;

    public:
        SingleThreadParallelTaskQueue(size_t max_task_count = 0)
            : m_bstop(false)
            , m_ptok(m_queue)
            , m_ctok(m_queue)
            , m_max_task_count(max_task_count)
        {}

        virtual ~SingleThreadParallelTaskQueue() { stop(); }

        inline bool empty() const { return !not_empty(); }

        void clear() {
            std::lock_guard<std::mutex> locker(m_mtx);
            clear_inner();
        }

        void start() {
            // ��λ����ֹ��־��
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            std::lock_guard<std::mutex> locker(m_mtx);
            // �Ƿ�����ֹ�ж�
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }

            if (bwait) {
                m_cv_not_full.notify_all();
                m_cv_not_empty.notify_all();
                return;
            }
            clear_inner();
        }

        void wait() {
            while (!empty()) std::this_thread::yield();
        }

        template <typename AsTFunction>
        bool add_task(AsTFunction&& func) {
            if (UNLIKELY(m_bstop.load())) return false;

            std::unique_lock<std::mutex> locker(m_mtx);
            m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });

            if (UNLIKELY(m_bstop.load())) return false;

            m_queue.enqueue(m_ptok, std::forward<AsTFunction>(func));
            m_cv_not_empty.notify_one();
            return true;
        }

        void pop_task() {
            if (LIKELY(!m_bstop.load())) {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
            }

            if (UNLIKELY(empty())) return;

            TaskItem pop_task(nullptr);
            if (m_queue.try_dequeue(m_ctok, pop_task)) {
                m_cv_not_full.notify_one();
                if (pop_task) pop_task();
            } else {
                m_cv_not_full.notify_one();
            }
        }

        inline bool full() const { return !not_full(); }

        inline size_t size() const { return m_queue.size_approx(); }

    protected:
        void clear_inner() {
            moodycamel::ConcurrentQueue<TaskItem> empty;
            m_queue.swap(empty);

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }
        // �Ƿ���δ��״̬
        inline bool not_full() const { return m_max_task_count == 0 || size() < m_max_task_count; }

        // �Ƿ��ڿ�״̬
        inline bool not_empty() const { return size() != 0; }

    protected:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>           m_bstop;
        // ���ݰ�ȫ��
        mutable std::mutex          m_mtx;

        // �ܴ�ִ���������,�������еĴ�ִ������
        moodycamel::ConcurrentQueue<TaskItem>   m_queue;
        moodycamel::ProducerToken               m_ptok;
        moodycamel::ConsumerToken               m_ctok;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                                  m_max_task_count;

        // ��Ϊ�յ���������
        std::condition_variable                 m_cv_not_empty;
        // û��������������
        std::condition_variable                 m_cv_not_full;
    };

    /*************************************************
    Description:�ṩ������pop����������, ���Ǹö��д���cpu���ߵ�����
                ������linux�µ��߳�Ҳ���ܳ���cpu���������ܵ͵�����
    *************************************************/
    class NoBlockingParallelTaskQueue {
    public:
        typedef std::function<void()> TaskItem;
        using NEED_SET_PROP = std::false_type;

    public:
        NoBlockingParallelTaskQueue(size_t max_task_count = 0)
            : m_bstop(false)
            , m_max_task_count(max_task_count)
        {}

        virtual ~NoBlockingParallelTaskQueue() { stop(); }

        inline bool empty() const { return size() == 0; }

        void clear() {
            moodycamel::ConcurrentQueue<TaskItem> empty;
            m_queue.swap(empty);
        }

        void start() {
            // ��λ����ֹ��־��
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }
        }

        void stop(bool bwait = false) {
            // �Ƿ�����ֹ�ж�
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }

            if (bwait) {
                return;
            }
            clear();
        }

        void wait() {
            while (!empty()) std::this_thread::yield();
        }

        template <typename AsTFunction>
        bool add_task(AsTFunction&& func) {
            if (UNLIKELY(m_bstop.load())) return false;

            // �����жϴ���
            while (m_max_task_count > 0 && size() > m_max_task_count) std::this_thread::yield();
            if (UNLIKELY(m_bstop.load())) return false;
            m_queue.enqueue(std::forward<AsTFunction>(func));
            return true;
        }

        void pop_task() {
            if (UNLIKELY(m_bstop.load() && empty())) return;

            TaskItem pop_task(nullptr);
            // if(m_queue.wait_dequeue_timed(pop_task, std::chrono::milliseconds(5))) {
            //     if (pop_task) pop_task();
            // } else {
            //     std::this_thread::yield();
            // }
            if (m_queue.try_dequeue(pop_task)) {
                if (pop_task) pop_task();
            }
        }

        inline size_t size() const { return m_queue.size_approx(); }

    protected:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool> m_bstop;
        // �ܴ�ִ���������,�������еĴ�ִ������
        moodycamel::ConcurrentQueue<TaskItem> m_queue;
        // moodycamel::BlockingConcurrentQueue<TaskItem>       m_queue;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t m_max_task_count;
    };

    /*************************************************
    Description:�ṩ�����Ի��ֵ�,����������״̬��FIFO�������
                ��ĳһ�������ڶ�����ʱ,ͬ���Ե�������������ʱ,ԭ����ᱻ����
    *************************************************/
    template <typename TPropType>
    class LastTaskQueue {
    public:
        typedef std::function<void()> TaskItem;
        using NEED_SET_PROP = std::false_type;

    public:
        // max_task_count: ����������,��������������������;0���ʾ������
        LastTaskQueue(size_t max_task_count = 0)
            : m_bstop(false)
            , m_max_task_count(max_task_count)
        {}

        virtual ~LastTaskQueue() { stop(); }

        inline bool empty() const {
            std::lock_guard<std::mutex> locker(m_mtx);
            return !not_empty();
        }

        void clear() {
            std::lock_guard<std::mutex> locker(m_mtx);
            clear_inner();
        }

        void start() {
            // ��λ����ֹ��־��
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            std::lock_guard<std::mutex> locker(m_mtx);
            // �Ƿ�����ֹ�ж�
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }
            if (bwait) {
                m_cv_not_full.notify_all();
                m_cv_not_empty.notify_all();
                return;
            }

            clear_inner();
        }

        void wait() {
            while (!empty()) std::this_thread::yield();
        }

        template <typename AsTPropType, typename AsTFunction>
        bool add_task(AsTPropType&& prop, AsTFunction&& func) {
            if (UNLIKELY(m_bstop.load())) return false;

            {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });

                if (UNLIKELY(m_bstop.load())) return false;

                auto iter = m_wait_tasks.find(prop);
                if (iter == m_wait_tasks.end()) m_wait_props.push_back(prop);
                m_wait_tasks[std::forward<AsTPropType>(prop)] = std::forward<AsTFunction>(func);
            }
            m_cv_not_empty.notify_one();
            return true;
        }

        void pop_task() {
            TaskItem pop_task(nullptr);
            TPropType pop_type;
            {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });

                if (UNLIKELY(m_bstop.load() && !not_empty())) return;

                // �Ƿ����޿�pop����
                if (UNLIKELY(m_wait_props.empty())) return;

                for (auto pop_type_iter = m_wait_props.begin(); pop_type_iter != m_wait_props.end(); pop_type_iter++) {
                    if (m_cur_pop_props.find(*pop_type_iter) != m_cur_pop_props.end()) continue;

                    pop_type = *pop_type_iter;
                    // ��ȡ����ָ��
                    pop_task = std::move(m_wait_tasks[pop_type]);
                    m_wait_tasks.erase(pop_type);
                    m_wait_props.erase(pop_type_iter);
                    m_cur_pop_props.emplace(pop_type);
                    break;
                }
            }

            if (pop_task) {
                pop_task();

                {
                    std::lock_guard<std::mutex> locker(m_mtx);
                    m_cur_pop_props.erase(pop_type);
                }
                m_cv_not_full.notify_one();
            }
        }

        // �Ƴ�����ָ����������,��ǰ����ִ�г���,���ܴ�������
        template <typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            {
                std::lock_guard<std::mutex> locker(m_mtx);
                m_wait_props.remove_if([prop](const TPropType& value) -> bool { return (value == prop); });
                m_wait_tasks.erase(std::forward<AsTPropType>(prop));
            }
            m_cv_not_full.notify_one();
        }

        bool full() const {
            std::lock_guard<std::mutex> locker(m_mtx);
            return !not_full();
        }

        inline size_t size() const {
            std::lock_guard<std::mutex> locker(m_mtx);
            return m_wait_props.size();
        }

    protected:
        void clear_inner() {
            m_wait_tasks.clear();
            m_wait_props.clear();

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }
        // �Ƿ���δ��״̬
        inline bool not_full() const { return m_max_task_count == 0 || m_wait_props.size() < m_max_task_count; }

        // �Ƿ��ڷǿ�״̬
        inline bool not_empty() const { return !m_wait_props.empty(); }

    protected:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool> m_bstop;

        // ���ݰ�ȫ��
        mutable std::mutex m_mtx;
        // �ܴ�ִ����������˳�����,�����ж�ִ�ж���˳��
        std::list<TPropType> m_wait_props;
        // �ܴ�ִ������������Լ����Ӧ����,���������ʼ����m_wait_tasks����ͬ��
        std::unordered_map<TPropType, TaskItem> m_wait_tasks;
        // ��ǰ����pop��������
        std::unordered_set<TPropType> m_cur_pop_props;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t m_max_task_count;

        // ��Ϊ�յ���������
        std::condition_variable m_cv_not_empty;
        // û��������������
        std::condition_variable m_cv_not_full;
    };

    class alignas(64) LockFreeTaskQueue {
    public:
        using Task = BTool::FastFunction;
        using NEED_SET_PROP = std::false_type;

        LockFreeTaskQueue() = default;

        Task pop_task() {
            Task task;
            if (m_queue.try_dequeue(task)) {
                return task;
            }
            return nullptr;
        }
        bool pop_task(Task& task) {
            if (m_queue.try_dequeue(task)) {
                return true;
            }
            task = nullptr;
            return false;
        }

        void add_task(Task&& task) { m_queue.enqueue(std::move(task)); }

        void clear() {
            moodycamel::ConcurrentQueue<Task> empty;
            m_queue.swap(empty);
        }

    private:
        moodycamel::ConcurrentQueue<Task> m_queue;
    };

    /*************************************************
    Description:�ṩ�����Ի��ֵ�,�������������FIFO�������,�����ú���תΪԪ�����洢
                ��ĳһ�������ڶ�����ʱ,ͬ���Ե�������������ʱ,��׷����ԭ����֮��ִ��
                ��ĳһ��������ִ��ʱ,ͬ�����������񽫲���ִ��,ͬһ����֮������������FIFO����ִ�����
    *************************************************/
    template <typename TPropType>
    class SerialTaskQueue {
    public:
        typedef BTool::FastFunction TaskItem;
        using NEED_SET_PROP = std::false_type;

    protected:
        // ��������˫������,���ڴ洢ͬһ��������λ��,��FIFO˳��
        // ���̰߳�ȫ
        class PropCountNodeList {
            // ��������ṹ��
            struct alignas(64) PropCountNode {
                PropCountNode* pre_same_prop_node_;  // ͬ������һ��������ָ��
                PropCountNode* next_same_prop_node_;  // ͬ������һ��������ָ��
                PropCountNode* pre_list_prop_node_;  // ���е���һ��������ָ��
                PropCountNode* next_list_prop_node_;  // ���е���һ��������ָ��

                size_t          count_;  // ��ǰ��������ͬ�����������,����������300��ͬ����,�ڶ�����ֻ����һ��PropCountNode,����Ϊ300
                bool            can_pop_;  // ��ǰ�ڵ��Ƿ�ɱ�pop, ÿ�θ�λ��/������������ᱻ��λΪtrue
                TPropType       prop_;

                template <typename AsTPropType>
                PropCountNode(AsTPropType&& prop, PropCountNode* pre_same_prop_node, PropCountNode* pre_list_prop_node, bool can_immediately_pop)
                    : can_pop_(can_immediately_pop)
                    , count_(1)
                    , pre_same_prop_node_(pre_same_prop_node)
                    , next_same_prop_node_(nullptr)
                    , pre_list_prop_node_(pre_list_prop_node)
                    , next_list_prop_node_(nullptr)
                    , prop_(std::forward<AsTPropType>(prop)) {
                    if (pre_same_prop_node) pre_same_prop_node->next_same_prop_node_ = this;
                    if (pre_list_prop_node) pre_list_prop_node->next_list_prop_node_ = this;
                }
                inline bool can_pop() { return can_pop_; }
                inline void reset_can_pop(bool bcan_pop) { can_pop_ = bcan_pop; }
                inline size_t add() { return ++count_; }
                inline size_t reduce() { return --count_; }
                inline size_t count() const { return count_; }
                inline PropCountNode* get_pre_same_prop_node() const { return pre_same_prop_node_; }
                inline PropCountNode* get_next_same_prop_node() const { return next_same_prop_node_; }
                inline PropCountNode* get_pre_list_prop_node() const { return pre_list_prop_node_; }
                inline PropCountNode* get_next_list_prop_node() const { return next_list_prop_node_; }

                inline void reset_pre_same_prop_node(PropCountNode* pre_same_prop_node) { pre_same_prop_node_ = pre_same_prop_node; }
                inline void reset_next_same_prop_node(PropCountNode* next_same_prop_node) { next_same_prop_node_ = next_same_prop_node; }
                inline void reset_pre_list_prop_node(PropCountNode* pre_list_prop_node) { pre_list_prop_node_ = pre_list_prop_node; }
                inline void reset_next_list_prop_node(PropCountNode* next_list_prop_node) { next_list_prop_node_ = next_list_prop_node; }

                inline const TPropType& get_prop_type() const { return prop_; }
            };

        public:
            PropCountNodeList() : m_begin_node(nullptr), m_end_node(nullptr) {}
            ~PropCountNodeList() { clear(); }

            template <typename AsTPropType>
            void push_back(AsTPropType&& prop, bool can_immediately_pop) {
                // �Ƿ��Ѵ��ڽڵ�
                if (!m_end_node) {
                    m_begin_node = m_end_node = m_obj_pool.allocate(std::forward<AsTPropType>(prop), nullptr, nullptr, can_immediately_pop);
                    m_all_nodes[m_end_node->get_prop_type()].emplace_back(m_end_node);
                    return;
                }

                // ���һ���ڵ��Ƿ���ͬ����
                if (m_end_node->get_prop_type() == prop) {
                    m_end_node->add();
                    return;
                }

                // �������Ƿ񲻴�����������
                auto all_nodes_iter = m_all_nodes.find(prop);
                if (all_nodes_iter == m_all_nodes.end()) {
                    PropCountNode* new_node = m_obj_pool.allocate(std::forward<AsTPropType>(prop), nullptr, m_end_node, can_immediately_pop);
                    m_end_node = new_node;
                    m_all_nodes[new_node->get_prop_type()].emplace_back(new_node);
                    return;
                }

                PropCountNode* pre_same_prop_node = all_nodes_iter->second.back();
                PropCountNode* new_node = m_obj_pool.allocate(std::forward<AsTPropType>(prop), pre_same_prop_node, m_end_node, false);
                m_end_node = new_node;
                m_all_nodes[new_node->get_prop_type()].emplace_back(new_node);
            }

            // ����ĳ�����Ե�����
            void reset_prop(const TPropType& prop_type) {
                auto all_nodes_iter = m_all_nodes.find(prop_type);
                if (all_nodes_iter == m_all_nodes.end()) return;

                all_nodes_iter->second.front()->reset_can_pop(true);
            }

            // ȥ���׸�ָ�����Լ��ϵ��׸��ڵ�,�޸ýڵ�ʱ����false
            TPropType pop_front() {
                auto pop_front_node = m_begin_node;
                while (pop_front_node) {
                    if (pop_front_node->can_pop()) break;
                    pop_front_node = pop_front_node->get_next_list_prop_node();
                }

                assert(pop_front_node);

                pop_front_node->reduce();
                if (pop_front_node->count() > 0) {
                    pop_front_node->reset_can_pop(false);
                    return pop_front_node->get_prop_type();
                }

                // ��ȡ��һͬ���Խڵ�,����һͬ���Խڵ����һ�ڵ�ָ���ÿ�
                auto next_same_prop_node = pop_front_node->get_next_same_prop_node();
                if (next_same_prop_node) {
                    next_same_prop_node->reset_pre_same_prop_node(nullptr);
                    m_all_nodes[pop_front_node->get_prop_type()].pop_front();
                } else {
                    m_all_nodes.erase(pop_front_node->get_prop_type());
                }

                // ��ȡ��һ�ڵ�,����һ�ڵ�� ��һ�ڵ�ָ��  ��ֵΪ ԭ��ָ��� ��һ�ڵ�ָ��
                // ������һ�ڵ����һ�ڵ�ָ��  ��ֵΪ ��ָ���  ��һ�ڵ�ָ��
                auto pre_list_prop_node = pop_front_node->get_pre_list_prop_node();
                auto next_list_prop_node = pop_front_node->get_next_list_prop_node();
                if (next_list_prop_node) next_list_prop_node->reset_pre_list_prop_node(pre_list_prop_node);
                if (pre_list_prop_node) pre_list_prop_node->reset_next_list_prop_node(next_list_prop_node);

                if (pop_front_node == m_begin_node) m_begin_node = next_list_prop_node;
                if (pop_front_node == m_end_node) m_end_node = pre_list_prop_node;

                auto prop_type = pop_front_node->get_prop_type();
                m_obj_pool.deallocate(pop_front_node);
                return prop_type;
            }

            template <typename AsTPropType>
            void remove_prop(AsTPropType&& prop) {
                auto all_node_iter = m_all_nodes.find(prop);
                if (all_node_iter == m_all_nodes.end()) return;

                bool need_comp_begin(false);  // �Ƿ���Ҫ�ȶ�begin�ڵ�
                if (m_begin_node && m_begin_node->get_prop_type() == prop) need_comp_begin = true;
                bool need_comp_end(false);  // �Ƿ���Ҫ�ȶ�end�ڵ�
                if (m_end_node && m_end_node->get_prop_type() == prop) need_comp_end = true;

                for (auto& item : all_node_iter->second) {
                    // �޸�������һ�ڵ�ָ���  ��һ�ڵ�Ϊ ��ǰ����һ�ڵ�
                    // ��֮ͬ��
                    auto pre_list_prop_node = item->get_pre_list_prop_node();
                    auto next_list_prop_node = item->get_next_list_prop_node();
                    if (pre_list_prop_node) pre_list_prop_node->reset_next_list_prop_node(next_list_prop_node);
                    if (next_list_prop_node) next_list_prop_node->reset_pre_list_prop_node(pre_list_prop_node);

                    // �жϲ�����begin�ڵ�
                    if (need_comp_begin && item == m_begin_node) {
                        m_begin_node = next_list_prop_node;
                        if (m_begin_node) m_begin_node->reset_pre_list_prop_node(nullptr);
                        if (!m_begin_node || m_begin_node->get_prop_type() != prop) need_comp_begin = false;
                    }
                    // �жϲ�����end�ڵ�
                    if (need_comp_end && item == m_end_node) {
                        m_end_node = pre_list_prop_node;
                        if (m_end_node) m_end_node->reset_next_list_prop_node(nullptr);
                        if (!m_end_node || m_end_node->get_prop_type() != prop) need_comp_end = false;
                    }

                    // ɾ���ýڵ�
                    m_obj_pool.deallocate(item);
                }
                m_all_nodes.erase(all_node_iter);
            }

            void clear() {
                PropCountNode* node_ptr(m_begin_node);
                while (node_ptr) {
                    auto tmp = node_ptr->get_next_list_prop_node();
                    m_obj_pool.deallocate(node_ptr);
                    node_ptr = tmp;
                }
                m_begin_node = nullptr;
                m_end_node = nullptr;
                m_all_nodes.clear();
            }

            bool empty() const { return m_begin_node == nullptr; }

        private:
            PropCountNode*  m_begin_node;  // ������ʼ�ڵ�
            PropCountNode*  m_end_node;  // ���н����ڵ�
            std::unordered_map<TPropType, std::list<PropCountNode*>> m_all_nodes;  // ���ж��нڵ�
            ObjectPoolNode<PropCountNode> m_obj_pool;  // �����
        };

    public:
        // max_task_count: ����������,��������������������;0���ʾ������
        SerialTaskQueue(size_t max_task_count = 0, size_t props_size = 128)
            : m_bstop(false)
            , m_max_task_count(max_task_count) {
            m_wait_tasks.reserve(props_size);
            m_cur_props.reserve(props_size);
        }

        virtual ~SerialTaskQueue() { stop(); }

        // Ԥ�����Ը���, ����Ƶ��rehash���������ܿ���
        void reserve(size_t props_size) {
            std::lock_guard<std::mutex> locker(m_mtx);
            m_wait_tasks.reserve(props_size);
            m_cur_props.reserve(props_size);
        }

        bool empty() const {
            std::lock_guard<std::mutex> locker(m_mtx);
            return m_wait_tasks.empty();
        }

        void clear() {
            std::lock_guard<std::mutex> locker(m_mtx);
            clear_inner();
        }

        void start() {
            // ��λ����ֹ��־��
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            std::lock_guard<std::mutex> locker(m_mtx);
            // �Ƿ�����ֹ�ж�
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }
            if (bwait) {
                m_cv_not_full.notify_all();
                m_cv_not_empty.notify_all();
                return;
            }

            clear_inner();
        }

        void wait() {
            while (!empty()) std::this_thread::yield();
        }

        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template <typename AsTPropType, typename AsTFunction>
        bool add_task(AsTPropType&& prop, AsTFunction&& func) {
            if (UNLIKELY(m_bstop.load(std::memory_order_relaxed))) return false;

            // ����·�����
            if (not_full()) {
                std::unique_lock<std::mutex> locker(m_mtx, std::defer_lock);
                if (locker.try_lock()) {
                    if (UNLIKELY(m_bstop.load(std::memory_order_relaxed))) return false;

                    auto& task_queue = m_wait_tasks[prop];
                    task_queue.push_back(std::forward<AsTFunction>(func));
                    bool is_first = m_cur_props.find(prop) == m_cur_props.end() && task_queue.size() == 1;
                    m_wait_props.push_back(std::forward<AsTPropType>(prop), is_first);
                    m_approx_size.fetch_add(1, std::memory_order_release);
                    locker.unlock();
                    m_cv_not_empty.notify_one();
                    return true;
                }
            }

            // ����·��
            {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_full.wait(locker, [this] { return m_bstop.load(std::memory_order_relaxed) || not_full(); });

                if (UNLIKELY(m_bstop.load(std::memory_order_relaxed))) return false;

                auto& task_queue = m_wait_tasks[prop];
                task_queue.push_back(std::forward<AsTFunction>(func));
                bool is_first = m_cur_props.find(prop) == m_cur_props.end() && task_queue.size() == 1;
                m_wait_props.push_back(std::forward<AsTPropType>(prop), is_first);
                m_approx_size.fetch_add(1, std::memory_order_relaxed);
            }
            m_cv_not_empty.notify_one();
            return true;
        }

        void pop_task() {
            TaskItem next_task(nullptr);
            TPropType prop_type;

            {
                std::unique_lock<std::mutex> locker(m_mtx);
                if (LIKELY(!m_bstop.load())) {
                    m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
                }

                if (UNLIKELY(!not_empty())) return;

                m_approx_size.fetch_sub(1, std::memory_order_release);
                prop_type = m_wait_props.pop_front();
                assert(m_cur_props.find(prop_type) == m_cur_props.end());
                m_cur_props.emplace(prop_type);
                auto wait_task_iter = m_wait_tasks.find(prop_type);
                if (wait_task_iter != m_wait_tasks.end()) {
                    next_task = std::move(wait_task_iter->second.front());
                }
            }

            if (next_task) {
                next_task();
                {
                    std::lock_guard<std::mutex> locker(m_mtx);
                    auto wait_task_iter = m_wait_tasks.find(prop_type);
                    if (wait_task_iter != m_wait_tasks.end()) {
                        wait_task_iter->second.pop_front();
                        if (wait_task_iter->second.empty()) m_wait_tasks.erase(wait_task_iter);
                    }
                    remove_cur_prop(prop_type);
                    m_wait_props.reset_prop(prop_type);
                }
                m_cv_not_full.notify_one();
            }
        }

        // �Ƴ�����ָ����������,��ǰ����ִ�г���,���ܴ�������
        // ���ڱ���,���ܱȽϺ�ʱ
        template <typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            {
                std::lock_guard<std::mutex> locker(m_mtx);
                auto iter = m_wait_tasks.find(prop);
                if (iter != m_wait_tasks.end()) m_wait_tasks.erase(prop);
                m_wait_props.remove_prop(prop);
            }
            m_cv_not_full.notify_one();
        }

        bool full() const {
            std::lock_guard<std::mutex> locker(m_mtx);
            return !not_full();
        }

        size_t size() const {
            std::lock_guard<std::mutex> locker(m_mtx);
            return m_wait_tasks.size();
        }

    protected:
        void clear_inner() {
            m_wait_props.clear();
            m_wait_tasks.clear();
            m_cur_props.clear();

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        // ɾ����ǰ��������
        template <typename AsTPropType>
        inline void remove_cur_prop(AsTPropType&& prop_type) {
            auto prop_iter = m_cur_props.find(std::forward<AsTPropType>(prop_type));
            if (prop_iter != m_cur_props.end()) m_cur_props.erase(prop_iter);
        }

        // �Ƿ���δ��״̬
        inline bool not_full() const { return m_max_task_count == 0 || m_approx_size.load(std::memory_order_relaxed) < m_max_task_count; }

        // �Ƿ��ڿ�״̬
        inline bool not_empty() const { return !m_wait_tasks.empty() && m_cur_props.size() < m_wait_tasks.size(); }

    protected:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>       m_bstop;
        std::atomic<size_t>     m_approx_size{0};

        // ���ݰ�ȫ��
        mutable std::mutex      m_mtx;
        // �ܴ�ִ����������˳�����,�����ж�ִ�ж���˳��
        PropCountNodeList       m_wait_props;
        // �ܴ�ִ������������Լ����Ӧ����
        std::unordered_map<TPropType, std::deque<TaskItem>> m_wait_tasks;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                  m_max_task_count;

        // ��Ϊ�յ���������
        std::condition_variable m_cv_not_empty;
        // û��������������
        std::condition_variable m_cv_not_full;

        // ��ǰ����ִ���е���������
        std::unordered_set<TPropType> m_cur_props;
    };

    /*************************************************
    Description:�ṩ�����Ի��ֵ�,�������������FIFO�������,�����ú���תΪԪ�����洢
                ��ĳһ�������ڶ�����ʱ,ͬ���Ե�������������ʱ,��׷����ԭ����֮��ִ��
                ��ĳһ��������ִ��ʱ,ͬ�����������񽫲���ִ��,ͬһ����֮������������FIFO����ִ�����
                ע��: ��������в����ӿڱ���Ϊ�̰߳�ȫ��, ���ṩ��ȫ����, ���迪ʼʱ��ע������
                    add_taskʱ, ��ͬ���Լ��̰߳�ȫ, ͬ�����ڷ��̰߳�ȫ
    *************************************************/
    template <typename TPropType>
    class SPMCSerialTaskQueue {
    public:
        using Task = BTool::FastFunction;
        using NEED_SET_PROP = std::true_type;

        struct alignas(64) AlignedStatus {
            TPropType prop_;
            std::atomic<bool> can_pop_;

            AlignedStatus(const TPropType& prop) : prop_(prop), can_pop_(true) {};
            AlignedStatus(const AlignedStatus& other) noexcept : prop_(other.prop_), can_pop_(other.can_pop_.load(std::memory_order_relaxed)) {}
            AlignedStatus(AlignedStatus&& other) noexcept : prop_(std::move(other.prop_)), can_pop_(other.can_pop_.load(std::memory_order_relaxed)) {}
        };

    public:
        // max_task_count: ����������,��������������������;0���ʾ������
        // max_single_task_count: ������������,��������������������;0���ʾ������
        SPMCSerialTaskQueue(size_t max_task_count = 0, size_t max_single_task_count = 0)
            : m_bstop(false), m_bclear(false), m_approx_size(0), m_max_task_count(max_task_count), m_max_single_task_count(max_single_task_count) {}

        virtual ~SPMCSerialTaskQueue() { stop(); }

        void reset_props(const std::vector<TPropType>& props) {
            m_cur_props_index.store(0);
            for (auto& prop : props) {
                m_wait_tasks.emplace(prop, moodycamel::ConcurrentQueue<Task>());
                m_all_props.emplace_back(AlignedStatus(prop));
                m_single_task_count.emplace(prop, 0);
            }
        }

        bool empty() const { return !not_empty(); }

        void clear() { clear_inner(); }

        void start() {
            // ��λ����ֹ��־��
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }
        }

        void stop(bool bwait = false) {
            // �Ƿ�����ֹ�ж�
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }
            if (!bwait) {
                clear();
            }
        }

        void wait() {
            while (!empty()) std::this_thread::yield();
        }

        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template <typename AsTPropType, typename AsTFunction>
        bool add_task(AsTPropType&& prop, AsTFunction&& func) {
            if (UNLIKELY(m_bstop.load(std::memory_order_relaxed))) return false;

            if (!not_full(prop)) {
                return false;
            }

            m_wait_tasks[prop].enqueue(std::forward<AsTFunction>(func));

            if (UNLIKELY(m_bclear.load(std::memory_order_relaxed))) {
                m_approx_size.store(0, std::memory_order_release);
                m_single_task_count[prop].store(0, std::memory_order_release);
            } else {
                m_approx_size.fetch_add(1, std::memory_order_release);
                m_single_task_count[prop].fetch_add(1, std::memory_order_release);
            }
            return true;
        }

        void pop_task() {
            // Ԥ��
            if (!not_empty()) {
                std::this_thread::yield();
                return;
            }

            auto cur_index = m_cur_props_index.fetch_add(1, std::memory_order_relaxed) % m_all_props.size();
            auto& cur_prop = m_all_props[cur_index];

            bool expected = true;
            if (!cur_prop.can_pop_.compare_exchange_strong(expected, false)) {
                return;
            }

            auto& wait_prop = m_wait_tasks[cur_prop.prop_];

            Task next_task(nullptr);
            while (wait_prop.try_dequeue(next_task)) {
                if (UNLIKELY(m_bclear.load(std::memory_order_relaxed))) {
                    m_approx_size.store(0, std::memory_order_release);
                    m_single_task_count[cur_prop.prop_].store(0, std::memory_order_release);
                    break;
                } else {
                    m_approx_size.fetch_sub(1, std::memory_order_release);
                    m_single_task_count[cur_prop.prop_].fetch_sub(1, std::memory_order_release);
                }
                if (next_task) {
                    next_task();
                }
            }
            cur_prop.can_pop_.store(true, std::memory_order_release);
        }

        bool full() const { return !not_full(); }

        size_t size() const { return m_wait_tasks.size(); }

    protected:
        void clear_inner() {
            m_bclear.store(true, std::memory_order_release);
            for (auto& item : m_all_props) {
                item.can_pop_.store(false, std::memory_order_release);
            }
            Task t;
            for (auto& item : m_wait_tasks) {
                auto& task_queue = item.second;
                while (task_queue.try_dequeue(t));
            }
            for (auto& item : m_single_task_count) {
                item.second.store(0, std::memory_order_release);
            }
            m_cur_props_index.store(0, std::memory_order_release);
            for (auto& item : m_all_props) {
                item.can_pop_.store(true);
            }
            m_approx_size.store(0, std::memory_order_release);
        }

        // �Ƿ���δ��״̬
        inline bool not_full(const TPropType& prop) {
            return m_max_single_task_count == 0 || m_single_task_count[prop].load(std::memory_order_relaxed) < m_max_single_task_count;
        }
        // �Ƿ��ڿ�״̬
        inline bool not_empty() const { return !m_bclear.load(std::memory_order_relaxed) && m_approx_size.load(std::memory_order_relaxed) > 0; }

    protected:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>           m_bstop;
        std::atomic<bool>           m_bclear;
        std::atomic<int>            m_approx_size;
        // ��ǰ�����±�
        std::atomic<size_t>         m_cur_props_index;
        // ��������
        std::vector<AlignedStatus>  m_all_props;
        // �ܴ�ִ������������Լ����Ӧ����
        std::unordered_map<TPropType, moodycamel::ConcurrentQueue<Task>> m_wait_tasks;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                      m_max_task_count;
        // ����������������,��Ϊ0ʱ��ʾ������
        size_t                      m_max_single_task_count;
        std::unordered_map<TPropType, std::atomic<int>> m_single_task_count;
    };

}