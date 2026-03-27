#include <gtest/gtest.h>
#include <tests/utils.hpp>

#include <lib/interprocess/spsc_ring_buffer.hpp>

#include <cstdint>
#include <thread>

using namespace hft;

constexpr uint32_t kAlignment = 64;

struct alignas(kAlignment) RingBufferData {
    char marker1;
    char padding[64];
    char marker2;
};

TEST(SpscRingBuffer, Size) {
    constexpr uint32_t buffer_length = 8;
    using RingBufferTest = SpscRingBuffer<RingBufferData, kAlignment, buffer_length>;

    const uint32_t expected_size
        = 1 * kAlignment                             // head_
        + 1 * kAlignment                             // tails_
        + 1 * kAlignment                             // active_consumers_
        + buffer_length * sizeof(RingBufferData);   // data_
    EXPECT_EQ(sizeof(RingBufferTest), expected_size);
}

TEST(SpscRingBuffer, ReadWritten) {
    constexpr uint32_t buffer_length = 1024;
    using RingBufferTest = SpscRingBuffer<RingBufferData, kAlignment, buffer_length>;
    RingBufferTest buffer;

    const uint32_t data_length = 10000;
    const std::string data1 = test::GenerateRandomString(data_length);
    const std::string data2 = test::GenerateRandomString(data_length);

    std::atomic<bool> is_stopped{false};
    uint32_t read_count = 0;
    uint32_t corrupted_count = 0;

    std::thread consumer([&]() {
        RingBufferData buffer_data;
        while (true) {
            ReadResult result = buffer.Read(buffer_data);
            if (result == ReadResult::kSuccess) {
                if (buffer_data.marker1 != data1[read_count] || buffer_data.marker2 != data2[read_count]) {
                    ++corrupted_count;
                }
                ++read_count;
            } else if (is_stopped.load(std::memory_order_acquire)) {
                break;
            }
        }
    });

    std::thread producer([&]() {
        RingBufferData buffer_data;
        for (uint64_t i = 0; i < data_length; ++i) {
            buffer_data.marker1 = data1[i];
            buffer_data.marker2 = data2[i];
            buffer.Write(buffer_data);
            if (i % (buffer_length / 2) == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        is_stopped.store(true, std::memory_order_release);
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(read_count, data_length);
    EXPECT_EQ(corrupted_count, 0);
}

TEST(SpscRingBuffer, DisableLaggingConsumer) {
    constexpr uint32_t buffer_length = 4;
    using RingBufferTest = SpscRingBuffer<RingBufferData, kAlignment, buffer_length>;
    RingBufferTest buffer;

    auto write = [&buffer](const std::string& data) {
        RingBufferData buffer_data;
        for (const char c : data) {
            buffer_data.marker1 = c;
            buffer.Write(buffer_data);
        }
    };

    auto read = [&buffer]() {
        RingBufferData buffer_data;
        std::string result;
        while (buffer.Read(buffer_data) == ReadResult::kSuccess) {
            result.push_back(buffer_data.marker1);
        }
        return result;
    };

    write("ab");
    EXPECT_EQ(read(), "ab");

    write("foobar");
    EXPECT_EQ(read(), "");

    buffer.ResetConsumer();
    write("baz");
    EXPECT_EQ(read(), "baz");
}
