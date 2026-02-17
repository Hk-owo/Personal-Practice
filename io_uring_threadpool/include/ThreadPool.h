//
// Created by lacas on 2025/12/9.
//

#ifndef IO_URING_SERVER_THREADPOOL_H
#define IO_URING_SERVER_THREADPOOL_H

#include "Wait_queue.h"

/*
 * 默认6线程
 * 5个工作线程
 * 1个分发线程
 */
class ThreadPool {
private:
    //线程数量
    static const size_t TSIZE = 6;

    //批次处理的数目 采样频率
    //一定要是2的幂次，后面采用的是位运算
    //根据任务执行时长调整大小
    const size_t BATCH_SIZE = 1 << 10, MINLDX_SIZE = 128;

    //线程池总开关
    std::atomic<bool> stop{false};

    //线程实例
    std::vector<std::thread> m_thread;

    //线程的SPCP工作队列
    //协程形式的阻塞工作队列
    Wait_queue<std::function<void()>> m_queue[TSIZE];
public:
    ThreadPool();
    ~ThreadPool();
    void submit(std::function<void()> f);
private:
    //原本打算每个线程分配一个协程工作队列，但是实际上还是需要两层阻塞速度更慢
    CoroWait work(int i);
    CoroWait distribute();
};

#endif //IO_URING_SERVER_THREADPOOL_H
