/******************************************************************************
File name:  http_callback.hpp
Author:	    AChar
Purpose:    Http�ص��ӿ�,Ĭ��ʹ��string��ʽ,����������ʽ���е���
*****************************************************************************/
#pragma once

#include <functional>
#include <boost/beast/http.hpp>

namespace BTool
{
    namespace BoostNet
    {
        template<bool isRequest, class ReadType, class WriteType = ReadType, class Fields = boost::beast::http::fields>
        class HttpNetCallBack
        {
        public:
            typedef unsigned long long  SessionID;
            typedef boost::beast::http::verb                                     method_type;

            typedef boost::beast::http::message<isRequest, ReadType, Fields>     read_msg_type;
            typedef boost::beast::http::message<!isRequest, WriteType, Fields>   send_msg_type;

            enum {
                InvalidSessionID = 0,
            };

            // �������ӻص�
            virtual void on_open_cbk(SessionID session_id) {}
            // �ر����ӻص�
            virtual void on_close_cbk(SessionID session_id) {}
            // ��ȡ��Ϣ�ص�
            virtual void on_read_cbk(SessionID session_id, const read_msg_type& read_msg) {}
            // д����Ϣ�ص�
            virtual void on_write_cbk(SessionID session_id, const send_msg_type& send_msg) {}
        };

        // Ĭ�ϵĿͻ��˻ص�, ��������,��ȡӦ��
        // send_msg_type: Ϊtypedef boost::beast::http::request<boost::beast::http::string_body>
        // read_msg_type: Ϊtypedef boost::beast::http::response<boost::beast::http::string_body>
        using HttpClientNetCallBack = HttpNetCallBack<false, boost::beast::http::string_body>;

        // Ĭ�ϵķ���˻ص�, ��ȡ����,����Ӧ��
        // read_msg_type: Ϊtypedef boost::beast::http::request<boost::beast::http::string_body>
        // send_msg_type: Ϊtypedef boost::beast::http::response<boost::beast::http::string_body>
        using HttpServiceNetCallBack = HttpNetCallBack<true, boost::beast::http::string_body>;

        // ��������
//             typedef boost::beast::http::message<boost::beast::http::basic_dynamic_body>  basic_dynamic_msg_type;
//             typedef boost::beast::http::message<boost::beast::http::basic_file_body>     basic_file_msg_type;
//             typedef boost::beast::http::message<boost::beast::http::buffer_body>         buffer_msg_type;
//             typedef boost::beast::http::message<boost::beast::http::dynamic_body>        dynamic_msg_type;
//             typedef boost::beast::http::message<boost::beast::http::empty_body>          empty_msg_type;
//             typedef boost::beast::http::message<boost::beast::http::file_body>           file_msg_type;
//             typedef boost::beast::http::message<boost::beast::http::span_body>           span_msg_type;
//             typedef boost::beast::http::message<boost::beast::http::vector_body>         vector_msg_type;
//             typedef boost::beast::http::message<boost::beast::http::string_body>         string_msg_type;
    }
}