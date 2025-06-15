#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <future>
#include "Utils/Logger/logger.h"


class ThreadPool {
public:
    ThreadPool(size_t n = std::thread::hardware_concurrency()) : stop(false) {}


    template<class F, class... Args> // function template has to be in header file
    auto ThreadPool::enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                Logger::error("enqueue on stopped ThreadPool");
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            tasks.emplace([task](){ (*task)(); });
        }
        condition.notify_one();
        return res;
    }


    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker: workers)
            worker.join();
}

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};