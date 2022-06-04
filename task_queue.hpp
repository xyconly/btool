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

namespace BTool
{
    /*************************************************
    Description:提供基于函数的FIFO任务队列
    *************************************************/
    class TaskQueue {
    public:
        typedef std::function<void()> TaskItem;
    public:
        TaskQueue(size_t max_task_count = 0)
            : m_bstop(false)
            , m_max_task_count(max_task_count)
        {}

        virtual ~TaskQueue() {
            stop();
        }

        bool empty() const {
            std::unique_lock<std::mutex> locker(m_mtx);
            return !not_empty();
        }

        void clear() {
            std::unique_lock<std::mutex> locker(m_mtx);
            std::queue<TaskItem> empty;
            m_queue.swap(empty);
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void start() {
            // 复位已终止标志符
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            std::unique_lock<std::mutex> locker(m_mtx);
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            // 是否已终止判断
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }

            if (bwait) {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_full.notify_all();
                m_cv_not_empty.notify_all();
                return;
            }
            clear();
        }

        template<typename AsTFunction>
        bool add_task(AsTFunction&& func) {
            std::unique_lock<std::mutex> locker(m_mtx);
            m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });

            if (m_bstop.load())
                return false;

            m_queue.push(std::forward<AsTFunction>(func));
            m_cv_not_empty.notify_one();
            return true;
        }

        void pop_task() {
            TaskItem pop_task(nullptr);
            {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });

                if (m_bstop.load() && !not_empty())
                    return;

                pop_task = std::move(m_queue.front());
                m_queue.pop();
                // queue不会主动释放已开辟空间,当任务清空时则释放一下,若实际环境中,可自己定义为list来避免queue长期占用的问题,但这会导致性能的略微下降,根据实际情况决定
                if (m_queue.empty()) {
                    std::queue<TaskItem> empty;
                    m_queue.swap(empty);
                }
                m_cv_not_full.notify_one();
            }

            if (pop_task) {
                pop_task();
            }
        }

        bool full() const {
            std::unique_lock<std::mutex> locker(m_mtx);
            return !not_full();
        }

        size_t size() const {
            std::unique_lock<std::mutex> locker(m_mtx);
            return m_queue.size();
        }

    protected:
        // 是否处于未满状态
        bool not_full() const {
            return m_max_task_count == 0 || m_queue.size() < m_max_task_count;
        }

        // 是否处于空状态
        bool not_empty() const {
            return !m_queue.empty();
        }

    protected:
        // 是否已终止标识符
        std::atomic<bool>           m_bstop;
        // 数据安全锁
        mutable std::mutex          m_mtx;

        // 总待执行任务队列,包含所有的待执行任务
        std::queue<TaskItem>       m_queue;
        // 最大任务个数,当为0时表示无限制
        size_t                      m_max_task_count;

        // 不为空的条件变量
        std::condition_variable     m_cv_not_empty;
        // 没有满的条件变量
        std::condition_variable     m_cv_not_full;
    };

    /*************************************************
    Description:提供按属性划分的,仅保留最新状态的FIFO任务队列
                当某一属性正在队列中时,同属性的其他任务新增时,原任务会被覆盖
    *************************************************/
    template<typename TPropType>
    class LastTaskQueue
    {
    public:
        typedef std::function<void()> TaskItem;
    public:
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        LastTaskQueue(size_t max_task_count = 0)
            : m_bstop(false)
            , m_max_task_count(max_task_count)
        {}
        virtual ~LastTaskQueue() {
            stop();
        }

        bool empty() const {
            std::unique_lock<std::mutex> locker(m_mtx);
            return !not_empty();
        }

        void clear() {
            std::unique_lock<std::mutex> locker(m_mtx);
            m_wait_tasks.clear();
            m_wait_props.clear();
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void start() {
            // 复位已终止标志符
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            std::unique_lock<std::mutex> locker(m_mtx);
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            // 是否已终止判断
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }
            if (bwait) {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_full.notify_all();
                m_cv_not_empty.notify_all();
                return;
            }
            clear();
        }

        template<typename AsTPropType, typename AsTFunction>
        bool add_task(AsTPropType&& prop, AsTFunction&& func) {
            std::unique_lock<std::mutex> locker(m_mtx);
            m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });

            if (m_bstop.load())
                return false;

            auto iter = m_wait_tasks.find(prop);
            if (iter == m_wait_tasks.end())
                m_wait_props.push_back(prop);
            m_wait_tasks[std::forward<AsTPropType>(prop)] = std::forward<AsTFunction>(func);
            m_cv_not_empty.notify_one();
            return true;
        }

        void pop_task() {
            TaskItem pop_task(nullptr);
            TPropType pop_type;

            {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });

                if (m_bstop.load() && !not_empty())
                    return;

                // 是否已无可pop队列
                if (m_wait_props.empty())
                    return;

                for (auto pop_type_iter = m_wait_props.begin(); pop_type_iter != m_wait_props.end(); pop_type_iter++) {
                    if (m_cur_pop_props.find(*pop_type_iter) != m_cur_pop_props.end())
                        continue;

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

                std::unique_lock<std::mutex> locker(m_mtx);
                m_cur_pop_props.erase(pop_type);
                m_cv_not_full.notify_one();
            }
        }

        // 移除所有指定属性任务,当前正在执行除外,可能存在阻塞
        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            std::unique_lock<std::mutex> locker(m_mtx);
            m_wait_props.remove_if([prop](const TPropType& value)->bool {return (value == prop); });
            m_wait_tasks.erase(std::forward<AsTPropType>(prop));
            m_cv_not_full.notify_one();
        }

        bool full() const {
            std::unique_lock<std::mutex> locker(m_mtx);
            return !not_full();
        }

        size_t size() const {
            std::unique_lock<std::mutex> locker(m_mtx);
            return m_wait_props.size();
        }

    protected:
        // 是否处于未满状态
        bool not_full() const {
            return m_max_task_count == 0 || m_wait_props.size() < m_max_task_count;
        }

        // 是否处于非空状态
        bool not_empty() const {
            return !m_wait_props.empty();
        }

    protected:
        // 是否已终止标识符
        std::atomic<bool>                m_bstop;

        // 数据安全锁
        mutable std::mutex               m_mtx;
        // 总待执行任务属性顺序队列,用于判断执行队列顺序
        std::list<TPropType>             m_wait_props;
        // 总待执行任务队列属性及其对应任务,其个数必须始终与m_wait_tasks个数同步
        std::map<TPropType, TaskItem>   m_wait_tasks;
        // 当前正在pop任务属性
        std::set<TPropType>              m_cur_pop_props;
        // 最大任务个数,当为0时表示无限制
        size_t                           m_max_task_count;

        // 不为空的条件变量
        std::condition_variable          m_cv_not_empty;
        // 没有满的条件变量
        std::condition_variable          m_cv_not_full;
    };

    /*************************************************
    Description:提供按属性划分的,保留所有任务的FIFO任务队列,将调用函数转为元祖对象存储
                当某一属性正在队列中时,同属性的其他任务新增时,会追加至原任务之后执行
                当某一任务正在执行时,同属性其他任务将不被执行,同一属性之间的任务均按照FIFO串行执行完毕
    *************************************************/
    template<typename TPropType>
    class SerialTaskQueue
    {
    public:
        typedef std::function<void()> TaskItem;

    protected:
        // 连续属性双向链表,用于存储同一属性上下位置,及FIFO顺序
        // 非线程安全
        class PropCountNodeList
        {
            // 连续任务结构体
            struct PropCountNode {
                bool         can_pop_;      // 当前节点是否可被pop, 每次复位后/新增后首链表会被复位为true
                size_t       count_;        // 当前连续新增同属性任务个数,如连续新增300个同属性,在队列中只创建一个PropCountNode,计数为300

                PropCountNode*  pre_same_prop_node_;  // 同属性上一连续任务指针
                PropCountNode*  next_same_prop_node_; // 同属性下一连续任务指针
                PropCountNode*  pre_list_prop_node_;  // 队列的上一连续任务指针
                PropCountNode*  next_list_prop_node_; // 队列的下一连续任务指针

                TPropType    prop_;

                template<typename AsTPropType>
                PropCountNode(AsTPropType&& prop, PropCountNode* pre_same_prop_node, PropCountNode* pre_list_prop_node, bool can_immediately_pop)
                    : can_pop_(can_immediately_pop)
                    , count_(1)
                    , pre_same_prop_node_(pre_same_prop_node)
                    , next_same_prop_node_(nullptr)
                    , pre_list_prop_node_(pre_list_prop_node)
                    , next_list_prop_node_(nullptr)
                    , prop_(std::forward<AsTPropType>(prop))
                {
                    if (pre_same_prop_node)
                        pre_same_prop_node->next_same_prop_node_ = this;
                    if (pre_list_prop_node)
                        pre_list_prop_node->next_list_prop_node_ = this;
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

            template<typename AsTPropType>
            void push_back(AsTPropType&& prop, bool can_immediately_pop) {
                // 是否已存在节点
                if (!m_end_node) {
                    m_begin_node = m_end_node = new PropCountNode(std::forward<AsTPropType>(prop), nullptr, nullptr, can_immediately_pop);
                    m_all_nodes[m_end_node->get_prop_type()].emplace_back(m_end_node);
                    return;
                }

                //最后一个节点是否相同属性
                if (m_end_node->get_prop_type() == prop) {
                    m_end_node->add();
                    return;
                }

                // 该属性是否不存在其他任务
                auto all_nodes_iter = m_all_nodes.find(prop);
                if (all_nodes_iter == m_all_nodes.end())
                {
                    PropCountNode* new_node = new PropCountNode(std::forward<AsTPropType>(prop), nullptr, m_end_node, can_immediately_pop);
                    m_end_node = new_node;
                    m_all_nodes[new_node->get_prop_type()].emplace_back(new_node);
                    return;
                }

                PropCountNode* pre_same_prop_node = all_nodes_iter->second.back();
                PropCountNode* new_node = new PropCountNode(std::forward<AsTPropType>(prop), pre_same_prop_node, m_end_node, false);
                m_end_node = new_node;
                m_all_nodes[new_node->get_prop_type()].emplace_back(new_node);
            }

            // 重置某个属性的任务
            void reset_prop(const TPropType& prop_type) {
                auto all_nodes_iter = m_all_nodes.find(prop_type);
                if (all_nodes_iter == m_all_nodes.end())
                    return;

                all_nodes_iter->second.front()->reset_can_pop(true);
            }

            // 去除首个指定属性集合的首个节点,无该节点时返回false
            TPropType pop_front() {
                auto pop_front_node = m_begin_node;
                while (pop_front_node) {
                    if (pop_front_node->can_pop())
                        break;
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
                }
                else {
                    m_all_nodes.erase(pop_front_node->get_prop_type());
                }

                // 获取下一节点,对下一节点的 上一节点指针  赋值为 原本指针的 上一节点指针
                // 并将上一节点的下一节点指针  赋值为 本指针的  下一节点指针
                auto pre_list_prop_node = pop_front_node->get_pre_list_prop_node();
                auto next_list_prop_node = pop_front_node->get_next_list_prop_node();
                if (next_list_prop_node)
                    next_list_prop_node->reset_pre_list_prop_node(pre_list_prop_node);
                if (pre_list_prop_node)
                    pre_list_prop_node->reset_next_list_prop_node(next_list_prop_node);

                if (pop_front_node == m_begin_node)
                    m_begin_node = next_list_prop_node;
                if (pop_front_node == m_end_node)
                    m_end_node = pre_list_prop_node;

                auto prop_type = pop_front_node->get_prop_type();
                delete pop_front_node;
                return prop_type;
            }

            template<typename AsTPropType>
            void remove_prop(AsTPropType&& prop) {
                auto all_node_iter = m_all_nodes.find(prop);
                if (all_node_iter == m_all_nodes.end())
                    return;

                bool need_comp_begin(false); // 是否需要比对begin节点
                if (m_begin_node && m_begin_node->get_prop_type() == prop)
                    need_comp_begin = true;
                bool need_comp_end(false); // 是否需要比对end节点
                if (m_end_node && m_end_node->get_prop_type() == prop)
                    need_comp_end = true;

                for (auto& item : all_node_iter->second) {
                    // 修改自身上一节点指针的  下一节点为 当前的下一节点
                    // 反之同理
                    auto pre_list_prop_node = item->get_pre_list_prop_node();
                    auto next_list_prop_node = item->get_next_list_prop_node();
                    if (pre_list_prop_node)
                        pre_list_prop_node->reset_next_list_prop_node(next_list_prop_node);
                    if (next_list_prop_node)
                        next_list_prop_node->reset_pre_list_prop_node(pre_list_prop_node);

                    // 判断并重置begin节点
                    if (need_comp_begin && item == m_begin_node) {
                        m_begin_node = next_list_prop_node;
                        if (m_begin_node)
                            m_begin_node->reset_pre_list_prop_node(nullptr);
                        if (!m_begin_node || m_begin_node->get_prop_type() != prop)
                            need_comp_begin = false;
                    }
                    // 判断并重置end节点
                    if (need_comp_end && item == m_end_node) {
                        m_end_node = pre_list_prop_node;
                        if (m_end_node)
                            m_end_node->reset_next_list_prop_node(nullptr);
                        if (!m_end_node || m_end_node->get_prop_type() != prop)
                            need_comp_end = false;
                    }

                    // 删除该节点
                    delete item;
                }
                m_all_nodes.erase(all_node_iter);
            }

            void clear() {
                PropCountNode* node_ptr(m_begin_node);
                while (node_ptr) {
                    auto tmp = node_ptr->get_next_list_prop_node();
                    delete node_ptr;
                    node_ptr = tmp;
                }
                m_begin_node = nullptr;
                m_end_node = nullptr;
                m_all_nodes.clear();
            }

        private:
            PropCountNode*                                  m_begin_node;   // 队列起始节点
            PropCountNode*                                  m_end_node;     // 队列结束节点
            std::map<TPropType, std::list<PropCountNode*>>  m_all_nodes;    // 所有队列节点
        };

    public:
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        SerialTaskQueue(size_t max_task_count = 0)
            : m_bstop(false)
            , m_max_task_count(max_task_count)
        {}

        virtual ~SerialTaskQueue() {
            stop();
        }

        bool empty() const {
            std::unique_lock<std::mutex> locker(m_mtx);
            return m_wait_tasks.empty();
        }

        void clear() {
            std::unique_lock<std::mutex> locker(m_mtx);
            m_wait_props.clear();
            m_wait_tasks.clear();
            m_cur_props.clear();
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void start() {
            // 复位已终止标志符
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }
            std::unique_lock<std::mutex> locker(m_mtx);
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            // 是否已终止判断
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }
            if (bwait) {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_full.notify_all();
                m_cv_not_empty.notify_all();
                return;
            }
            clear();
        }

        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        template<typename AsTPropType, typename AsTFunction>
        bool add_task(AsTPropType&& prop, AsTFunction&& func) {
            std::unique_lock<std::mutex> locker(m_mtx);
            m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });

            if (m_bstop.load())
                return false;

            m_wait_props.push_back(prop, m_cur_props.find(prop) == m_cur_props.end() && m_wait_tasks.find(prop) == m_wait_tasks.end());
            m_wait_tasks[std::forward<AsTPropType>(prop)].push_back(std::forward<AsTFunction>(func));
            m_cv_not_empty.notify_one();
            return true;
        }

        void pop_task() {
            TaskItem next_task(nullptr);
            TPropType prop_type;

            {
                std::unique_lock<std::mutex> locker(m_mtx);
                m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });

                if (m_bstop.load() && !not_empty())
                    return;

                prop_type = m_wait_props.pop_front();
                assert(m_cur_props.find(prop_type) == m_cur_props.end());
                m_cur_props.emplace(prop_type);
                auto wait_task_iter = m_wait_tasks.find(prop_type);
                if (wait_task_iter != m_wait_tasks.end()) {
                    next_task = wait_task_iter->second.front();
                }
            }

            if (next_task) {
                next_task();
                std::unique_lock<std::mutex> locker(m_mtx);
                auto wait_task_iter = m_wait_tasks.find(prop_type);
                if (wait_task_iter != m_wait_tasks.end()) {
                    wait_task_iter->second.pop_front();
                    if (wait_task_iter->second.empty())
                        m_wait_tasks.erase(wait_task_iter);
                }
                remove_cur_prop(prop_type);
                m_wait_props.reset_prop(prop_type);
                m_cv_not_full.notify_one();
            }
        }

        // 移除所有指定属性任务,当前正在执行除外,可能存在阻塞
        // 存在遍历,可能比较耗时
        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            std::unique_lock<std::mutex> locker(m_mtx);
            auto iter = m_wait_tasks.find(prop);
            if (iter != m_wait_tasks.end())
                m_wait_tasks.erase(prop);
            m_wait_props.remove_prop(prop);
