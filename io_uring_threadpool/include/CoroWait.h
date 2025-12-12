//
// Created by lacas on 2025/12/9.
//

#ifndef IO_URING_SERVER_COROWAIT_H
#define IO_URING_SERVER_COROWAIT_H

//协程的基本模板
class CoroWait{
public:
    //协程基础设施
    struct promise_type{
        std::suspend_always initial_suspend(){return {};}
        std::suspend_always final_suspend() noexcept {return {};}

        auto get_return_object(){return CoroWait(std::coroutine_handle<promise_type>::from_promise(*this));}
        void unhandled_exception(){throw ;}
        void return_void(){}
    };

    bool resume(){if(handle && !handle.done()){handle.resume();return true;}return false;};
    bool done(){return handle.done();}
    std::coroutine_handle<> get_handle() const noexcept {return handle;}

    CoroWait(std::coroutine_handle<promise_type> handle):handle(handle){};
    ~CoroWait(){if(handle) handle.destroy();};

private:
    std::coroutine_handle<promise_type> handle;
};

#endif //IO_URING_SERVER_COROWAIT_H
