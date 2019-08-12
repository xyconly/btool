/*************************************************
File name:  task_queue.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ�����������,��������ظ�����

Note:  condition_variableʹ��ע��:�ڽ���waitʱ������
       1.ִ���ж�,Ϊtrue���˳�
       2.�ͷ�������(�ź���)����
       3.����notify,������
       Ȼ���ظ�1-3����,ֱ���ﵽ�����������˳�,ע���ʱ����Ϊ1������,��δ�ͷ���
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
    Description:�ṩFIFO�������,�����ú���תΪԪ�����洢
    *************************************************/
    class TupleTaskQueue
    {
         // ��ֹ����
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

            // �˴�TTuple���ɲ���std::forward_as_tuple(std::forward<Args>(args)...)
            // ��ʹagrs�к���const & ʱ,�ᵼ��tuple�д洢����Ϊconst &����,�Ӷ��ⲿ�ͷŶ�������ڲ�������Ч
            // ����std::make_shared<TTuple>��ᵼ�´���һ�ο���,��std::make_tuple����(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<TupleTask<TFunction, TTuple>>(std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // �Ƴ�һ������ǵ�ǰִ����������,����Ϊ��ʱ��������
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
            // �Ƿ�����ֹ�ж�
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
        // �Ƿ���δ��״̬
        bool not_full() const {
            readLock locker(m_tasks_mtx);
            return m_max_task_count == 0 || m_queue.size() < m_max_task_count;
        }

        // �Ƿ��ڿ�״̬
        bool not_empty() const {
            readLock locker(m_tasks_mtx);
            return !m_queue.empty();
        }

      private:
          // �ȴ�ֱ�����Ƴ�����������
        void wait_for_can_pop() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
        }
        // �ȴ�ֱ���ɼ����µ�����������
        void wait_for_can_add() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });
        }

        // ��������������
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
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>           m_bstop;

        // ���������
        mutable rwMutex             m_tasks_mtx;
        // �ܴ�ִ���������,�������еĴ�ִ������
        std::queue<TaskPtr>         m_queue;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                      m_max_task_count;

        // ����������, ʹ��std::unique_lock��������,std::condition_variable�����ж��������ͷŸ���
        std::mutex                      m_cv_mtx;
        // ��Ϊ�յ���������
        std::condition_variable         m_cv_not_empty;
        // û��������������
        std::condition_variable         m_cv_not_full;
    };


    /*************************************************
    Description:�ṩ�����Ի��ֵ�,����������״̬��FIFO�������,�����ú���תΪԪ�����洢
                ��ĳһ�������ڶ�����ʱ,ͬ���Ե�������������ʱ,ԭ����ᱻ����
    *************************************************/
    template<typename TPropType>
    class LastTupleTaskQueue
    {
        typedef std::shared_ptr<PropTaskVirtual<TPropType>> PropTaskPtr;

    public:
        // max_task_count: ����������,��������������������;0���ʾ������
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

            // �˴�TTuple���ɲ���std::forward_as_tuple(std::forward<Args>(args)...)
            // ��ʹagrs�к���const & ʱ,�ᵼ��tuple�д洢����Ϊconst &����,�Ӷ��ⲿ�ͷŶ�������ڲ�������Ч
            // ����std::make_shared<TTuple>��ᵼ�´���һ�ο���,��std::make_tuple����(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(std::forward<AsTPropType>(prop), std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // �Ƴ�����ָ����������,��ǰ����ִ�г���,���ܴ�������
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

        // �Ƴ�һ������ǵ�ǰִ����������,����Ϊ��ʱ��������
        void pop_task() {
            wait_for_can_pop();

            if (m_bstop.load())
                return;

            PropTaskPtr pop_task_ptr(nullptr);

            {
                writeLock locker(m_tasks_mtx);
                // �Ƿ����޿�pop����
                if (m_wait_props.empty())
                    return;

                auto pop_type_iter = m_wait_props.begin();
                while (pop_type_iter != m_wait_props.end()) {
                    // ��ȡ����ָ��
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
            // �Ƿ�����ֹ�ж�
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
        // �Ƿ���δ��״̬
        bool not_full() const {
            readLock lock(m_tasks_mtx);
            return m_max_task_count == 0 || m_wait_props.size() < m_max_task_count;
        }

        // �Ƿ��ڷǿ�״̬
        bool not_empty() const {
            readLock lock(m_tasks_mtx);
            return !m_wait_props.empty();
        }

    private:
        // �ȴ�ֱ�����Ƴ�����������
        void wait_for_can_pop() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
        }
        // �ȴ�ֱ���ɼ����µ�����������
        void wait_for_can_add() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });
        }

        // ��������������
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
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>               m_bstop;

        // ���������
        mutable rwMutex                 m_tasks_mtx;
        // �ܴ�ִ����������˳�����,�����ж�ִ�ж���˳��
        std::list<TPropType>             m_wait_props;
        // �ܴ�ִ������������Լ����Ӧ����,���������ʼ����m_wait_tasks����ͬ��
        std::map<TPropType, PropTaskPtr> m_wait_tasks;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                           m_max_task_count;

        // ����������, ʹ��std::unique_lock��������,std::condition_variable�����ж��������ͷŸ���
        std::mutex                      m_cv_mtx;
        // ��Ϊ�յ���������
        std::condition_variable         m_cv_not_empty;
        // û��������������
        std::condition_variable         m_cv_not_full;
    };


    /*************************************************
    Description:�ṩ�����Ի��ֵ�,�������������FIFO�������,�����ú���תΪԪ�����洢
                ��ĳһ�������ڶ�����ʱ,ͬ���Ե�������������ʱ,��׷����ԭ����֮��ִ��
                ��ĳһ��������ִ��ʱ,ͬ�����������񽫲���ִ��,ͬһ����֮������������FIFO����ִ�����
    *************************************************/
    template<typename TPropType>
    class SerialTupleTaskQueue
    {
        typedef std::shared_ptr<PropTaskVirtual<TPropType>> PropTaskPtr;

        // ��������˫������,���ڴ洢ͬһ��������λ��,��FIFO˳��
        // ���̰߳�ȫ��
        class PropCountNodeList
        {
            // ��������ṹ��
            struct PropCountNode {
                TPropType    prop_;
                size_t       count_;

                PropCountNode*  pre_same_prop_node_;  // ͬ������һ��������ָ��
                PropCountNode*  next_same_prop_node_; // ͬ������һ��������ָ��
                PropCountNode*  pre_list_prop_node_;  // ���е���һ��������ָ��
                PropCountNode*  next_list_prop_node_; // ���е���һ��������ָ��

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
                // �Ƿ��Ѵ��ڽڵ�
                if (!m_end_node) {
                    m_begin_node = m_end_node = new PropCountNode(std::forward<AsTPropType>(prop), nullptr, nullptr);
                    m_all_nodes[m_end_node->get_prop_type()].emplace_back(m_end_node);
                    m_front_node[m_end_node->get_prop_type()] = m_end_node;
                    return;
                }

                //���һ���ڵ��Ƿ���ͬ����
                if (m_end_node->get_prop_type() == prop) {
                    m_end_node->add();
                    return;
                }

                // �������Ƿ񲻴�����������
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

            // ȥ���׸���ָ�����Լ��ϵ��׸��ڵ�,�޸ýڵ�ʱ����false
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

            // pop�����׽ڵ�,�޸ýڵ�ʱ�쳣
            TPropType pop_front() {
                auto prop_type = m_begin_node->get_prop_type();
                m_begin_node->reduce();
                if (m_begin_node->count() > 0)
                    return prop_type;

                // ��ʼ�ڵ�==��ֹ�ڵ�
                if (m_begin_node == m_end_node) {
                    delete m_begin_node;
                    m_begin_node = nullptr;
                    m_end_node = nullptr;
                    m_all_nodes.clear();
                    m_front_node.clear();
                    return prop_type;
                }
                // pop���е�һ��,�������Զ���Ϊ����ɾ�������Լ��׽ڵ�
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

            // popָ�������µ��׽ڵ�,�޸ýڵ�ʱ�쳣
            TPropType pop_point_prop_front(TPropType prop_type) {
                auto pop_front_node = m_front_node[prop_type];
                pop_front_node->reduce();
                if (pop_front_node->count() > 0)
                    return prop_type;

                // ��ȡ��һͬ���Խڵ�,����һͬ���Խڵ����һ�ڵ�ָ���ÿ�
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

                // ��ȡ��һ�ڵ�,����һ�ڵ�� ��һ�ڵ�ָ��  ��ֵΪ ԭ��ָ��� ��һ�ڵ�ָ��
                // ������һ�ڵ����һ�ڵ�ָ��  ��ֵΪ ��ָ���  ��һ�ڵ�ָ��
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

                bool need_comp_begin(false); // �Ƿ���Ҫ�ȶ�begin�ڵ�
                if (m_begin_node && m_begin_node->get_prop_type() == prop)
                    need_comp_begin = true;
                bool need_comp_end(false); // �Ƿ���Ҫ�ȶ�end�ڵ�
                if (m_end_node && m_end_node->get_prop_type() == prop)
                    need_comp_end = true;

                for (auto node_iter = all_node_iter->second.begin(); node_iter != all_node_iter->second.end(); ) {
                    auto& item = *node_iter;

                    // �޸�������һ�ڵ�ָ���  ��һ�ڵ�Ϊ ��ǰ����һ�ڵ�
                    // ��֮ͬ��
                    auto pre_list_prop_node = item->get_pre_list_prop_node();
                    auto next_list_prop_node = item->get_next_list_prop_node();
                    if (pre_list_prop_node)
                        pre_list_prop_node->reset_next_list_prop_node(next_list_prop_node);
                    if (next_list_prop_node)
                        next_list_prop_node->reset_pre_list_prop_node(pre_list_prop_node);

                    // �жϲ�����begin�ڵ�
                    if (need_comp_begin && item == m_begin_node) {
                        m_begin_node = next_list_prop_node;
                        if(m_begin_node)
                            m_begin_node->reset_pre_list_prop_node(nullptr);
                        if (!m_begin_node || m_begin_node->get_prop_type() != prop)
                            need_comp_begin = false;
                    }
                    // �жϲ�����end�ڵ�
                    if (need_comp_end && item == m_end_node) {
                        m_end_node = pre_list_prop_node;
                        if (m_end_node)
                            m_end_node->reset_next_list_prop_node(nullptr);
                        if (!m_end_node || m_end_node->get_prop_type() != prop)
                            need_comp_end = false;
                    }

                    // ɾ���ýڵ�
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
            PropCountNode*                                  m_begin_node;   // ������ʼ�ڵ�
            PropCountNode*                                  m_end_node;     // ���н����ڵ�
            std::map<TPropType, std::list<PropCountNode*>>  m_all_nodes;    // ���ж��нڵ�
            std::map<TPropType, PropCountNode*>             m_front_node;   // ���������׽ڵ�
        };

    public:
        // max_task_count: ����������,��������������������;0���ʾ������
        SerialTupleTaskQueue(size_t max_task_count = 0)
            : m_max_task_count(max_task_count)
            , m_bstop(false)
        {}

        ~SerialTupleTaskQueue() {
            clear();
            stop();
        }

        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template<typename AsTPropType, typename TFunction, typename... Args>
        bool add_task(AsTPropType&& prop, TFunction&& func, Args&&... args) {
            wait_for_can_add();
            if (m_bstop.load())
                return false;

            // �˴�TTuple���ɲ���std::forward_as_tuple(std::forward<Args>(args)...)
            // ��ʹagrs�к���const & ʱ,�ᵼ��tuple�д洢����Ϊconst &����,�Ӷ��ⲿ�ͷŶ�������ڲ�������Ч
            // ����std::make_shared<TTuple>��ᵼ�´���һ�ο���,��std::make_tuple����(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(std::forward<AsTPropType>(prop), std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // �Ƴ�����ָ����������,��ǰ����ִ�г���,���ܴ�������
        // ���ڱ���,���ܱȽϺ�ʱ
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

        // �Ƴ�һ������ǵ�ǰִ����������,����Ϊ��ʱ��������
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
            // �Ƿ�����ֹ�ж�
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
        // �ȴ�ֱ���ɼ����µ�����������
        void wait_for_can_add() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_full.wait(locker, [this] { return m_bstop.load() || not_full(); });
        }
        // �ȴ�ֱ����pop�µ�����
        void wait_for_can_pop() {
            std::unique_lock<std::mutex> locker(m_cv_mtx);
            m_cv_not_empty.wait(locker, [this] { return m_bstop.load() || not_empty(); });
        }

        // ��������������
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

        // ɾ����ǰ��������
        template<typename AsTPropType>
        void remove_cur_prop(AsTPropType&& prop_type) {
            std::lock_guard<std::mutex> locker(m_props_mtx);
            auto prop_iter = m_cur_props.find(std::forward<AsTPropType>(prop_type));
            if (prop_iter != m_cur_props.end())
                m_cur_props.erase(prop_iter);
        }

        // �Ƿ���δ��״̬
        bool not_full() const {
            readLock lock(m_tasks_mtx);
            return m_max_task_count == 0 || m_wait_tasks.size() < m_max_task_count;
        }

        // �Ƿ��ڿ�״̬
        bool not_empty() const {
            readLock lock(m_tasks_mtx);
            std::lock_guard<std::mutex> locker(m_props_mtx);
            return !m_wait_tasks.empty() && m_cur_props.size() < m_wait_tasks.size();
        }

    private:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>                           m_bstop;

        // ���������
        mutable rwMutex                             m_tasks_mtx;
        // �ܴ�ִ����������˳�����,�����ж�ִ�ж���˳��
        PropCountNodeList                           m_wait_props;
        // �ܴ�ִ������������Լ����Ӧ����
        std::map<TPropType, std::list<PropTaskPtr>> m_wait_tasks;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                                      m_max_task_count;

        // ����������, ʹ��std::unique_lock��������,std::condition_variable�����ж��������ͷŸ���
        std::mutex                                  m_cv_mtx;
        // ��Ϊ�յ���������
        std::condition_variable                     m_cv_not_empty;
        // û��������������
        std::condition_variable                     m_cv_not_full;

        // ��ǰִ���������Զ�����
        mutable std::mutex                          m_props_mtx;
        // ��ǰ����ִ���е���������
        std::set<TPropType>                         m_cur_props;
    };
}