//             remove_cur_prop(std::forward<AsTPropType>(prop));
            m_cv_not_full.notify_one();
        }

        bool full() const {
            std::unique_lock<std::mutex> locker(m_mtx);
            return !not_full();
        }

        size_t size() const {
            std::unique_lock<std::mutex> locker(m_mtx);
            return m_wait_tasks.size();
        }

    protected:
        // 删除当前运行属性
        template<typename AsTPropType>
        inline void remove_cur_prop(AsTPropType&& prop_type) {
            auto prop_iter = m_cur_props.find(std::forward<AsTPropType>(prop_type));
            if (prop_iter != m_cur_props.end())
                m_cur_props.erase(prop_iter);
        }

        // 是否处于未满状态
        inline bool not_full() const {
            return m_max_task_count == 0 || m_wait_tasks.size() < m_max_task_count;
        }

        // 是否处于空状态
        inline bool not_empty() const {
            return !m_wait_tasks.empty() && m_cur_props.size() < m_wait_tasks.size();
        }

    protected:
        // 是否已终止标识符
        std::atomic<bool>                           m_bstop;

        // 数据安全锁
        mutable std::mutex                          m_mtx;
        // 总待执行任务属性顺序队列,用于判断执行队列顺序
        PropCountNodeList                           m_wait_props;
        // 总待执行任务队列属性及其对应任务
        std::map<TPropType, std::list<TaskItem>>    m_wait_tasks;
        // 最大任务个数,当为0时表示无限制
        size_t                                      m_max_task_count;

        // 不为空的条件变量
        std::condition_variable                     m_cv_not_empty;
        // 没有满的条件变量
        std::condition_variable                     m_cv_not_full;

        // 当前正在执行中的任务属性
        std::set<TPropType>                         m_cur_props;

    };
}