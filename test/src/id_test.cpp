#include <iostream>
#include <thread>
#include "submodule/oneTBB/include/tbb/concurrent_unordered_set.h"
#include "submodule/oneTBB/include/tbb/parallel_for.h"
#include "unique_id.hpp"


int main() {
    tbb::concurrent_unordered_set<uint64_t> sets;
    tbb::parallel_for(tbb::blocked_range<uint64_t>(0, 100000000), [&](tbb::blocked_range<uint64_t> range) {
        for (auto prop = range.begin(); prop != range.end(); ++prop) {
            auto id = BTool::SnowFlakeID::instance()->get_id();
            auto [_, ok] = sets.emplace(id);
            if(!ok) {
                throw "contains";
            }
            //auto f1 = id & 0xFFFFFFFFFFC00000;
            //auto f2 = id &           0x3FF000;
            //auto f3 = id &              0xFFF;
            //std::cout << f1 << "  - " << f2 << " - " << f3 << std::endl;
        }
    });
    return 0;
}