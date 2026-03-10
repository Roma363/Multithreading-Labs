#pragma once

#include <vector>
#include <random>

// Структура для представлення масиву
struct Array {
    std::vector<int> data;
    size_t size;

    Array(size_t n) : size(n) {
        data.resize(n);
    }

    // Генерація випадкового масиву
    static Array generateRandom(size_t size, int minValue, int maxValue) {
        Array arr(size);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(minValue, maxValue);

        for (size_t i = 0; i < size; i++) {
            arr.data[i] = dist(gen);
        }
        return arr;
    }
};
