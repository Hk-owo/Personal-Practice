//
// Created by lacas on 2025/12/10.
//

#ifndef IO_URING_SERVER_WAIT_QUEUE_H
#define IO_URING_SERVER_WAIT_QUEUE_H

#include "TQueue.h"
#include "CoroWait.h"

/*
 * 没做多线程竞争冗余
 * 多消费者会free句柄多次
 * 目前只做SPSC的单消费者
 * 有uring后缀的是用io_uring来实现的阻塞等待队列
 * 还有就是协程和uring混合实现的阻塞等待队列
 * 最后就是完全由协程实现的消息队列，但是需要一个而外的不会终止线程来处理，因为协程挂起以后线程会直接继续执行后面的内容，导致线程提前结束，可以把一个线程当两个使用来解决
 */
template<typename T>
class Wait_queue : public TQueue<T>{
private:
    static const int SIZE = 128;
    //这一部分是启动信号处理
    struct io_uring ring;
    struct io_uring_cqe* cqe;
    int ring_fd, peek_times = 1;
    std::atomic<bool> suspend{true};

    T result{};
public:
    class Waiter{
    private:
        Wait_queue* q;
    public:
        Waiter(Wait_queue* queue): q(queue){};
        bool await_ready() { return q->dequeue(q->result);}
        void await_suspend(std::coroutine_handle<> h){q->m_waiter.store(h,std::memory_order_relaxed);}
        T await_resume(){ return std::exchange(q->result,{}); }
    };
private:
    std::atomic<std::coroutine_handle<>> m_waiter{nullptr};
public:
    Wait_queue();
    ~Wait_queue();

    //给一个wait接口
    auto operator co_await(){return Waiter{this};}
    //唤醒
    void on_data_ready_uring() noexcept;
    //等待
    void wait_for_data_uring() noexcept;
    //阻塞唤醒
    void notify_stop_uring() noexcept;
    //非阻塞查看
    void peek_uring() noexcept;
    //阻塞等待
    void wait_uring() noexcept;
    bool dequeue(T& task);

    //唤醒协程
    void on_data_ready() noexcept;
    //阻塞唤醒
    void notify_stop() noexcept;
    //阻塞等待协程
    void wait_for_data() noexcept;
};

template<typename T>
void Wait_queue<T>::wait_for_data() noexcept {
    if(!(peek_times & ((1 << 18) - 1 ))){
        peek_times = 1;
        wait_uring();
    }
    else peek_uring();
}

template<typename T>
void Wait_queue<T>::notify_stop() noexcept {
    notify_stop_uring();
}

template<typename T>
void Wait_queue<T>::on_data_ready() noexcept {
    std::coroutine_handle<> h = m_waiter.exchange(nullptr);
    if (h && !h.done()) {
        io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        sqe->user_data = 0;
        io_uring_prep_nop(sqe);
        io_uring_submit(&ring);
    }
}

template<typename T>
void Wait_queue<T>::wait_uring() noexcept {
    io_uring_wait_cqe(&ring,&cqe);
    if(cqe->user_data)
        return;
    io_uring_cqe_seen(&ring,cqe);
}

template<typename T>
bool Wait_queue<T>::dequeue(T &task) {
    bool res = TQueue<T>::dequeue(task);
    if(!res)
        peek_times++;
    else peek_times = 1;
    return res;
}

template<typename T>
void Wait_queue<T>::peek_uring() noexcept {
    cqe = nullptr;
    io_uring_peek_cqe(&ring,&cqe);
    if(cqe)
        io_uring_cqe_seen(&ring,cqe);
}

template<typename T>
void Wait_queue<T>::notify_stop_uring() noexcept {
    io_uring_sqe* stop = io_uring_get_sqe(&ring);
    stop->user_data = 1;
    io_uring_prep_nop(stop);
    io_uring_submit(&ring);
}

template<typename T>
void Wait_queue<T>::wait_for_data_uring() noexcept {
    suspend.store(true,std::memory_order_release);
    if(!(peek_times & ((1 << 18) - 1 ))){
        peek_times = 1;
        wait_uring();
    }
    else peek_uring();
    suspend.store(false,std::memory_order_release);
}

template<typename T>
void Wait_queue<T>::on_data_ready_uring() noexcept {
    if(suspend.load(std::memory_order_acquire)) {
        io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        sqe->user_data = 0;
        io_uring_submit(&ring);
    }
}

template<typename T>
Wait_queue<T>::~Wait_queue() {
    io_uring_queue_exit(&ring);
    cqe = nullptr;
}

template<typename T>
Wait_queue<T>::Wait_queue() {
    ring_fd = io_uring_queue_init(16, &ring, 0);
    if(ring_fd < 0){
        std::cerr << "create fd faild.\n";
        return;
    }
}

#endif //IO_URING_SERVER_WAIT_QUEUE_H
