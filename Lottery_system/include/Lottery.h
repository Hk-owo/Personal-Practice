//
// Created by lacas on 2026/1/16.
//

#ifndef LOTTERY_SYSTEM_LOTTERY_H
#define LOTTERY_SYSTEM_LOTTERY_H

#include "Tcp_server.h"
#include "Wait_queue.h"

class Lottery : public Tcp_server{
private:
    Wait_queue<bool> res_queue;
    Wait_queue<std::pair<int,int>> fd_queue;
    std::map<int, std::string> fd_buf;
    std::map<int, std::atomic<int>> question_times;

    std::thread m_thread;
    std::atomic<bool> stop = {false};
private:
    void generate_random_result();
public:
    Lottery(std::string port);
    ~Lottery();
//    void event_init();
    void handle_listen_event(io_uring_cqe* cqe) override;
    int handle_receice_event(io_uring_cqe* cqe, std::string& message) override;
    void handle_error_event(int fd) override;
};

#endif //LOTTERY_SYSTEM_LOTTERY_H
