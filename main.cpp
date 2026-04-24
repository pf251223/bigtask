#include "bank_system.h"

#include <iostream>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    bank::BankSystem sys;
    sys.run();
    return 0;
}
