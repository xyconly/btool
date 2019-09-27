/*************************************************
File name:      memory_stream.hpp
Author:			AChar
Version:
Date:
Purpose: 实现自管理的内存流接口
Note:    不在内部做线程安全管理,所有操作需在外界确保线程安全
*************************************************/
#pragma once

#include <memory>

#ifdef _MSC_VER
# include <stdint.h>
#elif defined(__GNUC__)
// # include <arpa/inet.h>
#endif

namespace BTool {
    class MemoryStream
    {
        MemoryStream(MemoryStream&& rhs) = delete;
        MemoryStream(const MemoryStream& rhs) = delete;
        MemoryStream& operator=(const MemoryStream& rhs) = delete;

    public:
        // 内存管理流,此后将自动管理内存释放
        MemoryStream()
            : m_buffer_size(0)
            , m_offset(0)
            , m_capacity(0)
            , m_buffer(nullptr)
            , m_auto_delete(true)
        {
        }

        // 内存管理流,此时将不再自动管理内存释放
        // buffer: 指向内存
        // len: 长度
        MemoryStream(char* buffer, size_t len)
            : m_buffer_size(len)
            , m_offset(0)
            , m_capacity(len)
            , m_buffer(buffer)
            , m_auto_delete(false)
        {
        }

        virtual ~MemoryStream() {
            clear();
        }

    public:
        // 获取缓存内容
        const char* const data() const {
            return m_buffer;
        }

        // 获取当前缓存长度
        size_t size() const {
            return m_buffer_size;
        }

        // 获取内存总容量
        size_t get_capacity() const {
            return m_capacity;
        }
        // 获取内存去除漂移后剩余长度
        size_t get_res_length() const {
            return m_buffer_size - m_offset;
        }
        // 获取当前数据长度
        size_t get_length() const {
            return m_buffer_size;
        }
        // 获取当前漂移位数
        size_t get_offset() const {
            return m_offset;
        }
        // 重置当前漂移位至指定位置
        void reset_offset(int offset) {
            m_offset = offset;
        }
        // 重置当前漂移位至最新位置
        void reset_offset() {
            m_offset = m_buffer_size;
        }
        // 重置内存指向,此时将不再自动管理内存释放
        void attach(char* src, size_t len) {
            if (m_buffer && m_auto_delete)
                free(m_buffer);

            m_buffer_size = len;
            m_buffer = src;
            m_capacity = len;
            m_offset = 0;
            m_auto_delete = false;
        }
        // 释放当前内存区域生命周期的管理,此后将自动管理内存释放
        char* detach() {
            char* buffer = m_buffer;
            m_buffer_size = 0;
            m_offset = 0;
            m_capacity = 0;
            m_buffer = nullptr;
            m_auto_delete = true;
            return buffer;
        }

        // 清空数据
        void clear() {
            m_buffer_size = 0;
            m_offset = 0;
            m_capacity = 0;
            if (m_buffer && m_auto_delete)
                free(m_buffer);
            m_buffer = nullptr;
            m_auto_delete = true;
        }

        // 重置,不释放冗余缓存,仅重置offset及size,data及capacity不变
        void reset() {
            m_buffer_size = 0;
            m_offset = 0;
        }

        // 追加数据至指定位置的内存处
        void append(const void* buffer, size_t len, size_t offset) {
            if (!buffer || len == 0)
                return;

            // 超长
            size_t new_capacity = offset + len;
            while (new_capacity > m_capacity) {
                reset_capacity(m_capacity == 0 ? new_capacity : m_capacity * 2);
            }

            memcpy(m_buffer + offset, buffer, len);

            if(offset + len > m_buffer_size)
                m_buffer_size = offset + len;
        }
        // 追加数据至最新位置的内存处
        void append(const void* buffer, size_t len) {
            if (!buffer || len == 0)
                return;

            // 超长
            size_t new_capacity = m_offset + len;
            while (new_capacity > m_capacity) {
                reset_capacity(m_capacity == 0 ? new_capacity : m_capacity * 2);
            }

            memcpy(m_buffer + m_offset, buffer, len);
            m_offset += len;
            m_buffer_size += len;
        }
        // 追加数据至最新位置的内存处
        template<typename Type>
        void append(Type src) {
            return append(&src, sizeof(Type));
        }

        // 读取数据当前漂移位下参数数值
        // offset_flag : 是否需要漂移当前漂移位
        void read(void* buffer, size_t len, bool offset_flag = true) {
            if (len == 0)
                return;

            memcpy(buffer, m_buffer + m_offset, len);

            if (offset_flag)
                m_offset += len;
        }
        template<typename Type>
        // 读取当前漂移位下参数数值
        // offset_flag : 是否需要漂移当前漂移位
        void read(Type* pDst, bool offset_flag = true) {
            read(pDst, sizeof(Type), offset_flag);
        }

    protected:
        // 扩容至len的长度
        void reset_capacity(size_t len) {
            if (!m_auto_delete)
            {
                m_buffer = (char*)malloc(len);
                m_auto_delete = true;
                m_capacity = len;
                return;
            }

            if (m_buffer)
                m_buffer = (char*)realloc(m_buffer, len);
            else
                m_buffer = (char*)malloc(len);

            m_capacity = len;
        }

    private:
        size_t      m_buffer_size;  // 当前缓存总长度
        size_t      m_offset;       // 当前内存漂移位
        size_t      m_capacity;     // 当前总容量
        char*       m_buffer;       // 当前缓存指针
        bool        m_auto_delete;  // 是否在析构时自动删除内存
    };
}