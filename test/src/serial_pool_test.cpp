
#include "task_pool.hpp"
#include <iostream>
#include "submodule/oneTBB/include/tbb/parallel_for.h"
#include "datetime_convert.hpp"

int g_count = 1000000;
int g_prop_count = 5;

struct RsltSt {
    size_t count_;
    size_t time_;
    RsltSt() : count_(0), time_(0) {}
    RsltSt(size_t count, size_t time) : count_(count), time_(time) {}

    RsltSt operator+=(const RsltSt& other) {
        count_ += other.count_;
        time_ += other.time_;
        return *this;
    }
};

template<typename TypeN>
RsltSt test(const std::string& title, TypeN& pool) {
    std::unordered_map<int, int> s_j;
    for (int i = 0; i < g_prop_count; i++) {
        s_j[i] = -1;
    }

    std::atomic<int> runCount{0};
    if constexpr (!std::is_same_v<TypeN, BTool::LockFreeRotateSerialTaskPool<int>>
        && !std::is_same_v<TypeN, BTool::ConditionRotateSerialTaskPool<int>>) {
        pool.start(std::min((size_t)std::thread::hardware_concurrency() - 2, (size_t)g_prop_count), true, 2);
    }

    auto start = BTool::DateTimeConvert::GetCurrentSystemTime();

    tbb::parallel_for(tbb::blocked_range<int>(0, g_prop_count), [&](tbb::blocked_range<int> range) {
        for (auto prop = range.begin(); prop != range.end(); ++prop) {
            for (int j = 0; j < g_count; j++) {
                auto ret = pool.add_task(prop, [prop , &s_j, j, &runCount] {
                    assert(s_j[prop] == j -1);
                    if (s_j[prop] != j -1) {
                        std::cerr << "err prop:" << prop << " j:" << j << " s_j:" << s_j[prop] << std::endl;
                    }
                    s_j[prop] = j;
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
    auto time = (end - start)/1000;
    // std::cout << title << " use time:" << time << "ms" << std::endl
    //     << "   runCount:" << runCount << std::endl
    //     << "   avg:" << runCount/time << std::endl;
    return RsltSt(runCount, time);
}

void run(int avg_count) {
    std::vector<int> props;
    for(int i = 0; i < g_prop_count; i++) {
        props.emplace_back(i);
    }

    RsltSt rslt;
    for (int i = 0; i < avg_count; i++) {
        BTool::ConditionRotateSerialTaskPool<int> new_pool(props, std::thread::hardware_concurrency() - 2, true, 2);
        rslt += test("ConditionRotateSerialTaskPool", new_pool);
    }
    std::cout << "ConditionRotateSerialTaskPool avg count: " << rslt.count_ / rslt.time_ << " count/ms;  avg time: " << rslt.time_ / avg_count << "ms" << std::endl << std::endl;

    rslt = RsltSt();
    for (int i = 0; i < avg_count; i++) {
        BTool::LockFreeRotateSerialTaskPool<int> new_pool(props, std::thread::hardware_concurrency() - 2, true, 2);
        rslt += test("LockFreeRotateSerialTaskPool", new_pool);
    }
    std::cout << "LockFreeRotateSerialTaskPool avg count: " << rslt.count_ / rslt.time_ << " count/ms;  avg time:" << rslt.time_ / avg_count << "ms" << std::endl << std::endl;

    rslt = RsltSt();
    for (int i = 0; i < avg_count; i++) {
        BTool::SerialTaskPool<int, BTool::SPMCSerialTaskQueue<int>> new_pool;
        new_pool.reset_props(props);
        rslt += test("SerialTaskPool SPMCSerialTaskQueue", new_pool);
    }
    std::cout << "SerialTaskPool SPMCSerialTaskQueue avg count: " << rslt.count_ / rslt.time_ << " count/ms;  avg time:" << rslt.time_ / avg_count << "ms" << std::endl << std::endl;

    rslt = RsltSt();
    for (int i = 0; i < avg_count; i++) {
        BTool::SerialTaskPool<int> new_pool;
        rslt += test("SerialTaskPool", new_pool);
    }
    std::cout << "SerialTaskPool avg count: " << rslt.count_ / rslt.time_ << " count/ms;  avg time:" << rslt.time_ / avg_count << "ms" << std::endl << std::endl;

    rslt = RsltSt();
    for (int i = 0; i < avg_count; i++) {
        BTool::RotateSerialTaskPool<int> new_pool;
        rslt += test("RotateSerialTaskPool", new_pool);
    }
    std::cout << "RotateSerialTaskPool avg count: " << rslt.count_ / rslt.time_ << " count/ms;  avg time:" << rslt.time_ / avg_count << "ms" << std::endl << std::endl;
}

int main()
{
    // Logger::instance().set_log_file("test.log");
    int avg_count = 20;

    std::cout << "================= g_count = 10000 ============ g_prop_count = 5 =================" << std::endl;
    g_count = 10000;
    g_prop_count = 5;
    run(avg_count);

    std::cout << "================ g_count = 1000000 =========== g_prop_count = 5 =================" << std::endl;
    g_count = 1000000;
    g_prop_count = 5;
    run(avg_count);

    std::cout << "================= g_count = 10000 ========== g_prop_count = 500 =================" << std::endl;
    g_count = 10000;
    g_prop_count = 500;
    run(avg_count);

    std::cout << "================ g_count = 100 =========== g_prop_count = 50000 =================" << std::endl;
    g_count = 100;
    g_prop_count = 50000;
    run(avg_count);
    return 0;
}