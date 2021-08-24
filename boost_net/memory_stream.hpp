/*************************************************
File name:      memory_stream.hpp
Author:			AChar
Version:
Date:
Purpose: ʵ���Թ�����ڴ����ӿ�
Note:    �����ڲ����̰߳�ȫ����,���в����������ȷ���̰߳�ȫ
*************************************************/
#pragma once

#include <string>
#include <memory>
#if defined(_HAS_CXX17) || (__cplusplus >= 201703L)
# include <string_view>
#endif

#ifdef _MSC_VER
# include <stdint.h>
#elif defined(__GNUC__)
// # include <arpa/inet.h>
#endif

namespace BTool {
    class MemoryStream
    {
        template <typename> struct is_tuple : std::false_type {};
        template <typename ...T> struct is_tuple<std::tuple<T...>> : std::true_type {};
        template <typename ...T> struct is_tuple<std::tuple<T...>&> : std::true_type {};
        template <typename ...T> struct is_tuple<const std::tuple<T...>&> : std::true_type {};
        template <typename ...T> struct is_tuple<std::tuple<T...>&&> : std::true_type {};

        MemoryStream(MemoryStream&& rhs) = delete;
        MemoryStream(const MemoryStream& rhs) = delete;
        MemoryStream& operator=(const MemoryStream& rhs) = delete;

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

        MemoryStream(size_t capacity)
            : m_buffer_size(0)
            , m_offset(0)
            , m_capacity(capacity)
            , m_auto_delete(true)
        {
            m_buffer = (char*)malloc(capacity);
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
            clear();
        }

