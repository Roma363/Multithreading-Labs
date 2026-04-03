#include <atomic>
#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <windows.h>
#include <mutex>

#include "thread_pool.h"

namespace {
    std::mutex logMutex;

    void logLine(const std::string &text) {
        std::lock_guard<std::mutex> lock(logMutex);
        std::cout << text << std::endl;
    }
}

int main(int argc, char **argv) {
    // Налаштування кодування консолі для коректного відображення українського тексту
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    constexpr std::size_t workerCount = 6;
    constexpr std::size_t queueCapacity = 20;
    constexpr int producerThreads = 3;
    constexpr int tasksPerProducer = 15;
    const auto testDuration = std::chrono::seconds(25);
    const bool useGraceful = (argc > 1 && std::string(argv[1]) == "graceful");

    // Ініціалізація пулу потоків.
    ThreadPool pool(workerCount, queueCapacity);
    const auto &stopNowFlag = pool.stop_now_flag();

    // Запуск потоків, що додають задачі.
    std::array<std::thread, producerThreads> producers;

    // Окремий потік-координатор для створення producers.
    std::thread producerCoordinator([&]() {
        for (int p = 0; p < producerThreads; ++p) {
            producers[p] = std::thread([&, p]() {
                std::random_device rd;
                std::mt19937 gen(rd() ^ (static_cast<unsigned int>(p) + 0x9e3779b9));
                std::uniform_int_distribution<int> durationDist(5, 10);
                for (int i = 1; i <= tasksPerProducer; ++i) {
                    int duration = durationDist(gen);
                    int taskId = p * tasksPerProducer + i;
                    bool accepted = pool.enqueue([taskId, duration, &stopNowFlag]() {
                        {
                            std::ostringstream message;
                            message << "Задача " << taskId << " стартувала на " << duration << "с";
                            logLine(message.str());
                        }
                        for (int step = 0; step < duration * 10; ++step) {
                            if (stopNowFlag.load()) {
                                std::ostringstream message;
                                message << "Задача " << taskId << " скасована";
                                logLine(message.str());
                                return;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                        {
                            std::ostringstream message;
                            message << "Задача " << taskId << " завершена";
                            logLine(message.str());
                        }
                    });

                    if (!accepted) {
                        std::ostringstream message;
                        message << "Задача " << taskId << " відхилена (черга заповнена або зупинка)";
                        logLine(message.str());
                    }
                }
            });
        }

        for (int p = 0; p < producerThreads; ++p) {
            if (producers[p].joinable()) {
                producers[p].join();
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    logLine("Пауза пулу на 4с");
    pool.pause();
    std::this_thread::sleep_for(std::chrono::seconds(4));
    logLine("Відновлення роботи пулу");
    pool.resume();

    // Обмежене тестування за часом.
    std::this_thread::sleep_for(testDuration);
    if (useGraceful) {
        logLine("Час тесту вичерпано. Коректне завершення.");
        pool.shutdown_graceful();
    } else {
        logLine("Час тесту вичерпано. Миттєве завершення.");
        pool.shutdown_now();
    }
    if (producerCoordinator.joinable()) {
        producerCoordinator.join();
    }

    // Вивід метрик.
    const auto stats = pool.snapshot_stats();
    const auto averageWaitMs = stats.averageWaitMs;

    std::ostringstream header;
    header << "\n=== Статистика ===";
    logLine(header.str());
    {
        std::ostringstream message;
        message << "Створено потоків: " << stats.createdThreads;
        logLine(message.str());
    }
    {
        std::ostringstream message;
        message << "Відхилено задач: " << stats.droppedTasks;
        logLine(message.str());
    }
    {
        std::ostringstream message;
        message << "Середній час очікування (мс): " << averageWaitMs;
        logLine(message.str());
    }
    {
        std::ostringstream message;
        message << "Сумарний час повної черги (мс): "
                << std::chrono::duration<double, std::milli>(stats.fullQueueTime).count();
        logLine(message.str());
    }
    {
        std::ostringstream message;
        message << "Мін. час повної черги (мс): "
                << std::chrono::duration<double, std::milli>(stats.fullQueueMin).count();
        logLine(message.str());
    }
    {
        std::ostringstream message;
        message << "Макс. час повної черги (мс): "
                << std::chrono::duration<double, std::milli>(stats.fullQueueMax).count();
        logLine(message.str());
    }

    return 0;
}

