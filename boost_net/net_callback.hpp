/******************************************************************************
File name:  websocket_callback.hpp
Author:	    AChar
Purpose:    TCP回调接口,采用std::function的形式便于绑定不同对象的函数地址;优于虚函数
            此处偷懒不采用绑定函数地址形式^^
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

            // 开启连接回调
            virtual void on_open_cbk(SessionID session_id) {}
            // 关闭连接回调
            virtual void on_close_cbk(SessionID session_id) {}
            // 读取消息回调
            virtual void on_read_cbk(SessionID session_id, const char* const msg, size_t bytes_transferred) {}
            // 已发送消息回调
            virtual void on_write_cbk(SessionID session_id, const char* const msg, size_t bytes_transferred) {}
        };
    }
}