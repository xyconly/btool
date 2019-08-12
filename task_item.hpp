/*************************************************
File name:  task_item.hpp
Author:     AChar
Version:
Date:
Description:    �ṩ�����������,��������ظ�����
*************************************************/
#pragma once
#include <type_traits>

namespace BTool
{
    //////////////////////////////////////////////////////////////////////////
    // �������
    class TaskVirtual {
    public:
        // ִ�е��ú���
        virtual void invoke() = 0;
    };
    typedef std::shared_ptr<TaskVirtual>  TaskPtr;

    //////////////////////////////////////////////////////////////////////////
    // Ԫ��ִ����
    template<typename TFunction, typename TTuple>
    class TupleInvoke
    {
    public:
        TupleInvoke(TFunction&& func, std::shared_ptr<TTuple>&& t)
            : m_fun_cbk(std::forward<TFunction>(func))
            , m_tuple(std::forward<std::shared_ptr<TTuple>>(t))
        {}
        ~TupleInvoke() {}

#  if (!defined _MSC_VER) || (_MSC_VER < 1920) // C++17�汾�ſ�ʼ֧��std::apply
        // ִ�е��ú���
        void invoke() {
            constexpr auto size = std::tuple_size<typename std::decay<TTuple>::type>::value;
            invoke_impl(std::make_index_sequence<size>{});
        }
    private:
        template<std::size_t... Index>
        decltype(auto) invoke_impl(std::index_sequence<Index...>) {
            return m_fun_cbk(std::get<Index>(std::forward<TTuple>(*m_tuple.get()))...);
        }
#  else
        // ִ�е��ú���
        void invoke() {
            std::apply(std::forward<TFunction>(m_fun_cbk), std::forward<TTuple>(*m_tuple.get()));
        }
#  endif

    private:
        std::shared_ptr<TTuple>     m_tuple;
        TFunction                   m_fun_cbk;
    };

    //////////////////////////////////////////////////////////////////////////
    // Ԫ��������
    template<typename TFunction, typename TTuple>
    class TupleTask : public TaskVirtual
    {
    public:
        TupleTask(TFunction&& func, std::shared_ptr<TTuple>&& t)
            : m_invoke(std::forward<TFunction>(func), std::forward<std::shared_ptr<TTuple>>(t))
        {}
        void invoke() override {
            m_invoke.invoke();
        }
    private:
        TupleInvoke<TFunction, TTuple>  m_invoke;
    };

    //////////////////////////////////////////////////////////////////////////
    // �������������
    template<typename TPropType>
    class PropTaskVirtual : public TaskVirtual
    {
    public:
        template<typename AsTPropType>
        PropTaskVirtual(AsTPropType&& prop) : m_prop(std::forward<AsTPropType>(prop)) {}
        virtual ~PropTaskVirtual() {}

        // ��ȡ��������
        const TPropType& get_prop_type() const {
            return m_prop;
        }
    private:
        TPropType m_prop;
    };

    //////////////////////////////////////////////////////////////////////////
    // ������Ԫ��������
    template<typename TPropType, typename TFunction, typename TTuple>
    class PropTupleTask : public PropTaskVirtual<TPropType>
    {
    public:
        template<typename AsTPropType>
        PropTupleTask(AsTPropType&& prop, TFunction&& func, std::shared_ptr<TTuple>&& t)
            : PropTaskVirtual<TPropType>(std::forward<AsTPropType>(prop))
            , m_invoke(std::forward<TFunction>(func), std::forward<std::shared_ptr<TTuple>>(t))
        {}

        void invoke() override {
            m_invoke.invoke();
        }
    private:
        TupleInvoke<TFunction, TTuple>  m_invoke;
    };


    //////////////////////////////////////////////////////////////////////////
    // ��ʱ���������
    class TimerTaskVirtual
    {
    public:
#pragma region ID����

#if defined(_MSC_VER)
        typedef unsigned __int64			        TimerId;
#elif __GNUC__
#if __WORDSIZE == 64
        typedef unsigned long int 			        TimerId;
#else
        __extension__ typedef unsigned long long	TimerId;
#endif
#else
        typedef unsigned long long			        TimerId;
#endif

