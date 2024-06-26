#include "threadsafe.h"
#include <cstdio>

double worker(int k) {
    double res = 4.0 / (2 * k + 1.0);
    return k % 2 == 0 ? res : -1.0 * res;
}

double pi(int n) {
    std::vector<std::future<double>> futures;
    ThreadPoolExecutor pool;
    for (int i = 0; i < n; ++i) {
        futures.push_back(pool.submit(worker, i));
    }
    double p = 0.0;
    for (auto& future : futures) {
        p += future.get();
    }
    return p;
}

int main() {
    printf("Pi = %.12f\n", pi(1'000'000));
}
