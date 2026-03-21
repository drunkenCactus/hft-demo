#include <gtest/gtest.h>

#include <lib/interprocess/shared_memory.hpp>

#include <thread>

using namespace hft;

constexpr uint32_t ALIGNMENT = 8;

struct Object4b {
    uint32_t value;
};

struct Object8b {
    uint32_t value[2];
};

struct Object13b {
    char value[13];
};

struct alignas(ALIGNMENT) SharedInt {
    uint32_t value;
};

struct alignas(ALIGNMENT) SharedChar {
    char value;
};

TEST(SharedMemory, Sizes) {
    {
        uint32_t size = GetSizeWithPadding<ALIGNMENT, Object4b>();
        EXPECT_EQ(size, 8);
    }
    {
        uint32_t size = GetSizeWithPadding<ALIGNMENT, Object8b>();
        EXPECT_EQ(size, 8);
    }
    {
        uint32_t size = GetSizeWithPadding<ALIGNMENT, Object13b>();
        EXPECT_EQ(size, 16);
    }
    {
        uint32_t size = GetObjectsSize<ALIGNMENT, Object4b, Object8b, Object13b>();
        EXPECT_EQ(size, 32);
    }
}

TEST(SharedMemory, FetchCreated) {
    uint32_t size = GetObjectsSize<ALIGNMENT, SharedInt, SharedChar>();
    void* const buffer = malloc(size);

    {
        auto [shared_int, shared_char] = CreateObjectsInBuffer<ALIGNMENT, SharedInt, SharedChar>(buffer);
        shared_int->value = 42;
        shared_char->value = 'h';
    }

    {
        auto [shared_int, shared_char] = FetchObjectsFromBuffer<ALIGNMENT, SharedInt, SharedChar>(buffer);
        EXPECT_EQ(shared_int->value, 42);
        EXPECT_EQ(shared_char->value, 'h');
    }

    free(buffer);
}

const char* const SHARED_MEMORY_NAME = "shm_test";
using SharedMemoryTest = SharedMemory<ALIGNMENT, SharedInt, SharedChar>;

inline void WaitForProducer() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

class SharedMemoryIpc : public ::testing::Test {
protected:
    void SetUp() override {}
    
    void TearDown() override {
        boost::interprocess::shared_memory_object::remove(SHARED_MEMORY_NAME);
    }
};

TEST_F(SharedMemoryIpc, ReadWritten) {
    int pid = fork();
    ASSERT_NE(-1, pid) << strerror(errno);

    if (pid == 0) {
        // producer logic
        SharedMemoryTest shared_memory(SHARED_MEMORY_NAME, MemoryRole::CREATE_ONLY);
        auto [shared_int, shared_char] = shared_memory.GetObjects();
        shared_int->value = 42;
        shared_char->value = 'h';
        _exit(0);
    }

    WaitForProducer();

    SharedMemoryTest shared_memory(SHARED_MEMORY_NAME, MemoryRole::OPEN_ONLY);
    auto [shared_int, shared_char] = shared_memory.GetObjects();
    EXPECT_EQ(shared_int->value, 42);
    EXPECT_EQ(shared_char->value, 'h');
}

TEST_F(SharedMemoryIpc, Exception_FailedToOpenSharedMemory) {
    EXPECT_THROW(
        SharedMemoryTest shared_memory(SHARED_MEMORY_NAME, MemoryRole::OPEN_ONLY),
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
            SHARED_MEMORY_NAME,
            boost::interprocess::read_write
        );
        _exit(0);
    }

    WaitForProducer();

    EXPECT_THROW(
        SharedMemoryTest shared_memory(SHARED_MEMORY_NAME, MemoryRole::OPEN_ONLY),
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
            SHARED_MEMORY_NAME,
            boost::interprocess::read_write
        );
        memory.truncate(192);
        _exit(0);
    }

    WaitForProducer();

    EXPECT_THROW(
        SharedMemoryTest shared_memory(SHARED_MEMORY_NAME, MemoryRole::OPEN_ONLY),
        std::exception
    );
}

TEST_F(SharedMemoryIpc, CheckHeartbeart) {
    int pid = fork();
    ASSERT_NE(-1, pid) << strerror(errno);

    if (pid == 0) {
        // producer logic
        SharedMemoryTest shared_memory(SHARED_MEMORY_NAME, MemoryRole::CREATE_ONLY);
        shared_memory.UpdateHeartbeat();
        _exit(0);
    }

    WaitForProducer();

    SharedMemoryTest shared_memory(SHARED_MEMORY_NAME, MemoryRole::OPEN_ONLY);
    EXPECT_TRUE(shared_memory.IsProducerAlive(1));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_FALSE(shared_memory.IsProducerAlive(1));
}
