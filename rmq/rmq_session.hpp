/******************************************************************************
File name:  rmq_session.hpp
Author:     AChar
Purpose:    rmq连接类
Note:       读写分为两个独立线程
Demo:
    大约性能在每秒3W左右
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
        // RMQ连接对象
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
                INVALID_CHANNEL_ID = 0,         // 无效通道id
                NOLIMIT_WRITE_BUFFER_SIZE = 0,  // 无限制
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
                int             read_timeout_ = 1;      // 监听超时秒数, 0为无限等待
                int             heart_beat_ = 0;        /* 链接心跳秒数, 0为无心跳,
                                                           无心跳则需要amqp_basic_publish/amqp_simple_wait_frame/amqp_simple_wait_frame_noblock来维系*/
                unsigned short  port_ = 5672;
                std::string     addr_ = "/";
                int             flags_ = 0;             /* 若无法找到以下定义则表示在此版本下无效
                                                           AMQP_NO_WAIT: 指定消费者应该立即返回, 如果没有消息可用, 则不会等待
                                                                如果设置了此标志, 则函数可能会立即返回, 而不管是否有消息可用
                                                           AMQP_AUTOACK: 指定消费者应自动确认收到的消息, 而不需要显式调用amqp_basic_ack方法
                                                                如果未设置此标志, 则必须在成功处理消息后手动调用amqp_basic_ack来确认收到的消息
                                                           AMQP_SINGLE_MESSAGE: 指定消费者只应接收一条消息, 然后返回
                                                                如果设置了此标志, 则消费者仅消费一条消息, 然后退出amqp_consume_message函数并返回
                                                           这些标志位可以组合在一起使用, 例如, 可以将AMQP_AUTOACK和AMQP_NO_WAIT一起设置, 以便消费者立即自动确认收到的消息, 如果没有消息可用, 则立即返回
                                                           注意, AMQP_NO_WAIT和AMQP_AUTOACK标志可能会导致一些消息的丢失或重复处理
                                                        */
            };

            struct RmqExchangeInfoSt {
                std::string     name_;                          // 交换机名
                std::string     type_;                          /* 交换机的类型
                                                                   fanout: 广播, 交换机可以向所有已经被消费者连接的队列发送相同的一条消息
                                                                   direct: 直接匹配, 当消息的routing_key与绑定的队列的routing_key完全匹配时, Exchange将消息发送到该队列中
                                                                   topic: 正则匹配, Exchange 将消息发送到与消息的routing_key匹配的所有队列中, routing_key可以使用通配符*(匹配单个单词)和#(匹配多个单词)
                                                                   headers: 使用消息头的键值对匹配, Exchange根据消息头中的键值对来选择绑定的队列, 如文本给队列1, 图片给队列2等等
                                                                */
                bool            passive_ = false;               // 检测是否消极创建交换机, 若否则不存在时主动创建
                bool            durable_ = true;                // 交换机是否持久化, 若是则exchange将在server重启前一直有效
                bool            internal_ = false;              // 交换机是否是内部专用, 若是则不能往该exchange里面发消息
                bool            auto_delete_ = false;           // 交换机是否需要自动删除, 若是则exchange将在与其绑定的queue都删除之后删除自己
                amqp_table_t    argument_ = amqp_empty_table;   // 交换机扩展参数, 默认为: amqp_empty_table
                amqp_bytes_t    name_bytes_ = amqp_cstring_bytes(name_.c_str());
                amqp_bytes_t    type_bytes_ = amqp_cstring_bytes(type_.c_str());
            };

            struct RmqQueueInfoSt {
                std::string     name_;                          // 队列名
                bool            passive_ = false;               // 检测是否消极创建队列, 若否则不存在时主动创建
                bool            durable_ = true;                // 队列是否持久化, 若是则queue将在server重启前一直有效
                bool            exclusive_ = false;             // 检测队列是否独占, 若是则queue将只能被声明它的消费者使用
                bool            auto_delete_ = false;           // 队列是否需要自动删除, 若是则queue将在其消费者停止使用之后自动删除自己
                amqp_table_t    argument_ = amqp_empty_table;   // 队列扩展参数, 默认为: amqp_empty_table
                amqp_bytes_t    name_bytes_ = amqp_cstring_bytes(name_.c_str());
            };

            struct RmqChannelInfoSt {
                uint32_t        prefetch_size_ = 0;             // 预取的数据量大小, 一般设置为0
                uint16_t        prefetch_count_ = 0;            // 每次预取的消息数量
            };

            struct RmqConsumerInfoSt {
                bool            no_local_ = false;              // 表示消费者是否不接收其自己发布的消息
                bool            no_ack_ = false;                // 是否自动应答, 若否则需要确认消息后再从队列中删除消息
                bool            exclusive_ = false;             // 检测连接是否独占, 若是则当前连接不在时, 队列自动删除
                amqp_bytes_t    tag_ = amqp_empty_bytes;        // 消费扩展参数, 默认为: amqp_empty_bytes
                amqp_table_t    argument_ = amqp_empty_table;   // 队列扩展参数, 默认为: amqp_empty_table
            };

        private:
            struct ChannelSt {
                RmqRWType           rw_type_;                   // 读写类型
                bool                auto_ack_;                  // 是否由Session自动确认消息, 若是则即使consumer中设置no_ack_为false, 也会在读取完消息后自动返回ack
                RmqExchangeInfoSt   exchange_;
                RmqQueueInfoSt      queue_;
                RmqChannelInfoSt    channel_;
                RmqConsumerInfoSt   consumer_;
                std::string         routing_key_;
                amqp_table_t        bind_arguments_;            // 绑定扩展参数, 默认为: amqp_empty_table
                amqp_bytes_t        routing_key_bytes_;
            };

            struct WriteBufInfo {
                amqp_channel_t              channel_id_;
                std::string                 routing_key_;
                MemoryStream                data_;
                int                         retry_count_;
                bool                        confirm_;           // 是否需要强制等待服务器返回ack
                bool                        mandatory_;         // 如果消息无法路由到指定队列, 则消息是否需要被返回给消息发布者
                bool                        immediate_;         // 如果消息无法立即被消费, 则消息是否需要被返回给消息发布者
                amqp_basic_properties_t*    route_props_;
            };

        public:
            // RMQ连接对象, 默认队列发送模式, 可通过set_only_one_mode设置为批量发送模式
            // max_buffer_size: 最大写缓冲区大小; 0则表示无限制
            // max_task_count: 写消息最大任务缓存个数, 超过该数量将产生阻塞; 0则表示无限制
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

            // 设置回调, 采用该形式可回调至不同类中分开处理
            Session& register_cbk(const CallBack& handler) {
                m_handler = handler;
                return *this;
            }
            // 设置开启连接回调
            Session& register_open_cbk(const CallBack::open_cbk& cbk) {
                m_handler.open_cbk_ = cbk;
                return *this;
            }
            // 设置关闭连接回调
            Session& register_close_cbk(const CallBack::close_cbk& cbk) {
                m_handler.close_cbk_ = cbk;
                return *this;
            }
            // 设置开启连接回调
            Session& register_open_channel_cbk(const CallBack::open_channel_cbk& cbk) {
                m_handler.open_channel_cbk_ = cbk;
                return *this;
            }
            // 设置关闭连接回调
            Session& register_close_channel_cbk(const CallBack::close_channel_cbk& cbk) {
                m_handler.close_channel_cbk_ = cbk;
                return *this;
            }
            // 设置读取消息回调
            Session& register_read_cbk(const CallBack::read_cbk& cbk) {
                m_handler.read_cbk_ = cbk;
                return *this;
            }
            // 设置已发送消息回调
            Session& register_write_cbk(const CallBack::write_cbk& cbk) {
                m_handler.write_cbk_ = cbk;
                return *this;
            }

            // 获取连接者IP
            const std::string& get_ip() const {
                return m_connect_info.ip_;
            }

            // 获取连接者port
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
            // 设置消息自动重发尝试次数, 暂不支持
            // void set_retry_send_count() {}

            // 设置通道默认属性, 注意必须在bind_queue之前使用
            void set_channel_default_param(const RmqChannelInfoSt& channel) {
                m_default_channel = channel;
            }
            const RmqChannelInfoSt& get_channel_default_param() const {
                return m_default_channel;
            }

            // 设置指定通道属性, 注意必须在bind_queue之后使用
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

            // 绑定队列
            bool bind(amqp_channel_t channel_id, const RmqExchangeInfoSt& exchange, const RmqQueueInfoSt& queue, const std::string& routing_key = "", amqp_table_t arguments = amqp_empty_table) {
                // 声明队列
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

            // 创建channel
            // auto_ack: 是否由Session自动确认消息, 若是则即使consumer中设置no_ack_为false, 也会在读取完消息后自动返回ack
            // 返回: channel_id, 无效返回0
            amqp_channel_t create_channel(const RmqExchangeInfoSt& exchange, const RmqQueueInfoSt& queue, RmqRWType rwtype = RmqRWType::read_write, const std::string& routing_key = "", bool auto_ack = true, amqp_table_t arguments = amqp_empty_table) {
                return create_channel_inner(exchange, queue, rwtype, routing_key, auto_ack, arguments);
            }
            amqp_channel_t create_channel(const RmqExchangeInfoSt& exchange, RmqRWType rwtype = RmqRWType::read_write, const std::string& routing_key = "", bool auto_ack = true, amqp_table_t arguments = amqp_empty_table) {
                return create_channel_inner(exchange, RmqQueueInfoSt(), rwtype, routing_key, auto_ack, arguments);
            }
            amqp_channel_t create_channel(const RmqQueueInfoSt& queue, RmqRWType rwtype = RmqRWType::read_write, const std::string& routing_key = "", bool auto_ack = true, amqp_table_t arguments = amqp_empty_table) {
                return create_channel_inner(RmqExchangeInfoSt(), queue, rwtype, routing_key, auto_ack, arguments);
            }

            // 客户端开启连接, 同时开启读取
            void connect(const RmqConnectInfoSt& connect_info) {
                if (!m_atomic_switch.init())
                    return;

                m_connect_info = connect_info;
                m_read_thread.add_task(std::bind(&Session::handle_connect, this));
            }

            // 客户端开启连接, 同时开启读取
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

            // 同步关闭
            void shutdown() {
                close("shutdown");
            }

            // 同步关闭
            void shutdown(amqp_channel_t channel_id) {
                // 关闭通道
                if (m_channels_switch.find(channel_id) != m_channels_switch.end())
                    close_channel("shutdown", channel_id);
            }

            // multiple: 是否一次性全部确认
            void ack(amqp_channel_t channel_id, uint64_t delivery_tag, bool multiple = false) {
                amqp_basic_ack(m_connection, channel_id, delivery_tag, multiple);
            }

            // 写入, 采用指定routing_key
            // retry_times: 自动重发尝试次数
            // confirm: 是否启用事务, 等待确认返回, 注意需将no_ack_设为false
            bool write(amqp_channel_t channel_id, const std::string& routing_key, const char* send_msg, size_t size, bool confirm = false, bool mandatory = false, bool immediate = false, amqp_basic_properties_t* route_props = nullptr) {
                if (!is_connecting(channel_id)) {
                    return false;
                }

                return write_inner(channel_id, routing_key, send_msg, size, confirm, mandatory, immediate, route_props);
            }
            // 写入, 采用bind_queue绑定时的默认routing_key
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

            // 异步读
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
                // 是否处于发送状态中
                if (m_cur_in_write) {
                    m_write_buf.push(buf);
                    return true;
                }
                m_cur_in_write = true;
                async_write_inner(buf);
                return true;
            }
            
            // 异步写
            void async_write_inner(WriteBufInfo* buf) {
                m_write_thread.add_task(std::bind(&Session::handle_write, this, buf));
            }

            // 处理连接回调
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

            // 处理开始
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

                // 是否需要开启监听
                bool need_receive = false;

                // 创建多个通道, 分别绑定 Exchange 和 Queue
                {
                    std::lock_guard<std::mutex> lock(m_channels_mtx);
                    for (auto& item : m_channels) {
                        if (handle_channel(item.first, item.second))
                            need_receive |= contain(item.second.rw_type_, RmqRWType::read);
                    }
                }

                // 若存在历史消息则补充发送
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

                // 多通道前置声明
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

                    // 声明交换机
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
                    // 声明队列
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

                    // 绑定队列
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

                    // 绑定消费消息
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

            // 处理读回调
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

                // 消费掉读缓存
                // amqp_maybe_release_buffers_on_channel(m_connection, envelope.channel);
                amqp_destroy_envelope(&envelope);

                if (has_stoped()) {
                    return;
                }

                read();
            }

            // 处理写回调
            void handle_write(WriteBufInfo* buf) {
                BTool::ScopeGuard ext([&] {
                    std::lock_guard<std::mutex> lock(m_write_mtx);
                    m_cur_wbuffer_size -= buf->data_.length();
                    delete buf;
                    if (m_write_buf.empty()) {
                        std::queue<WriteBufInfo*> empty;
                        m_write_buf.swap(empty); // 释放缓存空间
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
                        // 交易所为空且routing_key也为空, 则绑定队列
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
                    m_channels_switch[next_channel_id]; // 提前入队
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

            // 写缓存数据保护锁
            std::mutex                  m_write_mtx;
            // 写缓冲
            std::queue<WriteBufInfo*>   m_write_buf;
            // 最大写缓冲区大小
            size_t                      m_max_wbuffer_size;
            // 当前写缓冲区大小
            size_t                      m_cur_wbuffer_size;
            // 当前是否正在写
            bool                        m_cur_in_write;
            // 回调操作
            CallBack                    m_handler;
            // 原子启停标志
            AtomicSwitch                m_atomic_switch;
            // 连接信息
            RmqConnectInfoSt            m_connect_info;
            // 连接交易所+队列所对应的channel_id
            RmqChannelInfoSt            m_default_channel;
            // 连接通道
            std::mutex                              m_channels_mtx;
            std::map<amqp_channel_t, ChannelSt>     m_channels;
            std::map<amqp_channel_t, AtomicSwitch>  m_channels_switch;
        };
    }
}