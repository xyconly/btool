#include <iostream>
#include "boost_net/boost_net/websocket_session.hpp"
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
        RpcClient<BTool::BoostNet::WebsocketSession<true>,  DefaultProxyPkgHandle, DefaultProxyMsgHandle, 100000> client;
        client.register_open_cbk([&](NetCallBack::SessionID session_id) {
            std::cout << BTool::DateTimeConvert::GetCurrentSystemTime().to_local_string() << "    连接成功:" << session_id << std::endl;
            start_flag.store(true);
            });
        client.register_close_cbk([](NetCallBack::SessionID session_id, const char* const msg, size_t bytes_transferred) {
            std::cout << BTool::DateTimeConvert::GetCurrentSystemTime().to_local_string() << "    连接已断开:" << session_id << "; msg:" << std::string_view(msg, bytes_transferred) << std::endl;
            });

        client.connect("127.0.0.1", 41207, true);

        while (!start_flag.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        auto start_time = std::chrono::steady_clock::now();

        int for_count = 100000;

        // std::thread thr1([&] {
        //     for (int i = 0; i < 5; ++i) {
        //         auto [status, rslt] = client.call<int>("add1", i, 0);
        //         count1++;
        //         if (status != msg_status::ok || rslt != i) {
        //             std::cout << "thr1 err" << std::endl;
        //         }
        //     }
        // });
        // std::thread thr2([&] {
        //     for (int i = 0; i < 5; ++i) {
        //         auto [status, rslt] = client.call<int>("add2", i, i);
        //         count2++;
        //         if (status != msg_status::ok || rslt != 2 * i) {
        //             std::cout << "thr2 err" << std::endl;
        //         }
        //     }
        // });
        std::thread thr3([&] {
            for (int i = 0; i < for_count; ++i) {
                client.call_back("add3", (int)i, (int)2*i)([](NetCallBack::SessionID session_id, msg_status status, int rslt, int index) {
                    count3++;
                    if (status != msg_status::ok || rslt != 3 * index) {
                        std::cout << "thr3 err" << std::endl;
                    }
                });
            }
        });
        std::thread thr4([&] {
            for (int i = 0; i < for_count; ++i) {
                client.call_back<int, int>("add4", (int)i, (int)3*i)([](NetCallBack::SessionID session_id, msg_status status, int rslt, int index) {
                    count4++;
                    if (status != msg_status::ok || rslt != 4 * index) {
                        std::cout << "thr4 err: rslt=" << rslt << std::endl;
                    }
                });
            }
        });
        std::thread thr5([&] {
            for (int i = 0; i < for_count; ++i) {
                client.call_back_timer<10000, int, int>("test", (int)i, (int)4*i)([](NetCallBack::SessionID session_id, msg_status status, int rslt, int index, bool bl, test_st&& ts) {
                    ++count5;
                    if (status != msg_status::ok || rslt != 5 * index) {
                        std::cout << "thr5 err: status=" << (int)status << std::endl;
                    }
                });
            }
        });
        std::this_thread::sleep_for(std::chrono::seconds(10));
        exit_flag.store(true);
        // thr1.join();
        // thr2.join();
        // thr3.join();
        // thr4.join();
        // thr5.join();

        auto all_count = 3 * for_count;
        while (count3.load() + count4.load() + count5.load() < all_count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        auto end_time = std::chrono::steady_clock::now();
        auto use_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        std::cout << "  1:" << count1.load() << std::endl;
        std::cout << "  2:" << count2.load() << std::endl;
        std::cout << "  3:" << count3.load() << std::endl;
        std::cout << "  4:" << count4.load() << std::endl;
        std::cout << "  5:" << count5.load() << std::endl;
        std::cout << "耗时:" << use_time << "毫秒; 总计:" << (count1.load() + count2.load() + count3.load() + count4.load() + count5.load()) / use_time << "条每豪秒" << std::endl;

        std::cout << BTool::DateTimeConvert::GetCurrentSystemTime().to_local_string() << "---------------------------------------------------" << std::endl;
    }
    system("pause");
}



