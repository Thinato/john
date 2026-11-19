// throwaway: song JSON -> ABC via john_core's toAbc (issue #74 test material)
#include "john.hpp"
#include <fstream>
#include <iostream>
int main(int c, char** v) { for (int i = 1; i < c; i++) { std::ifstream f(v[i]); std::cout << john::toAbc(john::fromJson(nlohmann::json::parse(f))) << "\n"; } }
