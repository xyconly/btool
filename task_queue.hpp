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
    Description:�ṩ���ں�����FIFO�������
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
            // ��λ����ֹ��־��
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            std::unique_lock<std::mutex> locker(m_mtx);
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            // �Ƿ�����ֹ�ж�
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
                // queue���������ͷ��ѿ��ٿռ�,���������ʱ���ͷ�һ��,��ʵ�ʻ�����,���Լ�����Ϊlist������queue����ռ�õ�����,����ᵼ�����ܵ���΢�½�,����ʵ���������
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
        // �Ƿ���δ��״̬
        bool not_full() const {
            return m_max_task_count == 0 || m_queue.size() < m_max_task_count;
        }

        // �Ƿ��ڿ�״̬
        bool not_empty() const {
            return !m_queue.empty();
        }

    protected:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>           m_bstop;
        // ���ݰ�ȫ��
        mutable std::mutex          m_mtx;

        // �ܴ�ִ���������,�������еĴ�ִ������
        std::queue<TaskItem>       m_queue;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                      m_max_task_count;

        // ��Ϊ�յ���������
        std::condition_variable     m_cv_not_empty;
        // û��������������
        std::condition_variable     m_cv_not_full;
    };

    /*************************************************
    Description:�ṩ�����Ի��ֵ�,����������״̬��FIFO�������
                ��ĳһ�������ڶ�����ʱ,ͬ���Ե�������������ʱ,ԭ����ᱻ����
    *************************************************/
    template<typename TPropType>
    class LastTaskQueue
    {
    public:
        typedef std::function<void()> TaskItem;
    public:
        // max_task_count: ����������,��������������������;0���ʾ������
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
            // ��λ����ֹ��־��
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }

            std::unique_lock<std::mutex> locker(m_mtx);
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            // �Ƿ�����ֹ�ж�
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

                // �Ƿ����޿�pop����
                if (m_wait_props.empty())
                    return;

                for (auto pop_type_iter = m_wait_props.begin(); pop_type_iter != m_wait_props.end(); pop_type_iter++) {
                    if (m_cur_pop_props.find(*pop_type_iter) != m_cur_pop_props.end())
                        continue;

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

                std::unique_lock<std::mutex> locker(m_mtx);
                m_cur_pop_props.erase(pop_type);
                m_cv_not_full.notify_one();
            }
        }

        // �Ƴ�����ָ����������,��ǰ����ִ�г���,���ܴ�������
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
        // �Ƿ���δ��״̬
        bool not_full() const {
            return m_max_task_count == 0 || m_wait_props.size() < m_max_task_count;
        }

        // �Ƿ��ڷǿ�״̬
        bool not_empty() const {
            return !m_wait_props.empty();
        }

    protected:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>                m_bstop;

        // ���ݰ�ȫ��
        mutable std::mutex               m_mtx;
        // �ܴ�ִ����������˳�����,�����ж�ִ�ж���˳��
        std::list<TPropType>             m_wait_props;
        // �ܴ�ִ������������Լ����Ӧ����,���������ʼ����m_wait_tasks����ͬ��
        std::map<TPropType, TaskItem>   m_wait_tasks;
        // ��ǰ����pop��������
        std::set<TPropType>              m_cur_pop_props;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                           m_max_task_count;

        // ��Ϊ�յ���������
        std::condition_variable          m_cv_not_empty;
        // û��������������
        std::condition_variable          m_cv_not_full;
    };

    /*************************************************
    Description:�ṩ�����Ի��ֵ�,�������������FIFO�������,�����ú���תΪԪ�����洢
                ��ĳһ�������ڶ�����ʱ,ͬ���Ե�������������ʱ,��׷����ԭ����֮��ִ��
                ��ĳһ��������ִ��ʱ,ͬ�����������񽫲���ִ��,ͬһ����֮������������FIFO����ִ�����
    *************************************************/
    template<typename TPropType>
    class SerialTaskQueue
    {
    public:
        typedef std::function<void()> TaskItem;

    protected:
        // ��������˫������,���ڴ洢ͬһ��������λ��,��FIFO˳��
        // ���̰߳�ȫ
        class PropCountNodeList
        {
            // ��������ṹ��
            struct PropCountNode {
                bool         can_pop_;      // ��ǰ�ڵ��Ƿ�ɱ�pop, ÿ�θ�λ��/������������ᱻ��λΪtrue
                size_t       count_;        // ��ǰ��������ͬ�����������,����������300��ͬ����,�ڶ�����ֻ����һ��PropCountNode,����Ϊ300

                PropCountNode*  pre_same_prop_node_;  // ͬ������һ��������ָ��
                PropCountNode*  next_same_prop_node_; // ͬ������һ��������ָ��
                PropCountNode*  pre_list_prop_node_;  // ���е���һ��������ָ��
                PropCountNode*  next_list_prop_node_; // ���е���һ��������ָ��

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
                // �Ƿ��Ѵ��ڽڵ�
                if (!m_end_node) {
                    m_begin_node = m_end_node = new PropCountNode(std::forward<AsTPropType>(prop), nullptr, nullptr, can_immediately_pop);
                    m_all_nodes[m_end_node->get_prop_type()].emplace_back(m_end_node);
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

            // ����ĳ�����Ե�����
            void reset_prop(const TPropType& prop_type) {
                auto all_nodes_iter = m_all_nodes.find(prop_type);
                if (all_nodes_iter == m_all_nodes.end())
                    return;

                all_nodes_iter->second.front()->reset_can_pop(true);
            }

            // ȥ���׸�ָ�����Լ��ϵ��׸��ڵ�,�޸ýڵ�ʱ����false
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

                // ��ȡ��һͬ���Խڵ�,����һͬ���Խڵ����һ�ڵ�ָ���ÿ�
                auto next_same_prop_node = pop_front_node->get_next_same_prop_node();
                if (next_same_prop_node) {
                    next_same_prop_node->reset_pre_same_prop_node(nullptr);
                    m_all_nodes[pop_front_node->get_prop_type()].pop_front();
                }
                else {
                    m_all_nodes.erase(pop_front_node->get_prop_type());
                }

                // ��ȡ��һ�ڵ�,����һ�ڵ�� ��һ�ڵ�ָ��  ��ֵΪ ԭ��ָ��� ��һ�ڵ�ָ��
                // ������һ�ڵ����һ�ڵ�ָ��  ��ֵΪ ��ָ���  ��һ�ڵ�ָ��
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

                bool need_comp_begin(false); // �Ƿ���Ҫ�ȶ�begin�ڵ�
                if (m_begin_node && m_begin_node->get_prop_type() == prop)
                    need_comp_begin = true;
                bool need_comp_end(false); // �Ƿ���Ҫ�ȶ�end�ڵ�
                if (m_end_node && m_end_node->get_prop_type() == prop)
                    need_comp_end = true;

                for (auto& item : all_node_iter->second) {
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
                        if (m_begin_node)
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
            PropCountNode*                                  m_begin_node;   // ������ʼ�ڵ�
            PropCountNode*                                  m_end_node;     // ���н����ڵ�
            std::map<TPropType, std::list<PropCountNode*>>  m_all_nodes;    // ���ж��нڵ�
        };

    public:
        // max_task_count: ����������,��������������������;0���ʾ������
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
            // ��λ����ֹ��־��
            bool target(true);
            if (!m_bstop.compare_exchange_strong(target, false)) {
                return;
            }
            std::unique_lock<std::mutex> locker(m_mtx);
            m_cv_not_full.notify_all();
            m_cv_not_empty.notify_all();
        }

        void stop(bool bwait = false) {
            // �Ƿ�����ֹ�ж�
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

        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
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

        // �Ƴ�����ָ����������,��ǰ����ִ�г���,���ܴ�������
        // ���ڱ���,���ܱȽϺ�ʱ
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
        // ɾ����ǰ��������
        template<typename AsTPropType>
        inline void remove_cur_prop(AsTPropType&& prop_type) {
            auto prop_iter = m_cur_props.find(std::forward<AsTPropType>(prop_type));
            if (prop_iter != m_cur_props.end())
                m_cur_props.erase(prop_iter);
        }

        // �Ƿ���δ��״̬
        inline bool not_full() const {
            return m_max_task_count == 0 || m_wait_tasks.size() < m_max_task_count;
        }

        // �Ƿ��ڿ�״̬
        inline bool not_empty() const {
            return !m_wait_tasks.empty() && m_cur_props.size() < m_wait_tasks.size();
        }

    protected:
        // �Ƿ�����ֹ��ʶ��
        std::atomic<bool>                           m_bstop;

        // ���ݰ�ȫ��
        mutable std::mutex                          m_mtx;
        // �ܴ�ִ����������˳�����,�����ж�ִ�ж���˳��
        PropCountNodeList                           m_wait_props;
        // �ܴ�ִ������������Լ����Ӧ����
        std::map<TPropType, std::list<TaskItem>>    m_wait_tasks;
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                                      m_max_task_count;

        // ��Ϊ�յ���������
        std::condition_variable                     m_cv_not_empty;
        // û��������������
        std::condition_variable                     m_cv_not_full;

        // ��ǰ����ִ���е���������
        std::set<TPropType>                         m_cur_props;

    };
}