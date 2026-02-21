#include <iostream>
#include <cstddef>
#include <vector>
#include <thread>
#include "test_data.cpp"

using namespace std;

struct MinMax {
    int min;
    int max;
};

// Однопоточний варіант
MinMax findMinMax(const Test1& test) {
    MinMax result{test.arr[0][0], test.arr[0][0]};
    for (std::size_t i = 0; i < test.n; i++) {
        for (std::size_t j = 0; j < test.m; j++) {
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
void minMaxWorker(const Test1& test, std::size_t startRow, std::size_t endRow, MinMax& localResult) {
    localResult = {test.arr[startRow][0], test.arr[startRow][0]};
    for (std::size_t i = startRow; i < endRow; i++) {
        // Ітерація по поточному рядку
        for (std::size_t j = 0; j < test.m; j++) {
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
MinMax findMinMaxParallel(const Test1& test, std::size_t numThreads) {
    std::vector<std::thread> threads;
    std::vector<MinMax> localResults(numThreads);

    std::size_t rowsPerThread = test.n / numThreads; // Кількість рядків на один потік
    std::size_t remainder = test.n % numThreads; // Залишок рядків
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
    Test1 test;

    // Тест послідовного виконання
    MinMax seqResult = findMinMax(test);
    cout << "Sequential result: min=" << seqResult.min << ", max=" << seqResult.max << '\n';

    // Тест паралельного виконання
    std::size_t numThreads = 2; // Кількість потоків можна змінювати
    MinMax parResult = findMinMaxParallel(test, numThreads);
    cout << "Parallel result (" << numThreads << " threads): min=" << parResult.min << ", max=" << parResult.max << '\n';

    return 0;
}