/******************************************************************************
File name:  websocket_callback.hpp
Author:	    AChar
Purpose:    TCP�ص��ӿ�,����std::function����ʽ���ڰ󶨲�ͬ����ĺ�����ַ;�����麯��
            �˴�͵�������ð󶨺�����ַ��ʽ^^
*****************************************************************************/
#pragma once

#include <functional>

namespace BTool
{
    namespace BoostNet
    {
        class NetCallBack
        {
        public:
            typedef unsigned long long  SessionID;

            enum {
                InvalidSessionID = 0,
            };

            // �������ӻص�
            virtual void on_open_cbk(SessionID session_id) {}
            // �ر����ӻص�
            virtual void on_close_cbk(SessionID session_id) {}
            // ��ȡ��Ϣ�ص�
            virtual void on_read_cbk(SessionID session_id, const char* const msg, size_t bytes_transferred) {}
            // �ѷ�����Ϣ�ص�
            virtual void on_write_cbk(SessionID session_id, const char* const msg, size_t bytes_transferred) {}
        };
    }
}