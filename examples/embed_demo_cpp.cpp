/*
 * embed_demo_cpp.cpp - Demonstrates C++ embedding of approx.h
 *
 * Compile:
 *   c++ -std=c++11 -O2 -I.. embed_demo_cpp.cpp -o embed_demo_cpp
 */

#define APPROX_IMPLEMENTATION
#include "approx.h"

#include <iostream>
#include <vector>
#include <string>

int main()
{
    std::string pattern = "receive";
    std::vector<std::string> words = {
        "receive",
        "recieve",
        "receipt",
        "recipe",
        "perceive",
        "deceive"
    };

    std::cout << "=== C++ approx.h integration ===" << std::endl;
    for (const auto &w : words) {
        double score = approx_sim(pattern.c_str(), pattern.size(),
                                  w.c_str(), w.size(),
                                  APPROX_DAMERAU);
        std::cout << w << " -> score: " << score << std::endl;
    }

    return 0;
}
