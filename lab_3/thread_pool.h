#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

class ThreadPool {
public:
    using Task = std::function<void()>;

    // Знімок статистики роботи пулу.
    struct Stats {
        std::size_t createdThreads = 0;
        std::size_t droppedTasks = 0;
        std::chrono::nanoseconds totalWaitTime{0};
        std::size_t waitSamples = 0;
        std::chrono::nanoseconds fullQueueTime{0};
        std::chrono::nanoseconds fullQueueMin{0};
        std::chrono::nanoseconds fullQueueMax{0};
    };

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
    void workerLoop(); // Головний цикл воркера.

    // Параметри та стан пулу
    static constexpr std::size_t kWorkerLimit = 6;
    std::size_t actualWorkerCount = 0;
    std::size_t capacity;
    std::array<std::thread, kWorkerLimit> workers;
    std::queue<Task> tasks;
    std::mutex mutex;
    std::condition_variable cv;
    bool stop;
    bool stopNow;
    bool paused;
    bool accepting;
    std::atomic_bool stopNowFlag{false};
    mutable std::mutex statsMutex; // Статистика та контроль повної черги.
    Stats stats;
    bool queueWasFull = false;
    std::chrono::steady_clock::time_point queueFullStart{};
};
