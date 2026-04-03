#pragma once

#include <atomic>
#include <chrono>
#include <thread>

#include "stats_tracker.h"
#include "queue.h"

class Worker {
public:
    Worker(ThreadSafeQueue &queue, StatsTracker &stats);

    void start();
    void join();

private:
    void run();

    ThreadSafeQueue &queue;
    StatsTracker &stats;
    std::thread thread;
};
