#include <futures/algorithm.h>
#include <futures/futures.h>
#include <iostream>

int main() {
    using namespace futures;

    auto f1 = futures::async([] { std::cout << "f1" << std::endl; });
    auto f2 = futures::async([] { std::cout << "f2" << std::endl; });
    auto f3 = futures::async([] { std::cout << "f3" << std::endl; });
    auto f4 = futures::async([] { std::cout << "f4" << std::endl; });
    auto f5 = futures::when_all(f1, f2, f3, f4);
    f5.wait();

    auto f6 = futures::async([] { return 6; });
    auto f7 = futures::async([] { return 7; });
    auto f8 = futures::async([] { return 8; });
    auto f9 = f6 && f7 && f8;

    auto f10 = futures::then(
        f9, [](int a, int b, int c) { return a * b * c; });
    std::cout << f10.get() << '\n';

    return 0;
}