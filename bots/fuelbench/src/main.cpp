// Cost of printing a line, and of the first camp_groups() read (bench only).
#include <cambc.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
namespace bc = cambc;

int main() {
    bool done = false;
    while (true) {
        if (!bc::is_draft() && !done && bc::round_number() > 3) try {
            done = true;
            auto f0 = bc::fuel_remaining();
            std::cout << "FB probe line A" << std::endl;
            auto f1 = bc::fuel_remaining();
            std::cout << "FB probe line B" << std::endl;
            auto f2 = bc::fuel_remaining();
            char buf[96];
            std::snprintf(buf, sizeof buf, "FB snprintf r%u fuel %llu", bc::round_number(), static_cast<unsigned long long>(f2));
            auto f3 = bc::fuel_remaining();
            std::cout << "FB cout line " << (f0 - f1) << " second " << (f1 - f2) << " snprintf " << (f2 - f3) << std::endl;
            const auto g0 = bc::fuel_remaining();
            const auto groups = bc::map::camp_groups();
            const auto g1 = bc::fuel_remaining();
            std::cout << "FB camp_groups first " << (g0 - g1) << " n " << groups.size() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "FB error " << e.what() << std::endl;
        }
        bc::end_turn();
    }
}
