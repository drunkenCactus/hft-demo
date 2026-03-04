#include <random>

namespace hft::test {

std::string GenerateRandomString(const uint32_t length) {
    const std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    static std::random_device rd;
    static std::mt19937 generator(rd());
    static std::uniform_int_distribution<> distribution(0, characters.size() - 1);

    std::string result;
    result.reserve(length);
    for (uint32_t i = 0; i < length; ++i) {
        result += characters[distribution(generator)];
    }
    return result;
}

}  // namespace hft::test
