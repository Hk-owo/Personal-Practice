//
// Created by lacas on 2025/12/8.
//

#include <atomic>
#include <chrono>
#include <iostream>
#include "ThreadPool.h"

int main(){
    ThreadPool tp;                       // 6 线程（5 工作 + 1 分发）
    std::atomic<uint64_t> done{0};
    constexpr uint64_t TOTAL = 100'000'000;//1'000'000;

    auto t0 = std::chrono::high_resolution_clock::now();

    for(uint64_t i = 0; i < TOTAL; ++i)
        tp.submit([&done]{ done.fetch_add(1, std::memory_order_relaxed); });

    while(done.load(std::memory_order_relaxed) != TOTAL)
        ;   // 纯轮询等待

    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - t0).count();

    std::cout << "total jobs : " << TOTAL << '\n'
              << "total time : " << us / 1000 << " ms\n"
              << "throughput : " << (TOTAL * 1'000'000 / us) << " job/s\n";
    return 0;
}




