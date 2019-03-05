/*************************************************
File name:      memory_stream.hpp
Author:			AChar
Version:
Date:
Purpose: ʵ���Թ�����ڴ����ӿ�
Note:    �����ڲ����̰߳�ȫ����,���в����������ȷ���̰߳�ȫ
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
        // �ڴ������
        MemoryStream()
            : m_buffer_size(0)
            , m_offset(0)
            , m_capacity(0)
            , m_buffer(nullptr)
            , m_auto_delete(true)
        {
        }

        // �ڴ������
        // buffer: ָ���ڴ�
        // len: ����
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
        // ��ȡ��������
        const char* const data() const {
            return m_buffer;
        }

        // ��ȡ��ǰ���泤��
        size_t size() const {
            return m_buffer_size;
        }

        // ��ȡ�ڴ�������
        size_t get_capacity() const {
            return m_capacity;
        }
        // ��ȡ�ڴ�ȥ��Ư�ƺ�ʣ�೤��
        size_t get_length() const {
            return m_capacity - m_offset;
        }

        // ��ȡ��ǰƯ��λ
        size_t get_offset() const {
            return m_offset;
        }

        // ���õ�ǰƯ��λ
        void reset_offset(int offset) {
            m_offset = offset;
        }

        // �����ڴ�ָ��
        void attach(char* src, size_t len) {
            if (m_buffer && m_auto_delete)
                free(m_buffer);

            m_buffer_size = len;
            m_buffer = src;
            m_capacity = len;
            m_offset = 0;
            m_auto_delete = false;
        }

        // �ͷŵ�ǰ�ڴ������������ڵĹ���
        char* detach() {
            char* buffer = m_buffer;

            m_buffer_size = 0;
            m_offset = 0;
            m_capacity = 0;
            m_buffer = nullptr;
            m_auto_delete = true;

            return buffer;
        }

        // ����
        void reset() {
            m_buffer_size = 0;
            m_offset = 0;
            m_capacity = 0;
            if (m_buffer && m_auto_delete)
                free(m_buffer);
            m_buffer = nullptr;
            m_auto_delete = true;
        }

        // ׷���������ڴ�,����׷�ӳ���
        void append(const void* buffer, size_t len) {
            if (!buffer || len == 0)
                return;

            // ����
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

        // ��ȡ�������ڴ�
        void read(void* buffer, size_t len) {
            if (len == 0)
                return;

            memcpy(buffer, m_buffer + m_offset, len);
        }
        // ��ȡ��ǰƯ��λ�²�����ֵ,����Ư��λ����
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
        // ������len�ĳ���
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
        size_t      m_buffer_size;  // ��ǰ�����ܳ���
        size_t      m_offset;       // ��ǰ�ڴ�Ư��λ
        size_t      m_capacity;     // ��ǰ������
        char*       m_buffer;       // ��ǰ����ָ��
        bool        m_auto_delete;  // �Ƿ�������ʱ�Զ�ɾ���ڴ�
    };
}