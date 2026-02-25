//
// Created by lacas on 2026/2/20.
//

#ifndef TIME_WHEEL_TIMEWHEEL_H
#define TIME_WHEEL_TIMEWHEEL_H

#include "WaitQueue.h"
class TimeWheel;

class TimeWheelBasics{
public:
    //暴露给用户进行一个时间的输入
    struct tv{
        uint hour = 0;
        uint minute = 0;
        uint second = 0;
        uint millisecond = 0;
    };
protected:
    //这里是时间的位掩码
    enum class mask{
        MILLISECOND = 1023,//10位
        SECOND = (63 << 10),//6位
        MINUTE = (63 << 16),//6位
        HOUR = (511u << 22)//9位(511)
    };
    enum class xmask{
        MILLISECOND = 0,//10位
        SECOND = 1023,//6位
        MINUTE = (63 << 10) | 1023,//6位
        HOUR = (63 << 16) | (63 << 10) | 1023//9位(511)
    };
    class node{
    public:
        std::function<void()> task;
        uint time = 0;
        node():task(nullptr){}
        node(std::function<void()> task, struct tv t):task(std::move(task)){ time = (t.millisecond & 1023) | ((t.second & 63) << 10) | ((t.minute & 63) << 16) | ((t.hour & 511) << 22); }
        node(std::function<void()> task, uint time):task(std::move(task)),time(time){}
    };
    //时间轮的队列长度
    const int size = 1 << 12;
    //记录当前推进的指针位置,推进的时候是先加法再处理
    uint now_index;
public:
    TimeWheelBasics():now_index(0){};
    virtual ~TimeWheelBasics() = default;
};

class TimeWheelTop : public TimeWheelBasics {
private:
    //时间轮的定时器
    struct io_uring ring;
    //时间轮子组件,从小到大分别是ms,s,min,h
    std::vector<TimeWheel> m_sub_wheel;
    //保存时间轮的两个进程，一个推进指针，一个处理任务
    std::vector<std::thread> m_thread;
    //清除任务的队列
    Wait_queue<std::function<void()>> m_queue;
    //两个线程的完成标志
    bool finish[5]{false, false, false, false, false};
    //停止标志
    bool stop{false};
public:
    TimeWheelTop();
    ~TimeWheelTop() override;
    void add_task(std::function<void()> task , tv tv);
};

class TimeWheel : public TimeWheelBasics{
    friend TimeWheelTop;
private:
    //任务队列
    std::vector<std::list<node>> m_task;
    //当前队列的掩码
    uint m_mask = 0;
    uint m_xmask = 0;
    //时间掩码的偏移
    uint offset = 0;
    //来自顶层实体的任务队列
     Wait_queue<std::function<void()>>* m_queue = nullptr;
     size_t size = 0;
public:
    TimeWheel(uint mask, uint offset, Wait_queue<std::function<void()>>& queue):m_mask(mask),offset(offset),m_queue(&queue){
        if(mask == (uint)mask::MILLISECOND) {
            size = 1000;
            m_xmask = (uint)xmask::MILLISECOND;
        }
        else if(mask == (uint)mask::SECOND) {
            size = 60;
            m_xmask = (uint)xmask::SECOND;
        }
        else if(mask == (uint)mask::MINUTE) {
            size = 60;
            m_xmask = (uint)xmask::MINUTE;
        }
        else if(mask == (uint)mask::HOUR) {
            size = 512;
            m_xmask = (uint)xmask::HOUR;
        }
        m_task.resize(size);
    };
    ~TimeWheel() = default;
    void advance(TimeWheel& tw);
    void advance();
    void add_task(std::function<void()> task, uint run_time);
};

#endif //TIME_WHEEL_TIMEWHEEL_H
