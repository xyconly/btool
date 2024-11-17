/******************************************************************************
File name:  net_callback.hpp
Author:	    AChar
Purpose:    网络通讯回调接口
*****************************************************************************/
#pragma once

#include <functional>
#include <boost/lexical_cast.hpp>

namespace BTool
{
    namespace BoostNet
    {
        class NetCallBack
        {
        public:
            typedef unsigned long long  SessionID;
            typedef std::function<void(const SessionID& session_id)> open_cbk;
            typedef std::function<void(const SessionID& session_id, const char* const msg, size_t bytes_transferred)> close_cbk;
            typedef std::function<void(const SessionID& session_id, const char* const msg, size_t bytes_transferred)> read_cbk;
            typedef std::function<void(const SessionID& session_id, const char* const msg, size_t bytes_transferred)> write_cbk;

            typedef std::function<void()> server_error_cbk;

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

        // __attribute__((unused)) static boost::asio::ip::tcp::endpoint GetEndPointByHost(const char* host, unsigned short port, boost::system::error_code& ec) {
        //     if (host == nullptr) {
        //         return boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
        //     }

        //     ec = boost::asio::error::host_not_found;
        //     boost::asio::io_context ioc;
        //     boost::asio::ip::tcp::resolver rslv(ioc);
        //     boost::asio::ip::tcp::resolver::query qry(host, boost::lexical_cast<std::string>(port));
        //     try {
        //         boost::asio::ip::tcp::resolver::iterator iter = rslv.resolve(qry);
        //         if (iter != boost::asio::ip::tcp::resolver::iterator()) {
        //             ec.clear();
        //             return iter->endpoint();
        //         }
        //     }
        //     catch (...) {
        //         ec = boost::asio::error::fault;
        //     }
        //     return boost::asio::ip::tcp::endpoint();
        // }


        __attribute__((unused)) static boost::asio::ip::tcp::endpoint GetEndPointByHost(const char* host, unsigned short port, boost::system::error_code& ec) {
            // 检查 host 是否为 nullptr
            if (host == nullptr) {
                ec = boost::asio::error::host_not_found;
                return boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
            }

            // 检查 host 是否为有效的 IP 地址
            boost::asio::ip::address ip_address;
            try {
                ip_address = boost::asio::ip::make_address(host); // 尝试解析为 IP 地址
                ec.clear();
                return boost::asio::ip::tcp::endpoint(ip_address, port); // 返回指定 IP 地址
            } catch (const std::exception& e) {
                // 不是有效的 IP 地址，继续处理为域名
                ec = boost::asio::error::host_not_found;
            }

            boost::asio::io_context ioc;
            boost::asio::ip::tcp::resolver rslv(ioc);
            boost::asio::ip::tcp::resolver::query qry(host, boost::lexical_cast<std::string>(port));
            try {
                auto iter = rslv.resolve(qry);
                if (iter != boost::asio::ip::tcp::resolver::iterator()) {
                    ec.clear();
                    return iter->endpoint(); // 返回解析的 IP 地址
                }
            } catch (...) {
                ec = boost::asio::error::fault;
            }

            // 如果没有找到，返回一个空的 endpoint
            return boost::asio::ip::tcp::endpoint();
        }


    }
}