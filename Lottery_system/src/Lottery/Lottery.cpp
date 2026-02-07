//
// Created by lacas on 2026/1/16.
//

#include "Lottery.h"

using namespace std;

Lottery::Lottery(std::string port): Tcp_server(port) {
    io_uring_queue_init(256,&send_ring,0);
    m_thread[0] = thread([this]{
        io_uring_cqe* cqe = nullptr;
        while(!stop.load(memory_order_relaxed)){
            io_uring_wait_cqe(&send_ring,&cqe);
            handle_send_event(cqe);
            io_uring_cqe_seen(&send_ring,cqe);
        }
    });
    m_thread[1] = thread([this]{
        while(!stop.load(memory_order_relaxed)){
            std::pair<int,int> tmp;
            if(fd_queue.dequeue(tmp)){
                string message;
                for(int i = 0;i < tmp.second;i++) {
                    bool res;
                    if (!res_queue.dequeue(res)) {
                        generate_random_result();
                        res_queue.dequeue(res);
                    }
                    if (res) message += "Y\n";
                    else message += "N\n";
                }
                submit_send_event(tmp.first,message);
            }else fd_queue.wait_for_data();
        }
    });
}

Lottery::~Lottery(){
    stop.store(true,memory_order_relaxed);
    fd_queue.notify_stop();
    io_uring_queue_exit(&ring);
    io_uring_queue_exit(&send_ring);
    for(auto& e : m_thread)
        if(e.joinable()) e.join();
}

//void Lottery::event_init() {
//    eventfd = ::eventfd(1, EFD_NONBLOCK | EFD_CLOEXEC);
//    if(eventfd <= 0){
//        cout << "eventfd init error\n";
//        exit(eventfd);
//    }
//}

void Lottery::generate_random_result() {
    // 1. 引擎：32 位 Mersenne Twister（速度快、周期长 2^19937-1）
    static std::mt19937 rng(std::random_device{}()); // 只用一次系统真随机种子

    // 2. 分布：闭区间 [1,1 << 10]
    static std::uniform_int_distribution<int> dist(1, 1 << 10);

    for(int i = 0; i < 1 << 14; i++) {
        int res = dist(rng);
        bool tmp = (res >> 10) & 1;
        res_queue.enqueue(tmp);
    }
}

void Lottery::handle_listen_event(io_uring_cqe *cqe) {
    int fd = ((cqe_data*)cqe->user_data)->fd;
    delete (cqe_data*)cqe->user_data;

//    if(fd == eventfd)
//        generate_random_result();
//    else
    if(fd == listenfd){
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int client_fd = accept(listenfd,(sockaddr*)&addr,&len);
        if(client_fd <= 0)
            return;
        fds.insert(make_pair(client_fd,addr));
        submit_listen_event(listenfd);
        submit_listen_event(client_fd);
    }
    else
        submit_receice_event(fd);
}

int Lottery::handle_receice_event(io_uring_cqe *cqe, string& message) {
    cqe_data_receice* data = (cqe_data_receice*)cqe->user_data;
    if(cqe->res <= 0){
        handle_error_event(data->fd);
        delete data;
        return -1;
    }

    int fd = data->fd;
    auto it = fd_buf.find(fd);
    if(it == fd_buf.end()) {
        fd_buf.insert(make_pair(fd, string()));
        it = fd_buf.find(fd);
    }
    else message = it->second;
    data->message.resize(cqe->res);
    message += data->message;
    delete data;

    while(1) {
        int length = 0, i = 0;
        auto check = message.find('\n');
        if(check == string::npos) {
            it->second = message;
            break;
        }

        while (1) {
            if (message[i] == '\n')
                break;
            length = length * 10 + (message[i] - '0');
            i++;
        }

        it->second = message;
        message.erase(message.begin(), message.begin() + i + 1);
        check = message.find('\n');
        if(check == string::npos) {
            break;
        }

        it->second = message.substr(length);
        message.resize(length);
        if (message == "Request : Lottery(1)\n") {
            auto tmp = make_pair(fd,1);
            fd_queue.enqueue(tmp);
            fd_queue.on_data_ready();
        }
        else if(message == "Request : Lottery(10)\n") {
            auto tmp = make_pair(fd,10);
            fd_queue.enqueue(tmp);
            fd_queue.on_data_ready();
        }
//    else
//        message = "Please send corrert Request\n";
        if(!it->second.empty()){
            message = it->second;
        } else break;
    }

    if(!it->second.empty())
        submit_receice_event(fd);
    else submit_listen_event(fd);

    return fd;
}

void Lottery::handle_error_event(int fd) {
    auto it = fds.find(fd);
    if(it != fds.end()){
        close(fd);
        fds.erase(it);
        fd_buf.erase(it->first);
//        question_times.erase(it->first);
    }
}

bool Lottery::submit_send_event(int fd, std::string message) {
    auto tmp = fds.find(fd);
    if(tmp == fds.end()) return false;
    //建立零食缓存区
    struct io_uring_sqe* sqe = io_uring_get_sqe(&send_ring);
    cqe_data* data = new cqe_data(IORING_OP_SEND,fd);
    sqe->user_data = (unsigned long long)data;
    io_uring_prep_send(sqe,fd,message.data(),message.size(),0);
    io_uring_submit(&send_ring);
    return true;
}

int Lottery::handle_send_event(io_uring_cqe *cqe) {
    cqe_data* data = (cqe_data*)cqe->user_data;
    if(cqe->res <= 0){
        handle_error_event(data->fd);
        delete data;
        return -1;
    }
    delete data;
    return cqe->res;
}


