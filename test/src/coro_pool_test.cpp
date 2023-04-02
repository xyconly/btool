
#include "coro_task_pool.hpp"
#include "task_pool.hpp"
#include <iostream>
#include <stdio.h>
#include "submodule/oneTBB/include/tbb/parallel_for.h"
#include "datetime_convert.hpp"

int g_count = 2000;
int g_prop_count = 5000;

template<typename TypeN>
void test(const std::string& title, TypeN& pool) {
    std::unordered_map<int, int> s_j;
    for (int i = 0; i < g_prop_count; i++) {
        s_j[i] = -1;
    }

    std::atomic<int> runCount{0};
    pool.start();

    auto start = BTool::DateTimeConvert::GetCurrentSystemTime();

    tbb::parallel_for(tbb::blocked_range<int>(0, g_prop_count), [&](tbb::blocked_range<int> range) {
        for (auto prop = range.begin(); prop != range.end(); ++prop) {
            for (int j = 0; j < g_count; j++) {
                auto ret = pool.add_task(prop, [prop , &s_j, j, &runCount] {
                    assert(s_j[prop] == j -1);
                    s_j[prop] = j;
                    ++runCount;
                });
                if(!ret)
                    std::runtime_error("err");
            }
        }
    });

    pool.stop();

    auto end = BTool::DateTimeConvert::GetCurrentSystemTime();
    auto time= (end - start)/1000;
    std::cout << title << " use time:" << time << "ms" << std::endl
        << "   runCount:" << runCount << std::endl
        << "   avg:" << runCount/time << std::endl;
}

template<typename TypeN>
void test_withthread_pool(const std::string& title, TypeN& pool) {
    std::unordered_map<int, int> s_j;
    for (int i = 0; i < g_prop_count; i++) {
        s_j[i] = -1;
    }

    std::atomic<int> runCount{0};
    pool.start();

    auto start = BTool::DateTimeConvert::GetCurrentSystemTime();

    tbb::parallel_for(tbb::blocked_range<int>(0, g_prop_count), [&](tbb::blocked_range<int> range) {
        for (auto prop = range.begin(); prop != range.end(); ++prop) {
            for (int j = 0; j < g_count; j++) {
                auto ret = pool.add_task(prop, [&pool, prop , &s_j, j, &runCount] {
                    pool.push_threadpool(prop, [prop , &s_j, j, &runCount] { // 再抛回线程池处理
                        assert(s_j[prop] == j -1);
                        s_j[prop] = j;
                        ++runCount;
                    });
                });
                if(!ret)
                    std::runtime_error("err");
            }
        }
    });

    pool.stop(true);

    auto end = BTool::DateTimeConvert::GetCurrentSystemTime();
    auto time= (end - start)/1000;
    std::cout << title << " use time:" << time << "ms" << std::endl
        << "   runCount:" << runCount << std::endl
        << "   avg:" << runCount/time << std::endl;
}

int main()
{
    //co_opt.debug = co::dbg_channel;
    int avg_count = 10;

    std::set<int> props;
    for (int i = 0; i < g_prop_count; i++) {
        props.emplace(i);
    }
    
    for (int i = 0; i < avg_count; i++) {
        BTool::CoroSerialTaskPool<int> new_pool(props);
        //new_pool.init_props(props);
        test("CoroSerialTaskPool NoLock", new_pool);
    }    

    for (int i = 0; i < avg_count; i++) {
        BTool::CoroSerialTaskPoolWithThreadPool<BTool::RotateSerialTaskPool, int> new_pool(props);
        //new_pool.init_props(props);
        test_withthread_pool("CoroSerialTaskPoolWithThreadPool NoLock", new_pool);
    }

    g_count = 200;
    g_prop_count = 500;
    for (int i = 0; i < 1; i++) {
        BTool::CoroSerialTaskPool<int, false> new_pool;
        test("CoroSerialTaskPool Lock", new_pool);
    }
    return 0;
}