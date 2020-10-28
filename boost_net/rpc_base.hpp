/************************************************************************
* Copyright (C), 2020, AChar
* Purpose:  ���Ż����µ�rpcͨѶ
* Date:     2020-10-27 15:18:11
* ʹ�÷���:

    void foo_1() {}
    struct st_1 {
        void foo_1(int rsp_info, int rsp_info2) {}
    };
    void set(int a) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    int add(int a, int b) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return a + b;
    }
    void add_void(int a, int b) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return;
    }
    std::tuple<int, int> get_tuple(int a, int b) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return std::forward_as_tuple(a, b);
    }


 �����:
    class TestService {
        typedef BTool::BoostRpc::RpcServer<30000>   rpc_server;
        rpc_server                                  send_package;
    public:
        TestService() {
            send_package.set_connect_cbk([&](rpc_server::SessionID session_id) { std::cout << "connect" << std::endl; });
            send_package.set_close_cbk([&](rpc_server::SessionID session_id) { std::cout << "close" << std::endl; });
        }
        ~TestService() {}
    public:
        void init(){
            send_package.listen("127.0.0.1", 61239);
            BTool::ParallelTaskPool pool;
            pool.start();
            // ͬ���󶨲�ֱ�ӷ���,����������
            send_package.bind("bind lambda", [this, &pool](rpc_server::SessionID session, const rpc_server::message_head& head, const std::string& rpc_name, int rslt, int rsp_info) {
                pool.add_task([=] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    bool rsltbb = send_package.rsp_bind(session, head, rpc_name, rpc_server::msg_status::ok, rslt + rsp_info);
                });
            });

            // ͬ����ֱ�ӷ���
            send_package.bind_auto("foo_1", &foo_1);
            send_package.bind_auto("st_1 foo_1", &st_1::foo_1, new st_1);
            send_package.bind_auto("set", &set);
            send_package.bind_auto("add", &add);
            send_package.bind_auto("add_void", &add_void);
            send_package.bind_auto("get_tuple", &get_tuple);
            send_package.bind_auto("lambda", [] {});
            std::function<void(rpc_server::SessionID, const rpc_server::message_head&, const std::string&, std::future_status, int)> func = [&](rpc_server::SessionID, const rpc_server::message_head&, const std::string&, std::future_status rslt, int rsp_info) {};
            send_package.bind_auto_functional("functional", func);

            // ��������, ͬ���ȴ�
            auto [rsp_status, rsp_rslt] = send_package.push<int>(session_id, "push", req_params...);
            auto [rsp_status3, rsp_rslt3] = send_package.push<200, int>(session_id, "push", req_params...);
            auto rsp_status2 = send_package.push(session_id, "push2");
            // ��������, �첽�ȴ�
            send_package.push_back(session_id, "push_back", req_params...)([&](rpc_server::SessionID, rpc_server::msg_status, rsp_args...) {});
            send_package.push_back<200>(session_id, "push_back", req_params...)([&](rpc_server::SessionID, rpc_server::msg_status, rsp_args...) {});
        }
    };

�ͻ���:
    class TestClient {
        typedef BTool::BoostRpc::RpcClient<2000>    rpc_client;
        rpc_client                                  send_package;
    public:
        TestClient() {
            send_package.set_connect_cbk([&](rpc_client::SessionID session_id) { std::cout << "connect" << std::endl; });
            send_package.set_close_cbk([&](rpc_client::SessionID session_id) { std::cout << "close" << std::endl; });
        }
        ~TestClient() {}
    public:
        void init(){
            send_package.connect("127.0.0.1", 61239);

            BTool::ParallelTaskPool pool;
            pool.start();
            // ͬ���󶨲�ֱ�ӷ���,����������
            send_package.bind("bind lambda", [this, &pool](rpc_client::SessionID session, const rpc_client::message_head& head, const std::string& rpc_name, int rslt, int rsp_info) {
                pool.add_task([=] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    bool rsltbb = send_package.rsp_bind(session, head, rpc_name, rpc_client::msg_status::ok, rslt + rsp_info);
                });
            });

            // ͬ����ֱ�ӷ���
            send_package.bind_auto("foo_1", &foo_1);
            send_package.bind_auto("st_1 foo_1", &st_1::foo_1, new st_1);
            send_package.bind_auto("set", &set);
            send_package.bind_auto("add", &add);
            send_package.bind_auto("add_void", &add_void);
            send_package.bind_auto("get_tuple", &get_tuple);
            send_package.bind_auto("lambda", [] {});
            std::function<void(rpc_client::SessionID, const rpc_client::message_head&, const std::string&, std::future_status, int)> func = [&](rpc_client::SessionID, const rpc_client::message_head&, const std::string&, std::future_status rslt, int rsp_info) {};
            send_package.bind_auto_functional("functional", func);

            std::this_thread::sleep_for(std::chrono::seconds(2));

            // ͬ���ȴ�����
            clock_t start, ends;
            start = clock();
            auto [status01, rsp_rslt01] = send_package.call<int>("add", 10, 20);
            ends = clock();
            std::cout << "call add int ��ʱ:" << ends - start << "; ͨѶ���:" << status01 << ";�ص�ִ�н��:" << rsp_rslt01 << std::endl;
            auto status02 = send_package.call("add_void", 22, 30);
            start = clock();
            std::cout << "call add void ��ʱ:" << start - ends << "; ͨѶ���:" << status02 << std::endl;
            auto status03 = send_package.call<200>("add_void", 33, 40);
            ends = clock();
            std::cout << "call add void ��ʱ:" << ends - start << "; ͨѶ���:" << status03 << std::endl;

            // �첽�ȴ�����
            send_package.call_back("call_back", req_params...)([&](rpc_client::SessionID, rpc_client::msg_status, rsp_args...) {});
            send_package.call_back<200>("call_back", req_params...)([&](rpc_client::SessionID, rpc_client::msg_status, rsp_args...) {});
        }
    };

/************************************************************************/

#pragma once
#include <memory>
#include <deque>
#include <future>
#include <string>

#include "utility/timer_manager.hpp"
#include "utility/boost_net/tcp_server.hpp"

namespace BTool::BoostRpc {

#pragma region ������ͼ���������
    class pkg_helper_base {
    public:
        enum class msg_status : uint8_t {
            ok,             // �����ִ����ȷ
            fail,           // �����ִ��ʧ��
            timeout,        // �ȴ�Ӧ��ʱ
            unpack_error,   // ��������ʧ��
            send_error      // ����ʧ��
        };

