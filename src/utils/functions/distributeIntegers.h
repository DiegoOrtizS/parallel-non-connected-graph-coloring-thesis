#ifndef DISTRIBUTE_INTEGERS_H
#define DISTRIBUTE_INTEGERS_H

#include <vector>

std::vector<int> distributeIntegers(int n, int nPrime) {
    int quotient = n / nPrime;
    int remainder = n % nPrime;

    std::vector<int> result(nPrime, quotient);

    for (int i = 0; i < remainder; i++) {
        result[i] += 1;
    }

    return result;
}

#endif // DISTRIBUTE_INTEGERS_H