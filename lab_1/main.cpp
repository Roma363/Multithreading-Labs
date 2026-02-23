#include <iostream>
#include <cstddef>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <windows.h>
#include "matrix.h"

using namespace std;

struct MinMax {
    int min;
    int max;
};

// Однопоточний варіант
MinMax findMinMax(const Matrix& test) {
    MinMax result{test.arr[0][0], test.arr[0][0]};
    for (std::size_t i = 0; i < test.rows; i++) {
        for (std::size_t j = 0; j < test.cols; j++) {
            int value = test.arr[i][j];
            if (value < result.min) {
                result.min = value;
            }
            if (value > result.max) {
                result.max = value;
            }
        }
    }
    return result;
}

// Функція, яку буде виконувати кожен потік
void minMaxWorker(const Matrix& test, std::size_t startRow, std::size_t endRow, MinMax& localResult) {
    localResult = {test.arr[startRow][0], test.arr[startRow][0]};
    for (std::size_t i = startRow; i < endRow; i++) {
        // Ітерація по поточному рядку
        for (std::size_t j = 0; j < test.cols; j++) {
            int value = test.arr[i][j]; // Поточний елемент
            if (value < localResult.min) {
                localResult.min = value;
            }
            if (value > localResult.max) {
                localResult.max = value;
            }
        }
    }
}

// Паралельний варіант
MinMax findMinMaxParallel(const Matrix& test, std::size_t numThreads) {
    std::vector<std::thread> threads;
    std::vector<MinMax> localResults(numThreads);

    std::size_t rowsPerThread = test.rows / numThreads; // Кількість рядків на один потік
    std::size_t remainder = test.rows % numThreads; // Залишок рядків
    std::size_t currentRow = 0; // Поточний рядок

    // Розподіл рядків масиву між потоками
    for (std::size_t i = 0; i < numThreads; i++) {
        std::size_t startRow = currentRow;
        std::size_t endRow = startRow + rowsPerThread + (i < remainder ? 1 : 0);
        currentRow = endRow;

        if (startRow < endRow) {
            threads.emplace_back(minMaxWorker, std::ref(test), startRow, endRow, std::ref(localResults[i]));
        } else {
            // Якщо для потоку не залишилося рядків
            localResults[i] = {test.arr[0][0], test.arr[0][0]};
        }
    }

    // Очікування завершення всіх потоків
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Збір локальних результатів
    MinMax globalResult = localResults[0];
    for (std::size_t i = 1; i < numThreads; i++) {
        if (localResults[i].min < globalResult.min) {
            globalResult.min = localResults[i].min;
        }
        if (localResults[i].max > globalResult.max) {
            globalResult.max = localResults[i].max;
        }
    }

    return globalResult;
}

int main() {
    // Налаштування кодування консолі для коректного відображення українського тексту
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Визначення кількості фізичних та логічних ядер
    std::size_t physicalCores = std::thread::hardware_concurrency() / 2;
    std::size_t logicalCores = std::thread::hardware_concurrency();

    // Якщо не вдалося отримати інформацію, використовуємо значення з протоколу
    if (physicalCores == 0) {
        physicalCores = 10;
    }
    if (logicalCores == 0) {
        logicalCores = 16;
    }

    cout << "===== ІНФОРМАЦІЯ ПРО СИСТЕМУ =====" << '\n';
    cout << "Логічних ядер: " << logicalCores << '\n';
    cout << "Фізичних ядер (приблизно): " << physicalCores << '\n';
    cout << '\n';

    // Конфігурація кількостей потоків для тестування
    std::vector<std::size_t> threadConfigs = {
        physicalCores / 2,           // 2-рази менше, ніж фізичних ядер
        physicalCores,               // Рівно фізичним ядрам
        logicalCores,                // Рівно логічним ядрам
        logicalCores * 2,            // В 2 рази більше, ніж логічних ядер
        logicalCores * 4,            // В 4 рази більше
        logicalCores * 8,            // В 8 разів більше
        logicalCores * 16            // В 16 разів більше
    };

    // Конфігурація розмірів даних для тестування
    std::vector<std::pair<std::size_t, std::size_t>> dataSizes = {
        {100, 10},
        {100, 100},
        {1000, 100},
        {1000, 1000},
        {1000, 10000},

    };

    // Основний цикл тестування
    for (const auto& [rows, cols] : dataSizes) {
        cout << "===== ТЕСТУВАННЯ З РОЗМІРНІСТЮ: " << rows << "x" << cols << " (елементів: " << (rows * cols) << ") =====" << '\n';

        Matrix test = Matrix::generateRandom(rows, cols, -1000, 1000);

        // Тест послідовного виконання
        const auto seqStart = std::chrono::steady_clock::now();
        MinMax seqResult = findMinMax(test);
        const auto seqEnd = std::chrono::steady_clock::now();
        const auto seqUs = std::chrono::duration_cast<std::chrono::microseconds>(seqEnd - seqStart).count();
        double seqMs = seqUs / 1000.0;

        cout << "Послідовне виконання: " << fixed << setprecision(3) << setw(10) << seqMs << " ms"  << '\n';

        // Тест паралельного виконання з різною кількістю потоків
        for (std::size_t numThreads : threadConfigs) {
            const auto parStart = std::chrono::steady_clock::now();
            MinMax parResult = findMinMaxParallel(test, numThreads);
            const auto parEnd = std::chrono::steady_clock::now();
            const auto parUs = std::chrono::duration_cast<std::chrono::microseconds>(parEnd - parStart).count();
            double parMs = parUs / 1000.0;

            double speedup = seqMs / parMs;
            cout << "Потоків: " << setw(2) << numThreads << " | Час: " << fixed << setprecision(3) << setw(10) << parMs << " ms | "
                 << "Прискорення: " << setw(6) << fixed << setprecision(3) << speedup << "x";

            // Перевірка коректності результатів
            if (parResult.min == seqResult.min && parResult.max == seqResult.max) {
                cout << " [OK]" << '\n';
            } else {
                cout << " [ПОМИЛКА!]" << '\n';
            }
        }

        cout << '\n';
    }

    return 0;
}
