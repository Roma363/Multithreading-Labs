#include "queue.h"

ThreadSafeQueue::ThreadSafeQueue(std::size_t capacity)
    : capacityValue(capacity) {}

ThreadSafeQueue::PushResult ThreadSafeQueue::push(Task task) {
    std::unique_lock<std::mutex> lock(mutex);
    const auto sizeBefore = tasks.size();
    if (!accepting || stop || stopNowFlag || sizeBefore >= capacityValue) {
        return {false, false, sizeBefore};
    }
    tasks.push(std::move(task));
    const auto sizeAfter = tasks.size();
    const bool becameFull = sizeAfter >= capacityValue;
    cv.notify_one();
    return {true, becameFull, sizeAfter};
}

ThreadSafeQueue::PopResult ThreadSafeQueue::waitPop(Task &task) {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this]() {
        return stopNowFlag || (!paused && (!tasks.empty() || stop));
    });

    if (stopNowFlag) {
        return {false, tasks.size()};
    }

    if (stop && tasks.empty()) {
        return {false, tasks.size()};
    }

    if (tasks.empty()) {
        return {false, tasks.size()};
    }

    task = std::move(tasks.front());
    tasks.pop();
    return {true, tasks.size()};
}

void ThreadSafeQueue::pause() {
    std::unique_lock<std::mutex> lock(mutex);
    paused = true;
}

void ThreadSafeQueue::resume() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        paused = false;
    }
    cv.notify_all();
}

void ThreadSafeQueue::stopGraceful() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        accepting = false;
        stop = true;
        paused = false;
    }
    cv.notify_all();
}

void ThreadSafeQueue::stopNow() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        accepting = false;
        stopNowFlag = true;
        stop = true;
        paused = false;
        while (!tasks.empty()) {
            tasks.pop();
        }
    }
    cv.notify_all();
}

std::size_t ThreadSafeQueue::capacity() const {
    std::unique_lock<std::mutex> lock(mutex);
    return capacityValue;
}
