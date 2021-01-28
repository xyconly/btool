/************************************************************************
File name:  scope_guard.hpp
Author:     AChar
Purpose:  ͨ�õġ�����RAII˼�����Դ������
Note:   Դ��:https://stackoverflow.com/questions/10270328/the-simplest-and-neatest-c11-scopeguard

ʹ�÷���:
    // �˳���ִ��
    ScopeGuard scope_exit([&]{});
    // �쳣�˳�ʱ��ִ��
    ScopeGuard scope_fail(ScopeGuard::ExecutionType::Exception);
    // ���쳣�˳�ʱ��ִ��
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
        // ��������ִ�����
        enum class ExecutionType {
            Always,         // ����ִ��
            NoException,    // ���쳣ʱ��ִ��
            Exception       // ���쳣ʱ��ִ��
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
                // ��Ҫ���쳣����,��Ȼ�ú����������쳣����ʱҲ��ִ��
                // ��ִ�к�����Ӧ�����쳣,�������ҲӦ�����ⲿͨ���������Ʋ���
                // �������ά�����������Ѷ�;��ʵ����������ɷſ�ע�ͼ���
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
