//
// Created by lacas on 2026/1/17.
//

#include "Tcp_server.h"
#include <algorithm>

using namespace std;

//初始化监听节点
Tcp_server::Tcp_server(string port) {
    //初始化监听节点
    listen_init(port);

    //初始化io_uring
    int err = io_uring_queue_init(256,&ring,0);
    if(err){
        cout << "io_uring_queue_init error\n";
        exit(err);
    }

    //把监听事件加入io_uring
    submit_listen_event(listenfd);
}

//初始化监听端口
void Tcp_server::listen_init(string port) {
    struct addrinfo hints,* res,*head;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;
    int err = getaddrinfo(nullptr, port.c_str(), &hints, &res);
    if(err < 0){
        cout << "getaddrinfo error\n";
        exit(err);
    }

    if(!res){
        cout << "res(addrinfo) is null\n";
        exit(0);
    }
    head = res;
    do{
        if((listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
            continue;
        if(bind(listenfd,res->ai_addr,res->ai_addrlen) == 0)
            break;
        close(listenfd);
    } while ((res = res->ai_next));

    if(!res){
        cout << "bind is error\n";
        exit(0);
    }
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    listen(listenfd,1024);

    freeaddrinfo(head);
}

Tcp_server::~Tcp_server(){
    for(auto elem : fds)
        close(elem.first);
    close(listenfd);
    io_uring_queue_exit(&ring);
}

bool Tcp_server::submit_listen_event(int fd) {
    if(listenfd <= 1) return false;
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    if(sqe == nullptr){
        cout << "listenfd sqe is null\n";
        exit(0);
    }
    cqe_data* data = new cqe_data(IORING_OP_LISTEN, fd);
    sqe->user_data = (unsigned long long)data;
    io_uring_prep_poll_add(sqe,fd,POLL_IN);
    io_uring_submit(&ring);
    return true;
}

bool Tcp_server::submit_send_event(int fd, std::string message) {
    auto tmp = fds.find(fd);
    if(tmp == fds.end()) return false;
    //建立零食缓存区
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    cqe_data* data = new cqe_data(IORING_OP_SEND,fd);
    sqe->user_data = (unsigned long long)data;
    io_uring_prep_send(sqe,fd,message.data(),message.size(),0);
    io_uring_submit(&ring);
    return true;
}

void Tcp_server::test() {
    struct io_uring_cqe* cqe;
    while(1){
        io_uring_wait_cqe(&ring,&cqe);
        uint64_t op = ((cqe_data *) cqe->user_data)->op;
        if (op == IORING_OP_SEND)
            handle_send_event(cqe);
        else if (op == IORING_OP_RECV) {
            string message;
            message.clear();
            int fd = handle_receice_event(cqe, message);
            //if(!message.empty()){
            //cout << message;
            //submit_send_event(fd,message);
            //}
            if (fd == -1) cout << "连接关闭\n";
        } else if (op == IORING_OP_LISTEN)
            handle_listen_event(cqe);
        io_uring_cqe_seen(&ring,cqe);
    }
}

void Tcp_server::submit_receice_event(int fd) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    if(sqe == nullptr){
        cout << "get sqe error";
        return;
    }
    cqe_data_receice* data = new cqe_data_receice(fd);
    sqe->user_data = (unsigned long long)data;
    io_uring_prep_recv(sqe,data->fd,data->message.data(),1 << 14,0);
    io_uring_submit(&ring);
}

//data是分配在堆的内存，使用了这个函数以后会在堆上回收,同时会重新提交监听
int Tcp_server::handle_receice_event(io_uring_cqe* cqe,string &message) {
    cqe_data_receice* data = (cqe_data_receice*)cqe->user_data;
    if(cqe->res <= 0){
        handle_error_event(data->fd);
        delete data;
        return -1;
    }

    message = move(data->message);
    message.resize(cqe->res);
    int fd = data->fd;
    delete data;

    submit_listen_event(fd);

    return fd;
}

int Tcp_server::handle_receice_event(io_uring_cqe *cqe) {
    cqe_data_receice* data = (cqe_data_receice*)cqe->user_data;
    if(cqe->res <= 0){
        handle_error_event(data->fd);
        delete data;
        return -1;
    }

    int fd = data->fd;
    delete data;

    submit_listen_event(fd);

    return fd;
}

void Tcp_server::handle_listen_event(io_uring_cqe* cqe) {
    int fd = ((cqe_data*)cqe->user_data)->fd;
    delete (cqe_data*)cqe->user_data;
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

int Tcp_server::handle_send_event(io_uring_cqe *cqe) {
    cqe_data* data = (cqe_data*)cqe->user_data;
    if(cqe->res <= 0){
        handle_error_event(data->fd);
        delete data;
        return -1;
    }
    delete data;
    return cqe->res;
}

void Tcp_server::handle_error_event(int fd) {
    auto it = fds.find(fd);
    if(it != fds.end()){
        close(fd);
        fds.erase(it);
    }
}





