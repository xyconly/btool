/*************************************************
File name:  task_queue.hpp
Author:     AChar
Version:
Date:
Description:    提供各类任务队列,避免外界重复创建

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
            clear();
            stop();
        }

        template<typename TFunction, typename... Args>
        bool add_task(TFunction&& func, Args&&... args) {
            wait_for_can_add();

            if (m_bstop.load())
                return false;

            // 此处TTuple不可采用std::forward_as_tuple(std::forward<Args>(args)...)
            // 假使agrs中含有const & 时,会导致tuple中存储的亦为const &对象,从而外部释放对象后导致内部对象无效
            // 采用std::make_shared<TTuple>则会导致存在一次拷贝,由std::make_tuple引起(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<TupleTask<TFunction, TTuple>>(std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
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
            {
                std::unique_lock<std::mutex> locker(m_cv_mtx);
                m_cv_not_full.notify_one();
            }

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
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void clear() {
            {
                writeLock locker(m_tasks_mtx);
                std::queue<TaskPtr> empty;
                m_queue.swap(empty);
            }
            std::unique_lock<std::mutex> locker(m_cv_mtx);
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

            std::unique_lock<std::mutex> locker(m_cv_mtx);
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

        template<typename AsTPropType, typename TFunction, typename... Args>
        bool add_task(AsTPropType&& prop, TFunction&& func, Args&&... args) {
            wait_for_can_add();
            if (m_bstop.load())
                return false;

            // 此处TTuple不可采用std::forward_as_tuple(std::forward<Args>(args)...)
            // 假使agrs中含有const & 时,会导致tuple中存储的亦为const &对象,从而外部释放对象后导致内部对象无效
            // 采用std::make_shared<TTuple>则会导致存在一次拷贝,由std::make_tuple引起(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(std::forward<AsTPropType>(prop), std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // 移除所有指定属性任务,当前正在执行除外,可能存在阻塞
        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            {
                writeLock locker(m_tasks_mtx);
                m_wait_props.remove_if([prop](const TPropType& value)->bool {return (value == prop); });
                m_wait_tasks.erase(std::forward<AsTPropType>(prop));
            }
            {
                std::unique_lock<std::mutex> locker(m_cv_mtx);
                m_cv_not_full.notify_one();
            }
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
                    // 获取任务指针
                    pop_task_ptr = m_wait_tasks[*pop_type_iter];
                    m_wait_tasks.erase(*pop_type_iter);
                    m_wait_props.erase(pop_type_iter);
                    break;
                }
            }
            {
                std::unique_lock<std::mutex> locker(m_cv_mtx);
                m_cv_not_full.notify_one();
            }

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
            {
                std::unique_lock<std::mutex> locker(m_cv_mtx);
                m_cv_not_full.notify_all();
            }
        }

        void stop() {
            // 是否已终止判断
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }
            std::unique_lock<std::mutex> locker(m_cv_mtx);
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

            std::unique_lock<std::mutex> locker(m_cv_mtx);
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

        // 连续属性双向链表,用于存储同一属性上下位置,及FIFO顺序
        // 非线程安全你
        class PropCountNodeList
        {
            // 连续任务结构体
            struct PropCountNode {
                TPropType    prop_;
                size_t       count_;

                PropCountNode*  pre_same_prop_node_;  // 同属性上一连续任务指针
                PropCountNode*  next_same_prop_node_; // 同属性下一连续任务指针
                PropCountNode*  pre_list_prop_node_;  // 队列的上一连续任务指针
                PropCountNode*  next_list_prop_node_; // 队列的下一连续任务指针

                template<typename AsTPropType>
                PropCountNode(AsTPropType&& prop, PropCountNode* pre_same_prop_node, PropCountNode* pre_list_prop_node)
                    : prop_(std::forward<AsTPropType>(prop))
                    , pre_same_prop_node_(pre_same_prop_node)
                    , next_same_prop_node_(nullptr)
                    , pre_list_prop_node_(pre_list_prop_node)
                    , next_list_prop_node_(nullptr)
                    , count_(1)
                {
                    if (pre_same_prop_node)
                        pre_same_prop_node->next_same_prop_node_ = this;
                    if (pre_list_prop_node)
                        pre_list_prop_node->next_list_prop_node_ = this;
                }
                size_t add() { return ++count_; }
                size_t reduce() { return --count_; }
                size_t count() const { return count_; }
                PropCountNode* get_pre_same_prop_node() const { return pre_same_prop_node_; }
                PropCountNode* get_next_same_prop_node() const { return next_same_prop_node_; }
                PropCountNode* get_pre_list_prop_node() const { return pre_list_prop_node_; }
                PropCountNode* get_next_list_prop_node() const { return next_list_prop_node_; }

                void reset_pre_same_prop_node(PropCountNode* pre_same_prop_node) { pre_same_prop_node_ = pre_same_prop_node; }
                void reset_next_same_prop_node(PropCountNode* next_same_prop_node) { next_same_prop_node_ = next_same_prop_node; }
                void reset_pre_list_prop_node(PropCountNode* pre_list_prop_node) { pre_list_prop_node_ = pre_list_prop_node; }
                void reset_next_list_prop_node(PropCountNode* next_list_prop_node) { next_list_prop_node_ = next_list_prop_node; }

                const TPropType& get_prop_type() const { return prop_; }
            };
        public:
            PropCountNodeList() : m_begin_node(nullptr), m_end_node(nullptr) {}
            ~PropCountNodeList() { clear(); }

            template<typename AsTPropType>
            void push_back(AsTPropType&& prop) {
                // 是否已存在节点
                if (!m_end_node) {
                    m_begin_node = m_end_node = new PropCountNode(std::forward<AsTPropType>(prop), nullptr, nullptr);
                    m_all_nodes[m_end_node->get_prop_type()].emplace_back(m_end_node);
                    m_front_node[m_end_node->get_prop_type()] = m_end_node;
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
                    PropCountNode* new_node = new PropCountNode(std::forward<AsTPropType>(prop), nullptr, m_end_node);
                    m_end_node = new_node;
                    m_all_nodes[new_node->get_prop_type()].emplace_back(new_node);
                    m_front_node[new_node->get_prop_type()] = new_node;
                    return;
                }

                PropCountNode* pre_same_prop_node = all_nodes_iter->second.back();
                PropCountNode* new_node = new PropCountNode(std::forward<AsTPropType>(prop), pre_same_prop_node, m_end_node);
                m_end_node = new_node;
                m_all_nodes[new_node->get_prop_type()].emplace_back(new_node);
            }

            // 去除首个非指定属性集合的首个节点,无该节点时返回false
            std::tuple<bool, TPropType> pop_front(const std::set<TPropType>& except_props) {
                if (!m_begin_node)
                    return std::forward_as_tuple(false, TPropType());

                if (except_props.find(m_begin_node->get_prop_type()) == except_props.end())
                    return std::forward_as_tuple(true, pop_front());

                auto tmp_node = m_begin_node;
                while (tmp_node = tmp_node->get_next_list_prop_node()) {
                    if (except_props.find(tmp_node->get_prop_type()) == except_props.end())
                        return std::forward_as_tuple(true, pop_point_prop_front(tmp_node->get_prop_type()));
                }
                return std::forward_as_tuple(false, TPropType());
            }

            // pop队列首节点,无该节点时异常
            TPropType pop_front() {
                auto prop_type = m_begin_node->get_prop_type();
                m_begin_node->reduce();
                if (m_begin_node->count() > 0)
                    return prop_type;

                // 起始节点==终止节点
                if (m_begin_node == m_end_node) {
                    delete m_begin_node;
                    m_begin_node = nullptr;
                    m_end_node = nullptr;
                    m_all_nodes.clear();
                    m_front_node.clear();
                    return prop_type;
                }
                // pop队列第一个,若该属性队列为空则删除该属性及首节点
                m_all_nodes[prop_type].pop_front();
                if (m_all_nodes[prop_type].empty()) {
                    m_all_nodes.erase(prop_type);
                    m_front_node.erase(prop_type);
                }
                else {
                    auto next_same_prop_node = m_begin_node->get_next_same_prop_node();
                    if (next_same_prop_node)
                    {
                        next_same_prop_node->reset_pre_same_prop_node(nullptr);
                        m_front_node[prop_type] = next_same_prop_node;
                    }
                    else
                        m_front_node.erase(prop_type);
                }
                auto next_list_prop_node = m_begin_node->get_next_list_prop_node();
                next_list_prop_node->reset_pre_list_prop_node(nullptr);
                delete m_begin_node;
                m_begin_node = next_list_prop_node;
                return prop_type;
            }

            // pop指定属性下的首节点,无该节点时异常
            TPropType pop_point_prop_front(TPropType prop_type) {
                auto pop_front_node = m_front_node[prop_type];
                pop_front_node->reduce();
                if (pop_front_node->count() > 0)
                    return prop_type;

                // 获取下一同属性节点,对下一同属性节点的上一节点指针置空
                auto next_same_prop_node = pop_front_node->get_next_same_prop_node();
                if (next_same_prop_node) {
                    next_same_prop_node->reset_pre_same_prop_node(nullptr);
                    m_front_node[prop_type] = next_same_prop_node;
                    m_all_nodes[prop_type].pop_front();
                }
                else {
                    m_front_node.erase(prop_type);
                    m_all_nodes.erase(prop_type);
                }

                // 获取下一节点,对下一节点的 上一节点指针  赋值为 原本指针的 上一节点指针
                // 并将上一节点的下一节点指针  赋值为 本指针的  下一节点指针
                auto pre_list_prop_node = pop_front_node->get_pre_list_prop_node();
                auto next_list_prop_node = pop_front_node->get_next_list_prop_node();
                if (next_list_prop_node)
                    next_list_prop_node->reset_pre_list_prop_node(pre_list_prop_node);
                if(pre_list_prop_node)
                    pre_list_prop_node->reset_next_list_prop_node(next_list_prop_node);

                if (pop_front_node == m_begin_node)
                    m_begin_node = next_list_prop_node;
                if (pop_front_node == m_end_node)
                    m_end_node = pre_list_prop_node;

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

                for (auto node_iter = all_node_iter->second.begin(); node_iter != all_node_iter->second.end(); ) {
                    auto& item = *node_iter;

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
                        if(m_begin_node)
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
                    all_node_iter->second.erase(node_iter++);
                }
                m_all_nodes.erase(all_node_iter);
                m_front_node.erase(prop);
            }

            void clear() {
                m_begin_node = nullptr;
                m_end_node = nullptr;
                m_front_node.clear();
                for (auto& nodes : m_all_nodes) {
                    for (auto node_iter = nodes.second.begin(); node_iter != nodes.second.end(); ) {
                        delete *node_iter;
                        nodes.second.erase(node_iter++);
                    }
                }
                m_all_nodes.clear();
            }

        private:
            PropCountNode*                                  m_begin_node;   // 队列起始节点
            PropCountNode*                                  m_end_node;     // 队列结束节点
            std::map<TPropType, std::list<PropCountNode*>>  m_all_nodes;    // 所有队列节点
            std::map<TPropType, PropCountNode*>             m_front_node;   // 所有属性首节点
        };

    public:
        // max_task_count: 最大任务个数,超过该数量将产生阻塞;0则表示无限制
        SerialTupleTaskQueue(size_t max_task_count = 0)
            : m_max_task_count(max_task_count)
            , m_bstop(false)
        {}

        ~SerialTupleTaskQueue() {
            clear();
            stop();
        }

        // 特别注意!遇到char*/char[]等指针性质的临时指针,必须转换为string等实例对象,否则外界析构后,将指向野指针!!!!
        template<typename AsTPropType, typename TFunction, typename... Args>
        bool add_task(AsTPropType&& prop, TFunction&& func, Args&&... args) {
            wait_for_can_add();
            if (m_bstop.load())
                return false;

            // 此处TTuple不可采用std::forward_as_tuple(std::forward<Args>(args)...)
            // 假使agrs中含有const & 时,会导致tuple中存储的亦为const &对象,从而外部释放对象后导致内部对象无效
            // 采用std::make_shared<TTuple>则会导致存在一次拷贝,由std::make_tuple引起(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(std::forward<AsTPropType>(prop), std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // 移除所有指定属性任务,当前正在执行除外,可能存在阻塞
        // 存在遍历,可能比较耗时
        template<typename AsTPropType>
        void remove_prop(AsTPropType&& prop) {
            {
                writeLock locker(m_tasks_mtx);
                auto iter = m_wait_tasks.find(prop);
                if (iter != m_wait_tasks.end())
                    m_wait_tasks.erase(prop);
                m_wait_props.remove_prop(prop);
                remove_cur_prop(std::forward<AsTPropType>(prop));
            }
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_full.notify_one();
        }

        // 移除一个顶层非当前执行属性任务,队列为空时存在阻塞
        void pop_task() {
            wait_for_can_pop();
            if (m_bstop.load())
                return;

            TaskPtr next_task(nullptr);
            bool pop_rslt(false);
            TPropType prop_type;

            {
                writeLock locker(m_tasks_mtx);
                {
                    std::lock_guard<std::mutex> locker(m_props_mtx);
                    std::tie(pop_rslt, prop_type) = m_wait_props.pop_front(m_cur_props);
                    if (!pop_rslt)
                        return;
                    m_cur_props.emplace(prop_type);
                }
                auto wait_task_iter = m_wait_tasks.find(prop_type);
                if (wait_task_iter != m_wait_tasks.end()) {
                    next_task = wait_task_iter->second.front();
                    wait_task_iter->second.pop_front();
                    if (wait_task_iter->second.empty())
                        m_wait_tasks.erase(wait_task_iter);
                }
            }

            if (next_task) {
                next_task->invoke();
                remove_cur_prop(std::move(prop_type));
                std::unique_lock<std::mutex> locker(m_cv_mtx);
                m_cv_not_full.notify_one();
            }
        }

        void stop() {
            // 是否已终止判断
            bool target(false);
            if (!m_bstop.compare_exchange_strong(target, true)) {
                return;
            }
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_empty.notify_all();
            m_cv_not_full.notify_all();
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
                m_wait_props.clear();
                m_wait_tasks.clear();
                std::lock_guard<std::mutex> lck(m_props_mtx);
                m_cur_props.clear();
            }
            std::unique_lock<std::mutex> locker(m_cv_mtx);
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
            auto& prop = new_task_item->get_prop_type();
            {
                writeLock locker(m_tasks_mtx);
                m_wait_props.push_back(prop);
                m_wait_tasks[prop].push_back(std::forward<PropTaskPtr>(new_task_item));
            }
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_empty.notify_one();
            return true;
        }

        // 删除当前运行属性
        template<typename AsTPropType>
        void remove_cur_prop(AsTPropType&& prop_type) {
            std::lock_guard<std::mutex> locker(m_props_mtx);
            auto prop_iter = m_cur_props.find(std::forward<AsTPropType>(prop_type));
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

        // 任务队列锁
        mutable rwMutex                             m_tasks_mtx;
        // 总待执行任务属性顺序队列,用于判断执行队列顺序
        PropCountNodeList                           m_wait_props;
        // 总待执行任务队列属性及其对应任务
        std::map<TPropType, std::list<PropTaskPtr>> m_wait_tasks;
        // 最大任务个数,当为0时表示无限制
        size_t                                      m_max_task_count;

        // 条件阻塞锁, 使用std::unique_lock进行锁定,std::condition_variable会在判定结束后释放该锁
        std::mutex                                  m_cv_mtx;
        // 不为空的条件变量
        std::condition_variable                     m_cv_not_empty;
        // 没有满的条件变量
        std::condition_variable                     m_cv_not_full;

        // 当前执行任务属性队列锁
        mutable std::mutex                          m_props_mtx;
        // 当前正在执行中的任务属性
        std::set<TPropType>                         m_cur_props;
    };
}