/******************************************************************************
File name:  rmq_session.hpp
Author:     AChar
Purpose:    rmq������
Note:       ��д��Ϊ���������߳�
Demo:
    ��Լ������ÿ��3W����
    BTool::Rmq::Session session(1000000);
    BTool::Rmq::Session::RmqExchangeInfoSt exchange{ "exchange", "direct" };
    BTool::Rmq::Session::RmqQueueInfoSt queue{ "queue" };
    std::string key = "key";
    session.register_open_cbk([ & ] (auto session_id) {
        std::cout << std::this_thread::get_id() << "  - open: " << session_id << std::endl;
        auto channel_id = session.create_channel(exchange, BTool::Rmq::Session::RmqRWType::exchange_write, key);
        //auto channel_id = session.create_channel(queue, BTool::Rmq::Session::RmqRWType::queue_read, key);
        //auto channel_id = session.create_channel(exchange, queue, BTool::Rmq::Session::RmqRWType::read_write, key);
        if (channel_id == BTool::Rmq::Session::INVALID_CHANNEL_ID) {
            std::cout << "err: " << channel_id << std::endl;
        }
    }).register_close_cbk([ & ] (auto session_id, auto msg) {
        std::cout << std::this_thread::get_id() << "  - close: " << session_id << "; " << msg << std::endl;
        session.reconnect();
    }).register_open_channel_cbk([ & ] (auto session_id, auto channel_id) {
        static bool started = false;
        if (!started && session.bind(channel_id, exchange, queue, key)) {
            started = true;
        }
        std::cout << std::this_thread::get_id() << "  - open_channel: " << session_id << "  -  " << channel_id << std::endl;
    }).register_close_channel_cbk([ & ] (auto session_id, auto channel_id, auto msg) {
        std::cout << std::this_thread::get_id() << "  - close_channel: " << session_id << "  -  " << channel_id << "; " << msg << std::endl;
        session.reconnect_channel(channel_id);
    }).register_read_cbk([ & ] (auto session_id, auto channel_id, auto delivery_tag, auto msg, auto len) {
        std::cout << std::this_thread::get_id() << "  - read        : " << session_id << "  -  " << channel_id << "  -  " << delivery_tag << "  :  " << std::string(msg, len) << std::endl;
        static int i(0);
        std::string send = std::to_string(++i);
        bool ok = session.write(channel_id, send.c_str(), send.length());
        if (!ok) {
            std::cout << "send read err" << std::endl;
        }
    }).register_write_cbk([ & ] (auto ok, auto session_id, auto channel_id, auto msg, auto len) {
        std::cout << std::this_thread::get_id() << "  - write        : " << session_id << "  -  " << channel_id << "  -  " << (ok ? "success" : "fail") << "  :  " << std::string(msg, len) << std::endl;
    });

    session.connect({ "user", "password", "127.0.0.1" });
    std::this_thread::sleep_for(std::chrono::seconds(3));
    bool ok = session.write(1, "test", 4);
    if (!ok) {
        std::cout << "send msg err" << std::endl;
    }
    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
*****************************************************************************/
#pragma once

#include <queue>
#include <mutex>
#include <string>

#include "rabbitmq-c/include/amqp_tcp_socket.h"
#include "rabbitmq-c/include/amqp.h"
#include "rmq_callback.hpp"
#include "../scope_guard.hpp"
#include "../task_pool.hpp"
#include "../boost_net/memory_stream.hpp"
#include "../atomic_switch.hpp"

namespace BTool
{
    namespace Rmq
    {
        // RMQ���Ӷ���
        class Session : public std::enable_shared_from_this<Session>
        {
            enum {
                CONNECT_THREAD_ID = 0,
                READ_THREAD_ID = 1,
                WRITE_THREAD_ID = 2,
            };
        public:
            typedef CallBack::SessionID             SessionID;

            enum {
                INVALID_CHANNEL_ID = 0,         // ��Чͨ��id
                NOLIMIT_WRITE_BUFFER_SIZE = 0,  // ������
                MAX_WRITE_BUFFER_SIZE = 30000,
            };

            enum class RmqRWType : int {
                exchange_write = 0b0001,
                exchange_read = 0b0010,
                queue_write = 0b0100,
                queue_read = 0b1000,
                exchange_read_write = queue_write | exchange_read,
                queue_read_write = queue_write | queue_read,
                write = exchange_write | queue_write,
                read = exchange_read | queue_read,
                read_write = read | write,
            };

