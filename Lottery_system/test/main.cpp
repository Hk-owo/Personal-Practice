//
// Created by lacas on 2026/1/28.
//

#include <csignal>
#include <sys/socket.h>
#include <netdb.h>
#include <liburing.h>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>

using namespace std;

int main(){
    struct io_uring ring;
    io_uring_queue_init(8, &ring, 0);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port   = htons(9527),
            .sin_addr   = { htonl(INADDR_LOOPBACK) }
    };
    connect(fd, (struct sockaddr *)&addr, sizeof(addr));

    auto start = std::chrono::steady_clock::now();
    int times = 0,sum = 100000,error_times = 0,rece_times = 0;
    for(int i = 0;i < sum/100;i++) {
        for(int j = 0;j < 10;j++) {
            const char *msg = "Request : Lottery(10)\n";
            string str = to_string(strlen(msg)) + '\n';
            str += msg;
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
            io_uring_prep_send(sqe, fd, str.data(), strlen(str.data()), 0);
            io_uring_submit(&ring);

            struct io_uring_cqe *cqe;
            io_uring_wait_cqe(&ring, &cqe);
            //printf("sent %d bytes\n", cqe->res);
            io_uring_cqe_seen(&ring, cqe);

            string str1;
            str1.resize(1024);
            int n = read(fd, str1.data(), 1024);

            if (n > 0) {
                str1.resize(n);
                while(1){
                    auto it = str1.find('\n');
                    if(it == string::npos) break;
                    if(str1[it - 1] == 'Y')
                        times++;
                    str1.erase(0,it + 1);
                }
                rece_times++;
            } else if (n < 0)
                error_times++;
//        cout << buf << '\n';
        }
        std::this_thread::sleep_for(std::chrono::microseconds (100));
    }

    auto end = std::chrono::steady_clock::now();
    cout << "总共抽奖" << sum << "次\n";
    cout << "抽中" << times << "次\n";
    cout << "中奖率是" << std::fixed << std::setprecision(8) << (double)times/(double)sum*100 << "%\n";
    cout <<  "错误次数是" << error_times << "次\n";
    cout << "总接收次数是" << rece_times << "次\n";
    cout << "处理时间是" << std::fixed << std::setprecision(3) << std::chrono::duration<double, std::milli>(end - start).count() << "ms\n";
    cout << "处理速度是" << std::fixed << std::setprecision(3) << (double)sum/std::chrono::duration<double>(end - start).count() << "次/秒\n";

    close(fd);
    io_uring_queue_exit(&ring);
    return 0;
}