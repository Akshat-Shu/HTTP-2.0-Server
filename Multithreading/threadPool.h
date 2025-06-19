#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <future>
#include "Utils/Logger/logger.h"

#pragma once

class Task {
public:
    std::function<void()> task;
    int weight;

    Task(std::function<void()> t, int w = 0) : task(t), weight(w) {}

    bool operator<(const Task& other) const {
        return weight < other.weight;
    }
};

class ThreadPool {
public:
    ThreadPool(size_t n = std::thread::hardware_concurrency());


    template<class F, class... Args> // function template has to be in header file
    auto enqueue(int weight, F&& f, Args&&... args)
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

            tasks.push(Task([task](){ (*task)(); }, weight));
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
    std::priority_queue<Task> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};