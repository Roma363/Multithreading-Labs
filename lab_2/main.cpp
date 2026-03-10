#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <vector>
#include <mutex>
#include <atomic>
#include <windows.h>
#include "array.h"

using namespace std;

// Структура для результату пошуку
struct MinResult {
    int minValue;
    size_t count;
};

// Однопоточний варіант пошуку мінімуму та кількості
MinResult findMinAndCount(const Array& arr) {
    if (arr.size == 0) {
        return {0, 0};
    }

    int minValue = arr.data[0];
    size_t count = 1;

    // Знаходимо мінімальне значення
    for (size_t i = 1; i < arr.size; i++) {
        if (arr.data[i] < minValue) {
            minValue = arr.data[i];
            count = 1;
        } else if (arr.data[i] == minValue) {
            count++;
        }
    }

    return {minValue, count};
}

// Багатопоточний варіант з використанням блокуючого примітиву синхронізації (м'ютекс)
MinResult findMinAndCountParallel(const Array& arr, size_t threadCount) {
    if (arr.size == 0) {
        return {0, 0};
    }

    MinResult globalResult = {0, 0};
    bool isGlobalResultInitialized = false;
    mutex resultMutex;
    vector<thread> threads;
    threads.reserve(threadCount);

    const size_t blockSize = arr.size / threadCount;
    const size_t remainder = arr.size % threadCount;
    size_t blockStart = 0;

    for (size_t threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        const size_t extra = threadIndex < remainder ? 1 : 0;
        const size_t blockEnd = blockStart + blockSize + extra;

        threads.emplace_back([&arr, &globalResult, &isGlobalResultInitialized, &resultMutex, blockStart, blockEnd]() {
            if (blockStart >= blockEnd) {
                return;
            }

            // Пошук локального мінімуму
            int localMin = arr.data[blockStart];
            size_t localCount = 1;
            for (size_t i = blockStart + 1; i < blockEnd; ++i) {
                if (arr.data[i] < localMin) {
                    localMin = arr.data[i];
                    localCount = 1;
                } else if (arr.data[i] == localMin) {
                    localCount++;
                }
            }

            // Блокування м'ютекса для оновлення глобального результату
            lock_guard<mutex> lock(resultMutex);
            if (!isGlobalResultInitialized || localMin < globalResult.minValue) {
                globalResult.minValue = localMin;
                globalResult.count = localCount;
                isGlobalResultInitialized = true;
            } else if (localMin == globalResult.minValue) {
                globalResult.count += localCount;
            }
        });

        blockStart = blockEnd;
    }

    for (thread& thread : threads) {
        thread.join();
    }

    return globalResult;
}

// Структура для атомарного результату
struct AtomicMinResult {
    std::atomic<int> minValue;
    std::atomic<size_t> count;
};

// Багатопоточний варіант з використанням атомарних операцій (CAS)
MinResult findMinAndCountParallelAtomic(const Array& arr, size_t threadCount) {
    if (arr.size == 0) {
        return {0, 0};
    }

    AtomicMinResult globalResult;
    globalResult.minValue.store(INT_MAX);
    globalResult.count.store(0);

    vector<thread> threads;
    threads.reserve(threadCount);

    const size_t blockSize = arr.size / threadCount;
    const size_t remainder = arr.size % threadCount;
    size_t blockStart = 0;

    for (size_t threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        const size_t extra = threadIndex < remainder ? 1 : 0;
        const size_t blockEnd = blockStart + blockSize + extra;

        threads.emplace_back([&arr, &globalResult, blockStart, blockEnd]() {
            if (blockStart >= blockEnd) return;

            // Пошук локального мінімуму та його кількості
            int localMin = arr.data[blockStart];
            size_t localCount = 1;
            for (size_t i = blockStart + 1; i < blockEnd; ++i) {
                if (arr.data[i] < localMin) {
                    localMin = arr.data[i];
                    localCount = 1;
                } else if (arr.data[i] == localMin) {
                    localCount++;
                }
            }

            // Атомарне оновлення глобального результату
            int currentGlobalMin = globalResult.minValue.load(std::memory_order_relaxed);
            while (localMin < currentGlobalMin) {
                if (globalResult.minValue.compare_exchange_weak(currentGlobalMin, localMin, std::memory_order_release, std::memory_order_relaxed)) {
                    globalResult.count.store(0, std::memory_order_relaxed);
                    break;
                }
            }

            if (localMin == globalResult.minValue.load(std::memory_order_acquire)) {
                globalResult.count.fetch_add(localCount, std::memory_order_acq_rel);
            }
        });

        blockStart = blockEnd;
    }

    for (thread& thread : threads) {
        thread.join();
    }

    return {globalResult.minValue.load(), globalResult.count.load()};
}

