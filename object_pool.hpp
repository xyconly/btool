/*************************************************
File name:  object_pool.hpp
Author:     AChar
Version:
Date:
Purpose: 提供自动释放的对象池
Note:    必须确保使用对象的生命周期低于对象池本身
         确保所有子项均释放后方可析构对象池
*************************************************/
#pragma once
#include <thread>
#include <mutex>
#include <memory>
#include <queue>
#include <stack>
#include <functional>

namespace BTool {
    template <typename T>
    class ObjectPool {
    public:
        using PtrType = std::shared_ptr<T>;
        ObjectPool(int capacity = 0) : m_size(capacity) {
            if (capacity > 0) {
                for(int i = 0; i < capacity; ++i) {
                    m_pool.push(new T);
                }
            }
        }

        // 获取一个 T 类型的对象
        template<typename ...Args>
        PtrType acquire(Args... args) {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_pool.empty()) {
                ++m_size;
                if constexpr (std::is_trivial<T>::value) {
                    return PtrType(new T{ std::forward<Args>(args)... }, [this](T* ptr) { release(ptr); });
                }
                else {
                    return PtrType(new T( std::forward<Args>(args)... ), [this](T* ptr) { release(ptr); });
                }
            }
            else {
                auto pre_ptr = m_pool.top();
                PtrType ptr = nullptr;
                if constexpr (std::is_trivial<T>::value) {
                    ptr = PtrType(new (pre_ptr) T{ std::forward<Args>(args)... }, [this](T* ptr) { release(ptr); });
                }
                else {
                    ptr = PtrType(new (pre_ptr) T( std::forward<Args>(args)... ), [this](T* ptr) { release(ptr); });
                }
                m_pool.pop();
                return ptr;
            }
        }

        void release(T* ptr) {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_pool.push(ptr);
        }

    private:
        size_t                          m_size;
        std::mutex                      m_mtx;
        std::stack<T*>                  m_pool;
    };

    template <typename T>
    class ObjectPoolUni {
        using deleter_function = std::function<void(T*)>;
    public:
        using Type = std::unique_ptr<T, deleter_function>;
        ObjectPoolUni(){}

        template<typename ...Args>
        Type acquire(Args... args) {
            auto deleter = [ this ] (T* obj) {
                obj->~T();
                std::lock_guard<std::mutex> lock(m_mtx);
                m_pool.push(std::unique_ptr<T, deleter_function>(obj, std::free));
                // m_pool.push(std::unique_ptr<T, deleter_function>(obj, [] (T* obj) {
                //     printf("free:%p \n", (const char*)obj);
                //     std::free((void*)obj);
                // }));
            };

            std::lock_guard<std::mutex> lock(m_mtx);
            // 不为空则利用原始数据
            if (!m_pool.empty()) {
                auto ptr = std::move(m_pool.front());
                m_pool.pop();
                auto real_ptr = ptr.release();
                if constexpr (std::is_trivial<T>::value) {
                    real_ptr = new (real_ptr) T{ std::forward<Args>(args)... };
                }
                else {
                    real_ptr = new (real_ptr) T(std::forward<Args>(args)...);
                }

                return Type(real_ptr, deleter);
            }
            // 未满则新分配
            auto obj = std::malloc(sizeof(T));
            // printf("malloc:%p \n", (const char*)obj);
            if constexpr (std::is_trivial<T>::value) {
                return Type(new (obj) T{ std::forward<Args>(args)... }, deleter);
            }
            else {
                return Type(new (obj) T(std::forward<Args>(args)...), deleter);
            }
        }

    private:
        std::mutex                                          m_mtx;
        std::queue<std::unique_ptr<T, deleter_function>>    m_pool;
    };
}
