/************************************************************************
 * File name:      rpc_base.hpp
 * Author:			AChar
 * Purpose:  受信环境下的rpc通讯
 * Date:     2020-10-27 15:18:11
 * 使用方法:

    void foo_1(NetCallBack::SessionID session_id) {}
    struct st_1 {
        void foo_1(NetCallBack::SessionID session_id, int rsp_info, int rsp_info2) {}
    };
    void set(NetCallBack::SessionID session_id, int a) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    int add(NetCallBack::SessionID session_id, int a, int b) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return a + b;
    }
    void add_void(NetCallBack::SessionID session_id, int a, int b) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return;
    }
    std::tuple<int, int> get_tuple(NetCallBack::SessionID session_id, int a, int b) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return std::forward_as_tuple(a, b);
    }

 服务端:
     class TestService {
        typedef RpcService<DefaultProxyPkgHandle, DefaultProxyMsgHandle, 30000> rpc_server;
        rpc_server                                  m_rpc_server;
    public:
        TestService() {
            m_rpc_server.register_open_cbk([&](rpc_server::SessionID session_id) { std::cout << "connect" << std::endl; });
            m_rpc_server.register_close_cbk([&](rpc_server::SessionID session_id) { std::cout << "close" << std::endl; });
        }
        ~TestService() {}
    public:
        void init(){
            m_rpc_server.listen("127.0.0.1", 61239);
            BTool::ParallelTaskPool pool;
            pool.start();
            // 同步绑定不直接返回,需主动返回
            m_rpc_server.bind("bind lambda", [this, &pool](rpc_server::SessionID session_id, const rpc_server::message_head& head, int rslt, int rsp_info) {
                pool.add_task([=] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    bool rsltbb = m_rpc_server.rsp_bind("bind lambda", session_id, head.req_id_, head.rpc_model_, rpc_server::msg_status::ok, rslt + rsp_info);
                });
            });

            // 同步绑定直接返回
            m_rpc_server.bind_auto("foo_1", &foo_1);
            m_rpc_server.bind_auto("st_1 foo_1", &st_1::foo_1, new st_1);
            m_rpc_server.bind_auto("set", &set);
            m_rpc_server.bind_auto("add", &add);
            m_rpc_server.bind_auto("add_void", &add_void);
            m_rpc_server.bind_auto("get_tuple", &get_tuple);
            m_rpc_server.bind_auto("lambda", [](NetCallBack::SessionID session_id) {});
            std::function<int(rpc_server::SessionID, int)> func = [&](rpc_server::SessionID, int rsp_info)->int {};
            m_rpc_server.bind_auto_functional("functional", func);

            // 主动推送, 同步等待
            auto [rsp_status, rsp_rslt] = m_rpc_server.push<int>("push", session_id, req_params...);
            auto [rsp_status3, rsp_rslt3] = m_rpc_server.push<200, int>("push", session_id, req_params...);
            auto rsp_status2 = m_rpc_server.push("push2", session_id);
            // 主动推送, 异步等待
            m_rpc_server.push_back("push_back", session_id, req_params...)([&](rpc_server::SessionID, rpc_server::msg_status, rsp_args...) {});
            m_rpc_server.push_back<200>("push_back", session_id, req_params...)([&](rpc_server::SessionID, rpc_server::msg_status, rsp_args...) {});

            system("pause");
        }
    };

客户端:
    class TestClient {
        typedef RpcClient<DefaultProxyPkgHandle, DefaultProxyMsgHandle, 30000>    rpc_client;
        rpc_client                                  m_rpc_client;
    public:
        TestClient() {
            m_rpc_client.register_open_cbk([&](rpc_client::SessionID session_id) { std::cout << "connect" << std::endl; });
            m_rpc_client.register_close_cbk([&](rpc_client::SessionID session_id) { std::cout << "close" << std::endl; });
        }
        ~TestClient() {}
    public:
        void init(){
            m_rpc_client.connect("127.0.0.1", 61239);

            BTool::ParallelTaskPool pool;
            pool.start();
            // 同步绑定不直接返回,需主动返回
            m_rpc_client.bind("bind lambda", [this, &pool](m_rpc_client::SessionID session_id, const m_rpc_client::message_head& head, int rslt, int rsp_info) {
                pool.add_task([=] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    bool rsltbb = m_rpc_client.rsp_bind("bind lambda", session_id, head.req_id_, head.rpc_model_, m_rpc_client::msg_status::ok, rslt + rsp_info);
                });
            });

            // 同步绑定直接返回
            m_rpc_client.bind_auto("foo_1", &foo_1);
            m_rpc_client.bind_auto("st_1 foo_1", &st_1::foo_1, new st_1);
            m_rpc_client.bind_auto("set", &set);
            m_rpc_client.bind_auto("add", &add);
            m_rpc_client.bind_auto("add_void", &add_void);
            m_rpc_client.bind_auto("get_tuple", &get_tuple);
            m_rpc_client.bind_auto("lambda", [](NetCallBack::SessionID session_id) {});
            std::function<int(m_rpc_client::SessionID, int)> func = [&](m_rpc_client::SessionID, int rsp_info)->int {};
            m_rpc_client.bind_auto_functional("functional", func);

            // 主动推送, 同步等待
            auto [rsp_status, rsp_rslt] = m_rpc_client.call<int>("call", session_id, req_params...);
            auto [rsp_status3, rsp_rslt3] = m_rpc_client.call<200, int>("call", session_id, req_params...);
            auto rsp_status2 = m_rpc_client.call("push2", session_id);
            // 主动推送, 异步等待
            m_rpc_client.call_back("push_back", session_id, req_params...)([&](m_rpc_client::SessionID, m_rpc_client::msg_status, rsp_args...) {});
            m_rpc_client.call_back<200>("push_back", session_id, req_params...)([&](m_rpc_client::SessionID, m_rpc_client::msg_status, rsp_args...) {});

            system("pause");
        }
    };

*/

#pragma once
#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <functional>
#include <future>
#include "../timer_manager.hpp"

namespace BTool
{
    namespace BoostNet
    {
        enum class msg_status : int8_t {
            no_bind = -1,   // 未绑定
            ok = 0,         // 服务端执行正确
            fail,           // 服务端执行失败
            timeout,        // 等待应答超时
            unpack_error,   // 解析数据失败
            send_error,     // 发送失败
            wait_error      // 等待失败
        };

        // 调用远程服务模式
        enum class rpc_model : uint8_t {
            future,         // 请求端同步等待请求结果, 应答端同步执行并自动返回函数返回值
            callback,       // 请求端异步返回请求结果, 应答端同步执行并自动返回函数返回值
            bind            // bind_auto:应答端同步执行订阅, 自动返回函数 / bind:应答端异步执行订阅, 需主动显式返回
        };

        // 请求应答模式
        enum class comm_model : uint8_t {
            request,
            rsponse,
        };

        struct error {
            error() : status_(msg_status::ok), msg_("") {}
            error(msg_status status, std::string_view msg)
                : status_(status), msg_(msg) {}

