//
// Created by lacas on 2026/1/17.
//

#ifndef LOTTERY_SYSTEM_TCP_SERVER_H
#define LOTTERY_SYSTEM_TCP_SERVER_H

class Tcp_server{
protected:
    struct cqe_data{
        int64_t op;
        int fd;
        cqe_data(uint64_t op, int fd) : op(op), fd(fd){};
        ~cqe_data() = default;
    };
    struct cqe_data_receice{
        int64_t op;
        int fd;
        std::string message;
        cqe_data_receice(int fd):op(IORING_OP_RECV),fd(fd){message.resize(1 << 14,0);};
        ~cqe_data_receice() = default;
    };
protected:
    //eventfd仅仅只是作为随机数生成启动的信号存在，常态触发
    int listenfd;
    //保存每个客户端和监听节点的fd和ip
    std::map<int,sockaddr_in> fds;

    struct io_uring ring;
public:
    Tcp_server(std::string port);
    virtual ~Tcp_server();
    virtual void listen_init(std::string port);
    virtual bool submit_listen_event(int fd);
    virtual void test();
    virtual bool submit_send_event(int fd, std::string message);
    virtual int handle_send_event(io_uring_cqe* cqe);
    virtual void submit_receice_event(int fd);
    virtual int handle_receice_event(io_uring_cqe* cqe,std::string& message);
    virtual int handle_receice_event(io_uring_cqe* cqe);
    virtual void handle_listen_event(io_uring_cqe* cqe);
    virtual void handle_error_event(int fd);
//    virtual bool client_question();
//    virtual void work();
};

#endif //LOTTERY_SYSTEM_TCP_SERVER_H
