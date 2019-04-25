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
        // �ڴ������,�˺��Զ������ڴ��ͷ�
        MemoryStream()
            : m_buffer_size(0)
            , m_offset(0)
            , m_capacity(0)
            , m_buffer(nullptr)
            , m_auto_delete(true)
        {
        }

        // �ڴ������,��ʱ�������Զ������ڴ��ͷ�
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
        size_t get_res_length() const {
            return m_buffer_size - m_offset;
        }
        // ��ȡ��ǰ���ݳ���
        size_t get_length() const {
            return m_buffer_size;
        }
        // ��ȡ��ǰƯ��λ��
        size_t get_offset() const {
            return m_offset;
        }
        // ���õ�ǰƯ��λ��ָ��λ��
        void reset_offset(int offset) {
            m_offset = offset;
        }
        // ���õ�ǰƯ��λ������λ��
        void reset_offset() {
            m_offset = m_buffer_size;
        }
        // �����ڴ�ָ��,��ʱ�������Զ������ڴ��ͷ�
        void attach(char* src, size_t len) {
            if (m_buffer && m_auto_delete)
                free(m_buffer);

            m_buffer_size = len;
            m_buffer = src;
            m_capacity = len;
            m_offset = 0;
            m_auto_delete = false;
        }
        // �ͷŵ�ǰ�ڴ������������ڵĹ���,�˺��Զ������ڴ��ͷ�
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

        // ׷��������ָ��λ�õ��ڴ洦
        void append(const void* buffer, size_t len, size_t offset) {
            if (!buffer || len == 0)
                return;

            // ����
            size_t new_capacity = offset + len;
            while (new_capacity > m_capacity) {
                reset_capacity(m_capacity == 0 ? new_capacity : m_capacity * 2);
            }

            memcpy(m_buffer + offset, buffer, len);

            if(offset + len > m_buffer_size)
                m_buffer_size = offset + len;
        }
        // ׷������������λ�õ��ڴ洦
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
        // ׷������������λ�õ��ڴ洦
        template<typename Type>
        void append(Type src) {
            return append(&src, sizeof(Type));
        }

        // ��ȡ���ݵ�ǰƯ��λ�²�����ֵ
        // offset_flag : �Ƿ���ҪƯ�Ƶ�ǰƯ��λ
        void read(void* buffer, size_t len, bool offset_flag = true) {
            if (len == 0)
                return;

            memcpy(buffer, m_buffer + m_offset, len);

            if (offset_flag)
                m_offset += len;
        }
        template<typename Type>
        // ��ȡ��ǰƯ��λ�²�����ֵ
        // offset_flag : �Ƿ���ҪƯ�Ƶ�ǰƯ��λ
        void read(Type* pDst, bool offset_flag = true) {
            read(pDst, sizeof(Type), offset_flag);
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