            msg_status          status_;
            std::string_view    msg_;
        };

#pragma pack (1)
        struct message_head {
            comm_model      comm_model_;
            rpc_model       rpc_model_;
            uint32_t        req_id_;
            uint8_t         title_size_;   // 最多255
            uint32_t        content_size_; // 最多4,294,967,295
        };
#pragma pack ()

        struct DefaultProxyMsgHandle {
            // 获取请求
            template<typename Type = void>
            typename std::enable_if<std::is_void<Type>::value, Type>::type
                get_req_params(MemoryStream& msg, msg_status& status) {
                if (msg.size() < sizeof(msg_status)) {
                    status = msg_status::unpack_error;
                    return;
                }
                msg.read(&status);
            }
            template<typename Type>
            typename std::enable_if<!std::is_void<Type>::value, Type>::type
                get_req_params(MemoryStream& msg, msg_status& status) {
                if (msg.size() < MemoryStream::get_args_sizeof<Type>() + sizeof(msg_status)) {
                    status = msg_status::unpack_error;
                    return Type();
                }
                msg.read(&status);
                return msg.read_args<Type>();
            }

            template<typename Type = void>
            typename std::enable_if<std::is_void<Type>::value, Type>::type
                get_rsp_params(MemoryStream& msg, msg_status& status) {
                if (msg.size() < sizeof(msg_status)) {
                    status = msg_status::unpack_error;
                    return;
                }
                msg.read(&status);
            }
            template<typename Type>
            typename std::enable_if<!std::is_void<Type>::value, Type>::type
                get_rsp_params(MemoryStream& msg, msg_status& status) {
                if (msg.size() < sizeof(msg_status)) {
                    status = msg_status::unpack_error;
                    return Type();
                }
                msg.read(&status);
                if (status == msg_status::no_bind) {
                    return Type();
                }

                if (msg.get_res_length() < MemoryStream::get_args_sizeof<Type>()) {
                    status = msg_status::unpack_error;
                    return Type();
                }
                if (status > msg_status::wait_error) {
                    status = msg_status::unpack_error;
                    return Type();
                }
                return msg.read_args<Type>();
            }
        };

        template<typename TProxyMsgHandle>
        struct ProxyMsg {
            template<typename ...Args>
            ProxyMsg(const std::string& rpc_name, const message_head& head, Args&& ...args)
                : rpc_name_(rpc_name)
                , head_(head)
            {
                msg_.append_args(std::forward<Args>(args)...);
                msg_.reset_offset(0);
            }
            template<typename ...Args>
            ProxyMsg(const std::string& rpc_name, comm_model comm_model, rpc_model rpc_model, uint32_t req_id, Args&& ...args)
                : rpc_name_(rpc_name)
                , head_({ comm_model, rpc_model, req_id, uint8_t(rpc_name.length()), 0 })
            {
                msg_.append_args(std::forward<Args>(args)...);
                msg_.reset_offset(0);
                head_.content_size_ = msg_.size();
            }
            ProxyMsg(const std::string& rpc_name, const message_head& head, std::string_view&& msg)
                : rpc_name_(rpc_name)
                , head_(head)
            {
                msg_.load(msg.data(), msg.size());
            }
            ProxyMsg(const std::string& rpc_name, comm_model comm_model, rpc_model rpc_model, uint32_t req_id, std::string_view&& msg)
                : rpc_name_(rpc_name)
                , head_({ comm_model, rpc_model, req_id, uint8_t(rpc_name.length()), uint32_t(msg.length()) })
            {
                msg_.load(msg.data(), msg.size());
            }
            ProxyMsg(const std::string& rpc_name, const message_head& head, MemoryStream&& msg)
                : rpc_name_(rpc_name)
                , head_(head)
                , msg_(std::move(msg))
            {
            }
            ProxyMsg(const std::string& rpc_name, comm_model comm_model, rpc_model rpc_model, uint32_t req_id, MemoryStream&& msg)
                : rpc_name_(rpc_name)
                , head_({ comm_model, rpc_model, req_id, uint8_t(rpc_name.length()), uint32_t(msg.length()) })
                , msg_(std::move(msg))
            {
            }
            virtual ~ProxyMsg() {}

            inline uint32_t get_req_id() const {
                return head_.req_id_;
            }
            inline comm_model get_comm_model() const {
                return head_.comm_model_;
            }
            inline rpc_model get_rpc_model() const {
                return head_.rpc_model_;
            }
            inline const std::string& get_rpc_name()const {
                return rpc_name_;
            }
            inline const message_head& get_message_head() const {
                return head_;
            }
            inline const char* get_package() const {
                return msg_.data();
            }
            inline size_t get_length() const {
                return msg_.get_length();
            }

            template<typename Type = void>
            Type get_req_params(msg_status& status) {
                return handle_.template get_req_params<Type>(msg_, status);
            }

            template<typename Type = void>
            Type get_rsp_params(msg_status& status) {
                return handle_.template get_rsp_params<Type>(msg_, status);
            }

        protected:
            std::string     rpc_name_;
            message_head    head_;
            MemoryStream    msg_;
            TProxyMsgHandle handle_;
        };

        template<typename TProxyMsgHandle>
        class DefaultProxyPkgHandle {
        public:
            typedef std::shared_ptr<ProxyMsg<TProxyMsgHandle>> ProxyMsgPtr;

            // 打包消息
            // 此处默认采用 head + title + content(status + data)的内存直接打包
            template<typename... Args>
            ProxyMsgPtr package_msg(const std::string& rpc_name, comm_model comm_model, rpc_model model, uint32_t req_id, msg_status status, Args&&... args) {
                uint8_t title_size = (uint8_t)rpc_name.length();
                
                // content长度, content 包含 status + 内容
                uint32_t content_length = MemoryStream::get_args_length<msg_status, Args...>(status, std::forward<Args>(args)...);

                BTool::MemoryStream write_buffer(sizeof(message_head) + title_size + content_length);

                // head
                message_head head{ comm_model, model, req_id, title_size, content_length };
                write_buffer.append(head);

                // rpc_name
                write_buffer.append(rpc_name.c_str(), title_size);

                // content
                write_buffer.append_args(status, std::forward<Args>(args)...);

                return std::make_shared<ProxyMsg<TProxyMsgHandle>>(rpc_name, head, std::move(write_buffer));
            }
            
