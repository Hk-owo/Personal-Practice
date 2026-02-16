//
// Created by lacas on 2025/12/9.
//

#include "ThreadPool.h"

using namespace std;

ThreadPool::ThreadPool() {
    m_thread.resize(TSIZE);
    is_finish.resize(TSIZE, false);
    //工作线程
    for(int i = 0;i < TSIZE - 1;i++){
        m_thread[i] = thread([i,this]{
            CoroWait coro = work(i);
            while(!coro.done()) {
                coro.resume();
                if(!coro.done())
                    m_queue[i].wait_for_data();
            }
            is_finish[i] = true;
//            while (!stop.load(memory_order_relaxed)) {
//                std::function<void()> task;
//                while (!stop.load(memory_order_relaxed)) {
//                    if (m_queue[i].dequeue(task)) {
//                        if (task) task();
//                    } else break;
//                }
//                m_queue[i].wait_for_data_uring();
//            }
//            is_finish[i] = true;
        });
    }
    //分发线程
    m_thread[TSIZE - 1] = thread([this]{
        CoroWait coro = distribute();
        while(!coro.done()){
            coro.resume();
            if(!coro.done())
                m_queue[TSIZE - 1].wait_for_data();
        }
        is_finish[TSIZE - 1] = true;
//        while(!stop.load(memory_order_relaxed)){
//            std::function<void()> task;
//            size_t minldx = 0;
//            size_t batch = 0;
//            while(!stop.load(memory_order_relaxed)){
//                if(!m_queue[TSIZE - 1].dequeue(task)) {
//                    for(int i = 0;i < TSIZE - 1;i++)
//                        m_queue[i].on_data_ready_uring();
//                    if(!m_queue[TSIZE - 1].dequeue(task)) break;
//                }
//
//                //如果存货达到BATCH_SIZE时候唤醒
//                if(task && !(batch & (BATCH_SIZE - 1))){
//                    //入队
//                    m_queue[minldx].enqueue(task);
//                    m_queue[minldx].on_data_ready_uring();
//                    batch = 0;
//                }
//                else if(task){
//                    //分发的时候选择最少任务的派发实现负载均衡(每MINLDX_SIZE任务采样一次)
//                    if (!(batch & (MINLDX_SIZE - 1))) {
//                        for (int i = 0; i < TSIZE - 1; ++i)
//                            if (m_queue[i].size() < m_queue[minldx].size()) minldx = i;
//                    }
//                    //入队
//                    m_queue[minldx].enqueue(task);
//                    batch++;
//                }
//                task = nullptr;
//            }
//            m_queue[TSIZE - 1].wait_for_data_uring();
//        }
//        is_finish[TSIZE - 1] = true;
    });
}

ThreadPool::~ThreadPool() {
    stop.store(true, std::memory_order_release);
    for(int i = 0;i < TSIZE;i++){
        m_queue[i].notify_stop();
        if(is_finish[i] == true) {
            if (m_thread[i].joinable())
                m_thread[i].join();
        }
        else i--;
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
        if(task && !(batch & (BATCH_SIZE - 1))){
            //入队
            m_queue[minldx].enqueue(task);
            m_queue[minldx].on_data_ready();
            batch = 0;
        }
        else if(task){
            //分发的时候选择最少任务的派发实现负载均衡(每MINLDX_SIZE任务采样一次)
            if (!(batch & (MINLDX_SIZE - 1))) {
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
    if(isclose.load(std::memory_order_acquire)){
        cout << "this threadpool is close.error.";
        return;
    }
    auto& q = m_queue[TSIZE - 1];
    q.enqueue(f);
    q.on_data_ready();
}

void ThreadPool::close() {
    stop.store(true, std::memory_order_release);
    for(int i = 0;i < TSIZE;i++){
        m_queue[i].notify_stop();
        if(is_finish[i] == true) {
            if (m_thread[i].joinable())
                m_thread[i].join();
        }
        else i--;
    }
    isclose.store(true,std::memory_order_release);
}
