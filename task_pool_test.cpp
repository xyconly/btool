#include <sstream>
#include <stdio.h>
#include <vector>
#include "tbb/parallel_for.h"
#include "btool/concurrent_task_pool.hpp"
#include "btool/coro_task_pool.hpp"
#include "btool/task_pool.hpp"
#include "btool/datetime_convert.hpp"

const int g_count = 200;
const int g_prop_count = 50000;

template<typename TypeN>
void test(const std::string& title, TypeN& pool) {
    std::unordered_map<int, int> s_j;
    for (int i = 0; i < g_prop_count; i++) {
        s_j[i] = -1;
    }

    std::atomic<int> runCount = 0;
    pool.start(1);

    auto start = BTool::DateTimeConvert::GetCurrentSystemTime(BTool::DateTimeConvert::DTS_ALL);

    tbb::parallel_for(tbb::blocked_range<int>(0, g_prop_count), [&](tbb::blocked_range<int> range) {
        for (auto prop = range.begin(); prop != range.end(); ++prop) {
            for (int j = 0; j < g_count; j++) {
                pool.add_task(prop, [prop , &s_j, j, &runCount] {
                    assert(s_j[prop] == j -1);
                    s_j[prop] = j;
                    ++runCount;
                    //co_sleep(1010);
                });
            }
        }
    });

    pool.stop(true);

    auto end = BTool::DateTimeConvert::GetCurrentSystemTime(BTool::DateTimeConvert::DTS_ALL);
    std::cout << title << " use time:" << BTool::DateTimeConvert::GetMillSecondSpace(end, start) << std::endl
        << "   runCount:" << runCount << std::endl;
}

int main()
{
    co_opt.debug = co::dbg_channel;
    int avg_count = 10;

    std::set<int> props;
    for (int i = 0; i < g_prop_count; i++) {
        props.emplace(i);
    }

    for (int i = 0; i < avg_count; i++) {
        BTool::SerialTaskPool<int> thread_pool;
        test("SerialTaskPool", thread_pool);
    }

    for (int i = 0; i < avg_count; i++) {
        BTool::CoroSerialTaskPool<int> new_pool(props);
        test("CoroSerialTaskPool", new_pool);
    }

    //for (int i = 0; i < avg_count; i++) {
    //    BTool::ConcurrentSerialTaskPool<int> concurrent_pool(props);
    //    test("ConcurrentSerialTaskPool", concurrent_pool);
    //}
    system("pause");
    return 0;
}