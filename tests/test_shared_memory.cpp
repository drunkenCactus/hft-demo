#include <gtest/gtest.h>

#include <lib/interprocess/shared_memory.hpp>

#include <thread>

using namespace hft;

constexpr uint32_t kAlignment = 8;

struct Object4b {
    uint32_t value;
};

struct Object8b {
    uint32_t value[2];
};

struct Object13b {
    char value[13];
};

struct alignas(kAlignment) SharedInt {
    uint32_t value;
};

struct alignas(kAlignment) SharedChar {
    char value;
};

TEST(SharedMemory, Sizes) {
    {
        uint32_t size = GetSizeWithPadding<kAlignment, Object4b>();
        EXPECT_EQ(size, 8);
    }
    {
        uint32_t size = GetSizeWithPadding<kAlignment, Object8b>();
        EXPECT_EQ(size, 8);
    }
    {
        uint32_t size = GetSizeWithPadding<kAlignment, Object13b>();
        EXPECT_EQ(size, 16);
    }
    {
        uint32_t size = GetObjectsSize<kAlignment, Object4b, Object8b, Object13b>();
        EXPECT_EQ(size, 32);
    }
}

TEST(SharedMemory, FetchCreated) {
    uint32_t size = GetObjectsSize<kAlignment, SharedInt, SharedChar>();
    void* const buffer = malloc(size);

    {
        auto [shared_int, shared_char] = CreateObjectsInBuffer<kAlignment, SharedInt, SharedChar>(buffer);
        shared_int->value = 42;
        shared_char->value = 'h';
    }

    {
        auto [shared_int, shared_char] = FetchObjectsFromBuffer<kAlignment, SharedInt, SharedChar>(buffer);
        EXPECT_EQ(shared_int->value, 42);
        EXPECT_EQ(shared_char->value, 'h');
    }

    free(buffer);
}

const char* const kSharedMemoryName = "shm_test";
using SharedMemoryTest = SharedMemory<kAlignment, SharedInt, SharedChar>;

inline void WaitForProducer() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

class SharedMemoryIpc : public ::testing::Test {
protected:
    void SetUp() override {}
    
    void TearDown() override {
        boost::interprocess::shared_memory_object::remove(kSharedMemoryName);
    }
};

TEST_F(SharedMemoryIpc, ReadWritten) {
    int pid = fork();
    ASSERT_NE(-1, pid) << strerror(errno);

    if (pid == 0) {
        // producer logic
        SharedMemoryTest shared_memory(kSharedMemoryName, MemoryRole::kCreateOnly);
        auto [shared_int, shared_char] = shared_memory.GetObjects();
        shared_int->value = 42;
        shared_char->value = 'h';
        _exit(0);
    }

    WaitForProducer();

    SharedMemoryTest shared_memory(kSharedMemoryName, MemoryRole::kOpenOnly);
    auto [shared_int, shared_char] = shared_memory.GetObjects();
    EXPECT_EQ(shared_int->value, 42);
    EXPECT_EQ(shared_char->value, 'h');
}

TEST_F(SharedMemoryIpc, Exception_FailedToOpenSharedMemory) {
    EXPECT_THROW(
        SharedMemoryTest shared_memory(kSharedMemoryName, MemoryRole::kOpenOnly),
        std::exception
    );
}

TEST_F(SharedMemoryIpc, Exception_FailedToMapRegion) {
    int pid = fork();
    ASSERT_NE(-1, pid) << strerror(errno);

    if (pid == 0) {
        // producer logic
        auto memory = boost::interprocess::shared_memory_object(
            boost::interprocess::create_only,
            kSharedMemoryName,
            boost::interprocess::read_write
        );
        _exit(0);
    }

    WaitForProducer();

    EXPECT_THROW(
        SharedMemoryTest shared_memory(kSharedMemoryName, MemoryRole::kOpenOnly),
        std::exception
    );
}

TEST_F(SharedMemoryIpc, Exception_SharedMemoryIsEmpty) {
    int pid = fork();
    ASSERT_NE(-1, pid) << strerror(errno);

    if (pid == 0) {
        // producer logic
        auto memory = boost::interprocess::shared_memory_object(
            boost::interprocess::create_only,
            kSharedMemoryName,
            boost::interprocess::read_write
        );
        memory.truncate(192);
        _exit(0);
    }

    WaitForProducer();

    EXPECT_THROW(
        SharedMemoryTest shared_memory(kSharedMemoryName, MemoryRole::kOpenOnly),
        std::exception
    );
}

TEST_F(SharedMemoryIpc, CheckHeartbeat) {
    int pid = fork();
    ASSERT_NE(-1, pid) << strerror(errno);

    if (pid == 0) {
        // producer logic
        SharedMemoryTest shared_memory(kSharedMemoryName, MemoryRole::kCreateOnly);
        shared_memory.UpdateHeartbeat();
        _exit(0);
    }

    WaitForProducer();

    SharedMemoryTest shared_memory(kSharedMemoryName, MemoryRole::kOpenOnly);
    EXPECT_TRUE(shared_memory.IsProducerAlive(1));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_FALSE(shared_memory.IsProducerAlive(1));
}