            struct RmqConnectInfoSt {
                std::string     user_name_;
                std::string     password_;
                std::string     ip_ = "127.0.0.1";
                int             read_timeout_ = 1;      // ������ʱ����, 0Ϊ���޵ȴ�
                int             heart_beat_ = 0;        /* ������������, 0Ϊ������,
                                                           ����������Ҫamqp_basic_publish/amqp_simple_wait_frame/amqp_simple_wait_frame_noblock��άϵ*/
                unsigned short  port_ = 5672;
                std::string     addr_ = "/";
                int             flags_ = 0;             /* ���޷��ҵ����¶������ʾ�ڴ˰汾����Ч
                                                           AMQP_NO_WAIT: ָ��������Ӧ����������, ���û����Ϣ����, �򲻻�ȴ�
                                                                ��������˴˱�־, �������ܻ���������, �������Ƿ�����Ϣ����
                                                           AMQP_AUTOACK: ָ��������Ӧ�Զ�ȷ���յ�����Ϣ, ������Ҫ��ʽ����amqp_basic_ack����
                                                                ���δ���ô˱�־, ������ڳɹ�������Ϣ���ֶ�����amqp_basic_ack��ȷ���յ�����Ϣ
                                                           AMQP_SINGLE_MESSAGE: ָ��������ֻӦ����һ����Ϣ, Ȼ�󷵻�
                                                                ��������˴˱�־, �������߽�����һ����Ϣ, Ȼ���˳�amqp_consume_message����������
                                                           ��Щ��־λ���������һ��ʹ��, ����, ���Խ�AMQP_AUTOACK��AMQP_NO_WAITһ������, �Ա������������Զ�ȷ���յ�����Ϣ, ���û����Ϣ����, ����������
                                                           ע��, AMQP_NO_WAIT��AMQP_AUTOACK��־���ܻᵼ��һЩ��Ϣ�Ķ�ʧ���ظ�����
                                                        */
            };

            struct RmqExchangeInfoSt {
                std::string     name_;                          // ��������
                std::string     type_;                          /* ������������
                                                                   fanout: �㲥, �����������������Ѿ������������ӵĶ��з�����ͬ��һ����Ϣ
                                                                   direct: ֱ��ƥ��, ����Ϣ��routing_key��󶨵Ķ��е�routing_key��ȫƥ��ʱ, Exchange����Ϣ���͵��ö�����
                                                                   topic: ����ƥ��, Exchange ����Ϣ���͵�����Ϣ��routing_keyƥ������ж�����, routing_key����ʹ��ͨ���*(ƥ�䵥������)��#(ƥ��������)
                                                                   headers: ʹ����Ϣͷ�ļ�ֵ��ƥ��, Exchange������Ϣͷ�еļ�ֵ����ѡ��󶨵Ķ���, ���ı�������1, ͼƬ������2�ȵ�
                                                                */
                bool            passive_ = false;               // ����Ƿ���������������, �����򲻴���ʱ��������
                bool            durable_ = true;                // �������Ƿ�־û�, ������exchange����server����ǰһֱ��Ч
                bool            internal_ = false;              // �������Ƿ����ڲ�ר��, ������������exchange���淢��Ϣ
                bool            auto_delete_ = false;           // �������Ƿ���Ҫ�Զ�ɾ��, ������exchange��������󶨵�queue��ɾ��֮��ɾ���Լ�
                amqp_table_t    argument_ = amqp_empty_table;   // ��������չ����, Ĭ��Ϊ: amqp_empty_table
                amqp_bytes_t    name_bytes_ = amqp_cstring_bytes(name_.c_str());
                amqp_bytes_t    type_bytes_ = amqp_cstring_bytes(type_.c_str());
            };

            struct RmqQueueInfoSt {
                std::string     name_;                          // ������
                bool            passive_ = false;               // ����Ƿ�������������, �����򲻴���ʱ��������
                bool            durable_ = true;                // �����Ƿ�־û�, ������queue����server����ǰһֱ��Ч
                bool            exclusive_ = false;             // �������Ƿ��ռ, ������queue��ֻ�ܱ���������������ʹ��
                bool            auto_delete_ = false;           // �����Ƿ���Ҫ�Զ�ɾ��, ������queue������������ֹͣʹ��֮���Զ�ɾ���Լ�
                amqp_table_t    argument_ = amqp_empty_table;   // ������չ����, Ĭ��Ϊ: amqp_empty_table
                amqp_bytes_t    name_bytes_ = amqp_cstring_bytes(name_.c_str());
            };

            struct RmqChannelInfoSt {
                uint32_t        prefetch_size_ = 0;             // Ԥȡ����������С, һ������Ϊ0
                uint16_t        prefetch_count_ = 0;            // ÿ��Ԥȡ����Ϣ����
            };

            struct RmqConsumerInfoSt {
                bool            no_local_ = false;              // ��ʾ�������Ƿ񲻽������Լ���������Ϣ
                bool            no_ack_ = false;                // �Ƿ��Զ�Ӧ��, ��������Ҫȷ����Ϣ���ٴӶ�����ɾ����Ϣ
                bool            exclusive_ = false;             // ��������Ƿ��ռ, ������ǰ���Ӳ���ʱ, �����Զ�ɾ��
                amqp_bytes_t    tag_ = amqp_empty_bytes;        // ������չ����, Ĭ��Ϊ: amqp_empty_bytes
                amqp_table_t    argument_ = amqp_empty_table;   // ������չ����, Ĭ��Ϊ: amqp_empty_table
            };