        // ��װ��Ϣģʽ
        enum class PkgModel : uint8_t {
            Msg,            // ����msg�ڴ���ʽ��װ
            Protobuf,       // ����protobuf��ʽ��װ
        };

    protected:
        pkg_helper_base() = default;
        virtual ~pkg_helper_base() {}
    };

    template<pkg_helper_base::PkgModel>
    class pkg_helper : public pkg_helper_base {};

#pragma region �ڲ���������ʽ�ػ�ʵ��
    template<>
    class pkg_helper<pkg_helper_base::PkgModel::Msg> : public pkg_helper_base {
    public:
        pkg_helper(std::string_view data) { 
            data_.load(data); 
        }
        // ��ȡ����
        template<typename Type = void>
        typename std::enable_if<std::is_void<Type>::value, Type>::type
            get_req_params(msg_status& status) {
            status = msg_status::ok;
        }
        template<typename Type>
        typename std::enable_if<!std::is_void<Type>::value, Type>::type
            get_req_params(msg_status& status) {
            if (data_.size() < sizeof(Type)) {
                status = msg_status::unpack_error;
                return Type();
            }
            status = msg_status::ok;
            return data_.read_args<Type>();
        }
        template<typename Type1, typename Type2, typename... TParams>
        decltype(auto) get_req_params(msg_status& status) {
            status = msg_status::ok;
            return data_.read_args<Type1, Type2, typename std::decay<TParams>::type...>();
        }

        // ��ȡӦ��
        template<typename Type>
        typename std::enable_if<std::is_void<Type>::value, Type>::type
            get_rsp_params(msg_status& status) {
            data_.read(&status);
        }
        template<typename Type>
        typename std::enable_if<!std::is_void<Type>::value, Type>::type
            get_rsp_params(msg_status& status) {
            if (data_.size() < sizeof(msg_status)) {
                status = msg_status::unpack_error;
                return Type();
            }
            data_.read(&status);

            if (status > msg_status::send_error) {
                status = msg_status::unpack_error;
                return Type();
            }

            if (status == msg_status::fail || data_.size() < sizeof(msg_status) + sizeof(Type)) {
                return Type();
            }

            return data_.read_args<Type>();
        }
    private:
        BTool::MemoryStream data_;
    };
#pragma endregion

#pragma region protobuf��װ��ͬ������������
    //template<>
    //class pkg_helper<pkg_helper_base::PkgModel::Protobuf> : public pkg_helper_base {
    //public:
    //    pkg_helper(std::string_view data) : data_(data) {}
    //    void get_req_params(msg_status& status) { assert(0); }
    //    template<typename Type>
    //    typename std::enable_if<!std::is_void<Type>::value, Type>::type
    //        get_req_params(msg_status& status) {
    //        assert(0);
    //        return Type();
    //    }
    //    template<typename ...TParams>
    //    auto get_req_params(msg_status& status) {
    //        assert(0);
    //        return std::tuple<typename std::decay<TParams>::type...>{};
    //    }

    //    void get_rsp_params(msg_status& status) { assert(0); }
    //    template<typename Type>
    //    Type get_rsp_params(msg_status& status) {
    //        assert(0);
    //        return Type();
    //    }
    //private:
    //    std::string_view data_;

    //};
    //typedef std::shared_ptr<pkg_helper<PkgModel::Protobuf>> pb_pkg_helper_ptr;
#pragma endregion

#pragma endregion

    template<size_t DEFAULT_TIMEOUT, pkg_helper_base::PkgModel pkg_model>
    class RpcBase : public BTool::BoostNet::NetCallBack {
    protected:

        typedef pkg_helper_base::PkgModel           PkgModel;
        typedef pkg_helper_base::msg_status         msg_status;
        typedef typename pkg_helper<pkg_model>      pkg_helper_handler;
        typedef std::shared_ptr<pkg_helper_handler> pkg_helper_ptr;

        RpcBase(int async_thread_num) : m_async_timer(200, async_thread_num){}
        virtual ~RpcBase() {
            m_async_timer.stop();
            m_future_map.clear();
            m_callback_map.clear();
            m_bind_map.clear();
        }

    public:
#pragma region ͨѶ����
        // ����Զ�̷���ģʽ
        enum class RpcModel : uint8_t {
            Future,         // �����ͬ���ȴ�������, Ӧ���ͬ��ִ�в��Զ����غ�������ֵ
            Callback,       // ������첽����������, Ӧ���ͬ��ִ�в��Զ����غ�������ֵ
            Bind            // bind_auto:Ӧ���ͬ��ִ�ж���, �Զ����غ��� / bind:Ӧ����첽ִ�ж���, ��������ʽ����
        };
        // ����Ӧ��ģʽ
        enum class CommModel : uint8_t {
            Request,
            Rsponse,
        };
#pragma pack (1)
        struct message_head {
            CommModel       comm_model_;
            PkgModel        pkg_model_;
            RpcModel        rpc_model_;
            uint32_t        req_id_;
            uint8_t         title_size_;   // ���255
            uint32_t        content_size_; // ���4,294,967,295
        };
#pragma pack ()

#pragma endregion

