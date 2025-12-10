//
// Created by lacas on 2025/12/9.
//

#include "ThreadPool.h"
#include <algorithm>

using namespace std;

ThreadPool::ThreadPool() {
    m_thread.resize(TSIZE);
    //工作线程
    for(int i = 0;i < TSIZE - 1;i++){
        m_thread[i] = thread([i,this]{
            auto coro = work(i);
            auto w = coro.get_handle();
            while(w && !w.done()){
                w.resume();
                m_queue[i].wait_for_data();
            }
        });
    }
    //分发线程
    m_thread[TSIZE - 1] = thread([this]{
        auto coro = distribute();
        auto w = coro.get_handle();
        while(w && !w.done()){
            w.resume();
            m_queue[TSIZE - 1].wait_for_data();
        }
    });
}

ThreadPool::~ThreadPool() {
    stop.store(true, std::memory_order_release);
    for (auto& q : m_queue) q.notify_stop();
    for (auto& t : m_thread) {
        if(t.joinable())
            t.join();
    }
}

//工作线程的协程
CoroWait ThreadPool::work(int i) {
    std::function<void()> task;
    while(!stop.load(memory_order_relaxed)){
        task = co_await m_queue[i];
        if(task) task();
    }
    co_return;
}

//分发线程的协程
CoroWait ThreadPool::distribute() {
    std::function<void()> task;
    size_t minldx = 0;
    size_t batch = 0;
    while(!stop.load(memory_order_relaxed)){
        if(!m_queue[TSIZE - 1].dequeue(task)) {
            for(int i = 0;i < TSIZE - 1;i++)
                m_queue[i].on_data_ready();
            task = co_await m_queue[TSIZE - 1];
        }

        //如果存货达到BATCH_SIZE时候唤醒
        if(task && !(batch & BATCH_SIZE - 1)){
            //入队
            m_queue[minldx].enqueue(task);
            for(int i = 0;i < TSIZE - 1;i++)
                m_queue[i].on_data_ready();
            batch = 0;
        }
        else if(task){
            //分发的时候选择最少任务的派发实现负载均衡(每MINLDX_SIZE任务采样一次)
            if (!(batch & MINLDX_SIZE - 1)) {
                for (int i = 0; i < TSIZE - 1; ++i)
                    if (m_queue[i].size() < m_queue[minldx].size()) minldx = i;
            }
            //入队
            m_queue[minldx].enqueue(task);
            batch++;
        }
        task = nullptr;
    }
    co_return;
}

//线程池的提交函数
void ThreadPool::submit(std::function<void()> f) {
    auto& q = m_queue[TSIZE - 1];
    q.enqueue(f);
    q.on_data_ready();
}
