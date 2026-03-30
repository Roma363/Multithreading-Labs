#include "thread_pool.h"

#include <chrono>

ThreadPool::ThreadPool(std::size_t workerCount, std::size_t queueCapacity)
    : capacity(queueCapacity), stop(false), stopNow(false), paused(false), accepting(true) {
    // Запуск воркерів
    // Обмежуємо кількість воркерів максимально допустимим значенням.
    actualWorkerCount = workerCount > kWorkerLimit ? kWorkerLimit : workerCount;
    for (std::size_t i = 0; i < actualWorkerCount; ++i) {
        workers[i] = std::thread([this]() { workerLoop(); });
    }
    {
        std::unique_lock<std::mutex> lock(statsMutex);
        stats.createdThreads = actualWorkerCount;
    }
}

ThreadPool::~ThreadPool() {
    shutdown_graceful();
}

bool ThreadPool::enqueue(Task task) {
    const auto now = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(mutex);
    // Фіксація моменту, коли черга стала повною.
    if (tasks.size() >= capacity && !queueWasFull) {
        queueWasFull = true;
        queueFullStart = now;
    }
    if (!accepting || stop || stopNow || tasks.size() >= capacity) {
        {
            std::unique_lock<std::mutex> statsLock(statsMutex);
            stats.droppedTasks += 1;
        }
        return false;
    }
    tasks.push(std::move(task));
    cv.notify_one();
    return true;
}

void ThreadPool::pause() {
    std::unique_lock<std::mutex> lock(mutex);
    paused = true;
}

void ThreadPool::resume() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        paused = false;
    }
    cv.notify_all();
}

void ThreadPool::shutdown_graceful() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!accepting && stop) {
            return;
        }
        accepting = false;
        stop = true;
        paused = false;
        // Якщо черга була повною — закріплюємо статистику.
        if (queueWasFull) {
            const auto now = std::chrono::steady_clock::now();
            const auto duration = now - queueFullStart;
            std::unique_lock<std::mutex> statsLock(statsMutex);
            stats.fullQueueTime += duration;
            if (stats.fullQueueMin.count() == 0 || duration < stats.fullQueueMin) {
                stats.fullQueueMin = duration;
            }
            if (duration > stats.fullQueueMax) {
                stats.fullQueueMax = duration;
            }
            queueWasFull = false;
        }
    }
    cv.notify_all();
    for (std::size_t i = 0; i < actualWorkerCount; ++i) {
        if (workers[i].joinable()) {
            workers[i].join();
        }
    }
}

void ThreadPool::shutdown_now() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!accepting && stopNow) {
            return;
        }
        accepting = false;
        stopNow = true;
        stop = true;
        paused = false;
        // Відкидаємо всі задачі в черзі.
        while (!tasks.empty()) {
            tasks.pop();
        }
        if (queueWasFull) {
            const auto now = std::chrono::steady_clock::now();
            const auto duration = now - queueFullStart;
            std::unique_lock<std::mutex> statsLock(statsMutex);
            stats.fullQueueTime += duration;
            if (stats.fullQueueMin.count() == 0 || duration < stats.fullQueueMin) {
                stats.fullQueueMin = duration;
            }
            if (duration > stats.fullQueueMax) {
                stats.fullQueueMax = duration;
            }
            queueWasFull = false;
        }
    }
    stopNowFlag.store(true);
    cv.notify_all();
    for (std::size_t i = 0; i < actualWorkerCount; ++i) {
        if (workers[i].joinable()) {
            workers[i].join();
        }
    }
}

const std::atomic_bool &ThreadPool::stop_now_flag() const {
    return stopNowFlag;
}

ThreadPool::Stats ThreadPool::snapshot_stats() const {
    std::unique_lock<std::mutex> lock(statsMutex);
    return stats;
}

void ThreadPool::workerLoop() {
    while (true) {
        Task task;
        // Вимір часу очікування воркера.
        auto waitStart = std::chrono::steady_clock::now();
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [this]() {
                return stopNow || (!paused && (!tasks.empty() || stop));
            });

            const auto waitEnd = std::chrono::steady_clock::now();
            {
                std::unique_lock<std::mutex> statsLock(statsMutex);
                stats.totalWaitTime += (waitEnd - waitStart);
                stats.waitSamples += 1;
            }

            if (stopNow) {
                return;
            }

            if (stop && tasks.empty()) {
                return;
            }

            if (!tasks.empty() && !paused) {
                task = std::move(tasks.front());
                tasks.pop();
                // Якщо черга вже не повна — фіксуємо тривалість.
                if (queueWasFull && tasks.size() < capacity) {
                    const auto now = std::chrono::steady_clock::now();
                    const auto duration = now - queueFullStart;
                    std::unique_lock<std::mutex> statsLock(statsMutex);
                    stats.fullQueueTime += duration;
                    if (stats.fullQueueMin.count() == 0 || duration < stats.fullQueueMin) {
                        stats.fullQueueMin = duration;
                    }
                    if (duration > stats.fullQueueMax) {
                        stats.fullQueueMax = duration;
                    }
                    queueWasFull = false;
                }
            } else {
                continue;
            }
        }

        task();
    }
}
