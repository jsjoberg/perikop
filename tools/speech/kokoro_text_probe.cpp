#include "speech/kokoro_text.hpp"
#include <iostream>
#include <stdexcept>
// Reads one text per line and prints its Kokoro token IDs, for comparison
// with the upstream Python front end.
int main(int argc, char** argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("Usage: kokoro-text-probe PACK < lines");
        ortho::KokoroText text(
            std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(argv[1]))));
        for (std::string line; std::getline(std::cin, line);) {
            const char* separator = "";
            for (auto id : text.tokens(line)) {
                std::cout << separator << id;
                separator = " ";
            }
            std::cout << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
