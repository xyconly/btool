#include <iostream>
#include "rpc_base.hpp"

using namespace BTool;
using namespace BTool::BoostNet;

std::atomic<int>  count1 = 0;
std::atomic<int>  count2 = 0;
std::atomic<int>  count3 = 0;
std::atomic<int>  count4 = 0;

int add1(NetCallBack::SessionID session_id, int a, int b) {
    count1++;
    return a + b;
}

class test {
public:
    int add2(NetCallBack::SessionID session_id, int a, int b) {
        count2++;
        return a + b;
    }
    int add3(NetCallBack::SessionID session_id, int a, int b) {
        count3++;
        return a + b;
    }
};

int main() {
    {
        RpcService<DefaultProxyPkgHandle, DefaultProxyMsgHandle, 100000> service;
        service.register_open_cbk([](NetCallBack::SessionID session_id) {
            std::cout << "发现新的连接:" << session_id << std::endl;
            });
        service.register_close_cbk([](NetCallBack::SessionID session_id) {
            std::cout << "连接已断开:" << session_id << std::endl;
            });
        if (service.listen("192.168.50.31", 41207)) {
            std::cout << "开启监听:" << 41207 << std::endl;
        }
        else {
            std::cout << "开启监听失败!!!!" << std::endl;
            system("pause");
            return -1;
        }

        service.bind_auto("add1", &add1);
        test tt;
        service.bind_auto("add2", &test::add2, &tt);
        std::function<int(NetCallBack::SessionID, int, int)> func_add3 = std::bind(&test::add3, &tt, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        service.bind_auto_functional("add3", func_add3);

        service.bind_auto("add4", [](NetCallBack::SessionID session_id, int a, int b)->int {
            count4++;
            return a + b;
            });

        while(getchar() != EOF)
        {
            std::cout << "  1:" << count1 << std::endl
                << "  2:" << count2 << std::endl
                << "  3:" << count3 << std::endl
                << "  4:" << count4 << std::endl
                << "总计:" << (count1 + count2 + count3 + count4) / 60.0 << "条每秒" << std::endl;
            count1.store(0);
            count2.store(0);
            count3.store(0);
            count4.store(0);

        }
    }
    system("pause");
}



