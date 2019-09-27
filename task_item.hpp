/*************************************************
File name:  task_item.hpp
Author:     AChar
Version:
Date:
Description:    提供各类任务基类,避免外界重复创建
*************************************************/
#pragma once
#include <type_traits>
#include <future>

namespace BTool
{
    //////////////////////////////////////////////////////////////////////////
    // 任务基类
    class TaskVirtual {
    public:
        // 执行调用函数
        virtual void invoke() = 0;
    };
    typedef std::shared_ptr<TaskVirtual>  TaskPtr;

    //////////////////////////////////////////////////////////////////////////
    // 带属性任务基类
    template<typename TPropType>
    class PropTaskVirtual : public TaskVirtual
    {
    public:
        template<typename AsTPropType>
        PropTaskVirtual(AsTPropType&& prop) : m_prop(std::forward<AsTPropType>(prop)) {}
        virtual ~PropTaskVirtual() {}

        // 获取任务属性
        const TPropType& get_prop_type() const {
            return m_prop;
        }
    private:
        TPropType m_prop;
    };

    //////////////////////////////////////////////////////////////////////////
    // 元祖执行类
    template<typename TFunction, typename TTuple>
    class TupleInvoke
    {
    public:
        TupleInvoke(TFunction&& func, std::shared_ptr<TTuple>&& t)
            : m_fun_cbk(std::forward<TFunction>(func))
            , m_tuple(std::forward<std::shared_ptr<TTuple>>(t))
        {}
        ~TupleInvoke() {}

#  if (!defined _MSC_VER) || (_MSC_VER < 1920) // C++17版本才开始支持std::apply
        // 执行调用函数
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
        // 执行调用函数
        void invoke() {
            std::apply(std::forward<TFunction>(m_fun_cbk), std::forward<TTuple>(*m_tuple.get()));
        }
#  endif

    private:
        std::shared_ptr<TTuple>     m_tuple;
        TFunction                   m_fun_cbk;
    };

    //////////////////////////////////////////////////////////////////////////
    // 打包执行类
//     class PackagedInvoke : public TaskVirtual
//     {
//     public:
//         typedef  std::future<void>    future_type;
// 
//         template<typename TFunction, typename... Args>
//         PackagedInvoke(TFunction&& func, Args&&... args) {
//             typedef typename std::result_of<TFunction(Args&&...)>::type  result_type;
//             auto task = std::make_shared<std::packaged_task<result_type()>>(
//                 std::bind(std::forward<TFunction>(func), std::forward<Args>(args)...)
//                 );
//             m_invoke = std::async(std::launch::deferred, [task] {(*task)(); });
//         }
//         ~PackagedInvoke() {}
// 
//         void invoke() {
//             m_invoke.get();
//         }
// 
//     private:
//         future_type m_invoke;
//     };

    //////////////////////////////////////////////////////////////////////////
    // 元祖任务类
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
    // 打包任务类
//     class PackagedTask : public TaskVirtual
//     {
//     public:
//         template<typename TFunction, typename... Args>
//         PackagedTask(TFunction&& func, Args&&... args)
//             : m_invoke(std::forward<TFunction>(func), std::forward<Args>(args)...)
//         {}
//         void invoke() override {
//             m_invoke.invoke();
//         }
//     private:
//         PackagedInvoke  m_invoke;
//     };

    //////////////////////////////////////////////////////////////////////////
    // 带属性元祖任务类
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
    // 带属性打包任务类
//     template<typename TPropType>
//     class PropPackagedTask : public PropTaskVirtual<TPropType>
//     {
//     public:
//         template<typename AsTPropType, typename TFunction, typename... Args>
//         PropPackagedTask(AsTPropType&& prop, TFunction&& func, Args&&... args)
//             : PropTaskVirtual<TPropType>(std::forward<AsTPropType>(prop))
//             , m_invoke(std::forward<TFunction>(func), std::forward<Args>(args)...)
//         {
//         }
// 
//         void invoke() override {
//             m_invoke.invoke();
//         }
// 
//     private:
//         PackagedInvoke  m_invoke;
//     };