        private:
            struct ChannelSt {
                RmqRWType           rw_type_;                   // ��д����
                bool                auto_ack_;                  // �Ƿ���Session�Զ�ȷ����Ϣ, ������ʹconsumer������no_ack_Ϊfalse, Ҳ���ڶ�ȡ����Ϣ���Զ�����ack
                RmqExchangeInfoSt   exchange_;
                RmqQueueInfoSt      queue_;
                RmqChannelInfoSt    channel_;
                RmqConsumerInfoSt   consumer_;
                std::string         routing_key_;
                amqp_table_t        bind_arguments_;            // ����չ����, Ĭ��Ϊ: amqp_empty_table
                amqp_bytes_t        routing_key_bytes_;
            };

            struct WriteBufInfo {
                amqp_channel_t              channel_id_;
                std::string                 routing_key_;
                MemoryStream                data_;
                int                         retry_count_;
                bool                        confirm_;           // �Ƿ���Ҫǿ�Ƶȴ�����������ack
                bool                        mandatory_;         // �����Ϣ�޷�·�ɵ�ָ������, ����Ϣ�Ƿ���Ҫ�����ظ���Ϣ������
                bool                        immediate_;         // �����Ϣ�޷�����������, ����Ϣ�Ƿ���Ҫ�����ظ���Ϣ������
                amqp_basic_properties_t*    route_props_;
            };

        public:
            // RMQ���Ӷ���, Ĭ�϶��з���ģʽ, ��ͨ��set_only_one_mode����Ϊ��������ģʽ
            // max_buffer_size: ���д��������С; 0���ʾ������
            // max_task_count: д��Ϣ������񻺴����, ��������������������; 0���ʾ������
            Session(size_t max_buffer_size = 0)
                : m_read_thread(0)
                , m_write_thread(0)
                , m_session_id(GetNextSessionID())
                , m_max_wbuffer_size(max_buffer_size)
                , m_cur_wbuffer_size(0)
                , m_cur_in_write(false)
            {
                m_read_thread.start(1);
                m_write_thread.start(1);
            }

            ~Session() {
                m_handler = CallBack();
                close("");
            }

            // ���ûص�, ���ø���ʽ�ɻص�����ͬ���зֿ�����
            Session& register_cbk(const CallBack& handler) {
                m_handler = handler;
                return *this;
            }
            // ���ÿ������ӻص�
            Session& register_open_cbk(const CallBack::open_cbk& cbk) {
                m_handler.open_cbk_ = cbk;
                return *this;
            }
            // ���ùر����ӻص�
            Session& register_close_cbk(const CallBack::close_cbk& cbk) {
                m_handler.close_cbk_ = cbk;
                return *this;
            }
            // ���ÿ������ӻص�
            Session& register_open_channel_cbk(const CallBack::open_channel_cbk& cbk) {
                m_handler.open_channel_cbk_ = cbk;
                return *this;
            }
            // ���ùر����ӻص�
            Session& register_close_channel_cbk(const CallBack::close_channel_cbk& cbk) {
                m_handler.close_channel_cbk_ = cbk;
                return *this;
            }
            // ���ö�ȡ��Ϣ�ص�
            Session& register_read_cbk(const CallBack::read_cbk& cbk) {
                m_handler.read_cbk_ = cbk;
                return *this;
            }
            // �����ѷ�����Ϣ�ص�
            Session& register_write_cbk(const CallBack::write_cbk& cbk) {
                m_handler.write_cbk_ = cbk;
                return *this;
            }

            // ��ȡ������IP
            const std::string& get_ip() const {
                return m_connect_info.ip_;
            }

            // ��ȡ������port
            unsigned short get_port() const {
                return m_connect_info.port_;
            }

            bool is_connecting() const {
                return m_atomic_switch.has_started();
            }
            bool is_connecting(amqp_channel_t channel_id) const {
                auto iter = m_channels_switch.find(channel_id);
                if (iter == m_channels_switch.end()) {
                    return false;
                }
                return m_atomic_switch.has_started() && iter->second.has_started();
            }
            bool has_stoped() const {
                return m_atomic_switch.has_stoped();
            }
            bool has_stoped(amqp_channel_t channel_id) const {
                auto iter = m_channels_switch.find(channel_id);
                if (iter == m_channels_switch.end()) {
                    return true;
                }
                return iter->second.has_stoped() || m_atomic_switch.has_stoped();
            }

            // todo...
            // ������Ϣ�Զ��ط����Դ���, �ݲ�֧��
            // void set_retry_send_count() {}

            // ����ͨ��Ĭ������, ע�������bind_queue֮ǰʹ��
            void set_channel_default_param(const RmqChannelInfoSt& channel) {
                m_default_channel = channel;
            }
            const RmqChannelInfoSt& get_channel_default_param() const {
                return m_default_channel;
            }

