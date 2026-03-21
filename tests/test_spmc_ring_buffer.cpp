#include "lib/interprocess/ring_buffer_common.hpp"
#include <gtest/gtest.h>
#include <tests/utils.hpp>

#include <lib/interprocess/spmc_ring_buffer.hpp>

#include <cstdint>
#include <thread>

using namespace hft;

constexpr uint32_t ALIGNMENT = 64;

struct alignas(ALIGNMENT) RingBufferData {
    char marker1;
    char padding[64];
    char marker2;
};

TEST(SpmcRingBuffer, Size) {
    constexpr uint32_t consumers_count = 2;
    constexpr uint32_t buffer_length = 8;
    using RingBufferTest = SpmcRingBuffer<RingBufferData, ALIGNMENT, buffer_length, consumers_count>;

    const uint32_t expected_size
        = 1 * ALIGNMENT                             // head_
        + consumers_count * ALIGNMENT               // tails_
        + consumers_count * ALIGNMENT               // active_consumers_
        + buffer_length * sizeof(RingBufferData);   // data_
    EXPECT_EQ(sizeof(RingBufferTest), expected_size);
}

TEST(SpmcRingBuffer, ReadWritten) {
    constexpr uint32_t consumers_count = 2;
    constexpr uint32_t buffer_length = 1024;
    using RingBufferTest = SpmcRingBuffer<RingBufferData, ALIGNMENT, buffer_length, consumers_count>;
    RingBufferTest buffer;

    const uint32_t data_length = 10000;
    const std::string data1 = test::GenerateRandomString(data_length);
    const std::string data2 = test::GenerateRandomString(data_length);

    std::atomic<bool> is_stopped{false};
    std::atomic<uint32_t> read_count{0};
    std::atomic<uint32_t> corrupted_count{0};

    std::vector<std::thread> consumers;
    for (uint32_t consumer_id = 0; consumer_id < consumers_count; ++consumer_id) {
        consumers.emplace_back([&, consumer_id]() {
            RingBufferData buffer_data;
            uint32_t pos = 0;
            while (true) {
                ReadResult result = buffer.Read(buffer_data, consumer_id);
                if (result == ReadResult::SUCCESS) {
                    if (buffer_data.marker1 != data1[pos] || buffer_data.marker2 != data2[pos]) {
                        corrupted_count.fetch_add(1, std::memory_order_relaxed);
                    }
                    read_count.fetch_add(1, std::memory_order_relaxed);
                    ++pos;
                } else if (is_stopped.load(std::memory_order_acquire)) {
                    break;
                }
            }
        });
    }

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
    for (auto& consumer : consumers) {
        consumer.join();
    }

    EXPECT_EQ(read_count.load(), data_length * consumers_count);
    EXPECT_EQ(corrupted_count.load(), 0);
}

TEST(SpmcRingBuffer, DisableLaggingConsumer) {
    constexpr uint32_t consumers_count = 1;
    constexpr uint32_t buffer_length = 4;
    using RingBufferTest = SpmcRingBuffer<RingBufferData, ALIGNMENT, buffer_length, consumers_count>;
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
        while (buffer.Read(buffer_data, 0) == ReadResult::SUCCESS) {
            result.push_back(buffer_data.marker1);
        }
        return result;
    };

    write("ab");
    EXPECT_EQ(read(), "ab");

    write("foobar");
    EXPECT_EQ(read(), "");

    buffer.ResetConsumer(0);
    write("baz");
    EXPECT_EQ(read(), "baz");
}
