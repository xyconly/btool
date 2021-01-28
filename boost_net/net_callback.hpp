/******************************************************************************
File name:  websocket_callback.hpp
Author:	    AChar
Purpose:    网络通讯回调接口
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

            // 开启连接回调
            open_cbk open_cbk_ = nullptr;
            // 关闭连接回调
            close_cbk close_cbk_ = nullptr;
            // 读取消息回调
            read_cbk read_cbk_ = nullptr;
            // 已发送消息回调
            write_cbk write_cbk_ = nullptr;
        };
    }
}