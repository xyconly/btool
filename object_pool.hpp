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

    // 对象池,用于存储连续属性节点
    template<typename T>
    class ObjectPoolNode {
    private:
        struct FreeNode {
            FreeNode* next;
        };

    public:
        explicit ObjectPoolNode(size_t preallocate = 4096, size_t chunk_size = 1024)
            : m_free_list(nullptr), m_memory(nullptr), m_chunk_size(chunk_size), m_capacity(preallocate) {
                allocate_new_chunk(preallocate); // 初始分配
        }

        ~ObjectPoolNode() {
            // 清理内存池的所有内存块
            for (void* block : m_blocks) {
                free(block);
            }
            m_blocks.clear();
        }

        template<typename... Args>
        inline T* allocate(Args&&... args) {
            void* ptr = nullptr;
            if (m_free_list) {
                // 从空闲链表获取一个空闲块
                ptr = m_free_list;
                m_free_list = m_free_list->next;
            } else {
                // 如果空闲链表没有对象了，分配新的内存分片
                allocate_new_chunk(m_chunk_size);
                ptr = m_free_list;
                m_free_list = m_free_list->next;
            }
            return new(ptr) T(std::forward<Args>(args)...); // placement new
        }

        inline void deallocate(T* obj) {
            if (!obj) return;

            obj->~T(); // 显式调用析构

            // 将对象回收到空闲链表
            auto* node = reinterpret_cast<FreeNode*>(obj);
            node->next = m_free_list;
            m_free_list = node;
        }

    private:
        // 分配新的内存块，并链接到当前的内存池中
        void allocate_new_chunk(const size_t& chunk_size) {
            // 计算新分片的内存大小
            size_t bytes = sizeof(T) * chunk_size;

            // 分配新的内存
            char* new_memory = static_cast<char*>(malloc(bytes));
            m_blocks.emplace_back(new_memory);
            
            // 将新分片链接到原来的空闲链表
            FreeNode* current = reinterpret_cast<FreeNode*>(new_memory);
            for (size_t i = 1; i < chunk_size; ++i) {
                FreeNode* next = reinterpret_cast<FreeNode*>(new_memory + i * sizeof(T));
                current->next = next;
                current = next;
            }
            current->next = m_free_list; // 连接到原来的空闲链表

            m_free_list = reinterpret_cast<FreeNode*>(new_memory);
            m_memory = new_memory;
            m_capacity += chunk_size;
        }

    private:
        FreeNode*   m_free_list;    // 
        char*       m_memory;       // 保存大块内存的起始地址
        size_t      m_chunk_size;   
        size_t      m_capacity;
        std::list<void*> m_blocks; // 保存所有分配过的内存块
    };            

}
