#include <iostream>

int main() {
    // ..01
    int a = 1;

    // ..10
    int b = a << 1;

    //
    int c = 1 << 3;

    int d = c & 3;
    int e = c & 2;


    // outputs 2
    std::cout << b << std::endl;
    std::cout << c << std::endl;
    std::cout << d << std::endl;
    std::cout << e << std::endl;



}