        enum {
            INVALID_TID = 0, // ��Ч��ʱ��ID
        };
#pragma endregion

        typedef std::chrono::system_clock::time_point	system_time_point;

        TimerTaskVirtual()
            : m_id(INVALID_TID)
            , m_loop_index(0)
            , m_loop_count(-1)
            , m_interval(0)
        {}

        TimerTaskVirtual(unsigned int interval_ms, int loop_count, TimerId id, const system_time_point& time_point)
            : m_id(id)
            , m_time_point(time_point)
            , m_loop_index(0)
            , m_loop_count(loop_count)
            , m_interval(interval_ms)
        {}

        virtual ~TimerTaskVirtual() {}

        TimerId get_id() const {
            return m_id;
        }

        const system_time_point& get_time_point() const {
            return m_time_point;
        }

        void cancel() {
            m_loop_count = -1;
        }

        // ִ��һ�μ���ѭ��
        void loop_once_count() {
            if (!need_next()) {
                return;
            }
            if (m_loop_count != 0) {
                ++m_loop_index;
            }
        }

        // ִ��һ�ζ�ʱѭ��
        void loop_once_time() {
            m_time_point = m_time_point + std::chrono::milliseconds(m_interval);
        }

        bool need_next() const {
            return m_loop_count == 0 || m_loop_index.load() < m_loop_count;
        }

        // ִ�е��ú���
        virtual void invoke(const system_time_point& time_point) = 0;

    private:
        TimerId                 m_id;            // ��ʱ����ӦID
        int                     m_loop_count;    // ��ѭ��������Ĭ��Ϊ1, ��ʾһ��ѭ����0��ʾ����ѭ��
        std::atomic<int>        m_loop_index;    // ��ǰѭ������������ֵ
        unsigned int            m_interval;      // ѭ�����ʱ��,��λ����
        system_time_point       m_time_point;    // ��ʱ����ֹʱ��
    };
    typedef std::shared_ptr<TimerTaskVirtual> TimerTaskPtr;

    //////////////////////////////////////////////////////////////////////////
    // Ԫ�涨ʱ��������
    template<typename TFunction, typename TTuple>
    class TupleTimerTask : public TimerTaskVirtual
    {
    public:
        TupleTimerTask(unsigned int interval_ms, int loop_count, TimerId id
            , const system_time_point& time_point
            , TFunction&& func, std::shared_ptr<TTuple>&& t)
            : TimerTaskVirtual(interval_ms, loop_count, id, time_point)
            , m_fun_cbk(std::forward<TFunction>(func))
            , m_tuple(std::forward<std::shared_ptr<TTuple>>(t))
        {}

        ~TupleTimerTask() {}

#  if (!defined _MSC_VER) || (_MSC_VER < 1920) // C++17�汾�ſ�ʼ֧��std::apply
        // ִ�е��ú���
        void invoke(const system_time_point& time_point) {
            constexpr auto size = std::tuple_size<typename std::decay<TTuple>::type>::value;
            invoke_impl(time_point, std::make_index_sequence<size>{});
        }
    private:
        template<std::size_t... Index>
        decltype(auto) invoke_impl(const system_time_point& time_point, std::index_sequence<Index...>) {
            return m_fun_cbk(get_id(), time_point, std::get<Index>(std::forward<TTuple>(*m_tuple.get()))...);
        }
#  else
        // ִ�е��ú���
        void invoke(const system_time_point& time_point) {
            std::apply(get_id(), time_point, std::forward<TFunction>(m_fun_cbk), std::forward<TTuple>(*m_tuple.get()));
        }
#  endif

    private:
        std::shared_ptr<TTuple>         m_tuple;
        TFunction                       m_fun_cbk;
    };

}