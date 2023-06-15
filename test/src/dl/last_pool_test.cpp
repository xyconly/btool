
#include "task_pool.hpp"
#include <iostream>
#include "submodule/oneTBB/include/tbb/parallel_for.h"
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
                auto ret = pool.add_task(prop, [&runCount] {
                    ++runCount;
                });
                if(!ret)
                    std::runtime_error("err");
            }
        }
    });

    //pool.stop(false);
    pool.stop(true);

	double xx = {0};
	std::vector<int> ii;
	ii.emplace_back(2);
	ii[4] = 100/xx;
	ii.emplace_back(3);
	ii[4] = 100/xx;
	ii.emplace_back(3);
	
    auto end = BTool::DateTimeConvert::GetCurrentSystemTime();
    auto time= (end - start)/1000;
    std::cout << title << " use time:" << time << "ms" << std::endl
        << "   runCount:" << runCount.load() << std::endl
        << "   avg:" << runCount.load()/time << std::endl
		<< ii.size() << std::endl
		<< ii.at(4) << std::endl;
		
	char buf[10] = {0};
	sprintf(buf, " use time:%d ms:,runCount:%d,avg:%d",time,runCount.load(),runCount.load()/time);
}

int todo()
{
    int avg_count = 10;
    
    for (int i = 0; i < avg_count; i++) {
        BTool::LastTaskPool<int> new_pool;
        test("LastTaskPool", new_pool);
    }

    return 0;
}