    //////////////////////////////////////////////////////////////////////////
    // 定时器任务基类
    class TimerTaskVirtual
    {
    public:
#pragma region ID定义

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
            INVALID_TID = 0, // 无效定时器ID
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

        // 执行一次计数循环
        void loop_once_count() {
            if (!need_next()) {
                return;
            }
            if (m_loop_count != 0) {
                ++m_loop_index;
            }
        }

        // 执行一次定时循环
        void loop_once_time() {
            m_time_point = m_time_point + std::chrono::milliseconds(m_interval);
        }

        bool need_next() const {
            return m_loop_count == 0 || m_loop_index.load() < m_loop_count;
        }

        // 执行调用函数
        virtual void invoke(const system_time_point& time_point) = 0;

    private:
        TimerId                 m_id;            // 定时器对应ID
        int                     m_loop_count;    // 总循环次数，默认为1, 表示一次循环，0表示无限循环
        std::atomic<int>        m_loop_index;    // 当前循环计数器计数值
        unsigned int            m_interval;      // 循环间隔时间,单位毫秒
        system_time_point       m_time_point;    // 定时器终止时间
    };
    typedef std::shared_ptr<TimerTaskVirtual> TimerTaskPtr;

    //////////////////////////////////////////////////////////////////////////
    // 元祖定时器任务类
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

#  if (!defined _MSC_VER) || (_MSC_VER < 1920) // C++17版本才开始支持std::apply
        // 执行调用函数
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
        // 执行调用函数
        void invoke(const system_time_point& time_point) {
            std::apply(std::forward<TFunction>(m_fun_cbk), get_id(), time_point, std::forward<TTuple>(*m_tuple.get()));
        }
#  endif

    private:
        std::shared_ptr<TTuple>         m_tuple;
        TFunction                       m_fun_cbk;
    };

    //////////////////////////////////////////////////////////////////////////
    // 打包定时器任务类, 由于std::future仅能执行单次,shared_future不便存储,故而暂时不用
//     class PackagedTimerTask : public TimerTaskVirtual
//     {
//         // 打包执行类
//         class PackagedTimerInvoke
//         {
//         public:
//             typedef  std::function<void(TimerId id, const system_time_point& time_point)>    function_type;
// 
//             template<typename TFunction, typename... Args>
//             PackagedTimerInvoke(TFunction&& func, Args&&... args) {
//                 typedef typename std::result_of<TFunction(TimerId, const system_time_point&, Args&&...)>::type  result_type;
//                 auto task = std::make_shared<std::packaged_task<result_type(TimerId, const system_time_point&)>>(
//                     std::bind(std::forward<TFunction>(func), std::placeholders::_1, std::placeholders::_2, std::forward<Args>(args)...)
//                     );
// 
//                 m_invoke = function_type([task](TimerId id, const system_time_point& time_point) {
//                     (*task)(id, time_point);
//                 });
//             }
//             ~PackagedTimerInvoke() {}
// 
//             void invoke(TimerId id, const system_time_point& time_point) {
//                 m_invoke(id, time_point);
//             }
// 
//         private:
//             function_type m_invoke;
//         };
// 
//     public:
//         template<typename TFunction, typename... Args>
//         PackagedTimerTask(unsigned int interval_ms, int loop_count, TimerId id
//             , const system_time_point& time_point
//             , TFunction&& func, Args&&... args)
//             : TimerTaskVirtual(interval_ms, loop_count, id, time_point)
//             , m_invoke(std::forward<TFunction>(func), std::forward<Args>(args)...)
//         {}
// 
//         ~PackagedTimerTask() {}
// 
//         // 执行调用函数
//         void invoke(const system_time_point& time_point) override {
//             m_invoke.invoke(get_id(), time_point);
//         }
// 
//     private:
//         PackagedTimerInvoke  m_invoke;
//     };

}