            // 解包消息
            // 此处默认采用 head + title + content(status + data)的内存直接解包
            std::tuple<std::vector<ProxyMsgPtr>, size_t, error> unpackage_msg(const char* const msg, size_t bytes_transferred) {
                std::vector<ProxyMsgPtr>  resault;
                BTool::MemoryStream read_buffer(msg, bytes_transferred);
                while (read_buffer.get_res_length() >= sizeof(struct message_head) + sizeof(msg_status)) {
                    // 读取,自带漂移
                    message_head cur_head;
                    // head 读取, 自带漂移
                    read_buffer.read(&cur_head);

                    // 异常包, 此处可依据端口限制大小设置
                    if (cur_head.title_size_ == 0 || cur_head.content_size_ > (uint32_t)(-1) / 2) {
                        return { std::vector<ProxyMsgPtr>(), 0, error(msg_status::send_error, "异常包") };
                    }

                    // 断包判断
                    if (read_buffer.get_res_length() < cur_head.title_size_ + cur_head.content_size_) {
                        return { resault, read_buffer.get_offset() - sizeof(struct message_head), error() };
                    }

                    // rpc_name
                    std::string rpc_name(msg + read_buffer.get_offset(), cur_head.title_size_);
                    read_buffer.add_offset(cur_head.title_size_);

                    // content
                    std::string_view content(msg + read_buffer.get_offset(), cur_head.content_size_);
                    read_buffer.add_offset(cur_head.content_size_);

                    auto item = std::make_shared<ProxyMsg<TProxyMsgHandle>>(rpc_name, cur_head, std::move(content));

                    resault.push_back(item);
                }
                return { resault, read_buffer.get_offset(), error() };
            }
        };

        template<template<typename TProxyMsgHandle> typename TProxyPkgHandle, typename TProxyMsgHandle>
        struct ProxyPkg {
            typedef std::shared_ptr<ProxyMsg<TProxyMsgHandle>> ProxyMsgPtr;

            template<typename... TRspParams>
            ProxyMsgPtr package_msg(const std::string& rpc_name, comm_model comm_model, rpc_model model, uint32_t req_id, msg_status status, TRspParams&&... args) {
                return handle_.package_msg(rpc_name, comm_model, model, req_id, status, std::forward<TRspParams>(args)...);
            }
            // 返回: 消息集合, 处理长度, 错误信息
            std::tuple<std::vector<ProxyMsgPtr>, size_t, error> unpackage_msg(const char* const msg, size_t bytes_transferred) {
                return handle_.unpackage_msg(msg, bytes_transferred);
            }
        protected:
            TProxyPkgHandle<TProxyMsgHandle>  handle_;
        };

        template<typename TProxyMsgHandle>
        class SyncProxy {
            typedef std::shared_ptr<ProxyMsg<TProxyMsgHandle>>          ProxyMsgPtr;
            typedef std::shared_ptr<std::promise<ProxyMsgPtr>>         PromisePtr;

        public:
            void invoke(const ProxyMsgPtr& item) {
                uint32_t req_id = item->get_req_id();
                auto promise = get_promise(req_id);
                if (promise)
                    promise->set_value(item);
            }

            uint32_t get_next_promise() {
                PromisePtr p = std::make_shared<std::promise<ProxyMsgPtr>>();
                writeLock lock(m_mtx);
                m_future_map.emplace(++m_cur_req_id, p);
                return m_cur_req_id;
            }

            std::tuple<msg_status, ProxyMsgPtr> wait_for(uint32_t req_id, size_t milliseconds) {
                PromisePtr promise = get_promise(req_id);
                auto future = std::move(promise->get_future());
                auto future_status = future.wait_for(std::chrono::milliseconds(milliseconds));
                remove(req_id);
                if (future_status != std::future_status::ready) {
                    return std::forward_as_tuple(msg_status::timeout, nullptr);
                }
                return std::forward_as_tuple(msg_status::ok, future.get());
            }
            void remove(uint32_t req_id) {
                writeLock lock(m_mtx);
                m_future_map.erase(req_id);
            }
        private:
            PromisePtr get_promise(uint32_t req_id) {
                readLock lock(m_mtx);
                auto iter = m_future_map.find(req_id);
                if (iter == m_future_map.end()) {
                    return nullptr;
                }
                return iter->second;
            }
        private:
            rwMutex                                         m_mtx;
            uint32_t                                        m_cur_req_id = 0;
            std::unordered_map<uint32_t, PromisePtr>        m_future_map;
        };

        template<typename TProxyMsgHandle>
        class CallbackProxy {
            typedef std::shared_ptr<ProxyMsg<TProxyMsgHandle>>  ProxyMsgPtr;
            typedef NetCallBack::SessionID                      SessionID;
            // 回调模式内部解析函数
            typedef std::function<void(SessionID, msg_status, ProxyMsgPtr)>       rsp_callback_function_type;

            struct session_timer_st {
                SessionID               session_id_;
                TimerManager::TimerId   timer_id_;
            };

        public:
            CallbackProxy() : m_deadline_timer(1000, 1) { m_deadline_timer.start(); }

            void invoke(const ProxyMsgPtr& msg) {
                std::unique_lock<std::mutex> lock(m_mtx);
                auto sesson_timer = remove_timer(msg->get_req_id());

                auto iter = m_callback_map.find(msg->get_req_id());
                if (iter == m_callback_map.end())
                    return;
                iter->second(sesson_timer.session_id_, msg_status::ok, msg);
                m_callback_map.erase(iter);
            }

