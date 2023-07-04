#include <iostream>
#include "boost_net/boost_net1_71/websocket_server.hpp"
#include "boost_net/tcp_server.hpp"
#include "boost_net/rpc_base.hpp"

using namespace BTool;
using namespace BTool::BoostNet;

std::map<NetCallBack::SessionID, std::atomic<int>> count1;
std::map<NetCallBack::SessionID, std::atomic<int>> count2;
std::map<NetCallBack::SessionID, std::atomic<int>> count3;
std::map<NetCallBack::SessionID, std::atomic<int>> count4;
std::map<NetCallBack::SessionID, std::atomic<int>> count5;
NetCallBack::SessionID cur_session_id = 0;

struct test_st {
    int i;
    bool b;
};

int add1(NetCallBack::SessionID session_id, int a, int b) {
    count1[session_id]++;
    return a + b;
}

class test {
public:
    int add2(NetCallBack::SessionID session_id, int a, int b) {
        count2[session_id]++;
        return a + b;
    }
    int add3(NetCallBack::SessionID session_id, int a, int b) {
        count3[session_id]++;
        return a + b;
    }
};

void printSession(NetCallBack::SessionID session_id) {
    std::cout << "-------------------------session_id:  " << session_id << "-----------------------------------" << std::endl;
    std::cout << "  1:" << count1[session_id] << std::endl;
    std::cout << "  2:" << count2[session_id] << std::endl;
    std::cout << "  3:" << count3[session_id] << std::endl;
    std::cout << "  4:" << count4[session_id] << std::endl;
    std::cout << "  5:" << count5[session_id] << std::endl;
    std::cout << "  总计:" << (count1[session_id] + count2[session_id] + count3[session_id] + count4[session_id] + count5[session_id]) / 10.0 << "条每秒" << std::endl;
    std::cout << "--------------------------------------" << session_id << "-----------------------------------" << std::endl;
}

int main() {
    for (int i = 0; i <= 100; i++) {
        count1[i] = 0;
        count2[i] = 0;
        count3[i] = 0;
        count4[i] = 0;
        count5[i] = 0;
    }

    {
        RpcService<BTool::BoostNet1_71::WebsocketServer, DefaultProxyPkgHandle, DefaultProxyMsgHandle, 100000> service;
        service.register_open_cbk([](NetCallBack::SessionID session_id) {
            if(cur_session_id > 0)
                printSession(cur_session_id);
            cur_session_id = session_id;
            std::cout << "发现新的连接:" << session_id << std::endl;
            });
        service.register_close_cbk([](NetCallBack::SessionID session_id) {
            std::cout << "连接已断开:" << session_id << std::endl;
        });
        if (service.listen("127.0.0.1", 41207)) {
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
            count4[session_id]++;
            return a + b;
            });

        service.bind_auto("test", [](NetCallBack::SessionID session_id, int a, int b)->std::tuple<int,bool, test_st> {
            count5[session_id]++;
            return std::forward_as_tuple(a + b, true, test_st{ a + b , false});
            });


        std::cout << "输入session ID,查看请求数量(ctrl + z)则退出" << std::endl;
        char chr = 0;
        while((chr = getchar()) != EOF) {
            if (chr == '\n')
                continue;
            printSession(chr - '0');
        }
    }
    system("pause");
}



