//
// Created by lacas on 2025/12/10.
//

#ifndef IO_URING_SERVER_WAIT_QUEUE_H
#define IO_URING_SERVER_WAIT_QUEUE_H

#include "CoroWait.h"
#include "TQueue.h"

/*
 * 利用协程完成队列的阻塞等待
 * 没做多线程竞争冗余
 * 多消费者会free句柄多次
 * 目前只做SPSC的单消费者
 */
template<typename T>
class Wait_queue : public TQueue<T>{
private:
    //这一部分是启动信号处理
    struct io_uring ring;
    struct io_uring_sqe* sqe,* stop;
    struct io_uring_cqe* cqe;
    int ring_fd;
    std::condition_variable cv;

    T result;
    std::coroutine_handle<> m_waiter;
public:
    Wait_queue();
    ~Wait_queue();
public:
    class wait {
    public:
        Wait_queue* q;
        bool await_ready() noexcept {return q->dequeue(q->result);}
        void await_suspend(std::coroutine_handle<> h) noexcept { q->m_waiter = h;};
        T await_resume() noexcept { return std::move(q->result); }
    };

    //给一个wait接口
    auto operator co_await(){return wait{this};}
    //是否准备
    bool await_ready() noexcept {return wait{this}.await_ready();}
    //唤醒协程
    void on_data_ready() noexcept;
    //阻塞等待
    void wait_for_data() noexcept;
    //阻塞唤醒
    void notify_stop() noexcept;
};


template<typename T>
void Wait_queue<T>::notify_stop() noexcept {
    io_uring_prep_nop(stop);
    io_uring_submit(&ring);
}

template<typename T>
void Wait_queue<T>::wait_for_data() noexcept {
    io_uring_wait_cqe(&ring,&cqe);
    if(cqe->user_data)
        return;
    io_uring_cqe_seen(&ring,cqe);
}

template<typename T>
void Wait_queue<T>::on_data_ready() noexcept {
//    if (!m_waiter) return;
//        auto h = std::exchange(*m_waiter, {});
//        if (h && !h.done()) h.resume();
    io_uring_prep_nop(sqe);
    io_uring_submit(&ring);
}

template<typename T>
Wait_queue<T>::~Wait_queue() {
    io_uring_queue_exit(&ring);
    m_waiter= nullptr;
    sqe = nullptr, cqe = nullptr;
}

template<typename T>
Wait_queue<T>::Wait_queue() {
    ring_fd = io_uring_queue_init(256, &ring, 0);
    if(ring_fd < 0){
        std::cerr << "create fd faild.\n";
        return;
    }

    sqe = io_uring_get_sqe(&ring);
    //这个表明是on_data_ready发出的
    sqe->user_data = 0;
    stop = io_uring_get_sqe(&ring);
    //表明是notify_all发出的
    stop->user_data = 1;
}

#endif //IO_URING_SERVER_WAIT_QUEUE_H
