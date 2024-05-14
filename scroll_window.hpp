/******************************************************************************
File name:  scroll_window.hpp
Author:     AChar
Purpose:    ��������, ��������
            ScrollWindowArray: �����������Ժ�, ���´ӵ�һ�����ݿ�ʼ����, ͨ���α�m_cur_begin_index����ǵ�ǰ�ĵ�һ�����ݵ�ַ
                               �����ڴ治���������
            ScrollWindowVector: �����������Ժ�, pop��һ��
                                �ڴ�ʼ������, ���ܽϵ�

Note:       ���̰߳�ȫ
*****************************************************************************/
#pragma once

#include <array>
#include <vector>
#include <assert.h>

namespace BTool {
    // FIX_LENGTH: �̶�����, Ϊ0�򲻹̶�
    template<typename TDataType, size_t FIX_LENGTH>
    class ScrollWindowArray {    
    public:
        class iterator {
        public:
            iterator(TDataType* data, size_t cur_index, ScrollWindowArray<TDataType, FIX_LENGTH>* parent)
             : m_data(data), m_cur_index(cur_index), m_parent(parent)
            {
            }
            iterator(const TDataType* data, size_t cur_index, const ScrollWindowArray<TDataType, FIX_LENGTH>* parent)
             : m_data(const_cast<TDataType>(data)), m_cur_index(cur_index), m_parent(const_cast<ScrollWindowArray<TDataType, FIX_LENGTH>>(parent))
            {
            }

            // ǰ�õ�������
            iterator& operator++() {
                if (++m_cur_index == FIX_LENGTH && m_parent->m_cur_index + 1 < FIX_LENGTH) {
                    m_cur_index = 0;
                }
                
                if (m_cur_index == m_parent->m_cur_index + 1) {
                    m_cur_index = FIX_LENGTH;
                    m_data = nullptr;
                }
                else{
                    m_data = &m_parent->m_datas[m_cur_index];
                }
                return *this;
            }

            // �����ò���
            TDataType& operator*() {
                return *m_data;
            }
            // �����ò���
            const TDataType& operator*() const {
                return *m_data;
            }

            bool valid() const {
                return m_cur_index != FIX_LENGTH;
            }

            // �Ƚϲ���
            bool operator!=(const iterator& other) const {
                return m_cur_index != other.m_cur_index;
            }
        private:
            TDataType* m_data;
            size_t m_cur_index;
            ScrollWindowArray<TDataType, FIX_LENGTH>* m_parent;
        };
        friend class iterator;

    // private:
    //     template<size_t ...Ns>
    //     ScrollWindowArray(std::initializer_list<TDataType>& list, std::index_sequence<Ns...>)
    //     : m_datas{ *(list.begin() + Ns) ... }, m_cur_index(list.size() - 1), m_cur_length(list.size()) {
    //         if constexpr (FIX_LENGTH == 0)
    //             throw std::out_of_range("that's crazy!");
    //     }
    public:
        ScrollWindowArray() : m_cur_index(-1), m_cur_length(0) {}

        // template<typename... Args, typename t = typename std::enable_if<sizeof...(Args) >= 1, void >::type>
        // ScrollWindowArray(Args&&... args) : m_datas({std::forward<Args>(args)...}), m_cur_index(sizeof...(args) - 1), m_cur_length(sizeof...(args)) {}

        // ScrollWindowArray(std::initializer_list<TDataType>& list) 
        //     : ScrollWindowArray(list, std::make_index_sequence<FIX_LENGTH>())
        // { 
        //     if constexpr (FIX_LENGTH == 0 || FIX_LENGTH < list.size())
        //         throw std::out_of_range("that's crazy!");
        // }
        
        ~ScrollWindowArray() {
            clear();
        }

        // ��ʼ��Χ������
        iterator begin() {
            if (empty())
                return empty_iterator();
            if (m_cur_length < FIX_LENGTH)
                return iterator(&m_datas[0], 0, this);

            size_t index = (m_cur_index + 1) % FIX_LENGTH;
            return iterator(&m_datas[index], index, this);
        }
        
        const iterator begin() const {
            if (empty())
                return empty_iterator();
            if (m_cur_length < FIX_LENGTH)
                return iterator(&m_datas[0], 0, this);

            size_t index = (m_cur_index + 1) % FIX_LENGTH;
            return iterator(&m_datas[index], index, this);
        }

        // ������Χ������
        iterator end() {
            return empty_iterator();
        }
        const iterator end() const {
            return empty_iterator();
        }


        constexpr size_t capacity() const {
            return FIX_LENGTH;
        }

        inline size_t length() const {
            return m_cur_length;
        }

        inline bool empty() const {
            return length() == 0;
        }

        // �򻬶�����������µ���������
        template<typename T>
        void emplace_back(T&& data) {
            if (++m_cur_index >= FIX_LENGTH) {
                m_cur_index -= FIX_LENGTH;
            }
            if (m_cur_length < FIX_LENGTH)
                ++m_cur_length;
            m_datas[m_cur_index] = std::forward<T>(data);
        }

        // ��ȡ��ǰ���������е���������
        iterator at(size_t start_index) {
            size_t index = (m_cur_index + start_index + 1) % FIX_LENGTH;
            return iterator(&m_datas[index], index, this);
        }

        const iterator at(size_t start_index) const {
            size_t index = (m_cur_index + start_index + 1) % FIX_LENGTH;
            return iterator(&m_datas[index], index, this);
        }

        TDataType& back() {
            static TDataType* s_null = nullptr;
            if (empty())
                return *s_null;
            return m_datas[m_cur_index];
        }

