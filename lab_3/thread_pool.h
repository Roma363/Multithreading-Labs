#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>

#include "stats_tracker.h"
#include "queue.h"
#include "worker.h"

class ThreadPool {
public:
    using Task = std::function<void()>;

    using Stats = StatsTracker::Stats;

    ThreadPool(std::size_t workerCount, std::size_t queueCapacity); // Створює пул із фіксованою кількістю воркерів та розміром черги.
    ~ThreadPool(); // Гарантоване коректне завершення.

    bool enqueue(Task task); // Додає задачу в кінець черги або відхиляє.
    void pause(); // Тимчасова пауза обробки задач воркерами.
    void resume(); // Відновлення обробки задач воркерами.
    void shutdown_graceful(); // Коректне завершення з виконанням активних задач.
    void shutdown_now(); // Миттєве завершення з відкиданням черги.

    const std::atomic_bool &stop_now_flag() const; // Прапорець для кооперативного скасування задач.
    Stats snapshot_stats() const; // Отримати поточну статистику.

private:
    // Параметри та стан пулу
    static constexpr std::size_t kWorkerLimit = 6;
    std::size_t actualWorkerCount = 0;
    std::array<std::unique_ptr<Worker>, kWorkerLimit> workers;
    ThreadSafeQueue queue;
    StatsTracker statsTracker;
    std::atomic_bool stopNowFlag{false};
};