    public:
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
        // ��ǰƯ��λ����Ư��
        void add_offset(int offset) {
            m_offset += offset;
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
        // �����ǰ�ڴ������������ڵĹ���,�˺��Զ������ڴ��ͷ�
        char* detach() {
            char* buffer = m_buffer;
            m_buffer_size = 0;
            m_offset = 0;
            m_capacity = 0;
            m_buffer = nullptr;
            m_auto_delete = true;
            return buffer;
        }

        // �������,�˺��Զ������ڴ��ͷ�
        void clear() {
            m_buffer_size = 0;
            m_offset = 0;
            m_capacity = 0;
            if (m_buffer && m_auto_delete)
                free(m_buffer);
            m_buffer = nullptr;
            m_auto_delete = true;
        }

        // ��������,�˺��Զ������ڴ��ͷ�
        // ����ʱ�����ԭ������,�Ҽ��غ󲻻��Ư��λ�ý�����λ
        void load(const char* buffer, size_t len) {
            m_buffer_size = len;
            m_offset = 0;
            m_capacity = len;
            if (m_buffer && m_auto_delete)
                free(m_buffer);

            m_buffer = (char*)malloc(len);
            memcpy(m_buffer, buffer, len);
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
        template<typename Type>
        void append(Type&& src) {
            append(&src, sizeof(Type));
        }
        template<>
        void append<const std::string&>(const std::string& data) {
            append(size_t(data.length() + 1));
            append(data.c_str(), data.length() + 1);
        }
        template<>
        void append<std::string&>(std::string& data) {
            append(size_t(data.length() + 1));
            append(data.c_str(), data.length() + 1);
        }
        template<>
        void append<std::string>(std::string&& data) {
            append(size_t(data.length() + 1));
            append(data.c_str(), data.length() + 1);
        }
#if defined(_HAS_CXX17) || (__cplusplus >= 201703L)
        template<>
        void append<const std::string_view&>(const std::string_view& data) {
            append(size_t(data.length() + 1));
            append(data.data(), data.length() + 1);
        }
        template<>
        void append<std::string_view&>(std::string_view& data) {
            append(size_t(data.length() + 1));
            append(data.data(), data.length() + 1);
        }
        template<>
        void append<std::string_view>(std::string_view&& data) {
            append(size_t(data.length() + 1));
            append(data.data(), data.length() + 1);
        }
#endif

        template<>
        void append<MemoryStream&>(MemoryStream& data) {
            append(data.size());
            append(data.data(), data.size());
        }
        template<>
        void append<const MemoryStream&>(const MemoryStream& data) {
            append(data.size());
            append(data.data(), data.size());
        }

        // ׷������������λ�õ��ڴ洦
        void append_args() {}
        template <typename Type, typename...Args>
        typename std::enable_if<!is_tuple<Type>::value, void >::type
         append_args(Type&& src, Args&&...srcs) {
            append(std::forward<Type>(src));
            append_args(std::forward<Args>(srcs)...);
        }
        template <typename Type, typename...Args>
        typename std::enable_if<is_tuple<Type>::value, void >::type
            append_args(Type&& tp, Args&&...srcs) {
            append_args_tp(std::forward<Type>(tp), std::forward<Args>(srcs)...);
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
        // ��ȡ��ǰƯ��λ�²�����ֵ
        // offset_flag : �Ƿ���ҪƯ�Ƶ�ǰƯ��λ
        template<typename Type>
        void read(Type* pDst, bool offset_flag) {
            read(pDst, sizeof(Type), offset_flag);
        }
        template<>
        void read<MemoryStream>(MemoryStream* pDst, bool offset_flag) {
            size_t length = 0;
            read(&length, sizeof(size_t), true);

            pDst->clear();
            pDst->load(m_buffer + m_offset, length);

            if (!offset_flag)
                m_offset -= sizeof(size_t);
        }
        template<>
        void read<std::string>(std::string* pDst, bool offset_flag) {
            size_t length = 0;
            read(&length, sizeof(size_t), true);

            *pDst = std::string(m_buffer + m_offset, length);
            if (!offset_flag)
                m_offset -= sizeof(size_t);
            else
                m_offset += length;
        }
#if defined(_HAS_CXX17) || (__cplusplus >= 201703L)
        template<>
        void read<std::string_view>(std::string_view* pDst, bool offset_flag) {
            size_t length = 0;
            read(&length, sizeof(size_t), true);

            *pDst = std::string_view(m_buffer + m_offset, length);
            if (!offset_flag)
                m_offset -= sizeof(size_t);
            else
                m_offset += length;
        }
#endif

        // ��ȡ��ǰƯ��λ�²�����ֵ,��ȡ���Զ�Ư�Ƶ�ǰƯ��λ(�ػ�ģ�岻��ʹ��Ĭ�ϲ���)
        template<typename Type>
        void read(Type* pDst) {
            read(pDst, true);
        }
        template<>
        void read<MemoryStream>(MemoryStream* pDst) {
            read(pDst, true);
        }
        template<>
        void read<std::string>(std::string* pDst) {
            read(pDst, true);
        }
#if defined(_HAS_CXX17) || (__cplusplus >= 201703L)
        template<>
        void read<std::string_view>(std::string_view* pDst) {
            read(pDst, true);
        }
#endif

        // ��ȡ���ݵ�ǰƯ��λ�²�����ֵ
        // offset_flag : �Ƿ���ҪƯ�Ƶ�ǰƯ��λ
        template <typename Type>
        typename std::enable_if<is_tuple<Type>::value, Type >::type
            read_args(bool offset_flag = true) {
            size_t cur_offset(m_offset);
            auto rslt = tuple_map_r<std::decay<Type>::type>();
            if (!offset_flag)
                m_offset = cur_offset;
            return rslt;
        }
        template <typename Type>
        typename std::enable_if<!is_tuple<Type>::value, Type >::type
            read_args(bool offset_flag = true) {
            size_t cur_offset(m_offset);
            Type rslt;
            read(&rslt);
            if (!offset_flag)
                m_offset = cur_offset;
            return rslt;
        }

        template <typename Type, typename...Args>
        typename std::enable_if<sizeof...(Args) >= 1, std::tuple<Type, typename std::decay<Args>::type...> >::type
             read_args(bool offset_flag = true) {
            size_t cur_offset(m_offset);
            auto param1 = read_args<Type>();
            auto rslt = tuple_merge(std::move(param1), read_args<typename std::decay<Args>::type...>());
            if (!offset_flag)
                m_offset = cur_offset;
            return rslt;
        }
        //template <typename Type, typename...Args>
        //typename std::enable_if<sizeof...(Args) >= 2, std::tuple<Type, typename std::decay<Args>::type...> >::type
        //    read_args(bool offset_flag = true) {
        //    size_t cur_offset(m_offset);
        //    auto param1 = read_args<Type>();
        //    auto rslt = tuple_merge(std::move(param1), read_args<typename std::decay<Args>::type...>());
        //    if (!offset_flag)
        //        m_offset = cur_offset;
        //    return rslt;
        //}

        // tuple -> args...
        template<typename Type, typename... Args, typename type_is_tuple = typename std::enable_if<!is_tuple<Type>::value, void >::type>
        static std::tuple<Type, typename std::decay<Args>::type...> tuple_merge(Type&& type, std::tuple<Args...>&& tp) {
            return tuple_merge_impl(std::forward<Type>(type), typename std::make_index_sequence<sizeof...(Args)>{}, std::move(tp));
        }
        template<typename Type, typename... Args, typename type_is_tuple = typename std::enable_if<!is_tuple<Type>::value, void >::type>
        static std::tuple<Type, typename std::decay<Args>::type...> tuple_merge(Type&& type, const std::tuple<Args...>& tp) {
            return tuple_merge_impl(std::forward<Type>(type), typename std::make_index_sequence<sizeof...(Args)>{}, tp);
        }
        template<typename Type, typename... Args, typename type_is_tuple = typename std::enable_if<!is_tuple<Type>::value, void >::type>
        static std::tuple<typename std::decay<Args>::type..., Type> tuple_merge(std::tuple<Args...>&& tp, Type&& type) {
            return tuple_merge_impl(typename std::make_index_sequence<sizeof...(Args)>{}, std::move(tp), std::forward<Type>(type));
        }
        template<typename Type, typename... Args, typename type_is_tuple = typename std::enable_if<!is_tuple<Type>::value, void >::type>
        static std::tuple<typename std::decay<Args>::type..., Type> tuple_merge(const std::tuple<Args...>& tp, Type&& type) {
            return tuple_merge_impl(typename std::make_index_sequence<sizeof...(Args)>{}, tp, std::forward<Type>(type));
        }
        template<typename Type1, typename Type2
            , typename t1_is_tuple = typename std::enable_if<is_tuple<Type1>::value, void >::type
            , typename t2_is_tuple = typename std::enable_if<is_tuple<Type2>::value, void >::type
            >
        static decltype(auto) tuple_merge(Type1&& tp1, Type2&& tp2){
            return std::tuple_cat(std::forward<Type1>(tp1), std::forward<Type2>(tp2));
        }
        template<typename Type1, typename Type2
            , typename t1_is_tuple = typename std::enable_if<!is_tuple<Type1>::value, void >::type
            , typename t2_is_tuple = typename std::enable_if<!is_tuple<Type2>::value, void >::type
            >
        static std::tuple<Type1, Type2> tuple_merge(Type1&& tp1, Type2&& tp2){
            return std::forward_as_tuple(std::forward<Type1>(tp1), std::forward<Type2>(tp2));
        }

        template<typename Type>
        static typename std::enable_if<is_tuple<Type>::value, Type>::type
            tuple_merge(Type&& tp1) {
            return tp1;
        }
        template<typename Type>
        static typename std::enable_if<!is_tuple<Type>::value, std::tuple<Type>>::type
            tuple_merge(Type&& tp1) {
            return std::forward_as_tuple(std::forward<Type>(tp1));
        }

        // sizeof(std::tuple<...>) ��tuple����
        static size_t get_args_sizeof() { return 0; }
        template <typename Type>
        static typename std::enable_if<!is_tuple<Type>::value, size_t>::type
            get_args_sizeof() { return sizeof(Type); }
        template <typename Type>
        static typename std::enable_if<is_tuple<Type>::value, size_t>::type
            get_args_sizeof() { 
            return get_args_sizeof_impl<Type>(typename std::make_index_sequence<std::tuple_size<Type>::value>{});
        }
        template <typename Type, typename...Args>
        static typename std::enable_if<sizeof...(Args) >= 1, size_t>::type
            get_args_sizeof() {
            return get_args_sizeof<Type>() + get_args_sizeof<Args...>();
        }

    protected:
        // tuple -> args...
        template<typename Type, size_t... Indexes, typename... Args, typename type_is_tuple = typename std::enable_if<!is_tuple<Type>::value, void >::type>
        static std::tuple<Type, typename std::decay<Args>::type...> tuple_merge_impl(Type&& type, std::index_sequence<Indexes...>, std::tuple<Args...>&& tp) {
            return std::forward_as_tuple(std::forward<Type>(type), std::move(std::get<Indexes>(tp))...);
        }
        template<typename Type, size_t... Indexes, typename... Args, typename type_is_tuple = typename std::enable_if<!is_tuple<Type>::value, void >::type>
        static std::tuple<Type, typename std::decay<Args>::type...> tuple_merge_impl(Type&& type, std::index_sequence<Indexes...>, const std::tuple<Args...>& tp) {
            return std::forward_as_tuple(std::forward<Type>(type), std::forward<Args>(std::get<Indexes>(tp))...);
        }
        template<typename Type, size_t... Indexes, typename... Args, typename type_is_tuple = typename std::enable_if<!is_tuple<Type>::value, void >::type>
        static std::tuple<typename std::decay<Args>::type..., Type> tuple_merge_impl(std::index_sequence<Indexes...>, std::tuple<Args...>&& tp, Type&& type) {
            return std::forward_as_tuple(std::move(std::get<Indexes>(tp))..., std::forward<Type>(type));
        }
        template<typename Type, size_t... Indexes, typename... Args, typename type_is_tuple = typename std::enable_if<!is_tuple<Type>::value, void >::type>
        static std::tuple<typename std::decay<Args>::type..., Type> tuple_merge_impl(std::index_sequence<Indexes...>, const std::tuple<Args...>& tp, Type&& type) {
            return std::forward_as_tuple(std::forward<Args>(std::get<Indexes>(tp))..., std::forward<Type>(type));
        }

        // read tuple        
        template<class Tuple, std::size_t... Indexes>
        Tuple tuple_map_r(std::index_sequence<Indexes...>) {
            return read_args<typename std::tuple_element<Indexes, Tuple>::type...>();
        }
        template<class Tuple>
        Tuple tuple_map_r() {
            //return tuple_map_r<Tuple>(std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>>{}); // gcc ��֧��
            return tuple_map_r<Tuple>(std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
        }

        // append tuple
        template <typename... Args, typename...Types>
        void append_args_tp(std::tuple<Args...>&& tp, Types&&...srcs) {
            append_args_tp_impl(typename std::make_index_sequence<sizeof...(Args)>{}, std::move(tp), std::forward<Types>(srcs)...);
        }
        template <size_t... Indexes, typename... Args, typename...Types>
        void append_args_tp_impl(std::index_sequence<Indexes...>, std::tuple<Args...>&& tp, Types&&...srcs) {
            append_args((std::get<Indexes>(std::move(tp)))..., std::forward<Types>(srcs)...);
        }
        template <typename... Args, typename...Types>
        void append_args_tp(const std::tuple<Args...>& tp, Types&&...srcs) {
            append_args_tp_impl(typename std::make_index_sequence<sizeof...(Args)>{}, tp, std::forward<Types>(srcs)...);
        }
        template <size_t... Indexes, typename... Args, typename...Types>
        void append_args_tp_impl(std::index_sequence<Indexes...>, const std::tuple<Args...>& tp, Types&&...srcs) {
            append_args(std::get<Indexes>(std::forward<const std::tuple<Args...>&>(tp))..., std::forward<Types>(srcs)...);
        }

        // sizeof(std::tuple<...>) ��tuple����
        template<class Tuple, std::size_t... Indexes>
        static size_t get_args_sizeof_impl(std::index_sequence<Indexes...>) {
            return get_args_sizeof<typename std::tuple_element<Indexes, Tuple>::type...>();
        }

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