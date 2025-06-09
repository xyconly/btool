/*************************************************
File name:  fast_function.hpp
Author:
Version:
Date:
Description:    比 std::function 更快（避免堆分配、小对象优化）
                支持捕获 lambda 和普通函数
                无动态类型擦除虚表开销
Demo:
        std::vector<FastFunction> cbs;
        void do_something(FastFunction cb) {
            cbs.emplace_back(std::move(cb));
        }
        do_something([x]() {});
*************************************************/
#include <cassert>
#include <cstring>
#include <type_traits>
#include <utility>

namespace BTool {
    class FastFunction {
    public:
        // 最大捕获对象大小
        static constexpr size_t MAX_BUFFER_SIZE = 128;
        static constexpr size_t MAX_BUFFER_ALIGN = alignof(std::max_align_t) * 2;

        FastFunction() noexcept = default;
        FastFunction(std::nullptr_t) noexcept : m_invoke_fn(nullptr), m_destroy_fn(nullptr) {}

        template <typename TFunction>
        FastFunction(TFunction&& cb) noexcept {
            static_assert(sizeof(TFunction) <= MAX_BUFFER_SIZE, "TFunction too big for FastFunction");
            static_assert(alignof(TFunction) <= MAX_BUFFER_ALIGN, "TFunction alignment too strict");
            static_assert(std::is_invocable_r_v<void, TFunction>, "TFunction must be void()");

            new (&m_storage) TFunction(std::forward<TFunction>(cb));
            m_invoke_fn = [](void* ptr) {
                (*reinterpret_cast<TFunction*>(ptr))();
            };
            m_destroy_fn = [](void* ptr) {
                reinterpret_cast<TFunction*>(ptr)->~TFunction();
            };
        }

        FastFunction(const FastFunction&) = delete;  // 禁止拷贝
        FastFunction& operator=(const FastFunction&) = delete;

        FastFunction(FastFunction&& other) noexcept {
            std::memcpy(&m_storage, &other.m_storage, MAX_BUFFER_SIZE);
            m_invoke_fn = other.m_invoke_fn;
            m_destroy_fn = other.m_destroy_fn;
            other.m_invoke_fn = nullptr;
            other.m_destroy_fn = nullptr;
        }

        FastFunction& operator=(FastFunction&& other) noexcept {
            if (this != &other) {
                reset();
                std::memcpy(&m_storage, &other.m_storage, MAX_BUFFER_SIZE);
                m_invoke_fn = other.m_invoke_fn;
                m_destroy_fn = other.m_destroy_fn;
                other.m_invoke_fn = nullptr;
                other.m_destroy_fn = nullptr;
            }
            return *this;
        }

        ~FastFunction() {
            reset();
        }

        void operator()() {
            assert(m_invoke_fn != nullptr && "Calling empty FastFunction!");
            m_invoke_fn(&m_storage);
        }

        explicit operator bool() const {
            return m_invoke_fn != nullptr;
        }

    private:
        void reset() {
            if (m_destroy_fn) {
                m_destroy_fn(&m_storage);
            }
            m_invoke_fn = nullptr;
            m_destroy_fn = nullptr;
        }

    private:
        alignas(MAX_BUFFER_ALIGN) std::byte m_storage[MAX_BUFFER_SIZE];
        void (*m_invoke_fn)(void*) = nullptr;
        void (*m_destroy_fn)(void*) = nullptr;
    };
}