#include "worker.h"

Worker::Worker(ThreadSafeQueue &queueRef, StatsTracker &statsRef)
    : queue(queueRef), stats(statsRef) {}

void Worker::start() {
    thread = std::thread([this]() { run(); });
}

void Worker::join() {
    if (thread.joinable()) {
        thread.join();
    }
}

void Worker::run() {
    while (true) {
        ThreadSafeQueue::Task task;
        const auto waitStart = std::chrono::steady_clock::now();
        const auto popResult = queue.waitPop(task);
        const auto waitEnd = std::chrono::steady_clock::now();

        stats.recordWaitTime(waitEnd - waitStart, 1);

        if (!popResult.hasTask) {
            break;
        }

        stats.noteQueueNotFullIfNeeded(popResult.sizeAfter, queue.capacity(), waitEnd);
        task();
    }

}