    public:
        // ��������״̬�ص�
        template<typename TFunction = std::function<void(SessionID)>>
        void set_connect_cbk(TFunction&& func) {
            m_connect_cbk = std::forward<TFunction>(func);
        }
        // ���ùرջص�
        template<typename TFunction = std::function<void(SessionID)>>
        void set_close_cbk(TFunction&& func) {
            m_close_cbk = std::forward<TFunction>(func);
        }
        // ��������bind����
        template<typename... Args>
        bool rsp_bind(SessionID session_id, const message_head& head, const std::string& rpc_name, msg_status status, Args&&... args) {
            return write_msg(session_id, CommModel::Rsponse, head.rpc_model_, head.req_id_, rpc_name, status, std::forward<Args>(args)...);
        }

#pragma region call_back �����������
        // �ص�����������
        template<size_t TIMEOUT, typename TTuple>
        struct call_back_req_op {
            call_back_req_op(RpcBase* parent, SessionID session_id, const std::string& rpc_name, TTuple&& tp)
                : parent_(parent)
                , session_id_(session_id)
                , rpc_name_(rpc_name)
                , tp_(std::forward<TTuple>(tp)) {}
        public:
            // lambda
            template<typename TCallbackFunc>
            void operator()(TCallbackFunc&& callback) {
                functional(parent_->from_lambad(std::forward<TCallbackFunc>(callback)));
            }
            // std::functional
            template<typename TReturn, typename... TParams>
            void functional(const std::function<TReturn(SessionID, msg_status, TParams...)>& callback) {
                write_msg(parent_->insert_callback(callback));
            }
            template<typename TReturn, typename... TParams>
            void functional(std::function<TReturn(SessionID, msg_status, TParams...)>&& callback) {
                write_msg(parent_->insert_callback(std::move(callback)));
            }
            // &functional
            template<typename TReturn, typename... TParams>
            void operator()(TReturn(*callback)(SessionID, msg_status, TParams...)) {
                functional(std::function<TReturn(SessionID, msg_status, TParams...)>(callback));
            }
            // &object::functional, object
            template<typename TReturn, typename TObjClass, typename TObject, typename... TParams>
            void operator()(TReturn(TObjClass::* callback)(SessionID, msg_status, TParams...), TObject* obj) {
                operator()([=](SessionID session_id, msg_status status, TParams... ps)->TReturn { return (obj->*callback)(session_id, status, ps...); });
            }

        private:
            void write_msg(uint32_t req_id) {
                if (!parent_->write_msg(session_id_, CommModel::Request, RpcModel::Callback, req_id, rpc_name_, std::move(tp_))) {
                    parent_->m_async_timer.insert_now_once([parent = parent_, req_id, session_id = session_id_](BTool::TimerManager::TimerId, const BTool::TimerManager::system_time_point&) {
                        parent->remove_callback(session_id, req_id, msg_status::send_error);
                    });
                }
                else {
                    auto timer_id = parent_->m_async_timer.insert_duration_once(TIMEOUT, [parent = parent_, req_id, session_id = session_id_](BTool::TimerManager::TimerId, const BTool::TimerManager::system_time_point&) {
                       parent->remove_callback(session_id, req_id, msg_status::timeout);
                    });
                    parent_->emplace_timer_id(req_id, timer_id);
                }
            }