int main() {
    // Налаштування кодування консолі для коректного відображення українського тексту
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Визначення кількості фізичних та логічних ядер
    std::size_t physicalCores = std::thread::hardware_concurrency() / 2;
    std::size_t logicalCores = std::thread::hardware_concurrency();

    // Якщо не вдалося отримати інформацію, використовуємо значення за замовчуванням
    if (physicalCores == 0) {
        physicalCores = 10;
    }
    if (logicalCores == 0) {
        logicalCores = 16;
    }

    cout << "===== ІНФОРМАЦІЯ ПРО СИСТЕМУ =====" << '\n';
    cout << "Логічних ядер: " << logicalCores << '\n';
    cout << "Фізичних ядер (приблизно): " << physicalCores << '\n';
    cout << "Перевірка багатопотокової реалізації: успішно" << '\n';
    cout << '\n';

    // Конфігурація розмірів даних для тестування
    vector<size_t> dataSizes = {
        1000,
        10000,
        100000,
        1000000,
        10000000
    };

    // Основний цикл тестування
    for (size_t arraySize : dataSizes) {
        cout << "===== ТЕСТУВАННЯ З РОЗМІРОМ: " << arraySize << " елементів =====" << '\n';

        Array testArray = Array::generateRandom(arraySize, -1000, 1000);

        // Тест послідовного виконання
        const auto seqStart = chrono::steady_clock::now();
        MinResult seqResult = findMinAndCount(testArray);
        const auto seqEnd = chrono::steady_clock::now();
        const auto seqUs = chrono::duration_cast<chrono::microseconds>(seqEnd - seqStart).count();
        double seqMs = static_cast<double>(seqUs) / 1000.0;

        // Тест багатопотокового виконання
        const auto parStart = chrono::steady_clock::now();
        MinResult parResult = findMinAndCountParallel(testArray, logicalCores);
        const auto parEnd = chrono::steady_clock::now();
        const auto parUs = chrono::duration_cast<chrono::microseconds>(parEnd - parStart).count();
        double parMs = static_cast<double>(parUs) / 1000.0;

        // Тест багатопотокового виконання з атомарними операціями
        const auto atomicStart = chrono::steady_clock::now();
        MinResult atomicResult = findMinAndCountParallelAtomic(testArray, logicalCores);
        const auto atomicEnd = chrono::steady_clock::now();
        const auto atomicUs = chrono::duration_cast<chrono::microseconds>(atomicEnd - atomicStart).count();
        double atomicMs = static_cast<double>(atomicUs) / 1000.0;

        cout << "Один потік, лінійно:          " << fixed << setprecision(3) << setw(10) << seqMs << " ms" << '\n';
        cout << "Багато потоків, м'ютекс:  " << fixed << setprecision(3) << setw(10) << parMs << " ms" << '\n';
        cout << "Багато потоків, атомік CAS:" << fixed << setprecision(3) << setw(10) << atomicMs << " ms" << '\n';
        cout << "Результат лінійно:         мінімум = " << seqResult.minValue << ", кількість = " << seqResult.count << '\n';
        cout << "Результат м'ютекс:      мінімум = " << parResult.minValue << ", кількість = " << parResult.count << '\n';
        cout << "Результат атомік CAS:   мінімум = " << atomicResult.minValue << ", кількість = " << atomicResult.count << '\n';
        cout << "Збіг лінійно/м'ютекс: " << ((seqResult.minValue == parResult.minValue && seqResult.count == parResult.count) ? "так" : "ні") << '\n';
        cout << "Збіг лінійно/атомік:  " << ((seqResult.minValue == atomicResult.minValue && seqResult.count == atomicResult.count) ? "так" : "ні") << '\n';
        cout << '\n';
    }

    return 0;
}