            // ����ָ��ͨ������, ע�������bind_queue֮��ʹ��
            bool set_channel_param(amqp_channel_t channel_id, const RmqConsumerInfoSt& consumer) {
                std::lock_guard<std::mutex> lock(m_channels_mtx);
                auto iter = m_channels.find(channel_id);
                if (iter == m_channels.end()) {
                    return false;
                }
                iter->second.channel_ = m_default_channel;
                iter->second.consumer_ = consumer;
                return true;
            }
            bool set_channel_param(amqp_channel_t channel_id, const RmqConsumerInfoSt& consumer, const RmqChannelInfoSt& channel) {
                std::lock_guard<std::mutex> lock(m_channels_mtx);
                auto iter = m_channels.find(channel_id);
                if (iter == m_channels.end()) {
                    return false;
                }
                iter->second.channel_ = channel;
                iter->second.consumer_ = consumer;
                return true;
            }

            // �󶨶���
            bool bind(amqp_channel_t channel_id, const RmqExchangeInfoSt& exchange, const RmqQueueInfoSt& queue, const std::string& routing_key = "", amqp_table_t arguments = amqp_empty_table) {
                // ��������
                amqp_queue_declare_ok_t* queue_declare_ok = amqp_queue_declare(m_connection, channel_id
                                                                , queue.name_bytes_, queue.passive_, queue.durable_
                                                                , queue.exclusive_, queue.auto_delete_, queue.argument_);
                if (!queue_declare_ok) {
                    return false;
                }

                amqp_rpc_reply_t reply = amqp_get_rpc_reply(m_connection);
                if (AMQP_RESPONSE_NORMAL != reply.reply_type)
                    return false;

                amqp_queue_bind_ok_t* bind_ok = amqp_queue_bind(m_connection, channel_id, queue_declare_ok->queue
                                                    , exchange.name_bytes_, amqp_cstring_bytes(routing_key.c_str()), arguments);
                if (!bind_ok) {
                    return false;
                }
                reply = amqp_get_rpc_reply(m_connection);
                if (AMQP_RESPONSE_NORMAL != reply.reply_type)
                    return false;
                return true;
            }

            // ����channel
            // auto_ack: �Ƿ���Session�Զ�ȷ����Ϣ, ������ʹconsumer������no_ack_Ϊfalse, Ҳ���ڶ�ȡ����Ϣ���Զ�����ack
            // ����: channel_id, ��Ч����0
            amqp_channel_t create_channel(const RmqExchangeInfoSt& exchange, const RmqQueueInfoSt& queue, RmqRWType rwtype = RmqRWType::read_write, const std::string& routing_key = "", bool auto_ack = true, amqp_table_t arguments = amqp_empty_table) {
                return create_channel_inner(exchange, queue, rwtype, routing_key, auto_ack, arguments);
            }
            amqp_channel_t create_channel(const RmqExchangeInfoSt& exchange, RmqRWType rwtype = RmqRWType::read_write, const std::string& routing_key = "", bool auto_ack = true, amqp_table_t arguments = amqp_empty_table) {
                return create_channel_inner(exchange, RmqQueueInfoSt(), rwtype, routing_key, auto_ack, arguments);
            }
            amqp_channel_t create_channel(const RmqQueueInfoSt& queue, RmqRWType rwtype = RmqRWType::read_write, const std::string& routing_key = "", bool auto_ack = true, amqp_table_t arguments = amqp_empty_table) {
                return create_channel_inner(RmqExchangeInfoSt(), queue, rwtype, routing_key, auto_ack, arguments);
            }

            // �ͻ��˿�������, ͬʱ������ȡ
            void connect(const RmqConnectInfoSt& connect_info) {
                if (!m_atomic_switch.init())
                    return;

                m_connect_info = connect_info;
                m_read_thread.add_task(std::bind(&Session::handle_connect, this));
            }

            // �ͻ��˿�������, ͬʱ������ȡ
            void reconnect() {
                connect(m_connect_info);
            }
            void reconnect_channel(amqp_channel_t channel_id) {
                auto [ok, channel] = get_channel(channel_id);
                if (!ok) {
                    m_read_thread.add_task(std::bind(&Session::close_channel, this, "unbind channel", channel_id));
                    return;
                }
                m_read_thread.add_task(std::bind(&Session::handle_channel, this, channel_id, channel));
            }

            // ͬ���ر�
            void shutdown() {
                close("shutdown");
            }

            // ͬ���ر�
            void shutdown(amqp_channel_t channel_id) {
                // �ر�ͨ��
                if (m_channels_switch.find(channel_id) != m_channels_switch.end())
                    close_channel("shutdown", channel_id);
            }

            // multiple: �Ƿ�һ����ȫ��ȷ��
            void ack(amqp_channel_t channel_id, uint64_t delivery_tag, bool multiple = false) {
                amqp_basic_ack(m_connection, channel_id, delivery_tag, multiple);
            }

            // д��, ����ָ��routing_key
            // retry_times: �Զ��ط����Դ���
            // confirm: �Ƿ���������, �ȴ�ȷ�Ϸ���, ע���轫no_ack_��Ϊfalse
            bool write(amqp_channel_t channel_id, const std::string& routing_key, const char* send_msg, size_t size, bool confirm = false, bool mandatory = false, bool immediate = false, amqp_basic_properties_t* route_props = nullptr) {
                if (!is_connecting(channel_id)) {
                    return false;
                }

                return write_inner(channel_id, routing_key, send_msg, size, confirm, mandatory, immediate, route_props);
            }
            // д��, ����bind_queue��ʱ��Ĭ��routing_key
            bool write(amqp_channel_t channel_id, const char* send_msg, size_t size, bool confirm = false, bool mandatory = false, bool immediate = false, amqp_basic_properties_t* route_props = nullptr) {
                if (!is_connecting(channel_id)) {
                    return false;
                }

                return write_inner(channel_id, get_routing_key(channel_id), send_msg, size, confirm, mandatory, immediate, route_props);
            }

