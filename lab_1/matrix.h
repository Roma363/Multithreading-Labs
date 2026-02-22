#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

struct Matrix {
    std::size_t rows;
    std::size_t cols;
    std::vector<std::vector<int>> arr;

    static Matrix generateRandom(std::size_t rows, std::size_t cols, int minVal, int maxVal,
                                     std::uint32_t seed = std::random_device{}()) {
        Matrix test;
        test.rows = rows;
        test.cols = cols;
        test.arr.assign(rows, std::vector<int>(cols));

        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(minVal, maxVal);

        for (std::size_t i = 0; i < rows; i++) {
            for (std::size_t j = 0; j < cols; j++) {
                test.arr[i][j] = dist(rng);
            }
        }

        return test;
    }
};