        private:
            RpcBase*        parent_;
            std::string     rpc_name_;
            SessionID       session_id_;
            TTuple          tp_;
        };
#pragma endregion

#pragma region bind_auto ����, ������ִ�н��ֱ�ӷ��ظ������
        // lambda
        // ������ִ�н��ֱ�ӷ��ظ������
        template<typename TBindFunc>
        inline void bind_auto(const std::string& rpc_name, TBindFunc&& bindfunc) {
            bind_auto_functional(rpc_name, from_lambad(std::forward<TBindFunc>(bindfunc)));
        }
        // std::functional
        // ������ִ�н��ֱ�ӷ��ظ������
        template<typename TReturn, typename... TParams>
        inline void bind_auto_functional(const std::string& rpc_name, std::function<TReturn(TParams...)> bindfunc) {
            m_bind_map[rpc_name] = std::bind(&RpcBase::bindautoproxy<TReturn, TParams...>, this, bindfunc, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
        }
        // &functional
        // ������ִ�н��ֱ�ӷ��ظ������
        template<typename TReturn, typename... TParams>
        inline void bind_auto(const std::string& rpc_name, TReturn(*bindfunc)(TParams...)) {
            bind_auto_functional(rpc_name, std::function<TReturn(TParams...)>(bindfunc));
        }
        // &object::functional, object
        // ������ִ�н��ֱ�ӷ��ظ������
        template<typename TReturn, typename TObjClass, typename TObject, typename... TParams>
        inline void bind_auto(const std::string& rpc_name, TReturn(TObjClass::* bindfunc)(TParams...), TObject* obj) {
            bind_auto(rpc_name, [=](TParams... ps)->TReturn { return (obj->*bindfunc)(ps...); });
        }
#pragma endregion

#pragma region bind ����, �����������ʽ���ؽ��
        // lambda
        // �����������ʽ���ؽ��
        // �����������(SessionID, const message_head&, const std::string& /*rpc_name*/, ...)
        template<typename TBindFunc>
        inline void bind(const std::string& rpc_name, TBindFunc&& bindfunc) {
            bind_functional(rpc_name, from_lambad(std::forward<TBindFunc>(bindfunc)));
        }
        // std::functional
        // �����������ʽ���ؽ��
        // �����������(SessionID, const message_head&, const std::string& /*rpc_name*/, ...)
        template<typename TReturn, typename... TParams>
        inline void bind_functional(const std::string& rpc_name, std::function<TReturn(SessionID, const message_head&, const std::string&, TParams...)> bindfunc) {
            m_bind_map[rpc_name] = std::bind(&RpcBase::bindproxy<TReturn, TParams...>, this, bindfunc, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
        }
        // &functional
        // �����������ʽ���ؽ��
        // �����������(SessionID, const message_head&, const std::string& /*rpc_name*/, ...)
        template<typename TReturn, typename... TParams>
        inline void bind(const std::string& rpc_name, TReturn(*bindfunc)(SessionID, const message_head&, const std::string&, TParams...)) {
            bind_functional(rpc_name, std::function<TReturn(SessionID, const message_head&, const std::string&, TParams...)>(bindfunc));
        }
        // &object::functional, object
        // �����������ʽ���ؽ��
        // �����������(SessionID, const message_head&, const std::string& /*rpc_name*/, ...)
        template<typename TReturn, typename TObjClass, typename TObject, typename... TParams>
        inline void bind(const std::string& rpc_name, TReturn(TObjClass::* bindfunc)(SessionID, const message_head&, const std::string&, TParams...), TObject* obj) {
            bind(rpc_name, [=](SessionID session_id, const message_head& head, const std::string& rpc_name, TParams... ps)->TReturn { return (obj->*bindfunc)(session_id, head, rpc_name, ps...); });
        }
#pragma endregion

    protected:
#pragma region tcp�ص�
        // �������ӻص�
        void on_open_cbk(SessionID session_id) override {
            if (m_connect_cbk)
                m_connect_cbk(session_id);
        }
        // �ر����ӻص�
        void on_close_cbk(SessionID session_id) override {
            if (m_close_cbk)
                m_close_cbk(session_id);
        }
#pragma endregion

#pragma region ����ת��
        template <typename T> struct function_traits_base {
            typedef T type;
        };
        template <typename TObjClass, typename TReturn, typename... TParams>
        struct function_traits_base<TReturn(TObjClass::*)(TParams...) const> {
            typedef std::function<TReturn(TParams...)> type;
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
#pragma endregion

    protected:
#pragma region ͬ������, ��������
        // �޷��ز��� ͬ������,��������
        // ���ز�������: msg_status,  ��ʾ����״̬
        template<size_t TIMEOUT, typename TReturn = void, typename ...Args>
        typename std::enable_if<std::is_void<TReturn>::value, msg_status>::type
            sync_send(SessionID session_id, const std::string& rpc_name, Args&&... args) {
            auto [status, id, future] = this->sync_msg_req<TIMEOUT, RpcModel::Future>(session_id, rpc_name, std::forward<Args>(args)...);
            if (status == msg_status::ok)
                future->get_rsp_params<TReturn>(status);
            return status;
        }
        // �з��ز��� ͬ������,��������
        // ���ز�������: std::tuple<msg_status, TReturn>,  ��ʾ<����״̬, ���ؽ��>
        template<size_t TIMEOUT, typename TReturn, typename ...Args>
        std::tuple<msg_status, typename std::enable_if<!std::is_void<TReturn>::value, TReturn>::type>
            sync_send(SessionID session_id, const std::string& rpc_name, Args&&... args) {
            auto [status, id, future] = this->sync_msg_req<TIMEOUT, RpcModel::Future>(session_id, rpc_name, std::forward<Args>(args)...);
            if (status == msg_status::ok) {
                auto comm_rslt = future->get_rsp_params<TReturn>(status);
                return std::forward_as_tuple(status, comm_rslt);
            }
            return std::forward_as_tuple(status, TReturn());
        }
        // �첽����,����future���ڵȴ�
        // ������ͬ������������
        template<size_t TIMEOUT, RpcModel model, typename... Args>
        std::tuple<msg_status, uint32_t, pkg_helper_ptr> sync_msg_req(SessionID session_id, const std::string& rpc_name, Args&&... args) {
            auto p = std::make_shared<std::promise<pkg_helper_ptr>>();
            auto id = insert_future(p);
            bool write_rslt = write_msg(session_id, CommModel::Request, model, id, rpc_name, std::forward<Args>(args)...);
            auto future = std::move(p->get_future());
            msg_status status = msg_status::ok;
            if (write_rslt) {
                auto future_status = future.wait_for(std::chrono::milliseconds(TIMEOUT));
                if (future_status != std::future_status::ready)
                    status = msg_status::timeout;
            }
            else {
                status = msg_status::send_error;
            }

            if (status != msg_status::ok) {
                remove_future(id);
                return std::forward_as_tuple(status, id, nullptr);
            }
            return std::forward_as_tuple(status, id, future.get());
        }
        // ����ͬ����������
        inline uint32_t insert_future(const std::shared_ptr<std::promise<pkg_helper_ptr>>& p) {
            std::unique_lock<std::mutex> lock(m_func_map_mtx);
            m_future_map.emplace(++m_req_id, p);
            return m_req_id;
        }
        // ɾ��ͬ����������
        inline void remove_future(uint32_t id) {
            std::unique_lock<std::mutex> lock(m_func_map_mtx);
            m_future_map.erase(id);
        }
#pragma endregion

#pragma region �첽�ص�, ��������Ч
        template<typename TReturn, typename... TParams>
        inline uint32_t insert_callback(const std::function<TReturn(SessionID, msg_status, TParams...)>& callback) {
            std::unique_lock<std::mutex> lock(m_func_map_mtx);
            m_callback_map.emplace(++m_req_id, std::bind(&RpcBase::callbackproxy<TReturn, TParams...>, this, callback, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            return m_req_id;
        }
        template<typename TReturn, typename... TParams>
        inline uint32_t insert_callback(std::function<TReturn(SessionID, msg_status, TParams...)>&& callback) {
            std::unique_lock<std::mutex> lock(m_func_map_mtx);
            m_callback_map.emplace(++m_req_id, std::bind(&RpcBase::callbackproxy<TReturn, TParams...>, this, std::move(callback), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            return m_req_id;
        }
        // ɾ���첽��������
        // ע��: �첽����ɾ������Ϊʧ��״̬,������Ҫ��ԭ·����������
        inline void remove_callback(SessionID session_id, uint32_t req_id, msg_status status) {
            std::unique_lock<std::mutex> lock(m_func_map_mtx);
            m_async_overtimes.erase(req_id);

            auto iter = m_callback_map.find(req_id);
            if (iter == m_callback_map.end())
                return;
            iter->second(session_id, status, {});
            m_callback_map.erase(iter);
        }
        template<typename TReturn, typename... TParams>
        void callbackproxy(std::function<TReturn(SessionID, msg_status, TParams...)>& func, SessionID session_id, msg_status status, std::string_view data) {
            using args_type = std::tuple<typename std::decay<TParams>::type...>;
            if constexpr (sizeof...(TParams) == 0) {
                // ������ʧ��
                if (status != msg_status::ok) {
                    std::apply(func, std::forward_as_tuple(session_id, status));
                    return;
                }
                // ����ִ��ʧ��
                BTool::MemoryStream req(data);
                req.read(&status);
                if (status != msg_status::ok) {
                    std::apply(func, std::forward_as_tuple(session_id, status));
                    return;
                }
                // ���ط�����
                std::apply(func, std::forward_as_tuple(session_id, status));
            }
            else {
                // ������ʧ��
                if (status != msg_status::ok) {
                    std::apply(func, BTool::MemoryStream::tuple_merge(std::forward_as_tuple(session_id, status), args_type{}));
                    return;
                }
                // ����ִ��ʧ��
                BTool::MemoryStream req(data);
                req.read(&status);
                if (status != msg_status::ok) {
                    std::apply(func, BTool::MemoryStream::tuple_merge(std::forward_as_tuple(session_id, status), args_type{}));
                    return;
                }
                // ���ط�����
                auto comm_rslt = req.read_args<typename std::decay<TParams>::type...>(status);
                std::apply(func, BTool::MemoryStream::tuple_merge(std::forward_as_tuple(session_id, status), std::move(comm_rslt)));
            }
        }
        // ����callback��ʱ��ʱ����
        void emplace_timer_id(uint32_t req_id, TimerManager::TimerId timer_id) {
            std::unique_lock<std::mutex> lock(m_func_map_mtx);
            m_async_overtimes.emplace(req_id, timer_id);
        }

#pragma endregion

#pragma region ��, �ظ��󶨻Ḳ��
        // �󶨴���
        template<typename TReturn, typename... TParams>
        typename std::enable_if<std::is_void<TReturn>::value, void>::type
            bindautoproxy(std::function<TReturn(TParams...)> bindfunc, std::string_view data, const message_head& head, SessionID session_id, const std::string& rpc_name) {
            msg_status status = msg_status::ok;
            if constexpr (sizeof...(TParams) == 0) {
                bindsync_invoke(bindfunc);
                write_msg(session_id, CommModel::Rsponse, head.rpc_model_, head.req_id_, rpc_name, status);
            }
            else {
                bindsync_invoke(bindfunc, pkg_helper_handler(data).get_req_params<typename std::decay<TParams>::type...>(status));
                write_msg(session_id, CommModel::Rsponse, head.rpc_model_, head.req_id_, rpc_name, status);
            }
        }
        template<typename TReturn, typename... TParams>
        typename std::enable_if<!std::is_void<TReturn>::value, void>::type
            bindautoproxy(std::function<TReturn(TParams...)> bindfunc, std::string_view data, const message_head& head, SessionID session_id, const std::string& rpc_name) {
            msg_status status = msg_status::ok;
            if constexpr (sizeof...(TParams) == 0) {
                auto function_rslt = bindsync_invoke(bindfunc);
                write_msg(session_id, CommModel::Rsponse, head.rpc_model_, head.req_id_, rpc_name, status, std::move(function_rslt));
            }
            else {
                auto function_rslt = bindsync_invoke(bindfunc, pkg_helper_handler(data).get_req_params<typename std::decay<TParams>::type...>(status));
                write_msg(session_id, CommModel::Rsponse, head.rpc_model_, head.req_id_, rpc_name, status, std::move(function_rslt));
            }
        }
        // ����Ϊvoid
        template<typename TReturn, typename... TParams>
        typename std::enable_if<std::is_void<TReturn>::value, TReturn>::type
            bindsync_invoke(std::function<TReturn(TParams...)> bindfunc) {
            bindfunc();
        }
        template<typename ArgsTuple, typename TReturn, typename... TParams>
        typename std::enable_if<std::is_void<TReturn>::value, TReturn>::type
            bindsync_invoke(std::function<TReturn(TParams...)> bindfunc, ArgsTuple&& args) {
            std::apply(bindfunc, BTool::MemoryStream::tuple_merge(std::forward<ArgsTuple>(args)));
        }
        // ���ز�Ϊvoid
        template<typename TReturn, typename... TParams>
        typename std::enable_if<!std::is_void<TReturn>::value, TReturn>::type
            bindsync_invoke(std::function<TReturn(TParams...)> bindfunc) {
            return bindfunc();
        }
        template<typename ArgsTuple, typename TReturn, typename... TParams>
        typename std::enable_if<!std::is_void<TReturn>::value, TReturn>::type
            bindsync_invoke(std::function<TReturn(TParams...)> bindfunc, ArgsTuple&& args) {
            return std::apply(bindfunc, BTool::MemoryStream::tuple_merge(std::forward<ArgsTuple>(args)));
        }

        // �󶨴���
        template<typename TReturn, typename... TParams>
        void bindproxy(std::function<TReturn(SessionID, const message_head&, const std::string&, TParams...)> bindfunc, std::string_view data, const message_head& head, SessionID session_id, const std::string& rpc_name) {
            msg_status status = msg_status::ok;
            if constexpr (sizeof...(TParams) == 0) {
                bind_invoke(session_id, head, rpc_name, bindfunc);
            }
            else {
                bind_invoke(session_id, head, rpc_name, bindfunc, pkg_helper_handler(data).get_req_params<typename std::decay<TParams>::type...>(status));
            }
        }
        template<typename TReturn, typename... TParams>
        void bind_invoke(SessionID session_id, const message_head& head, const std::string& rpc_name, std::function<TReturn(SessionID, const message_head&, const std::string&, TParams...)> bindfunc) {
            std::apply(bindfunc, std::forward_as_tuple(session_id, head, rpc_name));
        }
        template<typename ArgsTuple, typename TReturn, typename... TParams>
        void bind_invoke(SessionID session_id, const message_head& head, const std::string& rpc_name, std::function<TReturn(SessionID, const message_head&, const std::string&, TParams...)> bindfunc, ArgsTuple&& args) {
            std::apply(bindfunc, BTool::MemoryStream::tuple_merge(std::forward_as_tuple(session_id, head, rpc_name), std::forward<ArgsTuple>(args)));
        }
#pragma endregion

#pragma region ��Ϣ��д����
        // ����msg��װ����Ϣ
        template<typename... Args>
        bool write_msg(SessionID session_id, CommModel comm_model, RpcModel rpc_model, uint32_t id, const std::string& rpc_name, Args&&... args) {
            BTool::MemoryStream content_buffer;
            content_buffer.append_args(std::forward<Args>(args)...);

            uint8_t title_size = (uint8_t)rpc_name.length();
            message_head head{ comm_model, pkg_model, rpc_model, id, title_size, (uint32_t)content_buffer.size() };

            BTool::MemoryStream write_buffer(sizeof(message_head) + title_size + content_buffer.size());

            // head
            write_buffer.append(std::move(head));

            // rpc_name
            write_buffer.append(rpc_name.c_str(), title_size);

            // content
            write_buffer.append(content_buffer.data(), content_buffer.size());

            return write_impl(session_id, write_buffer.data(), write_buffer.size());
        }

        template<typename... Args>
        bool write_msg(SessionID session_id, CommModel comm_model, RpcModel rpc_model, uint32_t id, const std::string& rpc_name, std::tuple<Args...>&& tp) {
            return write_tp_msg_impl(session_id, comm_model, rpc_model, id, rpc_name, typename std::make_index_sequence<sizeof...(Args)>{}, std::move(tp));
        }
        template<size_t... Indexes, typename... Args>
        bool write_tp_msg_impl(SessionID session_id, CommModel comm_model, RpcModel rpc_model, uint32_t id, const std::string& rpc_name, const std::index_sequence<Indexes...>&, std::tuple<Args...>&& tp) {
            return write_msg(session_id, comm_model, rpc_model, id, rpc_name, std::move(std::get<Indexes>(tp))...);
        }
        // �����ȡ��Ϣ
        bool deal_read_msg(SessionID session_id, const message_head& head, std::string_view rpc_name, std::string_view data) {
            switch (head.comm_model_) {
            case CommModel::Request: // ���������Ϊ��
                return deal_rpc_req(session_id, head, rpc_name, data);
                break;
            case CommModel::Rsponse:
                return deal_rpc_rsp(session_id, head, rpc_name, data);
                break;
            default:
                break;
            }
            return false;
        }
        // ����rpc_req
        bool deal_rpc_req(SessionID session_id, const message_head& head, std::string_view rpc_name, std::string_view data) {
            std::string rpc_name_str{ rpc_name.data(), rpc_name.size() };
            std::unique_lock<std::mutex> lock(m_func_map_mtx);
            auto iter = m_bind_map.find(rpc_name_str);
            if (iter == m_bind_map.end()) {
                return write_msg(session_id, CommModel::Rsponse, head.rpc_model_, head.req_id_, rpc_name_str, msg_status::fail);
            }
            iter->second(data, head, session_id, rpc_name_str);
            return true;
        }
        // ����rpc_rsp
        bool deal_rpc_rsp(SessionID session_id, const message_head& head, std::string_view rpc_name, std::string_view data) {
            switch (head.rpc_model_) {
            case RpcModel::Future:
                return deal_rpc_rsp<RpcModel::Future>(session_id, head, rpc_name, data);
                break;
            case RpcModel::Callback:
                return deal_rpc_rsp<RpcModel::Callback>(session_id, head, rpc_name, data);
                break;
            case RpcModel::Bind:
                return deal_rpc_rsp<RpcModel::Bind>(session_id, head, rpc_name, data);
                break;
            default:
                break;
            }
            return false;
        }
        template <RpcModel rpc_model>
        typename std::enable_if<rpc_model == RpcModel::Future, bool>::type
            deal_rpc_rsp(SessionID session_id, const message_head& head, std::string_view rpc_name, std::string_view data) {
            if(pkg_model != head.pkg_model_)
                return false;

            std::unique_lock<std::mutex> lock(m_func_map_mtx);
            auto iter = m_future_map.find(head.req_id_);
            if (iter == m_future_map.end())
                return true;

            iter->second->set_value(std::make_shared<pkg_helper<pkg_model>>(data));
            m_future_map.erase(iter);
            return true;
        }
        template <RpcModel rpc_model>
        typename std::enable_if<rpc_model == RpcModel::Callback, bool>::type
            deal_rpc_rsp(SessionID session_id, const message_head& head, std::string_view rpc_name, std::string_view data) {
            std::unique_lock<std::mutex> lock(m_func_map_mtx);
            auto timer_iter = m_async_overtimes.find(head.req_id_);
            if (timer_iter != m_async_overtimes.end()) {
                m_async_timer.erase(timer_iter->second);
                m_async_overtimes.erase(timer_iter);
            }

            auto iter = m_callback_map.find(head.req_id_);
            if (iter == m_callback_map.end())
                return true; // ���쳣�������false,δ�󶨶�Ӧ���������쳣,�쳣��ֱ���ͷ�����
            iter->second(session_id, msg_status::ok, data);
            m_callback_map.erase(iter);
            return true;
        }
        template <RpcModel rpc_model>
        typename std::enable_if<rpc_model == RpcModel::Bind, bool>::type
            deal_rpc_rsp(SessionID session_id, const message_head& head, std::string_view rpc_name, std::string_view data) {
            return true;
        }
#pragma endregion

    protected:
        virtual bool write_impl(SessionID session_id, const char* const data, size_t bytes_transferred) = 0;

    protected:
        // ��ȷ���ӻص�
        std::function<void(SessionID)>                           m_connect_cbk = nullptr;
        // ���ӶϿ��ص�
        std::function<void(SessionID)>                           m_close_cbk = nullptr;

        // �ص�ģʽ�ڲ���������
        typedef std::function<void(SessionID, msg_status, std::string_view)>           rsp_callback_function_type;
        // ��ģʽ�ڲ���������
        typedef std::function<void(std::string_view, const message_head&, SessionID, const std::string&)>                       rsp_bind_function_type;

        // ���ݰ�ȫ��
        std::mutex                                                                      m_func_map_mtx;
        // ��������ʱ������ID
        uint32_t                                                                        m_req_id = 0;
        // ��ʱɾ������
        BTool::TimerManager                                                             m_async_timer;
        // req_id, timer_id
        std::unordered_map<uint32_t, TimerManager::TimerId>                             m_async_overtimes;
        // call�����󶨼���, <req_id, ������>
        std::unordered_map<uint32_t, std::shared_ptr<std::promise<pkg_helper_ptr>>>     m_future_map;
        // callback�����󶨼���
        std::unordered_map<uint32_t, rsp_callback_function_type>                        m_callback_map;
        // bind�����󶨼���
        std::unordered_map<std::string, rsp_bind_function_type>                         m_bind_map;
    };

    // Ĭ�ϳ�ʱʱ��, ��λ����
    template<size_t DEFAULT_TIMEOUT = 1000, pkg_helper_base::PkgModel pkg_model = pkg_helper_base::PkgModel::Msg>
    class RpcServer : public RpcBase<DEFAULT_TIMEOUT, pkg_model>
    {
    public:
        typedef typename RpcBase<DEFAULT_TIMEOUT, pkg_model>::RpcModel          RpcModel;
        typedef typename RpcBase<DEFAULT_TIMEOUT, pkg_model>::CommModel         CommModel;
        typedef typename RpcBase<DEFAULT_TIMEOUT, pkg_model>::PkgModel          PkgModel;
        typedef typename RpcBase<DEFAULT_TIMEOUT, pkg_model>::message_head      message_head;
        typedef BTool::BoostNet::NetCallBack::SessionID                         SessionID;
        typedef typename RpcBase<DEFAULT_TIMEOUT, pkg_model>::msg_status        msg_status;

        // async_thread_num: �첽�ص������߳���, ����callback�ص��Լ�bind�ص�
        RpcServer(int async_thread_num = std::thread::hardware_concurrency()) : RpcBase<DEFAULT_TIMEOUT, pkg_model>(async_thread_num) {}
        ~RpcServer() {
            m_ioc_pool.stop();
            m_server_ptr.reset();
        }

    public:
        // �����첽����,����ǰ��������ص�
        bool listen(const std::string& ip, unsigned short port, bool reuse_address = true) {
            m_server_ptr = std::make_shared<BTool::BoostNet::TcpServer>(m_ioc_pool);
            if (!m_server_ptr) return false;
            this->m_async_timer.start();
            m_server_ptr->register_cbk(this);
            m_server_ptr->start(ip.c_str(), port, reuse_address);
            return true;
        }
        bool listen(unsigned short port, size_t async_thread_num = std::thread::hardware_concurrency(), bool reuse_address = true) {
            return listen("0.0.0.0", port, async_thread_num, reuse_address);
        }

    public:
#pragma region ͬ������, ��������
        // �޷��ز��� ͬ������,��������
        // ���ز�������: msg_status,  ��ʾ����״̬
        template<size_t TIMEOUT, typename TReturn = void, typename ...Args>
        typename std::enable_if<std::is_void<TReturn>::value, msg_status>::type
            push(SessionID session_id, const std::string& rpc_name, Args&&... args) {
            return this->sync_send<TIMEOUT, TReturn>(session_id, rpc_name, std::forward<Args>(args)...);
        }
        // �޷��ز���(Ĭ�ϳ�ʱʱ��) ͬ������,��������
        // ���ز�������: msg_status,  ��ʾ����״̬
        template<typename TReturn = void, typename ...Args>
        typename std::enable_if<std::is_void<TReturn>::value, msg_status>::type
            push(SessionID session_id, const std::string& rpc_name, Args&&... args) {
            return push<DEFAULT_TIMEOUT, TReturn>(session_id, rpc_name, std::forward<Args>(args)...);
        }
        // �з��ز��� ͬ������,��������
        // ���ز�������: std::tuple<msg_status, TReturn>,  ��ʾ<����״̬, ���ؽ��>
        template<size_t TIMEOUT, typename TReturn, typename ...Args>
        std::tuple<msg_status, typename std::enable_if<!std::is_void<TReturn>::value, TReturn>::type>
            push(SessionID session_id, const std::string& rpc_name, Args&&... args) {
            return this->sync_send<TIMEOUT, TReturn>(session_id, rpc_name, std::forward<Args>(args)...);
        }
        // �з��ز���(Ĭ�ϳ�ʱʱ��) ͬ������,��������
        // ���ز�������: std::tuple<msg_status, TReturn>,  ��ʾ<����״̬, ���ؽ��>
        template<typename TReturn, typename ...Args>
        std::tuple<msg_status, typename std::enable_if<!std::is_void<TReturn>::value, TReturn>::type>
            push(SessionID session_id, const std::string& rpc_name, Args&&... args) {
            return push<DEFAULT_TIMEOUT, TReturn>(session_id, rpc_name, std::forward<Args>(args)...);
        }
#pragma endregion

#pragma region �첽�ص�, ��������Ч
        // �첽�ص�
        // decltype(auto) ���Ƶ������õ�ԭ����
        // ���ز�������: call_back_req_op<...> , ͨ���䷢���첽����
        // �����������(SessionID, msg_status, ...)
        template<size_t TIMEOUT, typename ...Args>
        decltype(auto) push_back(SessionID session_id, const std::string& rpc_name, Args&&... args) {
            return RpcBase<DEFAULT_TIMEOUT, pkg_model>::call_back_req_op<TIMEOUT, std::tuple<typename std::decay<Args>::type...>>(this, session_id, rpc_name, std::forward_as_tuple(std::forward<Args>(args)...));
        }
        // �첽�ص�
        // decltype(auto) ���Ƶ������õ�ԭ����
        // ���ز�������: call_back_req_op<...> , ͨ���䷢���첽����
        // �����������(SessionID, msg_status, ...)
        template<typename ...Args>
        decltype(auto) push_back(SessionID session_id, const std::string& rpc_name, Args&&... args) {
            return push_back<DEFAULT_TIMEOUT>(session_id, rpc_name, std::forward<Args>(args)...);
        }
#pragma endregion

    protected:
        // ��ȡ��Ϣ�ص�
        void on_read_cbk(SessionID session_id, const char* const msg, size_t bytes_transferred) override {
            BTool::MemoryStream read_buffer(const_cast<char*>(msg), bytes_transferred);
            while (read_buffer.get_res_length() >= sizeof(struct message_head)) {
                // ��ȡ,�Դ�Ư��
                message_head cur_head;
                read_buffer.read(&cur_head);

                // �쳣��
                if (cur_head.title_size_ == 0 || cur_head.content_size_ > (uint32_t)(-1) / 2) {
                    m_server_ptr->close(session_id);
                    return;
                }

                // �ϰ��ж�
                if (read_buffer.get_res_length() < cur_head.title_size_ + cur_head.content_size_)
                    return;

                std::string_view rpc_name(msg + read_buffer.get_offset(), cur_head.title_size_);
                read_buffer.add_offset(cur_head.title_size_);

                std::string_view content(msg + read_buffer.get_offset(), cur_head.content_size_);
                read_buffer.add_offset(cur_head.content_size_);

                // �쳣
                if (!this->deal_read_msg(session_id, cur_head, rpc_name, content)) {
                    m_server_ptr->close(session_id);
                    return;
                }

                // ���������
                m_server_ptr->consume_read_buf(session_id, sizeof(struct message_head) + cur_head.title_size_ + cur_head.content_size_);
            }
        }
        bool write_impl(SessionID session_id, const char* const data, size_t bytes_transferred) override {
            if (m_server_ptr)
                return m_server_ptr->write(session_id, data, bytes_transferred);
            return false;
        }

    private:
        BTool::AsioContextPool                          m_ioc_pool;
        std::shared_ptr<BTool::BoostNet::TcpServer>     m_server_ptr = nullptr;
    };

    // Ĭ�ϳ�ʱʱ��, ��λ����
    template<size_t DEFAULT_TIMEOUT = 1000, pkg_helper_base::PkgModel pkg_model = pkg_helper_base::PkgModel::Msg>
    class RpcClient : public RpcBase<DEFAULT_TIMEOUT, pkg_model>
    {
    public:
        typedef typename RpcBase<DEFAULT_TIMEOUT, pkg_model>::RpcModel          RpcModel;
        typedef typename RpcBase<DEFAULT_TIMEOUT, pkg_model>::CommModel         CommModel;
        typedef typename RpcBase<DEFAULT_TIMEOUT, pkg_model>::PkgModel          PkgModel;
        typedef typename RpcBase<DEFAULT_TIMEOUT, pkg_model>::message_head      message_head;
        typedef BTool::BoostNet::NetCallBack::SessionID                         SessionID;
        typedef typename RpcBase<DEFAULT_TIMEOUT, pkg_model>::msg_status        msg_status;

    public:
        // async_thread_num: �첽�ص������߳���, ����callback�ص��Լ�bind�ص�
        RpcClient(int async_thread_num = 2) : RpcBase<DEFAULT_TIMEOUT, pkg_model>(async_thread_num) {}
        ~RpcClient() {
            m_ioc_pool.stop();
            m_session_ptr.reset();
        }

    public:
        // �����첽����,����ǰ��������ص�
        bool connect(const std::string& ip, unsigned short port, bool auto_reconnect = true) {
            m_session_ptr = std::make_shared<BTool::BoostNet::TcpSession>(m_ioc_pool.get_io_context());
            if (!m_session_ptr) return false;
            this->m_async_timer.start();
            m_session_ptr->register_cbk(this);
            m_b_auto_reconnect = auto_reconnect;
            m_session_ptr->connect(ip.c_str(), port);
            return true;
        }

    public:
#pragma region ͬ������, ��������
        // �޷��ز��� ͬ������,��������
        // ���ز�������: msg_status,  ��ʾ����״̬
        template<size_t TIMEOUT, typename TReturn = void, typename ...Args>
        typename std::enable_if<std::is_void<TReturn>::value, msg_status>::type
            call(const std::string& rpc_name, Args&&... args) {
            return this->sync_send<TIMEOUT, TReturn>(get_session_id(), rpc_name, std::forward<Args>(args)...);
        }
        // �޷��ز���(Ĭ�ϳ�ʱʱ��) ͬ������,��������
        // ���ز�������: msg_status,  ��ʾ����״̬
        template<typename TReturn = void, typename ...Args>
        typename std::enable_if<std::is_void<TReturn>::value, msg_status>::type
            call(const std::string& rpc_name, Args&&... args) {
            return call<DEFAULT_TIMEOUT, TReturn>(rpc_name, std::forward<Args>(args)...);
        }
        // �з��ز��� ͬ������,��������
        // ���ز�������: std::tuple<msg_status, TReturn>,  ��ʾ<����״̬, ���ؽ��>
        template<size_t TIMEOUT, typename TReturn, typename ...Args>
        std::tuple<msg_status, typename std::enable_if<!std::is_void<TReturn>::value, TReturn>::type>
            call(const std::string& rpc_name, Args&&... args) {
            return this->sync_send<TIMEOUT, TReturn>(get_session_id(), rpc_name, std::forward<Args>(args)...);
        }
        // �з��ز���(Ĭ�ϳ�ʱʱ��) ͬ������,��������
        // ���ز�������: std::tuple<msg_status, TReturn>,  ��ʾ<����״̬, ���ؽ��>
        template<typename TReturn, typename ...Args>
        std::tuple<msg_status, typename std::enable_if<!std::is_void<TReturn>::value, TReturn>::type>
            call(const std::string& rpc_name, Args&&... args) {
            return call<DEFAULT_TIMEOUT, TReturn>(rpc_name, std::forward<Args>(args)...);
        }
#pragma endregion

#pragma region �첽�ص�, ��������Ч
        // �첽�ص�
        // decltype(auto) ���Ƶ������õ�ԭ����
        // ���ز�������: call_back_req_op<...> , ͨ���䷢���첽����
        // �����������(SessionID, msg_status, ...)
        template<size_t TIMEOUT, typename ...Args>
        decltype(auto) call_back(const std::string& rpc_name, Args&&... args) {
            return RpcBase<DEFAULT_TIMEOUT, pkg_model>::call_back_req_op<TIMEOUT, std::tuple<typename std::decay<Args>::type...>>(this, get_session_id(), rpc_name, std::forward_as_tuple(std::forward<Args>(args)...));
        }
        // �첽�ص�
        // decltype(auto) ���Ƶ������õ�ԭ����
        // ���ز�������: call_back_req_op<...> , ͨ���䷢���첽����
        // �����������(SessionID, msg_status, ...)
        template<typename ...Args>
        decltype(auto) call_back(const std::string& rpc_name, Args&&... args) {
            return call_back<DEFAULT_TIMEOUT>(rpc_name, std::forward<Args>(args)...);
        }
#pragma endregion

    protected:
        // ��ȡ��Ϣ�ص�
        void on_read_cbk(SessionID session_id, const char* const msg, size_t bytes_transferred) override {
            BTool::MemoryStream read_buffer(const_cast<char*>(msg), bytes_transferred);
            while (read_buffer.get_res_length() >= sizeof(struct message_head)) {
                // ��ȡ,�Դ�Ư��
                message_head cur_head;
                read_buffer.read(&cur_head);

                // �쳣��
                if (cur_head.title_size_ == 0 || cur_head.content_size_ > (uint32_t)(-1) / 2) {
                    m_session_ptr->shutdown();
                    return;
                }

                // �ϰ��ж�
                if (read_buffer.get_res_length() < cur_head.title_size_ + cur_head.content_size_)
                    return;

                std::string_view rpc_name(msg + read_buffer.get_offset(), cur_head.title_size_);
                read_buffer.add_offset(cur_head.title_size_);

                std::string_view content(msg + read_buffer.get_offset(), cur_head.content_size_);

                read_buffer.add_offset(cur_head.content_size_);

                // �쳣
                if (!this->deal_read_msg(session_id, cur_head, rpc_name, content)) {
                    m_session_ptr->shutdown();
                    return;
                }

                // ���������
                m_session_ptr->consume_read_buf(sizeof(struct message_head) + cur_head.title_size_ + cur_head.content_size_);
            }
        }
        SessionID get_session_id() const {
            if (m_session_ptr)
                return m_session_ptr->get_session_id();
            return BTool::BoostNet::NetCallBack::InvalidSessionID;
        }
        bool write_impl(SessionID session_id, const char* const data, size_t bytes_transferred) override {
            if (m_session_ptr)
                return m_session_ptr->write(data, bytes_transferred);
            return false;
        }

    private:
        BTool::AsioContextPool                          m_ioc_pool;
        std::shared_ptr<BTool::BoostNet::TcpSession>    m_session_ptr = nullptr;
        bool                                            m_b_auto_reconnect = true;
    };

}