        protected:
            static SessionID GetNextSessionID() {
                static std::atomic<SessionID> next_session_id(CallBack::InvalidSessionID);
                return ++next_session_id;
            }

        private:
            amqp_channel_t create_channel_inner(const RmqExchangeInfoSt& exchange, const RmqQueueInfoSt& queue, RmqRWType rwtype, const std::string& routing_key, bool auto_ack, amqp_table_t arguments) {
                static std::atomic<amqp_channel_t> s_channel_id = INVALID_CHANNEL_ID;
                if (contain(rwtype, RmqRWType::exchange_read_write) && exchange.name_.empty())
                    return INVALID_CHANNEL_ID;
                if (contain(rwtype, RmqRWType::queue_read_write) && queue.name_.empty())
                    return INVALID_CHANNEL_ID;

                amqp_channel_t next_channel_id = INVALID_CHANNEL_ID;
                while ((next_channel_id = ++s_channel_id) == INVALID_CHANNEL_ID);

                return insert_channel(next_channel_id, ChannelSt{rwtype, auto_ack, exchange, queue, m_default_channel, RmqConsumerInfoSt(), routing_key, arguments});
            }

            inline bool contain(RmqRWType rwtype, RmqRWType des) {
                return ((int)rwtype & (int)des) != 0;
            }

            // �첽��
            void read() {
                m_read_thread.add_task(std::bind(&Session::handle_read, this));
            }

            bool write_inner(amqp_channel_t channel_id, const std::string& routing_key, const char* send_msg, size_t size, bool confirm, bool mandatory, bool immediate, amqp_basic_properties_t* route_props) {
                std::lock_guard<std::mutex> lock(m_write_mtx);
                if (m_max_wbuffer_size > NOLIMIT_WRITE_BUFFER_SIZE && m_cur_wbuffer_size + size > m_max_wbuffer_size) {
                    return false;
                }
                WriteBufInfo* buf = new WriteBufInfo{channel_id, routing_key, MemoryStream(send_msg, size, true), 0, confirm, mandatory, immediate, route_props};
                m_cur_wbuffer_size += buf->data_.size();
                // �Ƿ��ڷ���״̬��
                if (m_cur_in_write) {
                    m_write_buf.push(buf);
                    return true;
                }
                m_cur_in_write = true;
                async_write_inner(buf);
                return true;
            }
            
            // �첽д
            void async_write_inner(WriteBufInfo* buf) {
                m_write_thread.add_task(std::bind(&Session::handle_write, this, buf));
            }

            // �������ӻص�
            void handle_connect() {
                m_connection = amqp_new_connection();
                if (!m_connection) {
                    close("create new connection fail");
                    return;
                }
                m_socket = amqp_tcp_socket_new(m_connection);
                if (!m_socket) {
                    close("create tcp socket fail");
                    return;
                }

                int res = amqp_socket_open(m_socket, m_connect_info.ip_.c_str(), m_connect_info.port_);
                if (res != 0) {
                    close(std::string("open tcp socket fail: ") + amqp_error_string2(res));
                    return;
                }

                handle_login();
            }

            // ����ʼ
            void handle_login() {
                amqp_rpc_reply_t reply = amqp_login(m_connection, m_connect_info.addr_.c_str(),
                    0, 0x20000, m_connect_info.heart_beat_, AMQP_SASL_METHOD_PLAIN, m_connect_info.user_name_.c_str(), m_connect_info.password_.c_str());

                if (AMQP_RESPONSE_NORMAL != reply.reply_type) {
                    if (AMQP_RESPONSE_LIBRARY_EXCEPTION == reply.reply_type) {
                        close(std::string("login tcp socket fail: invalid vhost or authentication failure"));
                        return;
                    }
                    else if (AMQP_RESPONSE_SERVER_EXCEPTION == reply.reply_type) {
                        switch (reply.reply.id) {
                        case AMQP_CONNECTION_CLOSE_METHOD:
                            close((char *)((amqp_connection_close_t *)reply.reply.decoded)->reply_text.bytes);
                            return;
                        case AMQP_CHANNEL_CLOSE_METHOD:
                            close((char *)((amqp_channel_close_t *)reply.reply.decoded)->reply_text.bytes);
                            return;
                        default:
                            close("unknown server exception error: " + std::to_string(reply.reply.id));
                            return;
                        }
                    }
                    close("unknown exception");
                    return;
                }

                if (m_atomic_switch.start() && m_handler.open_cbk_) {
                    m_handler.open_cbk_(m_session_id);
                }

                // �Ƿ���Ҫ��������
                bool need_receive = false;

                // �������ͨ��, �ֱ�� Exchange �� Queue
                {
                    std::lock_guard<std::mutex> lock(m_channels_mtx);
                    for (auto& item : m_channels) {
                        if (handle_channel(item.first, item.second))
                            need_receive |= contain(item.second.rw_type_, RmqRWType::read);
                    }
                }

                // ��������ʷ��Ϣ�򲹳䷢��
                {
                    std::lock_guard<std::mutex> lock(m_write_mtx);
                    if (m_cur_in_write) {
                        auto buf = m_write_buf.front();
                        m_write_buf.pop();
                        async_write_inner(buf);
                    }
                }

                if (need_receive)
                    read();
            }

