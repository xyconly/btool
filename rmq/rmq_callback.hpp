/*************************************************
File name:  rmq_callback.hpp
Author:     AChar
Purpose:    rmqͨѶ�ص��ӿ�
Note:       ͳһrmq���ö�д��װ, rmq�汾Ŀǰ����0.13.0
*************************************************/

#pragma once
#include <functional>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include "rabbitmq-c/include/amqp.h"
namespace BTool {
    namespace Rmq {
        class CallBack {
        public:
            typedef unsigned long long SessionID;
            typedef std::function<void(SessionID session_id)> open_cbk;
            typedef std::function<void(SessionID session_id, const char* const msg)> close_cbk;
            typedef std::function<void(SessionID session_id, amqp_channel_t channel_id)> open_channel_cbk;
            typedef std::function<void(SessionID session_id, amqp_channel_t channel_id, const char* const msg)> close_channel_cbk;
            typedef std::function<void(SessionID session_id, amqp_channel_t channel_id, uint64_t delivery_tag, const char* const msg, size_t bytes_transferred)> read_cbk;
            typedef std::function<void(bool ok, SessionID session_id, amqp_channel_t channel_id, const char* const msg, size_t bytes_transferred)> write_cbk;

            enum {
                InvalidSessionID = 0,
            };

            // �������ӻص�
            open_cbk open_cbk_ = nullptr;
            // �ر����ӻص�
            close_cbk close_cbk_ = nullptr;
            // ����channel�ص�
            open_channel_cbk open_channel_cbk_ = nullptr;
            // �ر�channel�ص�
            close_channel_cbk close_channel_cbk_ = nullptr;
            // ��ȡ��Ϣ�ص�
            read_cbk read_cbk_ = nullptr;
            // �ѷ�����Ϣ�ص�
            write_cbk write_cbk_ = nullptr;
        };

        __attribute__((unused)) static boost::asio::ip::tcp::endpoint GetEndPointByHost(const char* host, unsigned short port, boost::system::error_code& ec) {
            if (host == nullptr) {
                return boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
            }

            ec = boost::asio::error::host_not_found;
            boost::asio::io_context ioc;
            boost::asio::ip::tcp::resolver rslv(ioc);
            boost::asio::ip::tcp::resolver::query qry(host, boost::lexical_cast<std::string>(port));
            try {
                boost::asio::ip::tcp::resolver::iterator iter = rslv.resolve(qry);
                if (iter != boost::asio::ip::tcp::resolver::iterator()) {
                    ec.clear();
                    return iter->endpoint();
                }
            }
            catch (...) {
                ec = boost::asio::error::fault;
            }
            return boost::asio::ip::tcp::endpoint();
        }
    }
}