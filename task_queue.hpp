/*************************************************
File name:  task_queue.hpp
Author:     AChar
Version:
Date:
Description:    提供各类任务队列,避免外界重复创建
                类不采用继承的形式,本身改动较少,可加快速度

Note:  condition_variable使用注意:在进行wait时会首先
    1.执行判断,为true则退出
    2.释放锁进入(信号量)休眠
    3.接收notify,竞争锁
    然后重复1-3操作,直至达到触发条件后退出,注意此时依旧为1操作中,并未释放锁

Note2:  queue是自己创建的, 可以通过swap等方式快速释放占用内存
        map/set等是内部创建的, 无法通过swap甚至析构去真实释放实际内存
            一定要释放的话, 自行在消费完毕后调用malloc_trim(0);
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
    Description:提供基于函数的FIFO任务队列
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
            // 复位已终止标志符
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            std::lock_guard<std::mutex> locker(m_mtx);
            // 是否已终止判断
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
                // queue不会主动释放已开辟空间,当任务清空时则释放一下,若实际环境中,可自己定义为list来避免queue长期占用的问题,但这会导致性能的略微下降,根据实际情况决定
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
        // 是否处于未满状态
        bool not_full() const { return m_max_task_count == 0 || m_queue.size() < m_max_task_count; }

        // 是否处于空状态
        bool not_empty() const { return !m_queue.empty(); }

    protected:
        // 是否已终止标识符
        std::atomic<bool>           m_bstop;
        // 数据安全锁
        mutable std::mutex          m_mtx;

        // 总待执行任务队列,包含所有的待执行任务
        std::queue<TaskItem>        m_queue;
        // 最大任务个数,当为0时表示无限制
        size_t                      m_max_task_count;

        // 不为空的条件变量
        std::condition_variable     m_cv_not_empty;
        // 没有满的条件变量
        std::condition_variable     m_cv_not_full;
    };

    /*************************************************
    Description:提供基于无锁以及信号量的FIFO任务队列
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
            // 复位已终止标志符
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            std::lock_guard<std::mutex> locker(m_mtx);
            // 是否已终止判断
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
        // 是否处于未满状态
        inline bool not_full() const { return m_max_task_count == 0 || size() < m_max_task_count; }

        // 是否处于空状态
        inline bool not_empty() const { return size() != 0; }

    protected:
        // 是否已终止标识符
        std::atomic<bool>           m_bstop;
        // 数据安全锁
        mutable std::mutex          m_mtx;

        // 总待执行任务队列,包含所有的待执行任务
        moodycamel::ConcurrentQueue<TaskItem>       m_queue;
        // 最大任务个数,当为0时表示无限制
        size_t                      m_max_task_count;

        // 不为空的条件变量
        std::condition_variable     m_cv_not_empty;
        // 没有满的条件变量
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
            // 复位已终止标志符
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            std::lock_guard<std::mutex> locker(m_mtx);
            // 是否已终止判断
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
        // 是否处于未满状态
        inline bool not_full() const { return m_max_task_count == 0 || size() < m_max_task_count; }

        // 是否处于空状态
        inline bool not_empty() const { return size() != 0; }

    protected:
        // 是否已终止标识符
        std::atomic<bool>           m_bstop;
        // 数据安全锁
        mutable std::mutex          m_mtx;

        // 总待执行任务队列,包含所有的待执行任务
        moodycamel::ConcurrentQueue<TaskItem>   m_queue;
        moodycamel::ProducerToken               m_ptok;
        moodycamel::ConsumerToken               m_ctok;
        // 最大任务个数,当为0时表示无限制
        size_t                                  m_max_task_count;

        // 不为空的条件变量
        std::condition_variable                 m_cv_not_empty;
        // 没有满的条件变量
        std::condition_variable                 m_cv_not_full;
    };

    /*************************************************
    Description:提供无限制pop的无锁队列, 但是该队列存在cpu过高的问题
                并且在linux下单线程也可能出现cpu爆满且性能低的问题
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
            // 复位已终止标志符
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }
        }

        void stop(bool bwait = false) {
            // 是否已终止判断
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

            // 不做判断处理
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
        // 是否已终止标识符
        std::atomic<bool> m_bstop;
        // 总待执行任务队列,包含所有的待执行任务
        moodycamel::ConcurrentQueue<TaskItem> m_queue;
        // moodycamel::BlockingConcurrentQueue<TaskItem>       m_queue;
        // 最大任务个数,当为0时表示无限制
        size_t m_max_task_count;
    };

    /*************************************************
    Description:提供按属性划分的,仅保留最新状态的FIFO任务队列
                当某一属性正在队列中时,同属性的其他任务新增时,原任务会被覆盖
    *************************************************/
    template <typename TPropType>
    class LastTaskQueue {
    public:
        typedef std::function<void()> TaskItem;
        using NEED_SET_PROP = std::false_type;

    public:
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
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
            // 复位已终止标志符
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            std::lock_guard<std::mutex> locker(m_mtx);
            // 是否已终止判断
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

                // 是否已无可pop队列
                if (UNLIKELY(m_wait_props.empty())) return;

                for (auto pop_type_iter = m_wait_props.begin(); pop_type_iter != m_wait_props.end(); pop_type_iter++) {
                    if (m_cur_pop_props.find(*pop_type_iter) != m_cur_pop_props.end()) continue;

                    pop_type = *pop_type_iter;
                    // 获取任务指针
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

        // 移除所有指定属性任务,当前正在执行除外,可能存在阻塞
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
        // 是否处于未满状态
        inline bool not_full() const { return m_max_task_count == 0 || m_wait_props.size() < m_max_task_count; }

        // 是否处于非空状态
        inline bool not_empty() const { return !m_wait_props.empty(); }

    protected:
        // 是否已终止标识符
        std::atomic<bool> m_bstop;

        // 数据安全锁
        mutable std::mutex m_mtx;
        // 总待执行任务属性顺序队列,用于判断执行队列顺序
        std::list<TPropType> m_wait_props;
        // 总待执行任务队列属性及其对应任务,其个数必须始终与m_wait_tasks个数同步
        std::unordered_map<TPropType, TaskItem> m_wait_tasks;
        // 当前正在pop任务属性
        std::unordered_set<TPropType> m_cur_pop_props;
        // 最大任务个数,当为0时表示无限制
        size_t m_max_task_count;

        // 不为空的条件变量
        std::condition_variable m_cv_not_empty;
        // 没有满的条件变量
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
    Description:提供按属性划分的,保留所有任务的FIFO任务队列,将调用函数转为元祖对象存储
                当某一属性正在队列中时,同属性的其他任务新增时,会追加至原任务之后执行
                当某一任务正在执行时,同属性其他任务将不被执行,同一属性之间的任务均按照FIFO串行执行完毕
    *************************************************/
    template <typename TPropType>
    class SerialTaskQueue {
    public:
        typedef BTool::FastFunction TaskItem;
        using NEED_SET_PROP = std::false_type;

    protected:
        // 连续属性双向链表,用于存储同一属性上下位置,及FIFO顺序
        // 非线程安全
        class PropCountNodeList {
            // 连续任务结构体
            struct alignas(64) PropCountNode {
                PropCountNode* pre_same_prop_node_;  // 同属性上一连续任务指针
                PropCountNode* next_same_prop_node_;  // 同属性下一连续任务指针
                PropCountNode* pre_list_prop_node_;  // 队列的上一连续任务指针
                PropCountNode* next_list_prop_node_;  // 队列的下一连续任务指针

                size_t          count_;  // 当前连续新增同属性任务个数,如连续新增300个同属性,在队列中只创建一个PropCountNode,计数为300
                bool            can_pop_;  // 当前节点是否可被pop, 每次复位后/新增后首链表会被复位为true
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
                // 是否已存在节点
                if (!m_end_node) {
                    m_begin_node = m_end_node = m_obj_pool.allocate(std::forward<AsTPropType>(prop), nullptr, nullptr, can_immediately_pop);
                    m_all_nodes[m_end_node->get_prop_type()].emplace_back(m_end_node);
                    return;
                }

                // 最后一个节点是否相同属性
                if (m_end_node->get_prop_type() == prop) {
                    m_end_node->add();
                    return;
                }

                // 该属性是否不存在其他任务
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

            // 重置某个属性的任务
            void reset_prop(const TPropType& prop_type) {
                auto all_nodes_iter = m_all_nodes.find(prop_type);
                if (all_nodes_iter == m_all_nodes.end()) return;

                all_nodes_iter->second.front()->reset_can_pop(true);
            }

            // 去除首个指定属性集合的首个节点,无该节点时返回false
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

                // 获取下一同属性节点,对下一同属性节点的上一节点指针置空
                auto next_same_prop_node = pop_front_node->get_next_same_prop_node();
                if (next_same_prop_node) {
                    next_same_prop_node->reset_pre_same_prop_node(nullptr);
                    m_all_nodes[pop_front_node->get_prop_type()].pop_front();
                } else {
                    m_all_nodes.erase(pop_front_node->get_prop_type());
                }

                // 获取下一节点,对下一节点的 上一节点指针  赋值为 原本指针的 上一节点指针
                // 并将上一节点的下一节点指针  赋值为 本指针的  下一节点指针
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

                bool need_comp_begin(false);  // 是否需要比对begin节点
                if (m_begin_node && m_begin_node->get_prop_type() == prop) need_comp_begin = true;
                bool need_comp_end(false);  // 是否需要比对end节点
                if (m_end_node && m_end_node->get_prop_type() == prop) need_comp_end = true;

                for (auto& item : all_node_iter->second) {
                    // 修改自身上一节点指针的  下一节点为 当前的下一节点
                    // 反之同理
                    auto pre_list_prop_node = item->get_pre_list_prop_node();
                    auto next_list_prop_node = item->get_next_list_prop_node();
                    if (pre_list_prop_node) pre_list_prop_node->reset_next_list_prop_node(next_list_prop_node);
                    if (next_list_prop_node) next_list_prop_node->reset_pre_list_prop_node(pre_list_prop_node);

                    // 判断并重置begin节点
                    if (need_comp_begin && item == m_begin_node) {
                        m_begin_node = next_list_prop_node;
                        if (m_begin_node) m_begin_node->reset_pre_list_prop_node(nullptr);
                        if (!m_begin_node || m_begin_node->get_prop_type() != prop) need_comp_begin = false;
                    }
                    // 判断并重置end节点
                    if (need_comp_end && item == m_end_node) {
                        m_end_node = pre_list_prop_node;
                        if (m_end_node) m_end_node->reset_next_list_prop_node(nullptr);
                        if (!m_end_node || m_end_node->get_prop_type() != prop) need_comp_end = false;
                    }

                    // 删除该节点
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
            PropCountNode*  m_begin_node;  // 队列起始节点
            PropCountNode*  m_end_node;  // 队列结束节点
            std::unordered_map<TPropType, std::list<PropCountNode*>> m_all_nodes;  // 所有队列节点
            ObjectPoolNode<PropCountNode> m_obj_pool;  // 对象池
        };

    public:
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        SerialTaskQueue(size_t max_task_count = 0, size_t props_size = 128)
            : m_bstop(false)
            , m_max_task_count(max_task_count) {
            m_wait_tasks.reserve(props_size);
            m_cur_props.reserve(props_size);
        }

        virtual ~SerialTaskQueue() { stop(); }

        // 预设属性个数, 避免频繁rehash带来的性能开销
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
            // 复位已终止标志符
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            std::lock_guard<std::mutex> locker(m_mtx);
            // 是否已终止判断
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

        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        template <typename AsTPropType, typename AsTFunction>
        bool add_task(AsTPropType&& prop, AsTFunction&& func) {
            if (UNLIKELY(m_bstop.load(std::memory_order_relaxed))) return false;

            // 快速路径检查
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

            // 慢速路径
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

        // 移除所有指定属性任务,当前正在执行除外,可能存在阻塞
        // 存在遍历,可能比较耗时
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

        // 删除当前运行属性
        template <typename AsTPropType>
        inline void remove_cur_prop(AsTPropType&& prop_type) {
            auto prop_iter = m_cur_props.find(std::forward<AsTPropType>(prop_type));
            if (prop_iter != m_cur_props.end()) m_cur_props.erase(prop_iter);
        }

        // 是否处于未满状态
        inline bool not_full() const { return m_max_task_count == 0 || m_approx_size.load(std::memory_order_relaxed) < m_max_task_count; }

        // 是否处于空状态
        inline bool not_empty() const { return !m_wait_tasks.empty() && m_cur_props.size() < m_wait_tasks.size(); }

    protected:
        // 是否已终止标识符
        std::atomic<bool>       m_bstop;
        std::atomic<size_t>     m_approx_size{0};

        // 数据安全锁
        mutable std::mutex      m_mtx;
        // 总待执行任务属性顺序队列,用于判断执行队列顺序
        PropCountNodeList       m_wait_props;
        // 总待执行任务队列属性及其对应任务
        std::unordered_map<TPropType, std::deque<TaskItem>> m_wait_tasks;
        // 最大任务个数,当为0时表示无限制
        size_t                  m_max_task_count;

        // 不为空的条件变量
        std::condition_variable m_cv_not_empty;
        // 没有满的条件变量
        std::condition_variable m_cv_not_full;

        // 当前正在执行中的任务属性
        std::unordered_set<TPropType> m_cur_props;
    };

    /*************************************************
    Description:提供按属性划分的,保留所有任务的FIFO任务队列,将调用函数转为元祖对象存储
                当某一属性正在队列中时,同属性的其他任务新增时,会追加至原任务之后执行
                当某一任务正在执行时,同属性其他任务将不被执行,同一属性之间的任务均按照FIFO串行执行完毕
                注意: 该任务队列操作接口必须为线程安全的, 不提供安全保护, 且需开始时就注册属性
                    add_task时, 不同属性间线程安全, 同属性内非线程安全
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
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        // max_single_task_count: 单任务最大个数,超过该数量将产生阻塞;0则表示无限制
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
            // 复位已终止标志符
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }
        }

        void stop(bool bwait = false) {
            // 是否已终止判断
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

        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
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
            // 预判
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

        // 是否处于未满状态
        inline bool not_full(const TPropType& prop) {
            return m_max_single_task_count == 0 || m_single_task_count[prop].load(std::memory_order_relaxed) < m_max_single_task_count;
        }
        // 是否处于空状态
        inline bool not_empty() const { return !m_bclear.load(std::memory_order_relaxed) && m_approx_size.load(std::memory_order_relaxed) > 0; }

    protected:
        // 是否已终止标识符
        std::atomic<bool>           m_bstop;
        std::atomic<bool>           m_bclear;
        std::atomic<int>            m_approx_size;
        // 当前属性下标
        std::atomic<size_t>         m_cur_props_index;
        // 所有属性
        std::vector<AlignedStatus>  m_all_props;
        // 总待执行任务队列属性及其对应任务
        std::unordered_map<TPropType, moodycamel::ConcurrentQueue<Task>> m_wait_tasks;
        // 最大任务个数,当为0时表示无限制
        size_t                      m_max_task_count;
        // 单属性最大任务个数,当为0时表示无限制
        size_t                      m_max_single_task_count;
        std::unordered_map<TPropType, std::atomic<int>> m_single_task_count;
    };

}