            bool handle_channel(const amqp_channel_t& channel_id, const ChannelSt& channel) {
                if (!m_channels_switch[channel_id].init())
                    return false;

                // ��ͨ��ǰ������
                amqp_channel_open_ok_t* channel_ok = amqp_channel_open(m_connection, channel_id);
                if (!channel_ok) {
                    close_channel(std::string("open channel fail"), channel_id);
                    return false;
                }

                if (!check_connection(channel_id, "check channel")) {
                    return false;
                }

                if (contain(channel.rw_type_, RmqRWType::exchange_write)) {
                    amqp_basic_qos(m_connection, channel_id, channel.channel_.prefetch_size_, channel.channel_.prefetch_count_, false);

                    // ����������
                    amqp_exchange_declare_ok_t* exchange_declare_ok = amqp_exchange_declare(m_connection, channel_id
                    , channel.exchange_.name_bytes_
                    , channel.exchange_.type_bytes_
                    , channel.exchange_.passive_, channel.exchange_.durable_, channel.exchange_.auto_delete_
                    , channel.exchange_.internal_, channel.exchange_.argument_);
                    if (!exchange_declare_ok) {
                        close_channel(std::string("open exchange fail"), channel_id);
                        return false;
                    }
                    if (!check_connection(channel_id, "check exchange")) {
                        return false;
                    }
                }

                if (contain(channel.rw_type_, RmqRWType::queue_read_write)) {
                    // ��������
                    amqp_queue_declare_ok_t* queue_declare_ok = amqp_queue_declare(m_connection, channel_id
                                    , channel.queue_.name_bytes_, channel.queue_.passive_
                                    , channel.queue_.durable_, channel.queue_.exclusive_
                                    , channel.queue_.auto_delete_, channel.queue_.argument_);
                    if (!queue_declare_ok) {
                        close_channel(std::string("open queue fail"), channel_id);
                        return false;
                    }
                    if (!check_connection(channel_id, "check queue")) {
                        return false;
                    }

                    // �󶨶���
                    // if (!channel.exchange_.name_.empty()) {
                    //     amqp_queue_bind_ok_t* bind_ok = amqp_queue_bind(m_connection, channel_id, queue_declare_ok->queue
                    //             , channel.exchange_.name_bytes_, channel.routing_key_bytes_, channel.bind_arguments_);
                    //     if (!bind_ok) {
                    //         close_channel(std::string("bind queue fail"), channel_id);
                    //         return false;
                    //     }
                    //     if (!check_connection(channel_id, "check bind")) {
                    //         return false;
                    //     }
                    // }

                    // ��������Ϣ
                    amqp_basic_consume_ok_t* consume_ok = amqp_basic_consume(m_connection, channel_id, queue_declare_ok->queue
                                    , channel.consumer_.tag_, channel.consumer_.no_local_
                                    , channel.consumer_.no_ack_, channel.consumer_.exclusive_
                                    , channel.consumer_.argument_);
                    if (!consume_ok) {
                        close_channel(std::string("bind consume fail"), channel_id);
                        return false;
                    }
                    if (!check_connection(channel_id, "check consume")) {
                        return false;
                    }
                }

                m_channels_switch[channel_id].start();
                if (m_handler.open_channel_cbk_) {
                    m_handler.open_channel_cbk_(m_session_id, channel_id);
                }
                return true;
            }

            bool check_connection(const amqp_channel_t& channel_id, const std::string& title) {
                amqp_rpc_reply_t reply = amqp_get_rpc_reply(m_connection);
                if (AMQP_RESPONSE_NORMAL == reply.reply_type)
                    return true;

                std::string exceptionText;
                if (AMQP_RESPONSE_LIBRARY_EXCEPTION == reply.reply_type) {
                    close_channel(title + " fail: " + amqp_error_string2(reply.library_error), channel_id);
                    return false;
                }
                else if (AMQP_RESPONSE_SERVER_EXCEPTION == reply.reply_type) {
                    switch (reply.reply.id) {
                    case AMQP_CONNECTION_CLOSE_METHOD:
                        exceptionText = (char *)((amqp_connection_close_t *)reply.reply.decoded)->reply_text.bytes;
                        break;
                    case AMQP_CHANNEL_CLOSE_METHOD:
                        exceptionText = (char *)((amqp_channel_close_t *)reply.reply.decoded)->reply_text.bytes;
                        break;
                    default:
                        exceptionText = "unknown server reply_type exception error";
                        break;
                    }
                    close_channel(title + " fail: " + exceptionText, channel_id);
                    return false;
                }
                close_channel(title + " fail: unknown exception", channel_id);
                return false;
            }

