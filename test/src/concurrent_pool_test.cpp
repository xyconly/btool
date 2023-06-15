
#include "concurrent_task_pool.hpp"
#include <iostream>
#include "submodule/oneTBB/include/tbb/parallel_for.h"
#include "datetime_convert.hpp"

const int g_count = 125000;
const int g_prop_count = 80;

template<typename TypeN>
void test(const std::string& title, TypeN& pool) {
    std::unordered_map<int, int> s_j;
    for (int i = 0; i < g_prop_count; i++) {
        s_j[i] = -1;
    }

    std::atomic<int> runCount{0};
    pool.start();

    auto start = BTool::DateTimeConvert::GetCurrentSystemTime();

    tbb::parallel_for(tbb::blocked_range<int>(0, g_count), [&](tbb::blocked_range<int> range) {
        for (auto prop = range.begin(); prop != range.end(); ++prop) {
            for (int j = 0; j < g_prop_count; j++) {
                auto ret = pool.add_task(j, [&runCount] {
                    ++runCount;
                });
                if(!ret)
                    throw std::runtime_error("err");
            }
        }
    });

    //pool.stop(false);
    pool.stop(true);

    auto end = BTool::DateTimeConvert::GetCurrentSystemTime();
    auto time= (end - start)/1000;
    std::cout << title << " use time:" << time << "ms" << std::endl
        << "   runCount:" << runCount << std::endl
        << "   avg:" << runCount/time << std::endl;
}

int main()
{
    int avg_count = 10;

    std::set<int> props;
    for (int i = 0; i < g_prop_count; i++) {
        props.emplace(i);
    }
    
    for (int i = 0; i < avg_count; i++) {
        BTool::ConcurrentSerialTaskPool<int> new_pool(props);
        //new_pool.init_props(props);
        test("ConcurrentSerialTaskPool", new_pool);
    }

    return 0;
}