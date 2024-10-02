#ifndef COCOS_SLEEP
#define COCOS_SLEEP
#include "eventloop.hpp"
#include <chrono>
#include <coroutine>
namespace cocos {
    inline TimePoint now() { return std::chrono::steady_clock::now(); }
    struct Sleep {
        std::chrono::time_point<std::chrono::steady_clock> awake_time;
        /**
         * @brief If the time to awake is already passed, just resume.
         * 
         * @return true It should resume immediately.
         * @return false It should wait.
         */
        bool await_ready() const {
            return awake_time <= now();
        }
        /**
         * @brief Delay the sleeping coroutine until the awake time.
         * 
         * @param hdl the sleeping coroutine.
         */
        void await_suspend(std::coroutine_handle<> hdl) {
            EventLoop::get_loop().add_delayed_task(hdl, awake_time);
        }
        /**
         * @brief Sleep returns no value.
         * 
         */
        void await_resume() {}
    };
    
    inline Sleep sleep_until(TimePoint time) {
        return {time};
    }
    inline Sleep sleep_for(Duration duration) {
        return sleep_until(now() + duration);
    }
    inline Sleep sleep(TimePoint time) {
        return {time};
    }
    inline Sleep sleep(Duration duration) {
        return sleep_until(now() + duration);
    }
}
#endif // COCOS_SLEEP