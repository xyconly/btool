/*************************************************
File name:  task_queue.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ�����������,��������ظ�����
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
            stop();
            clear();
        }

        template<typename Function, typename... Args>
        bool add_task(const Function& func, Args&&... args) {
            wait_for_can_add();

            if (m_bstop.load())
                return false;

            // �˴�TTuple���ɲ���std::forward_as_tuple(std::forward<Args>(args)...)
            // ��ʹagrs�к���const & ʱ,�ᵼ��tuple�д洢����Ϊconst &����,�Ӷ��ⲿ�ͷŶ�������ڲ�������Ч
            // ����std::make_shared<TTuple>��ᵼ�´���һ�ο���,��std::make_tuple����(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<TupleTask<Function, TTuple>>(func, std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        template<typename Function, typename... Args>
        bool add_task(Function&& func, Args&&... args) {
            wait_for_can_add();

            if (m_bstop.load())
                return false;

            // �˴�TTuple���ɲ���std::forward_as_tuple(std::forward<Args>(args)...)
            // ��ʹagrs�к���const & ʱ,�ᵼ��tuple�д洢����Ϊconst &����,�Ӷ��ⲿ�ͷŶ�������ڲ�������Ч
            // ����std::make_shared<TTuple>��ᵼ�´���һ�ο���,��std::make_tuple����(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<TupleTask<Function, TTuple>>(std::forward<Function>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
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
            m_cv_not_full.notify_one();

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

        template<typename TFunction, typename... Args>
        bool add_task(const TPropType& prop, TFunction&& func, Args&&... args) {
            wait_for_can_add();
            if (m_bstop.load())
                return false;

            // �˴�TTuple���ɲ���std::forward_as_tuple(std::forward<Args>(args)...)
            // ��ʹagrs�к���const & ʱ,�ᵼ��tuple�д洢����Ϊconst &����,�Ӷ��ⲿ�ͷŶ�������ڲ�������Ч
            // ����std::make_shared<TTuple>��ᵼ�´���һ�ο���,��std::make_tuple����(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(prop, std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        template<typename TFunction, typename... Args>
        bool add_task(TPropType&& prop, TFunction&& func, Args&&... args) {
            wait_for_can_add();
            if (m_bstop.load())
                return false;

            // �˴�TTuple���ɲ���std::forward_as_tuple(std::forward<Args>(args)...)
            // ��ʹagrs�к���const & ʱ,�ᵼ��tuple�д洢����Ϊconst &����,�Ӷ��ⲿ�ͷŶ�������ڲ�������Ч
            // ����std::make_shared<TTuple>��ᵼ�´���һ�ο���,��std::make_tuple����(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(std::forward<TPropType>(prop), std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // �Ƴ�����ָ����������,��ǰ����ִ�г���,���ܴ�������
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
                    assert(m_wait_tasks.find(*pop_type_iter) != m_wait_tasks.end());
                    // ��ȡ����ָ��
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
            // �Ƿ�����ֹ�ж�
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

        // �������Լ���Ӧ���������ṹ��
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
        // max_task_count: ����������,��������������������;0���ʾ������
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

        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template<typename TFunction, typename... Args>
        bool add_task(const TPropType& prop, TFunction&& func, Args&&... args)
        {
            wait_for_can_add();

            if (m_bstop.load())
                return false;

            // �˴�TTuple���ɲ���std::forward_as_tuple(std::forward<Args>(args)...)
            // ��ʹagrs�к���const & ʱ,�ᵼ��tuple�д洢����Ϊconst &����,�Ӷ��ⲿ�ͷŶ�������ڲ�������Ч
            // ����std::make_shared<TTuple>��ᵼ�´���һ�ο���,��std::make_tuple����(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(prop, std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // �ر�ע��!����char*/char[]��ָ�����ʵ���ʱָ��,����ת��Ϊstring��ʵ������,�������������,��ָ��Ұָ��!!!!
        template<typename TFunction, typename... Args>
        bool add_task(TPropType&& prop, TFunction&& func, Args&&... args)
        {
            wait_for_can_add();

            if (m_bstop.load())
                return false;

            // �˴�TTuple���ɲ���std::forward_as_tuple(std::forward<Args>(args)...)
            // ��ʹagrs�к���const & ʱ,�ᵼ��tuple�д洢����Ϊconst &����,�Ӷ��ⲿ�ͷŶ�������ڲ�������Ч
            // ����std::make_shared<TTuple>��ᵼ�´���һ�ο���,��std::make_tuple����(const&/&&)
            typedef decltype(std::make_tuple(std::forward<Args>(args)...)) TTuple;
            return add_task_tolist(std::make_shared<PropTupleTask<TPropType, TFunction, TTuple>>(std::forward<TPropType>(prop), std::forward<TFunction>(func), std::make_shared<TTuple>(std::forward_as_tuple(std::forward<Args>(args)...))));
        }

        // �Ƴ�����ָ����������,��ǰ����ִ�г���,���ܴ�������
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

        // �Ƴ�һ������ǵ�ǰִ����������,����Ϊ��ʱ��������
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
            // �Ƿ�����ֹ�ж�
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

        // ������ǰ��������
        // �Ѵ��ڷ���false���������뷵��true
        bool add_cur_prop(const TPropType& prop_type)
        {
            std::lock_guard<std::mutex> locker(m_props_mtx);
            if (m_cur_props.find(prop_type) != m_cur_props.end())
                return false;

            m_cur_props.insert(prop_type);
            return true;
        }

        // ɾ����ǰ��������
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
        // ����������,��Ϊ0ʱ��ʾ������
        size_t                                      m_max_task_count;

        // ���������
        mutable rwMutex                             m_tasks_mtx;
        // �ܴ�ִ����������ܸ���
        size_t                                      m_wait_task_count;
        // �ܴ�ִ���������,�������еĴ�ִ������
        std::map<TPropType, std::list<PropTaskPtr>> m_wait_tasks;
        // ��ǰ��˳������,��ִ�е� �������Լ��������������
        // ��������Ϊ0ʱ,ɾ����PropCountItemPtr
        std::list<PropCountItem>                    m_prop_counts;

        // ����������
        std::mutex                                  m_cv_mtx;
        // ��Ϊ�յ���������
        std::condition_variable                     m_cv_not_empty;
        // û��������������
        std::condition_variable                     m_cv_not_full;

        // ��ǰִ���������Զ�����
        mutable std::mutex                          m_props_mtx;
        // ��ǰ����ִ���е���������
        std::set<TPropType>                          m_cur_props;
    };


}