            // ������ص�
            void handle_read() {
                amqp_maybe_release_buffers(m_connection);
                amqp_envelope_t envelope = {0};
                amqp_rpc_reply_t rpc_reply;
                if (m_connect_info.read_timeout_ > 0) {
                    struct timeval timer = {m_connect_info.read_timeout_, 0};
                    rpc_reply = amqp_consume_message(m_connection,& envelope,& timer, m_connect_info.flags_);
                }
                else {
                    rpc_reply = amqp_consume_message(m_connection,& envelope, nullptr, m_connect_info.flags_);
                }
                
                auto [ok, channel] = get_channel(envelope.channel);
                if (!ok) {
                    if (AMQP_STATUS_TIMEOUT == rpc_reply.library_error) {
                        // auto tmp_err = std::string("unexpected state: ") + amqp_error_string2(rpc_reply.library_error);
                        read();
                        return;
                    }
                    close(std::string("read fail: ") + amqp_error_string2(rpc_reply.library_error));
                    return;
                }

                if (rpc_reply.reply_type != AMQP_RESPONSE_NORMAL) {
                    if (rpc_reply.library_error == AMQP_STATUS_TIMEOUT) {
                        close_channel(std::string("read timeout: ") + amqp_error_string2(rpc_reply.library_error), envelope.channel);
                        return;
                    }
                    if (rpc_reply.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION
                        && rpc_reply.library_error == AMQP_STATUS_UNEXPECTED_STATE) {
                        close_channel(std::string("unexpected state: ") + amqp_error_string2(rpc_reply.library_error), envelope.channel);
                        return;
                    }
                    if (rpc_reply.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION) {
                        close_channel(std::string("library error: ") + amqp_error_string2(rpc_reply.library_error), envelope.channel);
                        return;
                    }
                    close_channel(std::string("read error: ") + amqp_error_string2(rpc_reply.library_error), envelope.channel);
                    return;
                }

                if (m_handler.read_cbk_) {
                    m_handler.read_cbk_(m_session_id, envelope.channel, envelope.delivery_tag, (char*)envelope.message.body.bytes, envelope.message.body.len);
                }
                if (envelope.delivery_tag != 0 && channel.auto_ack_ && (m_connect_info.flags_ & /*AMQP_AUTOACK*/1) == 0 &&  !channel.consumer_.no_ack_) 
                    amqp_basic_ack(m_connection, envelope.channel, envelope.delivery_tag, false);

                // ���ѵ�������
                // amqp_maybe_release_buffers_on_channel(m_connection, envelope.channel);
                amqp_destroy_envelope(&envelope);

                if (has_stoped()) {
                    return;
                }

                read();
            }

            // ����д�ص�
            void handle_write(WriteBufInfo* buf) {
                BTool::ScopeGuard ext([&] {
                    std::lock_guard<std::mutex> lock(m_write_mtx);
                    m_cur_wbuffer_size -= buf->data_.length();
                    delete buf;
                    if (m_write_buf.empty()) {
                        std::queue<WriteBufInfo*> empty;
                        m_write_buf.swap(empty); // �ͷŻ���ռ�
                        m_cur_in_write = false;
                        return;
                    }
                    buf = m_write_buf.front();
                    m_write_buf.pop();
                    async_write_inner(buf);
                });

                if (has_stoped(buf->channel_id_)) {
                    if (m_handler.write_cbk_) {
                        m_handler.write_cbk_(false, m_session_id, buf->channel_id_, buf->data_.data(), buf->data_.size());
                    }
                    return;
                }

                auto [ok, channel] = get_channel(buf->channel_id_);
                if (!ok) {
                    return;
                }

                if (buf->confirm_) {
                    amqp_confirm_select(m_connection, buf->channel_id_);
                }
                int res = amqp_basic_publish(m_connection, buf->channel_id_, channel.exchange_.name_bytes_
                        // ������Ϊ����routing_keyҲΪ��, ��󶨶���
                        , (channel.exchange_.name_.empty() && channel.routing_key_.empty()) ? channel.queue_.name_bytes_ : channel.routing_key_bytes_
                        , buf->mandatory_, buf->immediate_, buf->route_props_
                        , amqp_bytes_t{buf->data_.length(), buf->data_.data()});

                if (AMQP_STATUS_OK != res) {
                    if (m_handler.write_cbk_) {
                        m_handler.write_cbk_(false, m_session_id, buf->channel_id_, buf->data_.data(), buf->data_.size());
                    }
                    close_channel(std::string("write fail: ") + amqp_error_string2(res), buf->channel_id_);
                    return;
                }

                if (!check_connection(buf->channel_id_, "check publish"))
                    return;

                if (buf->confirm_) {
                    amqp_frame_t frame;
                    if (AMQP_STATUS_OK != (res = amqp_simple_wait_frame(m_connection,&frame))) {
                        close_channel(std::string("wait confirm frame fail: ") + amqp_error_string2(res), buf->channel_id_);
                        return;
                    }

                    if (AMQP_FRAME_METHOD == frame.frame_type) {
                        switch (frame.payload.method.id) {
                        case AMQP_BASIC_DELIVER_METHOD:
                            break;
                        case AMQP_BASIC_ACK_METHOD:
                            break;
                        case AMQP_BASIC_NACK_METHOD:
                            break;
                        case AMQP_BASIC_RETURN_METHOD:
                        case AMQP_CHANNEL_CLOSE_METHOD:
                        case AMQP_CONNECTION_CLOSE_METHOD:
                            close_channel(std::string("wait confirm frame fail: ") + amqp_error_string2(frame.payload.method.id), buf->channel_id_);
                            return;
                        default:
                            break;
                        }
                    }
                }

                if (m_handler.write_cbk_) {
                    m_handler.write_cbk_(true, m_session_id, buf->channel_id_, buf->data_.data(), buf->data_.size());
                }
            }

