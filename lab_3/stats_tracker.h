#pragma once

#include <chrono>
#include <mutex>

class StatsTracker {
public:
    struct Stats {
        std::size_t createdThreads = 0;
        std::size_t droppedTasks = 0;
        double averageWaitMs = 0.0;
        std::size_t waitSamples = 0;
        std::chrono::nanoseconds fullQueueTime{0};
        std::chrono::nanoseconds fullQueueMin{0};
        std::chrono::nanoseconds fullQueueMax{0};
    };

    void setCreatedThreads(std::size_t count);
    void recordWaitTime(std::chrono::nanoseconds duration, std::size_t samples);
    void recordDroppedTask();

    void noteQueueFullIfNeeded(std::size_t size, std::size_t capacity,
                               std::chrono::steady_clock::time_point now);
    void noteQueueNotFullIfNeeded(std::size_t size, std::size_t capacity,
                                  std::chrono::steady_clock::time_point now);
    void finalizeQueueFullIfNeeded(std::chrono::steady_clock::time_point now);

    Stats getSnapshot() const;

private:
    void recordQueueFull(std::chrono::nanoseconds duration);

    mutable std::mutex mutex;
    Stats stats;
    bool queueWasFull = false;
    std::chrono::steady_clock::time_point queueFullStart{};
};
