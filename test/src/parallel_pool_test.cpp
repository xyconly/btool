
#include "task_pool.hpp"
#include <iostream>
#include "tbb/parallel_for.h"
#include "datetime_convert.hpp"

const int g_count = 2000;
const int g_prop_count = 5000;

std::unordered_map<int, int> s_j;
void init() {
    for (int i = 0; i < g_prop_count; i++) {
        s_j[i] = -1;
    }
}

template<typename TypeN>
void test(const std::string& title, TypeN& pool) {
    std::atomic<int> runCount{0};
    static int s_i(0);
    pool.start();

    auto start = BTool::DateTimeConvert::GetCurrentSystemTime();

    tbb::parallel_for(tbb::blocked_range<int>(0, g_prop_count), [&](tbb::blocked_range<int> range) {
        for (auto prop = range.begin(); prop != range.end(); ++prop) {
            for (int j = 0; j < g_count; j++) {
                auto ret = pool.add_task([&runCount] {
                    ++runCount;
                });
                if(!ret)
                    std::runtime_error("err");
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
    
    for (int i = 0; i < avg_count; i++) {
        BTool::ParallelTaskPool new_pool;
        test("ParallelTaskPool", new_pool);
    }
        
    for (int i = 0; i < avg_count; i++) {
        BTool::NoBlockingParallelTaskPool new_pool;
        test("NoBlockingParallelTaskPool", new_pool);
    }

    return 0;
}