#include <gtest/gtest.h>

#include <lib/shared_memory.hpp>

#include <cstdint>

using namespace hft;

constexpr uint32_t BUFFER_SIZE = 8;
constexpr uint32_t CONSUMERS_COUNT = 2;

struct alignas(CACHE_LINE_SIZE) TestData {
    char value;
};

using TestRingBuffer = RingBuffer<TestData, BUFFER_SIZE, CONSUMERS_COUNT>;

void Consume(
    TestRingBuffer& buffer,
    const uint32_t consumer_id,
    std::string& result,
    const bool& is_running,
    const uint32_t timeout_ms
) {
    while (is_running) {
        TestData data;
        const bool has_data = buffer.Read(data, consumer_id);
        if (has_data) {
            result += data.value;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }
}

void Produce(TestRingBuffer& buffer, const std::string& data) {
    for (const char item : data) {
        buffer.Write(TestData{
            .value = item
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
}

TEST(RingBuffer, AllFastConsumers) {
    TestRingBuffer buffer;
    const std::string data = "c478fy478bcf87ergfw48b7f7ch48hffhewiybfr78ey4378bc487gbci7rghfciucbf3478tr387c";
    bool is_running = true;

    std::vector<std::string> results(CONSUMERS_COUNT, "");

    std::vector<std::thread> consumers;
    for (uint32_t consumer_id = 0; consumer_id < CONSUMERS_COUNT; ++consumer_id) {
        const uint32_t timeout_ms = 10;
        consumers.emplace_back(
            Consume,
            std::ref(buffer),
            consumer_id,
            std::ref(results.at(consumer_id)),
            std::cref(is_running),
            timeout_ms
        );
    }

    Produce(buffer, data);
    is_running = false;
    for (auto& consumer : consumers) {
        consumer.join();
    }

    for (const auto& result : results) {
        EXPECT_EQ(data, result);
    }
}

TEST(RingBuffer, DisableLaggingConsumer) {
    TestRingBuffer buffer;
    const std::string data = "c478fy478bcf87ergfw48b7f7ch48hffhewiybfr78ey4378bc487gbci7rghfciucbf3478tr387c";
    bool is_running = true;

    std::vector<std::string> results(CONSUMERS_COUNT, "");
    std::vector<uint32_t> timeouts_ms = {10, 20};

    std::vector<std::thread> consumers;
    for (uint32_t consumer_id = 0; consumer_id < CONSUMERS_COUNT; ++consumer_id) {
        consumers.emplace_back(
            Consume,
            std::ref(buffer),
            consumer_id,
            std::ref(results.at(consumer_id)),
            std::cref(is_running),
            timeouts_ms[consumer_id]
        );
    }

    Produce(buffer, data);
    is_running = false;
    for (auto& consumer : consumers) {
        consumer.join();
    }

    {
        const auto& result = results.at(0);
        EXPECT_EQ(data, result);
    }
    {
        const auto& result = results.at(1);
        EXPECT_FALSE(result.empty());
        EXPECT_TRUE(result.size() < data.size());
    }
}
