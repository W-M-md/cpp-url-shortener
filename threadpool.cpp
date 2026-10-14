#include <iostream>
#include "threadpool.h"

ThreadPool::ThreadPool(int threads):stop(false) {
    for (int i = 0; i < threads; ++i) {
        workers.emplace_back([this]{
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);

                    condition.wait(lock, [this]{ return stop || !this->tasks.empty(); });

                    if (stop && this->tasks.empty()) return ;

                    task = std::move(this->tasks.front());

                    this->tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        stop = true;
    }

    condition.notify_all();
    for (auto& thread : workers) {
        thread.join();
    }
}

void ThreadPool::Task(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        tasks.push(std::move(task));
    }

    condition.notify_one();
}