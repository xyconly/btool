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

    public:
        // 内存管理流
        MemoryStream()
            : m_buffer_size(0)
            , m_offset(0)
            , m_capacity(0)
            , m_buffer(nullptr)
            , m_auto_delete(true)
        {
        }

        // 内存管理流
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
            reset();
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
        size_t get_length() const {
            return m_capacity - m_offset;
        }

        // 获取当前漂移位
        size_t get_offset() const {
            return m_offset;
        }

        // 重置当前漂移位
        void reset_offset(int offset) {
            m_offset = offset;
        }

        // 重置内存指向
        void attach(char* src, size_t len) {
            if (m_buffer && m_auto_delete)
                free(m_buffer);

            m_buffer_size = len;
            m_buffer = src;
            m_capacity = len;
            m_offset = 0;
            m_auto_delete = false;
        }

        // 释放当前内存区域生命周期的管理
        char* detach() {
            char* buffer = m_buffer;

            m_buffer_size = 0;
            m_offset = 0;
            m_capacity = 0;
            m_buffer = nullptr;
            m_auto_delete = true;

            return buffer;
        }

        // 重置
        void reset() {
            m_buffer_size = 0;
            m_offset = 0;
            m_capacity = 0;
            if (m_buffer && m_auto_delete)
                free(m_buffer);
            m_buffer = nullptr;
            m_auto_delete = true;
        }

        // 追加数据至内存,返回追加长度
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
        void append_int8(int8_t src) {
            return append(&src, sizeof(int8_t));
        }
        void append_uint8(uint8_t src) {
            return append(&src, sizeof(uint8_t));
        }
        void append_int16(int16_t src) {
            return append(&src, sizeof(int16_t));
        }
        void append_uint16(uint16_t src) {
            return append(&src, sizeof(uint16_t));
        }
        void append_int32(int32_t src) {
            return append(&src, sizeof(int32_t));
        }
        void append_uint32(uint32_t src) {
            return append(&src, sizeof(uint32_t));
        }
        void append_int64(int64_t src) {
            return append(&src, sizeof(int64_t));
        }
        void append_uint64(uint64_t src) {
            return append(&src, sizeof(uint64_t));
        }
        void append_int(int src) {
            return append(&src, sizeof(int));
        }
        void append_uint(unsigned int src) {
            return append(&src, sizeof(unsigned int));
        }
        void append_float(float src) {
            return append(&src, sizeof(float));
        }
        void append_double(double src) {
            return append(&src, sizeof(double));
        }
        void append_char(char src) {
            return append(&src, sizeof(char));
        }

        // 读取数据至内存
        void read(void* buffer, size_t len) {
            if (len == 0)
                return;

            memcpy(buffer, m_buffer + m_offset, len);
        }
        // 读取当前漂移位下参数数值,并将漂移位下移
        void read_int8(int8_t* pDst) {
            read(pDst, sizeof(int8_t));
        }
        void read_uint8(uint8_t* pDst) {
            read(pDst, sizeof(uint8_t));
        }
        void read_int16(int16_t* pDst) {
            read(pDst, sizeof(int16_t));
        }
        void read_uint16(uint16_t* pDst) {
            read(pDst, sizeof(uint16_t));
        }
        void read_int32(int32_t* pDst) {
            read(pDst, sizeof(int32_t));
        }
        void read_uint32(uint32_t* pDst) {
            read(pDst, sizeof(uint32_t));
        }
        void read_int64(int64_t* pDst) {
            read(pDst, sizeof(int64_t));
        }
        void read_uint64(uint64_t* pDst) {
            read(pDst, sizeof(int8_t));
        }
        void read_float(float* pDst) {
            read(pDst, sizeof(float));
        }
        void read_double(double* pDst) {
            read(pDst, sizeof(double));
        }
        void read_char(char* pDst) {
            read(pDst, sizeof(char));
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