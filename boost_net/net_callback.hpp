/******************************************************************************
File name:  net_callback.hpp
Author:	    AChar
Purpose:    ����ͨѶ�ص��ӿ�
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

            // �������ӻص�
            open_cbk open_cbk_ = nullptr;
            // �ر����ӻص�
            close_cbk close_cbk_ = nullptr;
            // ��ȡ��Ϣ�ص�
            read_cbk read_cbk_ = nullptr;
            // �ѷ�����Ϣ�ص�
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
            // ��� host �Ƿ�Ϊ nullptr
            if (host == nullptr) {
                ec = boost::asio::error::host_not_found;
                return boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
            }

            // ��� host �Ƿ�Ϊ��Ч�� IP ��ַ
            boost::asio::ip::address ip_address;
            try {
                ip_address = boost::asio::ip::make_address(host); // ���Խ���Ϊ IP ��ַ
                ec.clear();
                return boost::asio::ip::tcp::endpoint(ip_address, port); // ����ָ�� IP ��ַ
            } catch (const std::exception& e) {
                // ������Ч�� IP ��ַ����������Ϊ����
                ec = boost::asio::error::host_not_found;
            }

            boost::asio::io_context ioc;
            boost::asio::ip::tcp::resolver rslv(ioc);
            boost::asio::ip::tcp::resolver::query qry(host, boost::lexical_cast<std::string>(port));
            try {
                auto iter = rslv.resolve(qry);
                if (iter != boost::asio::ip::tcp::resolver::iterator()) {
                    ec.clear();
                    return iter->endpoint(); // ���ؽ����� IP ��ַ
                }
            } catch (...) {
                ec = boost::asio::error::fault;
            }

            // ���û���ҵ�������һ���յ� endpoint
            return boost::asio::ip::tcp::endpoint();
        }


    }
}