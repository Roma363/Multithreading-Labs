#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <vector>
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
        double seqMs = seqUs / 1000.0;

        cout << "Час виконання: " << fixed << setprecision(3) << setw(10) << seqMs << " ms"  << '\n';
        cout << "Результат: мінімум = " << seqResult.minValue << ", кількість = " << seqResult.count << '\n';
        cout << '\n';
    }

    return 0;
}
