#include <iostream>
#include <cstddef>
#include <vector>
#include <thread>
#include <chrono>
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
    const std::size_t rows = 1000;
    const std::size_t cols = 5000;
    Matrix test = Matrix::generateRandom(rows, cols, -1000, 1000);

    // Тест послідовного виконання
    const auto seqStart = std::chrono::steady_clock::now();
    MinMax seqResult = findMinMax(test);
    const auto seqEnd = std::chrono::steady_clock::now();
    const auto seqUs = std::chrono::duration_cast<std::chrono::microseconds>(seqEnd - seqStart).count();
    cout << "Sequential result: min=" << seqResult.min << ", max=" << seqResult.max
         << ", time=" << seqUs << " us" << '\n';

    // Тест паралельного виконання
    std::size_t numThreads = 16; // Кількість потоків можна змінювати
    const auto parStart = std::chrono::steady_clock::now();
    MinMax parResult = findMinMaxParallel(test, numThreads);
    const auto parEnd = std::chrono::steady_clock::now();
    const auto parUs = std::chrono::duration_cast<std::chrono::microseconds>(parEnd - parStart).count();
    cout << "Parallel result (" << numThreads << " threads): min=" << parResult.min << ", max=" << parResult.max
         << ", time=" << parUs << " us" << '\n';

    return 0;
}