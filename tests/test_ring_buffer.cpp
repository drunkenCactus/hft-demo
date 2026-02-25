#include <gtest/gtest.h>

#include <lib/ring_buffer.hpp>

#include <cstdint>

using namespace hft;

constexpr uint32_t CACHE_LINE_SIZE_TEST = 64;
constexpr uint32_t RING_BUFFER_LENGTH_TEST = 8;
constexpr uint32_t CONSUMERS_COUNT_TEST = 2;

struct alignas(CACHE_LINE_SIZE_TEST) TestData {
    char value;
};

using TestRingBuffer = RingBuffer<TestData, CACHE_LINE_SIZE_TEST, RING_BUFFER_LENGTH_TEST, CONSUMERS_COUNT_TEST>;

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

TEST(RingBuffer, Size) {
    const uint32_t expected_size = CACHE_LINE_SIZE_TEST * (
        1                           // head_
        + CONSUMERS_COUNT_TEST      // tails_
        + CONSUMERS_COUNT_TEST      // active_consumers_
        + RING_BUFFER_LENGTH_TEST   // data_
    );
    EXPECT_EQ(sizeof(TestRingBuffer), expected_size);
}

TEST(RingBuffer, AllFastConsumers) {
    TestRingBuffer buffer;
    const std::string data = "c478fy478bcf87ergfw48b7f7ch48hffhewiybfr78ey4378bc487gbci7rghfciucbf3478tr387c";
    bool is_running = true;

    std::vector<std::string> results(CONSUMERS_COUNT_TEST, "");

    std::vector<std::thread> consumers;
    for (uint32_t consumer_id = 0; consumer_id < CONSUMERS_COUNT_TEST; ++consumer_id) {
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

    std::vector<std::string> results(CONSUMERS_COUNT_TEST, "");
    std::vector<uint32_t> timeouts_ms = {10, 20};

    std::vector<std::thread> consumers;
    for (uint32_t consumer_id = 0; consumer_id < CONSUMERS_COUNT_TEST; ++consumer_id) {
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