        const TDataType& back() const {
            static TDataType* s_null = nullptr;
            if (empty())
                return *s_null;
            return m_datas[m_cur_index];
        }

        iterator latest(size_t need_size, size_t& real_size) {
            if (need_size >= m_cur_length) {
                real_size = m_cur_length;
                return iterator(&m_datas[(m_cur_index + 1) % FIX_LENGTH], (m_cur_index + 1) % FIX_LENGTH, this);
            }
            real_size = need_size;
            if (m_cur_index >= need_size - 1)
                return iterator(&m_datas[m_cur_index - need_size + 1], m_cur_index - need_size + 1, this);
            return iterator(&m_datas[(m_cur_index + m_cur_length - need_size + 1) % FIX_LENGTH], (m_cur_index + m_cur_length - need_size + 1) % FIX_LENGTH, this);
        }

        const iterator latest(size_t need_size, size_t& real_size) const {
            if (need_size >= m_cur_length) {
                real_size = m_cur_length;
                return iterator(&m_datas[(m_cur_index + 1) % FIX_LENGTH], (m_cur_index + 1) % FIX_LENGTH, this);
            }
            real_size = need_size;
            if (m_cur_index >= need_size - 1)
                return iterator(&m_datas[m_cur_index - need_size + 1], m_cur_index - need_size + 1, this);
            return iterator(&m_datas[(m_cur_index + m_cur_length - need_size + 1) % FIX_LENGTH], (m_cur_index + m_cur_length - need_size + 1) % FIX_LENGTH, this);
        }

        void clear() {
            m_cur_index = -1;
            m_cur_length = 0;
        }

    private:
        // ������Χ������
        iterator empty_iterator() {
            return iterator(nullptr, FIX_LENGTH, this);
        }
        const iterator empty_iterator() const {
            return iterator(nullptr, FIX_LENGTH, this);
        }

    private:
        // �洢����
        std::array<TDataType, FIX_LENGTH>       m_datas;
        // ��ǰ���ݽڵ�λ��
        size_t                                  m_cur_index = -1;
        // ��ǰ���ݳ���
        size_t                                  m_cur_length = 0;
    };

    // FIX_LENGTH: �̶�����, Ϊ0�򲻹̶�
    template<typename TDataType>
    class ScrollWindowVector {    
    public:
        using iterator = typename std::vector<TDataType>::iterator;
        using const_iterator = typename std::vector<TDataType>::const_iterator;
    public:
        template<typename... Args>
        ScrollWindowVector(Args&&... args) : m_datas({std::forward<Args>(args)...}) {}

        ScrollWindowVector(std::initializer_list<TDataType>& list) 
            : m_datas(list)
        {}

        ~ScrollWindowVector() {
            clear();
        }

        void set_fix_length(size_t fix_length, size_t multiple = 1) {
            assert(multiple > 0);
            m_fix_length = fix_length;
            m_multiple = multiple;
            m_datas.reserve(fix_length * multiple);
        }

        size_t get_fix_length() const {
            return m_fix_length;
        }

        // ��ʼ��Χ������
        iterator begin() {
            return m_datas.begin();
        }
        const_iterator begin() const {
            return m_datas.begin();
        }

        // ������Χ������
        iterator end() {
            return m_datas.end();
        }
        const_iterator end() const {
            return m_datas.end();
        }

        const size_t capacity() const {
            if (m_fix_length != 0)
                return m_fix_length;
            else
                return m_datas.capacity();
        }

        inline size_t length() const {
            return m_datas.size();
        }

        inline bool empty() const {
            return length() == 0;
        }

        // �򻬶�����������µ���������
        template<typename T>
        void emplace_back(T&& data) {
            if (m_fix_length == 0) {
                m_datas.emplace_back(std::forward<T>(data));
            }
            else if (m_fix_length == 1) {
                if (!m_datas.empty())
                    m_datas[0] = std::forward<T>(data);
                else
                    m_datas.emplace_back(std::forward<T>(data));
            }
            else {
                if (m_datas.size() >= m_fix_length * m_multiple) {
                    m_datas.erase(m_datas.begin(), m_datas.begin() + m_fix_length * (m_multiple - 1) + 1);
                }
                m_datas.emplace_back(std::forward<T>(data));
            }
        }

        // ��ȡ��ǰ���������е���������
        iterator at(size_t start_index) {
            return m_datas.at(start_index);
        }
        const_iterator at(size_t start_index) const {
            return m_datas.at(start_index);
        }

        TDataType& back() {
            return m_datas.back();
        }
        const TDataType& back() const {
            return m_datas.back();
        }

        // ��ȡ��������
        // need_size:��Ҫ�����ݳ���, Ϊ0���ʾ����
        // real_size:ʵ�ʵ����ݳ���
        iterator latest(size_t need_size, size_t& real_size) {
            size_t len = length();
            if (need_size == 0 || need_size >= len) {
                real_size = len;
                return m_datas.begin();
            }
            real_size = need_size;
            return m_datas.begin() + len - need_size;
        }
        const_iterator latest(size_t need_size, size_t& real_size) const {
            size_t len = length();
            if (need_size == 0 || need_size >= len) {
                real_size = len;
                return m_datas.begin();
            }
            real_size = need_size;
            return m_datas.begin() + len - need_size;
        }


        void clear() {
            m_datas.clear();
        }

    private:
        // �洢����
        std::vector<TDataType>                       m_datas;
        // �̶�����, Ϊ0�򲻹̶�
        size_t                                       m_fix_length = 0;
        // �ﵽ���ٱ�֮�����, ��������erase
        size_t                                       m_multiple = 1;
    };

}