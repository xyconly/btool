/*************************************************
File name:  task_queue.hpp
Author:     AChar
Version:
Date:
Description:    提供各类任务队列,避免外界重复创建
*************************************************/
#pragma once
#include <queue>
#include <list>
#include <map>
#include <set>
#include <assert.h>
#include <condition_variable>
#include "rwmutex.hpp"
#include "atomic_switch.hpp"
#include "task_item.hpp"

namespace BTool
{
    /*************************************************
    Description:提供FIFO任务队列,将调用函数转为元祖对象存储
    *************************************************/
    class TupleTaskQueue
    {
         // 禁止拷贝
        TupleTaskQueue(const TupleTaskQueue&) = delete;
        TupleTaskQueue& operator=(const TupleTaskQueue&) = delete;
    public:
        TupleTaskQueue(size_t max_task_count = 0)
            : m_max_task_count(max_task_count)
            , m_bstop(false)
        {}

        virtual ~TupleTaskQueue() {
            stop();
            clear();
        }

        template<typename Function, typename... Args>
        bool add_task(const Function& func, Args&&... args) {
            wait_for_can_add();

            if (m_bstop.load())
                return false;

            // 此处TTuple不可采用std::forward_as_tuple(std::forward<Args>(args)...)
            // 假使agrs中含有const & 时,会导致tuple中存储的亦为const &对象,从而外部释放对象后导致内部对象无效
            // 采用std::make_shared<TTuple>则会导致存在一次拷贝,由std::make_tuple引起(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<TupleTask<Function, TTuple>>(func, std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        template<typename Function, typename... Args>
        bool add_task(Function&& func, Args&&... args) {
            wait_for_can_add();

            if (m_bstop.load())
                return false;

            // 此处TTuple不可采用std::forward_as_tuple(std::forward<Args>(args)...)
            // 假使agrs中含有const & 时,会导致tuple中存储的亦为const &对象,从而外部释放对象后导致内部对象无效
            // 采用std::make_shared<TTuple>则会导致存在一次拷贝,由std::make_tuple引起(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<TupleTask<Function, TTuple>>(std::forward<Function>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // 移除一个顶层非当前执行属性任务,队列为空时存在阻塞
        void pop_task() {
            wait_for_can_pop();

            if (m_bstop.load())
                return;

            TaskPtr pop_task_ptr(nullptr);

            {
                writeLock locker(m_tasks_mtx);
                if (m_queue.empty())
                    return;
                pop_task_ptr = m_queue.front();
                m_queue.pop();
            }
            m_cv_not_full.notify_one();

            if (pop_task_ptr) {
                pop_task_ptr->invoke();
            }
        }

        void stop() {
            // 是否已终止判断
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void clear() {
            {
                writeLock locker(m_tasks_mtx);
                std::queue<TaskPtr> empty;
                m_queue.swap(empty);
            }
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        bool empty() const {
            return !not_empty();
        }

        bool full() const {
            return !not_full();
        }

        size_t size() const {
            readLock locker(m_tasks_mtx);
            return m_queue.size();
        }

    protected:
        // 是否处于未满状态
        bool not_full() const {
            readLock locker(m_tasks_mtx);
            return m_max_task_count == 0 || m_queue.size() < m_max_task_count;
        }

        // 是否处于空状态
        bool not_empty() const {
            readLock locker(m_tasks_mtx);
            return !m_queue.empty();
        }

      private:
          // 等待直到可移除任务至队列
        void wait_for_can_pop() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
        }
        // 等待直到可加入新的任务至队列
        void wait_for_can_add() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });
        }

        // 新增任务至队列
        bool add_task_tolist(TaskPtr&& new_task_item)
        {
            if (!new_task_item)
                return false;

            {
                writeLock locker(m_tasks_mtx);
                m_queue.push(std::forward<TaskPtr>(new_task_item));
            }

            m_cv_not_empty.notify_one();
            return true;
        }

    private:
        // 是否已终止标识符
        std::atomic<bool>           m_bstop;

