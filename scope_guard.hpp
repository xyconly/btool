/************************************************************************
File name:  scope_guard.hpp
Author:     AChar
Purpose:  通用的、采用RAII思想的资源管理类
Note:   源自:https://stackoverflow.com/questions/10270328/the-simplest-and-neatest-c11-scopeguard

使用方法:
    // 退出即执行
    ScopeGuard scope_exit([&]{});
    // 异常退出时才执行
    ScopeGuard scope_fail(ScopeGuard::ExecutionType::Exception);
    // 无异常退出时才执行
    ScopeGuard scope_safe_exit([&]{}, ScopeGuard::ExecutionType::NoException);

    scope_exit += [&]{ ... };
    scope_exit.add([&]{ ... });

    scope_fail += [&]{ ... };
    scope_fail.add([&]{ ... });

    scope_safe_exit += [&]{ ... };
    scope_safe_exit.add([&]{ ... });

    scope_exit.dismiss();
    scope_fail.dismiss();
    scope_safe_exit.dismiss();
/************************************************************************/

#pragma once

#include <exception>
#include <functional>
#include <deque>

namespace BTool
{
    class ScopeGuard
    {
    public:
        // 函数队列执行情况
        enum class ExecutionType {
            Always,         // 总是执行
            NoException,    // 无异常时才执行
            Exception       // 有异常时才执行
        };

        explicit ScopeGuard(ExecutionType policy = ExecutionType::Always)
            : m_policy(policy)
        {}

        template<typename Function>
        ScopeGuard(Function&& func, ExecutionType policy = ExecutionType::Always)
            : m_policy(policy)
        {
            this->operator += <Function>(std::forward<Function>(func));
        }

        template<class Function>
        void add(Function && func) {
            this->operator += <Function>(std::forward<Function>(func));
        }

        template<class Function>
        ScopeGuard& operator += (Function && func) try {
            m_handlers.emplace_front(std::forward<Function>(func));
            return *this;
        }
        catch (...) {
            if (m_policy != ExecutionType::NoException) func();
            throw;
        }

        ~ScopeGuard() {
            if (m_policy == ExecutionType::Always ||
                (std::uncaught_exceptions() > 0 && (m_policy == ExecutionType::Exception)))
            {
                // 不要做异常捕获,虽然该函数是期望异常发生时也能执行
                // 但执行函数不应发生异常,如果发生也应该在外部通过其他机制捕获
                // 否则后期维护反而增加难度;若实在有需求则可放开注释即可
                for (auto &f : m_handlers)/* try {*/
                    f();
                /*}*/
//                 catch (...) { }
            }
        }

        void dismiss() noexcept {
            m_handlers.clear();
        }

    private:
        ScopeGuard(const ScopeGuard&) = delete;
        void operator=(const ScopeGuard&) = delete;

    private:
        std::deque<std::function<void()>>   m_handlers;
        ExecutionType                       m_policy;
    };
}
