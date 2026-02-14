#include <iostream>
#include <cstddef>
#include "test_data.cpp"

using namespace std;

struct MinMax {
    int min;
    int max;
};

MinMax findMinMax(const Test1 test) {
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

int main() {
    Test1 test;

    MinMax result = findMinMax(test);
    cout << "min=" << result.min << ", max=" << result.max << '\n';

    return 0;
}