        // 任务队列锁
        mutable rwMutex             m_tasks_mtx;
        // 总待执行任务队列,包含所有的待执行任务
        std::queue<TaskPtr>         m_queue;
        // 最大任务个数,当为0时表示无限制
        size_t                      m_max_task_count;

        // 条件阻塞锁, 使用std::unique_lock进行锁定,std::condition_variable会在判定结束后释放该锁
        std::mutex                      m_cv_mtx;
        // 不为空的条件变量
        std::condition_variable         m_cv_not_empty;
        // 没有满的条件变量
        std::condition_variable         m_cv_not_full;
    };


    /*************************************************
    Description:提供按属性划分的,仅保留最新状态的FIFO任务队列,将调用函数转为元祖对象存储
                当某一属性正在队列中时,同属性的其他任务新增时,原任务会被覆盖
    *************************************************/
    template<typename TPropType>
    class LastTupleTaskQueue
    {
        typedef std::shared_ptr<PropTaskVirtual<TPropType>> PropTaskPtr;

    public:
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        LastTupleTaskQueue(size_t max_task_count = 0)
            : m_max_task_count(max_task_count)
            , m_bstop(false)
        {}
        ~LastTupleTaskQueue() {
            clear();
            stop();
        }

        template<typename TFunction, typename... Args>
        bool add_task(const TPropType& prop, TFunction&& func, Args&&... args) {
            wait_for_can_add();
            if (m_bstop.load())
                return false;

            // 此处TTuple不可采用std::forward_as_tuple(std::forward<Args>(args)...)
            // 假使agrs中含有const & 时,会导致tuple中存储的亦为const &对象,从而外部释放对象后导致内部对象无效
            // 采用std::make_shared<TTuple>则会导致存在一次拷贝,由std::make_tuple引起(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(prop, std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        template<typename TFunction, typename... Args>
        bool add_task(TPropType&& prop, TFunction&& func, Args&&... args) {
            wait_for_can_add();
            if (m_bstop.load())
                return false;

            // 此处TTuple不可采用std::forward_as_tuple(std::forward<Args>(args)...)
            // 假使agrs中含有const & 时,会导致tuple中存储的亦为const &对象,从而外部释放对象后导致内部对象无效
            // 采用std::make_shared<TTuple>则会导致存在一次拷贝,由std::make_tuple引起(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(std::forward<TPropType>(prop), std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // 移除所有指定属性任务,当前正在执行除外,可能存在阻塞
        void remove_prop(const TPropType& prop) {
            {
                writeLock locker(m_tasks_mtx);
                m_wait_props.remove_if([prop](const TPropType& value)->bool {return (value == prop); });
                m_wait_tasks.erase(prop);
            }
            m_cv_not_full.notify_one();
        }
        void remove_prop(TPropType&& prop) {
            {
                writeLock locker(m_tasks_mtx);
                m_wait_props.remove_if([prop](TPropType&& value)->bool {return (value == prop); });
                m_wait_tasks.erase(std::forward<TPropType>(prop));
            }
            m_cv_not_full.notify_one();
        }

        // 移除一个顶层非当前执行属性任务,队列为空时存在阻塞
        void pop_task() {
            wait_for_can_pop();

            if (m_bstop.load())
                return;

            PropTaskPtr pop_task_ptr(nullptr);

            {
                writeLock locker(m_tasks_mtx);
                // 是否已无可pop队列
                if (m_wait_props.empty())
                    return;

                auto pop_type_iter = m_wait_props.begin();
                while (pop_type_iter != m_wait_props.end()) {
                    assert(m_wait_tasks.find(*pop_type_iter) != m_wait_tasks.end());
                    // 获取任务指针
                    pop_task_ptr = m_wait_tasks[*pop_type_iter];
                    m_wait_tasks.erase(*pop_type_iter);
                    m_wait_props.erase(pop_type_iter);
                    break;
                }
            }
            m_cv_not_full.notify_one();

            if (pop_task_ptr) {
                pop_task_ptr->invoke();
            }
        }

        void clear() {
            {
                writeLock locker(m_tasks_mtx);
                m_wait_tasks.clear();
                m_wait_props.clear();
            }
            m_cv_not_full.notify_all();
        }

        void stop() {
            // 是否已终止判断
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true))
                return;

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        bool empty() const {
            return !not_empty();
        }

        bool full() const {
            return !not_full();
        }

        size_t size() const {
            readLock locker(m_tasks_mtx);
            return m_wait_props.size();
        }

    private:
        // 是否处于未满状态
        bool not_full() const {
            readLock lock(m_tasks_mtx);
            return m_max_task_count == 0 || m_wait_props.size() < m_max_task_count;
        }

        // 是否处于非空状态
        bool not_empty() const {
            readLock lock(m_tasks_mtx);
            return !m_wait_props.empty();
        }

    private:
        // 等待直到可移除任务至队列
        void wait_for_can_pop() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
        }
        // 等待直到可加入新的任务至队列
        void wait_for_can_add() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });
        }

        // 新增任务至队列
        bool add_task_tolist(PropTaskPtr&& new_task_item)
        {
            if (!new_task_item)
                return false;

            {
                writeLock locker(m_tasks_mtx);
                auto& prop_type = new_task_item->get_prop_type();
                auto iter = m_wait_tasks.find(prop_type);
                if (iter == m_wait_tasks.end())
                    m_wait_props.push_back(prop_type);
                m_wait_tasks[prop_type] = std::forward<PropTaskPtr>(new_task_item);
            }

            m_cv_not_empty.notify_one();
            return true;
        }

    private:
        // 是否已终止标识符
        std::atomic<bool>               m_bstop;

        // 任务队列锁
        mutable rwMutex                 m_tasks_mtx;
        // 总待执行任务属性顺序队列,用于判断执行队列顺序
        std::list<TPropType>             m_wait_props;
        // 总待执行任务队列属性及其对应任务,其个数必须始终与m_wait_tasks个数同步
        std::map<TPropType, PropTaskPtr> m_wait_tasks;
        // 最大任务个数,当为0时表示无限制
        size_t                           m_max_task_count;

        // 条件阻塞锁, 使用std::unique_lock进行锁定,std::condition_variable会在判定结束后释放该锁
        std::mutex                      m_cv_mtx;
        // 不为空的条件变量
        std::condition_variable         m_cv_not_empty;
        // 没有满的条件变量
        std::condition_variable         m_cv_not_full;
    };


    /*************************************************
    Description:提供按属性划分的,保留所有任务的FIFO任务队列,将调用函数转为元祖对象存储
                当某一属性正在队列中时,同属性的其他任务新增时,会追加至原任务之后执行
                当某一任务正在执行时,同属性其他任务将不被执行,同一属性之间的任务均按照FIFO串行执行完毕
    *************************************************/
    template<typename TPropType>
    class SerialTupleTaskQueue
    {
        typedef std::shared_ptr<PropTaskVirtual<TPropType>> PropTaskPtr;

        // 任务属性及对应连续个数结构体
        struct PropCountItem {
            TPropType    prop_;
            size_t       count_;

            PropCountItem(const TPropType& prop) : prop_(prop), count_(1) {}
            PropCountItem(TPropType&& prop) : prop_(std::forward<TPropType>(prop)), count_(1) {}
            inline size_t add() { return ++count_; }
            inline size_t reduce() { return --count_; }
            inline const TPropType& get_prop_type() const { return prop_; }
        };

    public:
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        SerialTupleTaskQueue(size_t max_task_count = 0)
            : m_max_task_count(max_task_count)
            , m_wait_task_count(0)
            , m_bstop(false)
        {
        }

        ~SerialTupleTaskQueue() {
            clear();
            stop();
        }

        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        template<typename TFunction, typename... Args>
        bool add_task(const TPropType& prop, TFunction&& func, Args&&... args)
        {
            wait_for_can_add();

            if (m_bstop.load())
                return false;

            // 此处TTuple不可采用std::forward_as_tuple(std::forward<Args>(args)...)
            // 假使agrs中含有const & 时,会导致tuple中存储的亦为const &对象,从而外部释放对象后导致内部对象无效
            // 采用std::make_shared<TTuple>则会导致存在一次拷贝,由std::make_tuple引起(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(prop, std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        template<typename TFunction, typename... Args>
        bool add_task(TPropType&& prop, TFunction&& func, Args&&... args)
        {
            wait_for_can_add();

            if (m_bstop.load())
                return false;

            // 此处TTuple不可采用std::forward_as_tuple(std::forward<Args>(args)...)
            // 假使agrs中含有const & 时,会导致tuple中存储的亦为const &对象,从而外部释放对象后导致内部对象无效
            // 采用std::make_shared<TTuple>则会导致存在一次拷贝,由std::make_tuple引起(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(std::forward<TPropType>(prop), std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // 移除所有指定属性任务,当前正在执行除外,可能存在阻塞
        void remove_prop(const TPropType& prop)
        {
            {
                writeLock locker(m_tasks_mtx);
                auto iter = m_wait_tasks.find(prop);
                if (iter != m_wait_tasks.end())
                {
                    m_wait_tasks.erase(prop);
                }
                for (auto iter = m_prop_counts.begin(); iter != m_prop_counts.end();)
                {
                    if (iter->prop_ == prop) {
                        m_wait_task_count -= iter->count_;
                        m_prop_counts.erase(iter++);
                    }
                    else {
                        iter++;
                    }
                }
                remove_cur_prop(prop);
            }

            m_cv_not_full.notify_one();
        }
        void remove_prop(TPropType&& prop)
        {
            {
                writeLock locker(m_tasks_mtx);
                auto iter = m_wait_tasks.find(prop);
                if (iter != m_wait_tasks.end())
                {
                    m_wait_tasks.erase(prop);
                }
                for (auto iter = m_prop_counts.begin(); iter != m_prop_counts.end();)
                {
                    if (iter->prop_ == prop) {
                        m_wait_task_count -= iter->count_;
                        m_prop_counts.erase(iter++);
                    }
                    else {
                        iter++;
                    }
                }
                remove_cur_prop(std::forward<TPropType>(prop));
            }

            m_cv_not_full.notify_one();
        }

        // 移除一个顶层非当前执行属性任务,队列为空时存在阻塞
        void pop_task()
        {
            wait_for_can_pop();

            if (m_bstop.load())
                return;

            TaskPtr next_task(nullptr);
            TPropType prop_type;

            {
                writeLock locker(m_tasks_mtx);
                auto prop_count_iter = m_prop_counts.begin();
                while (prop_count_iter != m_prop_counts.end())
                {
                    prop_type = prop_count_iter->get_prop_type();
                    if (!add_cur_prop(prop_type)) {
                        prop_count_iter++;
                        continue;
                    }
                    auto wait_task_iter = m_wait_tasks.find(prop_type);
                    if (wait_task_iter != m_wait_tasks.end())
                    {
                        next_task = wait_task_iter->second.front();
                        wait_task_iter->second.pop_front();
                        m_wait_task_count--;
                        if (wait_task_iter->second.empty())
                            m_wait_tasks.erase(wait_task_iter);

                        if (prop_count_iter->reduce() == 0)
                            m_prop_counts.erase(prop_count_iter);
                    }
                    break;
                }
            }

            if (next_task) {
                next_task->invoke();
                remove_cur_prop(std::move(prop_type));
                m_cv_not_full.notify_one();
            }
        }

        void stop() {
            // 是否已终止判断
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true))
                return;

            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        bool empty() const {
            readLock locker(m_tasks_mtx);
            return m_wait_tasks.empty();
        }

        bool full() const {
            return !not_full();
        }

        size_t size() const {
            readLock locker(m_tasks_mtx);
            return m_wait_tasks.size();
        }

        void clear() {
            {
                writeLock locker(m_tasks_mtx);
                m_wait_task_count = 0;
                m_wait_tasks.clear();
                m_prop_counts.clear();
            }
            {
                std::lock_guard<std::mutex> lck(m_props_mtx);
                m_cur_props.clear();
            }

            m_cv_not_full.notify_all();
        }

    private:
        // 等待直到可加入新的任务至队列
        void wait_for_can_add() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });
        }
        // 等待直到可pop新的任务
        void wait_for_can_pop() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
        }

        // 新增任务至队列
        bool add_task_tolist(PropTaskPtr&& new_task_item)
        {
            if (!new_task_item)
                return false;

            const TPropType& prop = new_task_item->get_prop_type();
            {
                writeLock locker(m_tasks_mtx);
                if (!m_prop_counts.empty())
                {
                    auto& prop_count_ref = m_prop_counts.back();
                    if (prop_count_ref.get_prop_type() == prop)
                        prop_count_ref.add();
                    else
                        m_prop_counts.push_back(PropCountItem(prop));
                }
                else
                    m_prop_counts.push_back(PropCountItem(prop));
                m_wait_tasks[prop].push_back(std::forward<PropTaskPtr>(new_task_item));
                m_wait_task_count++;
            }

            m_cv_not_empty.notify_one();
            return true;
        }

        // 新增当前运行属性
        // 已存在返回false，正常插入返回true
        bool add_cur_prop(const TPropType& prop_type)
        {
            std::lock_guard<std::mutex> locker(m_props_mtx);
            if (m_cur_props.find(prop_type) != m_cur_props.end())
                return false;

            m_cur_props.insert(prop_type);
            return true;
        }

        // 删除当前运行属性
        void remove_cur_prop(const TPropType& prop_type)
        {
            std::lock_guard<std::mutex> locker(m_props_mtx);

            auto prop_iter = m_cur_props.find(prop_type);
            if (prop_iter != m_cur_props.end())
                m_cur_props.erase(prop_iter);
        }
        void remove_cur_prop(TPropType&& prop_type)
        {
            std::lock_guard<std::mutex> locker(m_props_mtx);

            auto prop_iter = m_cur_props.find(std::forward<TPropType>(prop_type));
            if (prop_iter != m_cur_props.end())
                m_cur_props.erase(prop_iter);
        }

        // 是否处于未满状态
        bool not_full() const {
            readLock lock(m_tasks_mtx);
            return m_max_task_count == 0 || m_wait_tasks.size() < m_max_task_count;
        }

        // 是否处于空状态
        bool not_empty() const {
            readLock lock(m_tasks_mtx);
            std::lock_guard<std::mutex> locker(m_props_mtx);
            return !m_wait_tasks.empty() && m_cur_props.size() < m_wait_tasks.size();
        }

    private:
        // 是否已终止标识符
        std::atomic<bool>                           m_bstop;
        // 最大任务个数,当为0时表示无限制
        size_t                                      m_max_task_count;

        // 任务队列锁
        mutable rwMutex                             m_tasks_mtx;
        // 总待执行任务队列总个数
        size_t                                      m_wait_task_count;
        // 总待执行任务队列,包含所有的待执行任务
        std::map<TPropType, std::list<PropTaskPtr>> m_wait_tasks;
        // 当前按顺序插入的,待执行的 任务属性及其连续任务个数
        // 当连续数为0时,删除该PropCountItemPtr
        std::list<PropCountItem>                    m_prop_counts;

        // 条件阻塞锁
        std::mutex                                  m_cv_mtx;
        // 不为空的条件变量
        std::condition_variable                     m_cv_not_empty;
        // 没有满的条件变量
        std::condition_variable                     m_cv_not_full;

        // 当前执行任务属性队列锁
        mutable std::mutex                          m_props_mtx;
        // 当前正在执行中的任务属性
        std::set<TPropType>                          m_cur_props;
    };


}