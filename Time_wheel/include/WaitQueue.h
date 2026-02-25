//
// Created by lacas on 2026/2/20.
//

#ifndef TIME_WHEEL_WAITQUEUE_H
#define TIME_WHEEL_WAITQUEUE_H

/*
 * MPMC队列 多生产者单消费者
 */
template<typename T>
class TQueue{
private:
    // 队列节点
    struct node {
        T data;
        std::atomic<node*> next{nullptr};
        node():data(){}
        explicit node(T data):data(data){}
    };

    std::atomic<node*> head_;
    std::atomic<node*> tail_;
    std::atomic<size_t> size_;
public:
    TQueue();
    virtual ~TQueue();
    TQueue(const TQueue&) = delete;
    TQueue& operator=(const TQueue&) = delete;
    virtual bool enqueue(T& task);
    virtual bool dequeue(T& task);
    size_t size() const;
};

template<typename T>
TQueue<T>::~TQueue() {
    while (node* old_head = head_.load(std::memory_order_relaxed)) {
        head_.store(old_head->next.load(std::memory_order_relaxed), std::memory_order_relaxed);
        delete old_head;
    }
}

template<typename T>
size_t TQueue<T>::size() const {
    return size_.load(std::memory_order_acquire);
}

template<typename T>
TQueue<T>::TQueue() {
    // 创建哨兵节点
    node* dummy = new node();
    head_.store(dummy, std::memory_order_relaxed);
    tail_.store(dummy, std::memory_order_relaxed);
    size_.store(0, std::memory_order_relaxed);
}

template<typename T>
bool TQueue<T>::enqueue(T& task) {
    node* new_node = new node();
    new_node->data = std::move(task);

    node* tail = tail_.load(std::memory_order_acquire);  //读取 tail

    while (true) {
        node* next = tail->next.load(std::memory_order_acquire);

        if (next == nullptr) {
            // 尝试将新节点链接到 tail
            if (tail->next.compare_exchange_weak(next, new_node, std::memory_order_release, std::memory_order_relaxed)) {
                //更新 tail
                tail_.compare_exchange_strong(tail, new_node, std::memory_order_release, std::memory_order_relaxed);
                size_.fetch_add(1,std::memory_order_relaxed);
                return true;
            }
            // 失败：next 被其他线程更新了，重试
        } else {
            // 帮助推进 tail
            tail_.compare_exchange_weak(tail, next, std::memory_order_release, std::memory_order_relaxed);
            tail = tail_.load(std::memory_order_acquire);
        }
    }
}

template<typename T>
bool TQueue<T>::dequeue(T &task) {
    node* old_head = head_.load(std::memory_order_acquire);
    while (true) {
        node* next = old_head->next.load(std::memory_order_acquire);
        // 队列为空
        if (next == nullptr) {
            return false;
        }
        // 尝试移动 head
        if (head_.compare_exchange_weak(old_head, next, std::memory_order_release, std::memory_order_acquire)) {
            // 成功获取节点
            task = std::move(next->data);
            delete old_head;
            size_.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
        old_head = head_.load(std::memory_order_acquire);
    }
}


/*
 * 有uring后缀的是用io_uring来实现的阻塞等待队列
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
    Wait_queue();
    ~Wait_queue();

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
};

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

#endif //TIME_WHEEL_WAITQUEUE_H
