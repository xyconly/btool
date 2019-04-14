/******************************************************************************
File name:  websocket_callback.hpp
Author:	    AChar
Purpose:    TCP�ص��ӿ�,����std::function����ʽ���ڰ󶨲�ͬ����ĺ�����ַ;�����麯��
            �˴�͵�������ð󶨺�����ַ��ʽ^^
*****************************************************************************/
#pragma once

#include <functional>
#include <boost/beast/http.hpp>

namespace BTool
{
    namespace BoostNet
    {
        class HttpNetCallBack
        {
        public:
            typedef unsigned long long  SessionID;
//             typedef boost::beast::http::request<boost::beast::http::basic_dynamic_body> basic_dynamic_request_type;
//             typedef boost::beast::http::request<boost::beast::http::basic_file_body> basic_file_request_type;
//             typedef boost::beast::http::request<boost::beast::http::buffer_body> buffer_request_type;
//             typedef boost::beast::http::request<boost::beast::http::dynamic_body> dynamic_request_type;
//             typedef boost::beast::http::request<boost::beast::http::empty_body> empty_request_type;
//             typedef boost::beast::http::request<boost::beast::http::file_body> file_request_type;
//             typedef boost::beast::http::request<boost::beast::http::span_body> span_request_type;
//             typedef boost::beast::http::request<boost::beast::http::vector_body> vector_request_type;
//             typedef boost::beast::http::request<boost::beast::http::string_body> string_request_type;

            typedef boost::beast::http::request<boost::beast::http::string_body> request_type;

            enum {
                InvalidSessionID = 0,
            };

            // �������ӻص�
            virtual void on_open_cbk(SessionID session_id) {}
            // �ر����ӻص�
            virtual void on_close_cbk(SessionID session_id) {}
            // ��ȡ��Ϣ�ص�
            virtual void on_read_cbk(SessionID session_id, const request_type& request) {}

//             virtual void on_read_cbk(SessionID session_id, const basic_dynamic_request_type& msg) {}
//             virtual void on_read_cbk(SessionID session_id, const basic_file_request_type& msg) {}
//             virtual void on_read_cbk(SessionID session_id, const buffer_request_type& msg) {}
//             virtual void on_read_cbk(SessionID session_id, const dynamic_request_type& msg) {}
//             virtual void on_read_cbk(SessionID session_id, const empty_request_type& msg) {}
//             virtual void on_read_cbk(SessionID session_id, const file_request_type& msg) {}
//             virtual void on_read_cbk(SessionID session_id, const span_request_type& msg) {}
//             virtual void on_read_cbk(SessionID session_id, const vector_request_type& msg) {}
//             virtual void on_read_cbk(SessionID session_id, const string_request_type& msg) {}
        };

    }
}