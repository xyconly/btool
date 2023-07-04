#include <iostream>
#include "boost_net/boost_net1_71/websocket_session.hpp"
#include "boost_net/tcp_session.hpp"
#include "boost_net/rpc_base.hpp"
#include "datetime_convert.hpp"

using namespace BTool;
using namespace BTool::BoostNet;

std::atomic<int>  count1 = 0;
std::atomic<int>  count2 = 0;
std::atomic<int>  count3 = 0;
std::atomic<int>  count4 = 0;
std::atomic<int>  count5 = 0;

struct test_st {
    int i;
    bool b;
};

int main() {
    for (int i =0; i < 100; i++)
    {
        count1.store(0);
        count2.store(0);
        count3.store(0);
        count4.store(0);
        count5.store(0);

        std::atomic<bool>  start_flag = false;
        std::atomic<bool>  exit_flag = false;
        RpcClient<BTool::BoostNet1_71::WebsocketSession,  DefaultProxyPkgHandle, DefaultProxyMsgHandle, 100000> client;
        client.register_open_cbk([&](NetCallBack::SessionID session_id) {
            std::cout << BTool::DateTimeConvert::GetCurrentSystemTime().to_local_string() << "    连接成功:" << session_id << std::endl;
            start_flag.store(true);
            });
        client.register_close_cbk([](NetCallBack::SessionID session_id) {
            std::cout << BTool::DateTimeConvert::GetCurrentSystemTime().to_local_string() << "    连接已断开:" << session_id << std::endl;
            });

        client.connect("127.0.0.1", 41207, true);

        while (!start_flag.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::thread thr1([&] {
            while (!exit_flag.load()) {
                auto [status, rslt] = client.call<int>("add1", 1, 1);
                if (status == msg_status::ok)
                    assert(rslt == 2);
                else {
                    assert(0);
                    return;
                }
                count1++;
            }
            });
        std::thread thr2([&] {
            while (!exit_flag.load()) {
                auto [status, rslt] = client.call<int>("add2", 1, 2);
                if (status == msg_status::ok)
                    assert(rslt == 3);
                else {
                    assert(0);
                    return;
                }
                count2++;
            }
            });
        std::thread thr3([&] {
            while (!exit_flag.load()) {
                client.call_back<int>("add3", 1, 3)([](NetCallBack::SessionID session_id, msg_status status, int rslt) {
                    if(status == msg_status::ok)
                        assert(rslt == 4);
                    else {
                        assert(0);
                        return;
                    }
                    count3++;
                    });
            }
            });
        std::thread thr4([&] {
            while (!exit_flag.load()) {
                client.call_back<int>("add4", 1, 4)([](NetCallBack::SessionID session_id, msg_status status, int rslt) {
                    if (status == msg_status::ok)
                        assert(rslt == 5);
                    else {
                        assert(0);
                        return;
                    }
                    count4++;
                    });
            }
            });
        std::thread thr5([&] {
            while (!exit_flag.load()) {
                client.call_back<int>("test", 1, 5)([](NetCallBack::SessionID session_id, msg_status status, int rslt, bool bl, test_st ts) {
                    if (status == msg_status::ok)
                        assert(rslt == 6);
                    else {
                        assert(0);
                        return;
                    }
                    count5++;
                });
            }
            });
        std::this_thread::sleep_for(std::chrono::seconds(10));
        exit_flag.store(true);
        thr1.join();
        thr2.join();
        thr3.join();
        thr4.join();
        thr5.join();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "  1:" << count1.load() << std::endl;
        std::cout << "  2:" << count2.load() << std::endl;
        std::cout << "  3:" << count3.load() << std::endl;
        std::cout << "  4:" << count4.load() << std::endl;
        std::cout << "  5:" << count5.load() << std::endl;
        std::cout << "总计:" << (count1.load() + count2.load() + count3.load() + count4.load() + count5.load()) / 10.0 << "条每秒" << std::endl;

        std::cout << BTool::DateTimeConvert::GetCurrentSystemTime().to_local_string() << "---------------------------------------------------" << std::endl;
    }
    system("pause");
}



