#include "stats_tracker.h"

void StatsTracker::setCreatedThreads(std::size_t count) {
    std::unique_lock<std::mutex> lock(mutex);
    stats.createdThreads = count;
}

void StatsTracker::recordWaitTime(std::chrono::nanoseconds duration, std::size_t samples) {
    if (samples == 0) {
        return;
    }
    const double durationMs = std::chrono::duration<double, std::milli>(duration).count();
    std::unique_lock<std::mutex> lock(mutex);
    for (std::size_t i = 0; i < samples; ++i) {
        stats.waitSamples += 1;
        const double delta = durationMs - stats.averageWaitMs;
        stats.averageWaitMs += delta / static_cast<double>(stats.waitSamples);
    }
}

void StatsTracker::recordDroppedTask() {
    std::unique_lock<std::mutex> lock(mutex);
    stats.droppedTasks += 1;
}

void StatsTracker::noteQueueFullIfNeeded(std::size_t size, std::size_t capacity,
                                         std::chrono::steady_clock::time_point now) {
    if (capacity == 0) {
        return;
    }
    std::unique_lock<std::mutex> lock(mutex);
    if (size >= capacity && !queueWasFull) {
        queueWasFull = true;
        queueFullStart = now;
    }
}

void StatsTracker::noteQueueNotFullIfNeeded(std::size_t size, std::size_t capacity,
                                            std::chrono::steady_clock::time_point now) {
    if (capacity == 0) {
        return;
    }
    std::unique_lock<std::mutex> lock(mutex);
    if (queueWasFull && size < capacity) {
        recordQueueFull(now - queueFullStart);
        queueWasFull = false;
    }
}

void StatsTracker::finalizeQueueFullIfNeeded(std::chrono::steady_clock::time_point now) {
    std::unique_lock<std::mutex> lock(mutex);
    if (queueWasFull) {
        recordQueueFull(now - queueFullStart);
        queueWasFull = false;
    }
}

StatsTracker::Stats StatsTracker::getSnapshot() const {
    std::unique_lock<std::mutex> lock(mutex);
    return stats;
}

void StatsTracker::recordQueueFull(std::chrono::nanoseconds duration) {
    if (duration.count() < 0) {
        duration = std::chrono::nanoseconds::zero();
    }
    stats.fullQueueTime += duration;
    if (stats.fullQueueMin.count() == 0 || duration < stats.fullQueueMin) {
        stats.fullQueueMin = duration;
    }
    if (duration > stats.fullQueueMax) {
        stats.fullQueueMax = duration;
    }
}
