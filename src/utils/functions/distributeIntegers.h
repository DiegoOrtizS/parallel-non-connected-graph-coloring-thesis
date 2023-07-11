#ifndef DISTRIBUTE_INTEGERS_H
#define DISTRIBUTE_INTEGERS_H

#include <vector>

std::vector<lli> distributeIntegers(lli n, lli nPrime) {
    lli quotient = n / nPrime;
    lli remainder = n % nPrime;

    std::vector<lli> result(nPrime, quotient);

    for (lli i = 0; i < remainder; i++) {
        result[i] += 1;
    }

    return result;
}

#endif // DISTRIBUTE_INTEGERS_H