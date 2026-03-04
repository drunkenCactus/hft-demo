#include <gtest/gtest.h>

#include <lib/interprocess/ring_buffer.hpp>
#include <lib/interprocess/shared_memory.hpp>

#include <cstdint>
#include <random>

using namespace hft;

constexpr uint32_t ALIGNMENT = 64;

struct alignas(ALIGNMENT) RingBufferData {
    char marker1;
    char padding[64];
    char marker2;
};

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

TEST(RingBuffer, Size) {
    constexpr uint32_t consumers_count = 2;
    constexpr uint32_t buffer_length = 8;
    using RingBufferTest = RingBuffer<RingBufferData, ALIGNMENT, buffer_length, consumers_count>;

    const uint32_t expected_size
        = 1 * ALIGNMENT                             // head_
        + consumers_count * ALIGNMENT               // tails_
        + consumers_count * ALIGNMENT               // active_consumers_
        + buffer_length * sizeof(RingBufferData);   // data_
    EXPECT_EQ(sizeof(RingBufferTest), expected_size);
}

TEST(RingBuffer, MultiConsumers) {
    constexpr uint32_t consumers_count = 2;
    constexpr uint32_t buffer_length = 1024;
    using RingBufferTest = RingBuffer<RingBufferData, ALIGNMENT, buffer_length, consumers_count>;
    RingBufferTest buffer;

    const uint32_t data_length = 10000;
    const std::string data1 = GenerateRandomString(data_length);
    const std::string data2 = GenerateRandomString(data_length);

    std::atomic<bool> is_stopped{false};
    std::atomic<uint32_t> read_count{0};
    std::atomic<uint32_t> corrupted_count{0};

    std::vector<std::thread> consumers;
    for (uint32_t consumer_id = 0; consumer_id < consumers_count; ++consumer_id) {
        consumers.emplace_back([&, consumer_id]() {
            RingBufferData buffer_data;
            uint32_t pos = 0;
            while (true) {
                if (buffer.Read(buffer_data, consumer_id)) {
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

TEST(RingBuffer, DisableLaggingConsumer) {
    constexpr uint32_t consumers_count = 1;
    constexpr uint32_t buffer_length = 4;
    using RingBufferTest = RingBuffer<RingBufferData, ALIGNMENT, buffer_length, consumers_count>;
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
        while (buffer.Read(buffer_data, 0)) {
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

const char* const SHM_NAME = "shm_test";

class RingBufferIpc : public ::testing::Test {
protected:
    void SetUp() override {}
    
    void TearDown() override {
        boost::interprocess::shared_memory_object::remove(SHM_NAME);
    }
};

TEST_F(RingBufferIpc, IpcSingleConsumer) {
    constexpr uint32_t consumers_count = 1;
    constexpr uint32_t buffer_length = 1024;
    using RingBufferTest = RingBuffer<RingBufferData, ALIGNMENT, buffer_length, consumers_count>;
    using SharedMemoryTest = SharedMemory<ALIGNMENT, RingBufferTest>;

    const uint32_t data_length = 10000;
    const std::string data1 = GenerateRandomString(data_length);
    const std::string data2 = GenerateRandomString(data_length);

    int pid = fork();
    ASSERT_NE(-1, pid) << strerror(errno);

    if (pid == 0) {
        // producer logic
        SharedMemoryTest shm(SHM_NAME, MemoryRole::CREATE_ONLY);
        auto [buffer] = shm.GetObjects();
        RingBufferData buffer_data;
        for (uint64_t i = 0; i < data_length; ++i) {
            buffer_data.marker1 = data1[i];
            buffer_data.marker2 = data2[i];
            buffer->Write(buffer_data);
            shm.UpdateHeartbeat();
            if (i % (buffer_length / 2) == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        _exit(0);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    uint32_t read_count = 0;
    uint32_t corrupted_count = 0;

    SharedMemoryTest shm(SHM_NAME, MemoryRole::OPEN_ONLY);
    auto [buffer] = shm.GetObjects();

    RingBufferData buffer_data;
    uint32_t pos = 0;
    while (true) {
        if (buffer->Read(buffer_data, 0)) {
            if (buffer_data.marker1 != data1[pos] || buffer_data.marker2 != data2[pos]) {
                ++corrupted_count;
            }
            ++read_count;
            ++pos;
        } else if (!shm.IsProducerAlive(3)) {
            break;
        }
    }

    EXPECT_EQ(read_count, data_length);
    EXPECT_EQ(corrupted_count, 0);
}
