#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

struct MinMaxResult {
    std::int32_t minValue;
    std::int32_t maxValue;
};

inline MinMaxResult findMinMaxSequential(const std::vector<std::int32_t>& data) {
    MinMaxResult result{data[0], data[0]};
    for (std::size_t i = 1; i < data.size(); ++i) {
        if (data[i] < result.minValue) {
            result.minValue = data[i];
        }
        if (data[i] > result.maxValue) {
            result.maxValue = data[i];
        }
    }
    return result;
}

inline MinMaxResult findMinMaxParallel(const std::vector<std::int32_t>& data,
                                       std::size_t rows,
                                       std::size_t cols,
                                       std::size_t numThreads) {
    if (data.empty()) {
        return {0, 0};
    }

    if (numThreads == 0) {
        numThreads = 1;
    }

    if (numThreads == 1 || rows == 1) {
        return findMinMaxSequential(data);
    }

    if (numThreads > rows) {
        numThreads = rows;
    }

    std::vector<std::thread> threads;
    std::vector<MinMaxResult> localResults(numThreads);

    std::size_t rowsPerThread = rows / numThreads;
    std::size_t remainder = rows % numThreads;
    std::size_t currentRow = 0;

    for (std::size_t i = 0; i < numThreads; ++i) {
        std::size_t startRow = currentRow;
        std::size_t endRow = startRow + rowsPerThread + (i < remainder ? 1 : 0);
        currentRow = endRow;

        threads.emplace_back([&, startRow, endRow, i]() {
            std::size_t startIndex = startRow * cols;
            std::size_t endIndex = endRow * cols;
            MinMaxResult local{data[startIndex], data[startIndex]};
            for (std::size_t index = startIndex + 1; index < endIndex; ++index) {
                if (data[index] < local.minValue) {
                    local.minValue = data[index];
                }
                if (data[index] > local.maxValue) {
                    local.maxValue = data[index];
                }
            }
            localResults[i] = local;
        });
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    MinMaxResult result = localResults[0];
    for (std::size_t i = 1; i < localResults.size(); ++i) {
        if (localResults[i].minValue < result.minValue) {
            result.minValue = localResults[i].minValue;
        }
        if (localResults[i].maxValue > result.maxValue) {
            result.maxValue = localResults[i].maxValue;
        }
    }

    return result;
}
