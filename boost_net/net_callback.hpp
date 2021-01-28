/******************************************************************************
File name:  websocket_callback.hpp
Author:	    AChar
Purpose:    ����ͨѶ�ص��ӿ�
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
            typedef std::function<void(SessionID session_id)> open_cbk;
            typedef std::function<void(SessionID session_id)> close_cbk;
            typedef std::function<void(SessionID session_id, const char* const msg, size_t bytes_transferred)> read_cbk;
            typedef std::function<void(SessionID session_id, const char* const msg, size_t bytes_transferred)> write_cbk;

            enum {
                InvalidSessionID = 0,
            };

            // �������ӻص�
            open_cbk open_cbk_ = nullptr;
            // �ر����ӻص�
            close_cbk close_cbk_ = nullptr;
            // ��ȡ��Ϣ�ص�
            read_cbk read_cbk_ = nullptr;
            // �ѷ�����Ϣ�ص�
            write_cbk write_cbk_ = nullptr;
        };
    }
}