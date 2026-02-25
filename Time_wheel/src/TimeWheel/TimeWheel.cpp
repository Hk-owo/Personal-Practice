//
// Created by lacas on 2026/2/20.
//

#include "TimeWheel.h"

using namespace std;

TimeWheelTop::TimeWheelTop() : TimeWheelBasics() {
    io_uring_queue_init(2,&ring,0);

    //初始化子时间轮
    m_sub_wheel.emplace_back((uint)mask::MILLISECOND,0,m_queue);
    m_sub_wheel.emplace_back((uint)mask::SECOND,10,m_queue);
    m_sub_wheel.emplace_back((uint)mask::MINUTE,16,m_queue);
    m_sub_wheel.emplace_back((uint)mask::HOUR,22,m_queue);

    m_thread.resize(5);
    //0号线程是处理指针推进的
    m_thread[0] = std::thread([this]{
        io_uring_cqe* cqe = nullptr;
        struct __kernel_timespec ts = {.tv_sec = 0,.tv_nsec = 1'000'000};
        uint start = chrono::duration_cast<chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        uint times = 0;
        //开始正常推进
        const uint t1 = 1000,t2 = 60 * 1000,t3 = 60 * 60 * 1000;
        while(!stop) {
            io_uring_wait_cqe_timeout(&ring, &cqe, &ts);
            do {
                now_index++;
                if (now_index >= t3) now_index -= t3;
                m_sub_wheel[0].advance();
                if ((now_index % t3) == 0) m_sub_wheel[3].advance(m_sub_wheel[2]);
                if ((now_index % t2) == 0) m_sub_wheel[2].advance(m_sub_wheel[1]);
                if ((now_index % t1) == 0) m_sub_wheel[1].advance(m_sub_wheel[0]);
            }while(now_index < times);
            //提供推进追赶机制
            times = ( chrono::duration_cast<chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() - start );
            times %= (60 * 60 * 1000);
        }
        finish[0] = true;
    });
    //进行任务处理的
    for(int i = 1;i < 5;i++) {
        m_thread[i] = std::thread([i, this] {
            std::function<void()> task = nullptr;
            while (!stop) {
                if (m_queue.dequeue(task)) {
                    if (task) task();
                } else m_queue.wait_for_data_uring();
                task = nullptr;
            }
            finish[i] = true;
        });
    }
}

TimeWheelTop::~TimeWheelTop() {
    io_uring_queue_exit(&ring);
    stop = true;
    for(int i = 0;i < m_thread.size();i++){
        m_queue.notify_stop_uring();
        if(finish[i]) {
            if (m_thread[i].joinable())
                m_thread[i].join();
        }
        else i--;
    }
}

void TimeWheelTop::add_task(function<void()> task, TimeWheelBasics::tv tv) {
    tv.millisecond += m_sub_wheel[0].now_index;
    tv.second += m_sub_wheel[1].now_index;
    tv.minute += m_sub_wheel[2].now_index;
    if(tv.millisecond >= 1000) {
        tv.second += tv.millisecond / 1000;
        tv.millisecond %= 1000;
    }
    if(tv.second >= 60) {
        tv.minute += tv.second / 60;
        tv.second %= 60;
    }
    if(tv.minute >= 60) {
        tv.hour += tv.minute / 60;
        tv.minute %= 60;
    }
    //在对hour修正坐标前确定没有溢出
    if(tv.hour >= 512) {
        cout << "limit 511 hour\n";
        return;
    }
    tv.hour += m_sub_wheel[3].now_index;
    tv.hour &= 511;
    //如果时间间隔小于3ms，直接加入清除队列，避免在处理任务的过程中同时进行插入任务导致读写重合
    //对于位保存，3ms = 0x00000003
    uint run_time = ((tv.millisecond & 1023) | ((tv.second & 63) << 10) | ((tv.minute & 63) << 16) | ((tv.hour & 511) << 22));
    if(run_time < 2) {
        m_queue.enqueue(task);
        m_queue.on_data_ready_uring();
    }
    else{
        if(tv.hour) m_sub_wheel[3].add_task(task,run_time);
        else if(tv.minute) m_sub_wheel[2].add_task(task,run_time);
        else if(tv.second) m_sub_wheel[1].add_task(task,run_time);
        else m_sub_wheel[0].add_task(task,run_time);
    }
}

//进行为时间轮的指针移动，并且把任务下交给其他时间轮
void TimeWheel::advance(TimeWheel& tw) {
    now_index++;
    if(now_index >= size) now_index -= size;
    if(m_task[now_index].empty())
        return;
    for(auto& elem : m_task[now_index]){
        if(!elem.time) {
            m_queue->enqueue(elem.task);
            m_queue->on_data_ready_uring();
        }
        else
            tw.add_task(elem.task,elem.time);
    }
    m_task[now_index].clear();
}

void TimeWheel::add_task(function<void()> task, uint run_time) {
    uint time = 0;
    //通过压缩以后的运行时间和掩码位运算然后位移来获取当前时间轮的时间
    time = (run_time & m_mask) >> offset;
    run_time &= m_xmask;
    m_task[time].emplace_back(task,run_time);
}

void TimeWheel::advance() {
    now_index++;
    if(now_index >= size) now_index -= size;
    if(m_task[now_index].empty())
        return;
    for(auto& elem : m_task[now_index]){
        m_queue->enqueue(elem.task);
        m_queue->on_data_ready_uring();
    }
    m_task[now_index].clear();
}