            template<typename TReturn, typename... TRspParams>
            inline uint32_t insert(size_t overtime, const std::function<TReturn(SessionID, msg_status, TRspParams...)>& callback) {
                std::unique_lock<std::mutex> lock(m_mtx);
                ++m_cur_req_id;
                m_callback_map.emplace(m_cur_req_id, std::bind(&CallbackProxy::proxy<TRspParams...>, this, callback, m_cur_req_id, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
                return m_cur_req_id;
            }
            template<typename TReturn, typename... TRspParams>
            inline uint32_t insert(std::function<TReturn(SessionID, msg_status, TRspParams...)>&& callback) {
                std::unique_lock<std::mutex> lock(m_mtx);
                ++m_cur_req_id;
                m_callback_map.emplace(m_cur_req_id, std::bind(&CallbackProxy::proxy<TRspParams...>, this, std::move(callback), m_cur_req_id, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
                return m_cur_req_id;
            }

            void insert_timer(size_t overtime, SessionID session_id, uint32_t req_id) {
                session_timer_st session_timer = { session_id, 0 };
                session_timer.timer_id_ = m_deadline_timer.insert_duration_once(overtime, [this, req_id](TimerManager::TimerId, const TimerManager::system_time_point&) {
                    remove(req_id, msg_status::timeout);
                });
                std::unique_lock<std::mutex> lock(m_mtx);
                m_overtime_reqs.emplace(req_id, std::move(session_timer));
            }

            void remove(uint32_t req_id, msg_status status) {
                std::unique_lock<std::mutex> lock(m_mtx);
                auto sesson_timer = remove_timer(req_id);

                auto iter = m_callback_map.find(req_id);
                if (iter == m_callback_map.end())
                    return;
                iter->second(sesson_timer.session_id_, status, nullptr);
                m_callback_map.erase(iter);
            }

        private:
            session_timer_st remove_timer(uint32_t req_id) {
                session_timer_st sesson_timer = { 0 };
                auto timer_iter = m_overtime_reqs.find(req_id);
                if (timer_iter != m_overtime_reqs.end()) {
                    sesson_timer = std::move(timer_iter->second);
                    m_deadline_timer.erase(sesson_timer.timer_id_);
                    m_overtime_reqs.erase(timer_iter);
                }
                return sesson_timer;
            }

            template<typename... TRspParams>
            void proxy(std::function<void(SessionID, msg_status, TRspParams...)>& func, uint32_t req_id, SessionID session_id, msg_status status, ProxyMsgPtr msg) {
                if constexpr (sizeof...(TRspParams) == 0) {
                    // 本身发送失败
                    if (status != msg_status::ok || !msg) {
                        std::apply(func, MemoryStream::tuple_merge(std::forward_as_tuple(session_id, status)));
                        return;
                    }
                    msg->template get_rsp_params<void>(status);
                    std::apply(func, std::forward_as_tuple(session_id, status));
                }
                else {
                    using args_type = std::tuple<typename std::decay<TRspParams>::type...>;
                    // 本身发送失败
                    if (status != msg_status::ok || !msg) {
                        std::apply(func, MemoryStream::tuple_merge(std::forward_as_tuple(session_id, status), args_type{}));
                        return;
                    }
                    auto comm_rslt = msg->template get_rsp_params<args_type>(status);
                    std::apply(func, MemoryStream::tuple_merge(std::forward_as_tuple(session_id, status), std::move(comm_rslt)));
                }
            }

        private:
            // 定时删除任务
            TimerManager                                                    m_deadline_timer;
            // 数据安全锁
            std::mutex                                                      m_mtx;
            // 当前请求id
            uint32_t                                                        m_cur_req_id = 0;
            // req_id, timer_id
            std::unordered_map<uint32_t, session_timer_st>                  m_overtime_reqs;
            // callback函数绑定集合
            std::unordered_map<uint32_t, rsp_callback_function_type>        m_callback_map;
        };

        template<template<typename TProxyMsgHandle> typename TProxyPkgHandle, typename TProxyMsgHandle, size_t DEFAULT_TIMEOUT>
        class RpcBase;

        template<template<typename TProxyMsgHandle> typename TProxyPkgHandle, typename TProxyMsgHandle, size_t DEFAULT_TIMEOUT = 1000>
        class BindProxy {
            typedef std::shared_ptr<ProxyMsg<TProxyMsgHandle>>  ProxyMsgPtr;
            typedef NetCallBack::SessionID                      SessionID;
            // 回调模式内部解析函数
            typedef std::function<void(SessionID, msg_status, ProxyMsgPtr)>       rsp_callback_function_type;

        public:
            BindProxy(RpcBase<TProxyPkgHandle, TProxyMsgHandle, DEFAULT_TIMEOUT>* parent) {
                m_parent = parent;
            }

            // 返回是否已绑定
            bool invoke(SessionID session_id, const ProxyMsgPtr& msg) {
                auto iter = m_bind_map.find(msg->get_rpc_name());
                if (iter == m_bind_map.end())
                    return false;
                iter->second(session_id, msg);
                return true;
            }

            template<typename TReturn, typename... TRspParams>
            inline void insert(const std::string& rpc_name, const std::function<TReturn(SessionID, const message_head&, TRspParams...)>& bindfunc) {
                m_bind_map[rpc_name] = std::bind(&BindProxy::bind_proxy<TReturn, TRspParams...>, this, bindfunc, rpc_name, std::placeholders::_1, std::placeholders::_2);
            }

            template<typename TReturn, typename... TRspParams>
            inline void insert_auto_rsp(const std::string& rpc_name, const std::function<TReturn(SessionID, TRspParams...)>& bindfunc) {
                m_bind_map[rpc_name] = std::bind(&BindProxy::bind_auto_proxy<TReturn, TRspParams...>, this, bindfunc, rpc_name, std::placeholders::_1, std::placeholders::_2);
            }

        protected:
/**************   bind_proxy  ******************/
            template<typename TReturn>
            typename std::enable_if<std::is_void<TReturn>::value, void>::type
                bind_proxy(std::function<TReturn(SessionID, const message_head&)> bindfunc, const std::string& rpc_name, SessionID session_id, const ProxyMsgPtr& msg) {
                msg_status status = msg_status::fail;
                msg->get_req_params(status);
                if (status != msg_status::ok) {
                    m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status);
                    return;
                }
                bind_invoke(bindfunc, session_id, msg->get_message_head());
            }
            template<typename TReturn>
            typename std::enable_if<!std::is_void<TReturn>::value, void>::type
                bind_proxy(std::function<TReturn(SessionID, const message_head&)> bindfunc, const std::string& rpc_name, SessionID session_id, const ProxyMsgPtr& msg) {
                msg_status status = msg_status::fail;
                msg->get_req_params(status);
                if (status != msg_status::ok) {
                    m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status, TReturn());
                    return;
                }
                bind_invoke(bindfunc, session_id, msg->get_message_head());
            }
            template<typename TReturn, typename TParam0, typename... TRspParams>
            typename std::enable_if<std::is_void<TReturn>::value, void>::type
                bind_proxy(std::function<TReturn(SessionID, const message_head&, TParam0, TRspParams...)> bindfunc, const std::string& rpc_name, SessionID session_id, const ProxyMsgPtr& msg) {
                msg_status status = msg_status::fail;
                using args_type = std::tuple<typename std::decay<TParam0>::type, typename std::decay<TRspParams>::type...>;
                auto rsp = msg->template get_req_params<args_type>(status);
                if (status != msg_status::ok) {
                    m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status);
                    return;
                }
                bind_invoke(bindfunc, session_id, msg->get_message_head(), std::move(rsp));
            }
            template<typename TReturn, typename TParam0, typename... TRspParams>
            typename std::enable_if<!std::is_void<TReturn>::value, void>::type
                bind_proxy(std::function<TReturn(SessionID, const message_head&, TParam0, TRspParams...)> bindfunc, const std::string& rpc_name, SessionID session_id, const ProxyMsgPtr& msg) {
                msg_status status = msg_status::fail;
                using args_type = std::tuple<typename std::decay<TParam0>::type, typename std::decay<TRspParams>::type...>;
                auto rsp = msg->template get_req_params<args_type>(status);
                if (status != msg_status::ok) {
                    m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status, TReturn());
                    return;
                }
                bind_invoke(bindfunc, session_id, msg->get_message_head(), std::move(rsp));
            }

            template<typename TReturn>
            decltype(auto) bind_invoke(const std::function<TReturn(SessionID, const message_head&)>& bindfunc, SessionID session_id, const message_head& head) {
                return std::apply(bindfunc, std::forward_as_tuple(session_id, head));
            }
            template<typename ArgsTuple, typename TReturn, typename... TRspParams>
            decltype(auto) bind_invoke(const std::function<TReturn(SessionID, const message_head&, TRspParams...)>& bindfunc, SessionID session_id, const message_head& head, ArgsTuple&& args) {
                return std::apply(bindfunc, MemoryStream::tuple_merge(std::forward_as_tuple(session_id, head), std::forward<ArgsTuple>(args)));
            }