            void close(const std::string& msg) {
                if (!m_atomic_switch.stop())
                    return;

                amqp_maybe_release_buffers(m_connection);
                amqp_connection_close(m_connection, AMQP_REPLY_SUCCESS);
                amqp_destroy_connection(m_connection);
                m_connection = nullptr;

                for (auto& item : m_channels_switch) {
                    item.second.reset();
                }

                m_atomic_switch.reset();

                if (m_handler.close_cbk_) {
                    m_handler.close_cbk_(m_session_id, msg.c_str());
                }
            }

            void close_channel(const std::string& msg, amqp_channel_t channel_id) {
                if (!m_channels_switch[channel_id].has_started()) {
                    m_channels_switch[channel_id].reset();
                    if (m_handler.close_channel_cbk_) {
                        m_handler.close_channel_cbk_(m_session_id, channel_id, msg.c_str());
                    }
                    return;
                }

                if (!m_channels_switch[channel_id].stop()) {
                    return;
                }

                amqp_maybe_release_buffers_on_channel(m_connection, channel_id);
                amqp_rpc_reply_t ret = amqp_channel_close(m_connection, channel_id, AMQP_REPLY_SUCCESS);

                m_channels_switch[channel_id].reset();

                if (m_handler.close_channel_cbk_) {
                    m_handler.close_channel_cbk_(m_session_id, channel_id, msg.c_str());
                }
            }

            bool check_channel(amqp_channel_t channel_id) {
                std::lock_guard<std::mutex> lock(m_channels_mtx);
                auto iter = m_channels.find(channel_id);
                return iter != m_channels.end();
            }

            std::pair<bool, ChannelSt> get_channel(amqp_channel_t channel_id) {
                std::lock_guard<std::mutex> lock(m_channels_mtx);
                auto iter = m_channels.find(channel_id);
                if (iter != m_channels.end()) {
                    return std::make_pair(true, iter->second);
                }
                return std::make_pair(false, ChannelSt());
            }

            std::string get_routing_key(amqp_channel_t channel_id) {
                std::lock_guard<std::mutex> lock(m_channels_mtx);
                auto iter = m_channels.find(channel_id);
                if (iter != m_channels.end()) {
                    return iter->second.routing_key_;
                }
                return "";
            }

            amqp_channel_t insert_channel(amqp_channel_t next_channel_id, ChannelSt&& channel) {
                std::lock_guard<std::mutex> lock(m_channels_mtx);
                auto [iter, ok] = m_channels.emplace(next_channel_id, std::move(channel));
                if (ok) {
                    iter->second.routing_key_bytes_ = amqp_cstring_bytes(iter->second.routing_key_.c_str());
                    m_channels_switch[next_channel_id]; // ��ǰ���
                    return iter->first;
                }
                return INVALID_CHANNEL_ID;
            }

        private:
            BTool::ParallelTaskPool     m_read_thread;
            BTool::ParallelTaskPool     m_write_thread;

            amqp_connection_state_t     m_connection = nullptr;
            amqp_socket_t*              m_socket = nullptr;
            SessionID                   m_session_id;

            // д�������ݱ�����
            std::mutex                  m_write_mtx;
            // д����
            std::queue<WriteBufInfo*>   m_write_buf;
            // ���д��������С
            size_t                      m_max_wbuffer_size;
            // ��ǰд��������С
            size_t                      m_cur_wbuffer_size;
            // ��ǰ�Ƿ�����д
            bool                        m_cur_in_write;
            // �ص�����
            CallBack                    m_handler;
            // ԭ����ͣ��־
            AtomicSwitch                m_atomic_switch;
            // ������Ϣ
            RmqConnectInfoSt            m_connect_info;
            // ���ӽ�����+��������Ӧ��channel_id
            RmqChannelInfoSt            m_default_channel;
            // ����ͨ��
            std::mutex                              m_channels_mtx;
            std::map<amqp_channel_t, ChannelSt>     m_channels;
            std::map<amqp_channel_t, AtomicSwitch>  m_channels_switch;
        };
    }
}