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
#include <functional>

namespace BTool {

    template <typename T>
    class ObjectPool {
        using deleter_function = std::function<void(T*)>;
    public:
        using Type = std::unique_ptr<T, deleter_function>;
        ObjectPool(size_t capacity = 0) : m_capacity(capacity), m_size(0){}

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
            for (;;) {
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
                // 为空则判断是否已满, 已满则强制等待
                if(m_capacity > 0 && m_size >= m_capacity) {
                    std::this_thread::yield();
                    continue;
                }
                // 未满则新分配
                ++m_size;
                auto obj = std::malloc(sizeof(T));
                // printf("malloc:%p \n", (const char*)obj);
                if constexpr (std::is_trivial<T>::value) {
                    return Type(new (obj) T{ std::forward<Args>(args)... }, deleter);
                }
                else {
                    return Type(new (obj) T(std::forward<Args>(args)...), deleter);
                }
            }
        }

    private:
        size_t                                              m_capacity;
        size_t                                              m_size;
        std::mutex                                          m_mtx;
        std::queue<std::unique_ptr<T, deleter_function>>    m_pool;
    };
}