#include "thread_pool.h"

#include <chrono>

ThreadPool::ThreadPool(std::size_t workerCount, std::size_t queueCapacity)
    : actualWorkerCount(workerCount > kWorkerLimit ? kWorkerLimit : workerCount),
      queue(queueCapacity) {
    for (std::size_t i = 0; i < actualWorkerCount; ++i) {
        workers[i] = std::unique_ptr<Worker>(new Worker(queue, statsTracker));
        workers[i]->start();
    }
    statsTracker.setCreatedThreads(actualWorkerCount);
}

ThreadPool::~ThreadPool() {
    shutdown_graceful();
}

bool ThreadPool::enqueue(Task task) {
    const auto now = std::chrono::steady_clock::now();
    const auto result = queue.push(std::move(task));
    statsTracker.noteQueueFullIfNeeded(result.sizeAfter, queue.capacity(), now);
    if (!result.accepted) {
        statsTracker.recordDroppedTask();
    }
    return result.accepted;
}

void ThreadPool::pause() {
    queue.pause();
}

void ThreadPool::resume() {
    queue.resume();
}

void ThreadPool::shutdown_graceful() {
    queue.stopGraceful();
    statsTracker.finalizeQueueFullIfNeeded(std::chrono::steady_clock::now());
    for (std::size_t i = 0; i < actualWorkerCount; ++i) {
        if (workers[i]) {
            workers[i]->join();
        }
    }
}

void ThreadPool::shutdown_now() {
    stopNowFlag.store(true);
    queue.stopNow();
    statsTracker.finalizeQueueFullIfNeeded(std::chrono::steady_clock::now());
    for (std::size_t i = 0; i < actualWorkerCount; ++i) {
        if (workers[i]) {
            workers[i]->join();
        }
    }
}

const std::atomic_bool &ThreadPool::stop_now_flag() const {
    return stopNowFlag;
}

ThreadPool::Stats ThreadPool::snapshot_stats() const {
    return statsTracker.getSnapshot();
}
