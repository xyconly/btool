/*************************************************
File name:  task_item.hpp
Author:     AChar
Version:
Date:
Description:    提供各类任务基类,避免外界重复创建
*************************************************/
#pragma once
#include <type_traits>

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
    // 元祖执行类
    template<typename TFunction, typename TTuple>
    class TupleInvoke
    {
    public:
        TupleInvoke(TFunction&& func, std::shared_ptr<TTuple>&& t)
            : fun_cbk_(std::forward<TFunction>(func))
            , tuple_(std::forward<std::shared_ptr<TTuple>>(t))
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
            return fun_cbk_(std::get<Index>(std::forward<TTuple>(*tuple_.get()))...);
        }
#  else
        // 执行调用函数
        void invoke() {
            std::apply(std::forward<Function>(fun_cbk_), std::forward<TTuple>(*tuple_.get()));
        }
#  endif

    private:
        std::shared_ptr<TTuple>     tuple_;
        TFunction                   fun_cbk_;
    };

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
    // 带属性任务基类
    template<typename TPropType>
    class PropTaskVirtual : public TaskVirtual
    {
    public:
        PropTaskVirtual(const TPropType& prop) : m_prop(prop) {}
        PropTaskVirtual(TPropType&& prop) : m_prop(std::forward<TPropType>(prop)) {}
        virtual ~PropTaskVirtual() {}

        // 获取任务属性
        const TPropType& get_prop_type() const {
            return m_prop;
        }
    private:
        TPropType m_prop;
    };

    //////////////////////////////////////////////////////////////////////////
    // 带属性元祖任务类
    template<typename TPropType, typename TFunction, typename TTuple>
    class PropTupleTask : public PropTaskVirtual<TPropType>
    {
    public:
        PropTupleTask(const TPropType& prop, TFunction&& func, std::shared_ptr<TTuple>&& t)
            : PropTaskVirtual<TPropType>(prop)
            , m_invoke(std::forward<TFunction>(func), std::forward<std::shared_ptr<TTuple>>(t))
        {}
        PropTupleTask(TPropType&& prop, TFunction&& func, std::shared_ptr<TTuple>&& t)
            : PropTaskVirtual<TPropType>(std::forward<TPropType>(prop))
            , m_invoke(std::forward<TFunction>(func), std::forward<std::shared_ptr<TTuple>>(t))
        {}

        void invoke() override {
            m_invoke.invoke();
        }
    private:
        TupleInvoke<TFunction, TTuple>  m_invoke;
    };

}