            template<typename TReturn>
            typename std::enable_if<std::is_void<TReturn>::value, void>::type
                bind_auto_proxy(std::function<TReturn(SessionID)> bindfunc, const std::string& rpc_name, SessionID session_id, const ProxyMsgPtr& msg) {
                msg_status status = msg_status::fail;
                msg->get_req_params(status);
                if (status != msg_status::ok) {
                    m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status);
                    return;
                }
                bind_auto_invoke(bindfunc, session_id);
                m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status);
            }
            template<typename TReturn>
            typename std::enable_if<!std::is_void<TReturn>::value, void>::type
                bind_auto_proxy(std::function<TReturn(SessionID)> bindfunc, const std::string& rpc_name, SessionID session_id, const ProxyMsgPtr& msg) {
                msg_status status = msg_status::fail;
                msg->get_req_params(status);
                if (status != msg_status::ok) {
                    m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status, TReturn());
                    return;
                }
                auto resault = bind_auto_invoke(bindfunc, session_id);
                m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status, std::move(resault));
            }
            template<typename TReturn, typename TParam0, typename... TRspParams>
            typename std::enable_if<std::is_void<TReturn>::value, void>::type
                bind_auto_proxy(std::function<TReturn(SessionID, TParam0, TRspParams...)> bindfunc, const std::string& rpc_name, SessionID session_id, const ProxyMsgPtr& msg) {
                msg_status status = msg_status::fail;
                using args_type = std::tuple<typename std::decay<TParam0>::type, typename std::decay<TRspParams>::type...>;
                auto rsp = msg->template get_req_params<args_type>(status);
                if (status != msg_status::ok) {
                    m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status);
                    return;
                }
                bind_auto_invoke(bindfunc, session_id, std::move(rsp));
                m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status);
            }
            template<typename TReturn, typename TParam0, typename... TRspParams>
            typename std::enable_if<!std::is_void<TReturn>::value, void>::type
                bind_auto_proxy(std::function<TReturn(SessionID, TParam0, TRspParams...)> bindfunc, const std::string& rpc_name, SessionID session_id, const ProxyMsgPtr& msg) {
                msg_status status = msg_status::fail;
                using args_type = std::tuple<typename std::decay<TParam0>::type, typename std::decay<TRspParams>::type...>;
                auto rsp = msg->template get_req_params<args_type>(status);
                if (status != msg_status::ok) {
                    m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status, TReturn());
                    return;
                }
                auto resault = bind_auto_invoke(bindfunc, session_id, std::move(rsp));
                m_parent->rsp_bind(rpc_name, session_id, msg->get_req_id(), msg->get_rpc_model(), status, std::move(resault));
            }

            template<typename TReturn>
            decltype(auto) bind_auto_invoke(const std::function<TReturn(SessionID)>& bindfunc, SessionID session_id) {
                return std::apply(bindfunc, std::forward_as_tuple(session_id));
            }
            template<typename ArgsTuple, typename TReturn, typename... TRspParams>
            decltype(auto) bind_auto_invoke(const std::function<TReturn(SessionID, TRspParams...)>& bindfunc, SessionID session_id, ArgsTuple&& args) {
                return std::apply(bindfunc, MemoryStream::tuple_merge(session_id, std::forward<ArgsTuple>(args)));
            }

            // 绑定模式内部解析函数
            typedef std::function<void(SessionID, const ProxyMsgPtr&)>  bind_function_type;
            // bind函数绑定集合
            std::unordered_map<std::string, bind_function_type>          m_bind_map;
            // 所属对象
            RpcBase<TProxyPkgHandle, TProxyMsgHandle, DEFAULT_TIMEOUT>*  m_parent;
        };

        template<template<typename TProxyMsgHandle> typename TProxyPkgHandle, typename TProxyMsgHandle, size_t DEFAULT_TIMEOUT = 1000>
        class RpcBase {
        protected:
            typedef std::shared_ptr<ProxyMsg<TProxyMsgHandle>>  ProxyMsgPtr;
            typedef std::shared_ptr<std::promise<ProxyMsgPtr>>  PromisePtr;
            typedef NetCallBack::SessionID                      SessionID;

        public:
            RpcBase() : m_bind_proxy(this){}
            virtual ~RpcBase() {}

            /**************   call_back 请求操作定义  ******************/
            // 回调辅助操作类
            template<size_t TIMEOUT, typename TParam = std::nullptr_t>
            struct callback_req_op {
                // 无参数
                template<bool = std::is_null_pointer<TParam>::value>
                callback_req_op(RpcBase* parent, const std::string& rpc_name, SessionID session_id)
                    : parent_(parent)
                    , rpc_name_(rpc_name)
                    , session_id_(session_id) {}
                // 有参数
                template<bool = !std::is_null_pointer<TParam>::value>
                callback_req_op(RpcBase* parent, const std::string& rpc_name, SessionID session_id, TParam&& arg)
                    : parent_(parent)
                    , rpc_name_(rpc_name)
                    , session_id_(session_id)
                    , arg_(std::forward<TParam>(arg)) {}
            public:
                // lambda
                template<typename TCallbackFunc>
                void operator()(TCallbackFunc&& callback) {
                    functional(parent_->from_lambad(std::forward<TCallbackFunc>(callback)));
                }
                // std::functional(const &)
                template<typename TReturn, typename... TRspParams>
                void functional(const std::function<TReturn(SessionID, msg_status, TRspParams...)>& callback) {
                    write_msg(parent_->m_callback_proxy.insert(callback));
                }
                // std::functional(&&)
                template<typename TReturn, typename... TRspParams>
                void functional(std::function<TReturn(SessionID, msg_status, TRspParams...)>&& callback) {
                    write_msg(parent_->m_callback_proxy.insert(std::move(callback)));
                }
                // &functional
                template<typename TReturn, typename... TRspParams>
                void operator()(TReturn(*callback)(SessionID, msg_status, TRspParams...)) {
                    functional(std::function<TReturn(SessionID, msg_status, TRspParams...)>(callback));
                }
                // &object::functional, object
                template<typename TReturn, typename TObjClass, typename TObject, typename... TRspParams>
                void operator()(TReturn(TObjClass::* callback)(SessionID, msg_status, TRspParams...), TObject* obj) {
                    operator()([=](SessionID session_id, msg_status status, TRspParams... ps)->TReturn { return (obj->*callback)(session_id, status, ps...); });
                }

            private:
                // 无参数
                template <typename TType = TParam>
                typename std::enable_if<std::is_null_pointer<TType>::value, void>::type
                    write_msg(uint32_t req_id) {
                    parent_->callback_send(TIMEOUT, rpc_name_, session_id_, req_id);
                }
                // 有参数
                template <typename TType = TParam>
                typename std::enable_if<!std::is_null_pointer<TType>::value, void>::type
                    write_msg(uint32_t req_id) {
                    parent_->callback_send(TIMEOUT, rpc_name_, session_id_, req_id, std::forward<TParam>(arg_));
                }

            private:
                RpcBase*        parent_;
                std::string     rpc_name_;
                SessionID       session_id_;
                TParam          arg_;
            };

        public:
            /**************   bind && bind_auto  ******************/
            // lambda
            // 函数的执行结果需主动调动rsp_bind
            template<typename TBindFunc>
            inline void bind(const std::string& rpc_name, TBindFunc&& bindfunc) {
                bind_functional(rpc_name, from_lambad(std::forward<TBindFunc>(bindfunc)));
            }
            // std::functional
            // 函数的执行结果需主动调动rsp_bind
            template<typename TReturn, typename... TRspParams>
            inline void bind_functional(const std::string& rpc_name, std::function<TReturn(SessionID, const message_head&, TRspParams...)> bindfunc) {
                m_bind_proxy.insert(rpc_name, bindfunc);
            }
            // &functional
            // 函数的执行结果需主动调动rsp_bind
            template<typename TReturn, typename... TRspParams>
            inline void bind(const std::string& rpc_name, TReturn(*bindfunc)(SessionID, const message_head&, TRspParams...)) {
                bind_functional(rpc_name, std::function<TReturn(SessionID, const message_head&, TRspParams...)>(bindfunc));
            }
            // &object::functional, object
            // 函数的执行结果需主动调动rsp_bind
            template<typename TReturn, typename TObjClass, typename TObject, typename... TRspParams>
            inline void bind(const std::string& rpc_name, TReturn(TObjClass::* bindfunc)(SessionID, const message_head&, TRspParams...), TObject* obj) {
                bind(rpc_name, [=](SessionID session_id, const message_head& head, TRspParams... ps)->TReturn { return (obj->*bindfunc)(session_id, head, ps...); });
            }

            // lambda
            // 函数的执行结果直接返回给请求端
            template<typename TBindFunc>
            inline void bind_auto(const std::string& rpc_name, TBindFunc&& bindfunc) {
                bind_auto_functional(rpc_name, from_lambad(std::forward<TBindFunc>(bindfunc)));
            }
            // std::functional
            // 函数的执行结果直接返回给请求端
            template<typename TReturn, typename... TRspParams>
            inline void bind_auto_functional(const std::string& rpc_name, std::function<TReturn(SessionID, TRspParams...)> bindfunc) {
                m_bind_proxy.insert_auto_rsp(rpc_name, bindfunc);
            }
            // &functional
            // 函数的执行结果直接返回给请求端
            template<typename TReturn, typename... TRspParams>
            inline void bind_auto(const std::string& rpc_name, TReturn(*bindfunc)(SessionID, TRspParams...)) {
                bind_auto_functional(rpc_name, std::function<TReturn(SessionID, TRspParams...)>(bindfunc));
            }
            // &object::functional, object
            // 函数的执行结果直接返回给请求端
            template<typename TReturn, typename TObjClass, typename TObject, typename... TRspParams>
            inline void bind_auto(const std::string& rpc_name, TReturn(TObjClass::* bindfunc)(SessionID, TRspParams...), TObject* obj) {
                bind_auto(rpc_name, [=](SessionID session_id, TRspParams... ps)->TReturn { return (obj->*bindfunc)(session_id, ps...); });
            }

            template<typename... Args>
            bool rsp_bind(const std::string& rpc_name, SessionID session_id, uint32_t req_id, rpc_model model, msg_status status, Args&&... args) {
                auto unit = m_proxy_deal.package_msg(rpc_name, comm_model::rsponse, model, req_id, status, std::forward<Args>(args)...);
                return write_impl(session_id, unit->get_package(), unit->get_length());
            }


        protected:
            /**************   异步调用发送信息  ******************/
            template<typename... Args>
            void callback_send(size_t overtime, const std::string& rpc_name, SessionID session_id, uint32_t req_id, Args&&... args) {
                m_callback_proxy.insert_timer(overtime, session_id, req_id);
                // todo... 后期考虑将该方法移动至其继承类自身实现, 免去TProxyPkgHandle的引入
                auto unit = m_proxy_deal.package_msg(rpc_name, comm_model::request, rpc_model::callback, req_id, msg_status::ok, std::forward<Args>(args)...);
                if (!write_impl(session_id, unit->get_package(), unit->get_length())) {
                    m_callback_proxy.remove(req_id, msg_status::send_error);
                }
            }

            /**************   同步调用发送信息, 存在阻塞  ******************/
            // 无返回参数 同步调用,存在阻塞
            // 返回参数类型: msg_status,  表示最终状态
            template<size_t TIMEOUT, typename TReturn = void, typename ...Args>
            typename std::enable_if<std::is_void<TReturn>::value, msg_status>::type
                sync_send(const std::string& rpc_name, SessionID session_id, Args&&... args) {
                auto [status, item] = sync_send_impl<TIMEOUT>(rpc_name, session_id, std::forward<Args>(args)...);
                if (status == msg_status::ok) {
                    item->get_rsp_params(status);
                }
                return status;
            }
            // 有返回参数 同步调用,存在阻塞
            // 返回参数类型: std::tuple<msg_status, TReturn>,  表示<最终状态, 返回结果>
            template<size_t TIMEOUT, typename TReturn, typename ...Args>
            std::tuple<msg_status, typename std::enable_if<!std::is_void<TReturn>::value, TReturn>::type>
                sync_send(const std::string& rpc_name, SessionID session_id, Args&&... args) {
                auto [status, item] = sync_send_impl<TIMEOUT>(rpc_name, session_id, std::forward<Args>(args)...);
                if (status != msg_status::ok) {
                    return std::forward_as_tuple(status, TReturn());
                }
                auto comm_rslt = item->template get_rsp_params<TReturn>(status);
                return std::forward_as_tuple(status, comm_rslt);
            }

        private:
            template<size_t TIMEOUT, typename ...Args>
            std::tuple<msg_status, ProxyMsgPtr> sync_send_impl(const std::string& rpc_name, SessionID session_id, Args&&... args) {
                auto req_id = m_sync_proxy.get_next_promise();
                // todo... 后期考虑将该方法移动至其继承类自身实现, 免去TProxyPkgHandle的引入
                auto unit = m_proxy_deal.package_msg(rpc_name, comm_model::request, rpc_model::future, req_id, msg_status::ok, std::forward<Args>(args)...);
                if (!write_impl(session_id, unit->get_package(), unit->get_length())) {
                    return std::forward_as_tuple(msg_status::send_error, nullptr);
                }
                return m_sync_proxy.wait_for(req_id, TIMEOUT);
            }

        protected:
            virtual bool write_impl(SessionID session_id, const char* const data, size_t bytes_transferred) = 0;

        private:
            /**************   函数转换  ******************/
            template <typename T> struct function_traits_base {
                typedef T type;
            };
            template <typename TObjClass, typename TReturn, typename... TRspParams>
            struct function_traits_base<TReturn(TObjClass::*)(TRspParams...) const> {
                typedef std::function<TReturn(TRspParams...)> type;
            };
            template <typename TFunction>
            struct lambda_traits : public function_traits_base<decltype(&TFunction::operator())> {};
            template<typename TFunction >
            struct functional_traits : public function_traits_base<TFunction> {};
            template<typename TFunction>
            typename lambda_traits<TFunction>::type from_lambad(TFunction&& func) {
                return func;
            }
            template<typename TFunction>
            typename functional_traits<TFunction>::type from_functional(TFunction&& func) {
                return func;
            }

        protected:
            ProxyPkg<TProxyPkgHandle, TProxyMsgHandle>                      m_proxy_deal;
            SyncProxy<TProxyMsgHandle>                                      m_sync_proxy;
            CallbackProxy<TProxyMsgHandle>                                  m_callback_proxy;
            BindProxy<TProxyPkgHandle, TProxyMsgHandle, DEFAULT_TIMEOUT>    m_bind_proxy;
        };

        template<typename TServer, template<typename TProxyMsgHandle> typename TProxyPkgHandle = DefaultProxyPkgHandle, typename TProxyMsgHandle = DefaultProxyMsgHandle, size_t DEFAULT_TIMEOUT = 1000>
        class RpcService : public RpcBase<TProxyPkgHandle, TProxyMsgHandle, DEFAULT_TIMEOUT> {
            typedef RpcBase<TProxyPkgHandle, TProxyMsgHandle, DEFAULT_TIMEOUT> RpcBaseType;
            typedef typename RpcBaseType::ProxyMsgPtr  ProxyMsgPtr;
            typedef typename RpcBaseType::PromisePtr   PromisePtr;
            typedef typename RpcBaseType::SessionID    SessionID;

        public:
            // max_buffer_size: 最大写缓冲区大小,0表示无限制
            // max_rbuffer_size: 单次读取最大缓冲区大小
            RpcService(int works = 0, size_t max_wbuffer_size = 0, size_t max_rbuffer_size = 2000) : m_ioc_pool(works) {
                m_service = std::make_shared<TServer>(m_ioc_pool, max_wbuffer_size, max_rbuffer_size);
                using std::placeholders::_1;
                using std::placeholders::_2;
                using std::placeholders::_3;
                m_service->register_read_cbk(std::bind(&RpcService::read_cbk, this, _1, _2, _3));
            }
            ~RpcService() {
                m_service.reset();
                m_ioc_pool.stop();
            }

            // 设置监听错误回调
            void register_error_cbk(const NetCallBack::server_error_cbk& cbk) {
                m_service->register_error_cbk(cbk);
            }
            // 设置开启连接回调
            void register_open_cbk(const NetCallBack::open_cbk& cbk) {
                m_service->register_open_cbk(cbk);
            }
            // 设置关闭连接回调
            void register_close_cbk(const NetCallBack::close_cbk& cbk) {
                m_service->register_close_cbk(cbk);
            }

            bool listen(const std::string& ip, unsigned short port, bool reuse_address = true) {
                return m_service->start(ip.c_str(), port, reuse_address);
            }
            void run(const std::string& ip, unsigned short port, bool reuse_address = true) {
                if (m_service->start(ip.c_str(), port, reuse_address))
                    m_ioc_pool.run();
            }

            void close(SessionID session_id) {
                m_service->close(session_id);
            }

        public:
         /**************   tcp回调  ******************/
            void read_cbk(NetCallBack::SessionID session_id, const char* const msg, size_t bytes_transferred) {
                auto [units, deal_len, err] = this->m_proxy_deal.unpackage_msg(msg, bytes_transferred);
                if (err.status_ != msg_status::ok) {
                    //this->m_error_proxy.error(err);
                    m_service->close(session_id);
                    return;
                }

                for (auto& item : units) {
                    if (item->get_comm_model() == comm_model::request) {
                        if (!this->m_bind_proxy.invoke(session_id, item)) {
                            //this->m_error_proxy.invoke(item);
                            //m_service->close(session_id);
                            this->rsp_bind(item->get_rpc_name(), session_id, item->get_req_id(), item->get_rpc_model(), msg_status::no_bind);
                            //return;
                        }
                        continue;
                    }

                    switch (item->get_rpc_model()) {
                    case rpc_model::future:
                        this->m_sync_proxy.invoke(item);
                        break;
                    case rpc_model::callback:
                        this->m_callback_proxy.invoke(item);
                        break;
                    default:
                        //this->m_error_proxy.invoke(item);
                        m_service->close(session_id);
                        return;
                        break;
                    }
                }

                m_service->consume_read_buf(session_id, deal_len);
            }

        public:
         /**************   push  ******************/
            template<size_t TIMEOUT, typename TReturn = void, typename ...Args>
            decltype(auto) push(const std::string& rpc_name, SessionID session_id, Args&&... args) {
                return this->template sync_send<TIMEOUT, TReturn>(rpc_name, session_id, std::forward<Args>(args)...);
            }
            template<typename TReturn = void, typename ...Args>
            decltype(auto) push(const std::string& rpc_name, SessionID session_id, Args&&... args) {
                return this->template sync_send<DEFAULT_TIMEOUT, TReturn>(rpc_name, session_id, std::forward<Args>(args)...);
            }

         /**************   pushback  ******************/
            // 异步回调
            // decltype(auto) 可推导出引用等原类型
            // 返回参数类型: callback_req_op<...>, 通过其发送异步调用
            // 函数必须参数(SessionID, msg_status, ...)
            template<size_t TIMEOUT>
            decltype(auto) push_back(const std::string& rpc_name, SessionID session_id) {
                return typename RpcBaseType::template callback_req_op<TIMEOUT>
                    (this, rpc_name, session_id);
            }
            template<size_t TIMEOUT, typename TParam>
            decltype(auto) push_back(const std::string& rpc_name, SessionID session_id, TParam&& param) {
                return typename RpcBaseType::template callback_req_op<TIMEOUT, TParam>
                    (this, rpc_name, session_id, std::forward<TParam>(param));
            }
            template<size_t TIMEOUT, typename TParam, typename ...Args>
            decltype(auto) push_back(const std::string& rpc_name, SessionID session_id, TParam&& param, Args&&... args) {
                return typename RpcBaseType::template callback_req_op<TIMEOUT, std::tuple<TParam, Args...>>
                    (this, rpc_name, session_id, std::forward_as_tuple(std::forward<TParam>(param), std::forward<Args>(args)...));
            }
            // 异步回调
            // decltype(auto) 可推导出引用等原类型
            // 返回参数类型: callback_req_op<...>, 通过其发送异步调用
            // 函数必须参数(SessionID, msg_status, ...)
            template<typename ...Args>
            decltype(auto) push_back(const std::string& rpc_name, SessionID session_id, Args&&... args) {
                return push_back<DEFAULT_TIMEOUT>(rpc_name, session_id, std::forward<Args>(args)...);
            }

        private:

            bool write_impl(SessionID session_id, const char* const data, size_t bytes_transferred) override {
                if (m_service)
                    return m_service->write(session_id, data, bytes_transferred);
                return false;
            }

        private:
            AsioContextPool                          m_ioc_pool;
            std::shared_ptr<TServer>                 m_service;
        };

        template<typename TSession, template<typename TProxyMsgHandle> typename TProxyPkgHandle = DefaultProxyPkgHandle, typename TProxyMsgHandle = DefaultProxyMsgHandle, size_t DEFAULT_TIMEOUT = 1000>
        class RpcClient : public RpcBase<TProxyPkgHandle, TProxyMsgHandle, DEFAULT_TIMEOUT> {
            typedef RpcBase<TProxyPkgHandle, TProxyMsgHandle, DEFAULT_TIMEOUT> RpcBaseType;
            typedef typename RpcBaseType::ProxyMsgPtr  ProxyMsgPtr;
            typedef typename RpcBaseType::PromisePtr   PromisePtr;
            typedef typename RpcBaseType::SessionID    SessionID;

        public:
            // works: 回调线程池数
            // max_buffer_size: 最大写缓冲区大小,0表示无限制
            // max_rbuffer_size: 单次读取最大缓冲区大小
            RpcClient(size_t max_wbuffer_size = 0, size_t max_rbuffer_size = 2000) :m_ioc_pool(1) {
                m_session = std::make_shared<TSession>(m_ioc_pool.get_io_context(), max_wbuffer_size, max_rbuffer_size);
                using std::placeholders::_1;
                using std::placeholders::_2;
                using std::placeholders::_3;
                //m_session->register_cbk(m_cbk);
                m_session->register_open_cbk(std::bind(&RpcClient::open_cbk, this, _1))
                    .register_close_cbk(std::bind(&RpcClient::close_cbk, this, _1, _2, _3))
                    .register_read_cbk(std::bind(&RpcClient::read_cbk, this, _1, _2, _3));
            }
            ~RpcClient() {
                m_session->register_cbk(NetCallBack());
                shutdown();
                m_session.reset();
                m_ioc_pool.stop();
            }

            void connect(const std::string& ip, unsigned short port, bool auto_reconnect = true) {
                m_b_auto_reconnect = auto_reconnect;
                m_session->connect(ip.c_str(), port);
            }

            // 设置开启连接回调
            void register_open_cbk(const NetCallBack::open_cbk& cbk) {
                m_cbk.open_cbk_ = cbk;
            }
            // 设置关闭连接回调
            void register_close_cbk(const NetCallBack::close_cbk& cbk) {
                m_cbk.close_cbk_ = cbk;
            }

            bool is_open() const {
                return m_session->is_open();
            }

            void shutdown() {
                m_b_auto_reconnect = false;
                m_session->shutdown();
            }

        public:
            /**************   tcp回调  ******************/
            void open_cbk(NetCallBack::SessionID session_id) {
                if (m_cbk.open_cbk_)
                    m_cbk.open_cbk_(session_id);
            }
            void close_cbk(NetCallBack::SessionID session_id, const char* const msg, size_t bytes_transferred) {
                if (m_cbk.close_cbk_)
                    m_cbk.close_cbk_(session_id, msg, bytes_transferred);

                if (m_b_auto_reconnect) {
#ifdef __linux
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    if (m_b_auto_reconnect)
#endif
                        m_session->reconnect();
                }
            }
            void read_cbk(NetCallBack::SessionID session_id, const char* const msg, size_t bytes_transferred) {
                auto [units, deal_len, err] = this->m_proxy_deal.unpackage_msg(msg, bytes_transferred);
                if (err.status_ != msg_status::ok) {
                    //this->m_error_proxy.error(err);
                    m_session->shutdown(boost::asio::error::invalid_argument);
                    return;
                }

                for (auto& item : units) {
                    if (item->get_comm_model() == comm_model::request) {
                        if (!this->m_bind_proxy.invoke(session_id, item)) {
                            //this->m_error_proxy.invoke(item);
                            //m_session->shutdown();
                            this->rsp_bind(item->get_rpc_name(), session_id, item->get_req_id(), item->get_rpc_model(), msg_status::no_bind);
                            //return;
                        }
                        continue;
                    }

                    switch (item->get_rpc_model()) {
                    case rpc_model::future:
                        this->m_sync_proxy.invoke(item);
                        break;
                    case rpc_model::callback:
                        this->m_callback_proxy.invoke(item);
                        break;
                    default:
                        //this->m_error_proxy.invoke(item);
                        m_session->shutdown(boost::asio::error::invalid_argument);
                        return;
                        break;
                    }
                }

                m_session->consume_read_buf(deal_len);
            }

        public:
            template <typename T, typename = void>
            struct HAS_EXPLICIT_TIME : std::false_type {};
            template <typename T>
            struct HAS_EXPLICIT_TIME<T, std::enable_if_t<
                                            std::is_integral_v<std::remove_reference_t<T>>
                                        >
                                    > : std::true_type {};

            /**************   call  ******************/
            template<size_t TIMEOUT, typename TReturn = void, typename ...Args>
            decltype(auto) call(const std::string& rpc_name, Args&&... args) {
                return this->template sync_send<TIMEOUT, TReturn>(rpc_name, get_session_id(), std::forward<Args>(args)...);
            }
            template<typename TReturn = void, typename ...Args>
            decltype(auto) call(const std::string& rpc_name, Args&&... args) {
                return this->template sync_send<DEFAULT_TIMEOUT, TReturn>(rpc_name, get_session_id(), std::forward<Args>(args)...);
            }

            /**************   callback  ******************/
            // 异步回调
            // decltype(auto) 可推导出引用等原类型
            // 返回参数类型: callback_req_op<...>, 通过其发送异步调用
            // 函数必须参数(SessionID, msg_status, ...)
            template<size_t TIMEOUT>
            decltype(auto) call_back_timer(const std::string& rpc_name) {
                return typename RpcBaseType::template callback_req_op<TIMEOUT>
                    (this, rpc_name, get_session_id());
            }
            template<size_t TIMEOUT, typename TParam>
            decltype(auto) call_back_timer(const std::string& rpc_name, TParam&& param) {
                return typename RpcBaseType::template callback_req_op<TIMEOUT, TParam>
                    (this, rpc_name, get_session_id(), std::forward<TParam>(param));
            }
            template<size_t TIMEOUT, typename TParam, typename ...Args>
            decltype(auto) call_back_timer(const std::string& rpc_name, TParam&& param, Args&&... args) {
                return typename RpcBaseType::template callback_req_op<TIMEOUT, std::tuple<TParam, Args...>>
                    (this, rpc_name, get_session_id(), std::forward_as_tuple(std::forward<TParam>(param), std::forward<Args>(args)...));
            }
            // 异步回调
            // decltype(auto) 可推导出引用等原类型
            // 返回参数类型: callback_req_op<...>, 通过其发送异步调用
            // 函数必须参数(SessionID, msg_status, ...)
            template<typename ...Args>
            decltype(auto) call_back(const std::string& rpc_name, Args&&... args) {
                return call_back_timer<DEFAULT_TIMEOUT>(rpc_name, std::forward<Args>(args)...);
            }

        private:
            SessionID get_session_id() const {
                if (m_session)
                    return m_session->get_session_id();
                return BoostNet::NetCallBack::InvalidSessionID;
            }
            bool write_impl(SessionID session_id, const char* const data, size_t bytes_transferred) override {
                if (m_session)
                    return m_session->write(data, bytes_transferred);
                return false;
            }

        private:
            AsioContextPool                          m_ioc_pool;
            std::shared_ptr<TSession>                m_session;
            volatile bool                            m_b_auto_reconnect = true;
            NetCallBack                              m_cbk;
        };
    }
}