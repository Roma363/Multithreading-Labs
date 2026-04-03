#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

class ThreadSafeQueue {
public:
    using Task = std::function<void()>;

    struct PushResult {
        bool accepted = false;
        bool becameFull = false;
        std::size_t sizeAfter = 0;

        PushResult() = default;
        PushResult(bool acceptedValue, bool becameFullValue, std::size_t sizeAfterValue)
            : accepted(acceptedValue), becameFull(becameFullValue), sizeAfter(sizeAfterValue) {}
    };

    struct PopResult {
        bool hasTask = false;
        std::size_t sizeAfter = 0;

        PopResult() = default;
        PopResult(bool hasTaskValue, std::size_t sizeAfterValue)
            : hasTask(hasTaskValue), sizeAfter(sizeAfterValue) {}
    };

    explicit ThreadSafeQueue(std::size_t capacity);

    PushResult push(Task task);
    PopResult waitPop(Task &task);

    void pause();
    void resume();
    void stopGraceful();
    void stopNow();

    std::size_t capacity() const;

private:
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::queue<Task> tasks;
    std::size_t capacityValue;
    bool accepting = true;
    bool stop = false;
    bool stopNowFlag = false;
    bool paused = false;
};
