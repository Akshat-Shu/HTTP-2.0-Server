#include "threadPool.h"
// see https://www.geeksforgeeks.org/cpp/thread-pool-in-cpp/

ThreadPool::ThreadPool(size_t n) : stop(false) {
    for (size_t i = 0; i < n; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock,
                        [this]{ return stop || !tasks.empty(); });
                    if (stop && tasks.empty()) return;
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}