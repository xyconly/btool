#include <iostream>
#include "rpc_base.hpp"

using namespace BTool;
using namespace BTool::BoostNet;

std::atomic<int>  count1 = 0;
std::atomic<int>  count2 = 0;
std::atomic<int>  count3 = 0;
std::atomic<int>  count4 = 0;

std::atomic<bool>  start_flag = false;
std::atomic<bool>  exit_flag = false;

int main() {
    {
        RpcClient<DefaultProxyPkgHandle, DefaultProxyMsgHandle, 100000> client;
        client.register_open_cbk([](NetCallBack::SessionID session_id) {
            std::cout << "连接成功:" << session_id << std::endl;
            start_flag.store(true);
            });
        client.register_close_cbk([](NetCallBack::SessionID session_id) {
            std::cout << "连接已断开:" << session_id << std::endl;
            });

        client.connect("192.168.50.31", 41207, false);

        while (!start_flag.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::thread thr1([&] {
            while (!exit_flag.load()) {
                auto [status, rslt] = client.call<int>("add1", 1, 2);
                assert(rslt == 3 && status == msg_status::ok);
                count1++;
            }
            });
        std::thread thr2([&] {
            while (!exit_flag.load()) {
                auto [status, rslt] = client.call<int>("add2", 1, 2);
                assert(rslt == 3 && status == msg_status::ok);
                count2++;
            }
            });

        std::thread thr3([&] {
            while (!exit_flag.load()) {
                client.call_back<int>("add3", 1, 2)([](NetCallBack::SessionID session_id, msg_status status, int rslt) {
                    assert(rslt == 3 && status == msg_status::ok);
                    count3++;
                    });
            }
            });
        std::thread thr4([&] {
            while (!exit_flag.load()) {
                client.call_back<int>("add4", 1, 2)([](NetCallBack::SessionID session_id, msg_status status, int rslt) {
                    assert(rslt == 3 && status == msg_status::ok);
                    count4++;
                    });
            }
            });

        std::this_thread::sleep_for(std::chrono::seconds(60));
        exit_flag.store(true);
        thr1.join();
        thr2.join();
        thr3.join();
        thr4.join();

        std::cout << "  1:" << count1 << std::endl
            << "  2:" << count2 << std::endl
            << "  3:" << count3 << std::endl
            << "  4:" << count4 << std::endl
            << "总计:" << (count1 + count2 + count3 + count4) / 60.0 << "条每秒" << std::endl;
    }
    system("pause");
}



