#include <atomic>
#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>

#include "thread_pool.h"

int main(int argc, char **argv) {
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
    std::atomic_bool producersDone{false};
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
                        std::cout << "Задача " << taskId << " стартувала на " << duration << "с" << std::endl;
                        for (int step = 0; step < duration * 10; ++step) {
                            if (stopNowFlag.load()) {
                                std::cout << "Задача " << taskId << " скасована" << std::endl;
                                return;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                        std::cout << "Задача " << taskId << " завершена" << std::endl;
                    });

                    if (!accepted) {
                        std::cout << "Задача " << taskId << " відхилена (черга заповнена або зупинка)"
                                  << std::endl;
                    }
                }
            });
        }

        for (int p = 0; p < producerThreads; ++p) {
            if (producers[p].joinable()) {
                producers[p].join();
            }
        }
        producersDone.store(true);
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Пауза пулу на 4с" << std::endl;
    pool.pause();
    std::this_thread::sleep_for(std::chrono::seconds(4));
    std::cout << "Відновлення роботи пулу" << std::endl;
    pool.resume();

    // Обмежене тестування за часом.
    std::this_thread::sleep_for(testDuration);
    if (useGraceful) {
        std::cout << "Час тесту вичерпано. Коректне завершення." << std::endl;
        pool.shutdown_graceful();
    } else {
        std::cout << "Час тесту вичерпано. Миттєве завершення." << std::endl;
        pool.shutdown_now();
    }
    if (producerCoordinator.joinable()) {
        producerCoordinator.join();
    }

    // Вивід метрик.
    const auto stats = pool.snapshot_stats();
    const auto averageWaitMs = stats.waitSamples == 0
        ? 0.0
        : std::chrono::duration<double, std::milli>(stats.totalWaitTime).count() / stats.waitSamples;

    std::cout << "\n=== Статистика ===" << std::endl;
    std::cout << "Створено потоків: " << stats.createdThreads << std::endl;
    std::cout << "Відхилено задач: " << stats.droppedTasks << std::endl;
    std::cout << "Середній час очікування (мс): " << averageWaitMs << std::endl;
    std::cout << "Сумарний час повної черги (мс): "
              << std::chrono::duration<double, std::milli>(stats.fullQueueTime).count() << std::endl;
    std::cout << "Мін. час повної черги (мс): "
              << std::chrono::duration<double, std::milli>(stats.fullQueueMin).count() << std::endl;
    std::cout << "Макс. час повної черги (мс): "
              << std::chrono::duration<double, std::milli>(stats.fullQueueMax).count() << std::endl;

    return 0;
}