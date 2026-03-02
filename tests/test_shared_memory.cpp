#include <gtest/gtest.h>

#include <lib/interprocess/shared_memory.hpp>

using namespace hft;

const char* const SHARED_MEMORY_NAME = "shm_test";
constexpr uint32_t ALIGNMENT = 8;

struct SharedInt {
    uint32_t value;
};

struct SharedChar {
    char value;
};

using SharedMemoryTest = SharedMemory<ALIGNMENT, SharedInt, SharedChar>;

inline void WaitForProducer() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

class SharedMemoryTestuite : public ::testing::Test {
protected:
    void SetUp() override {}
    
    void TearDown() override {
        boost::interprocess::shared_memory_object::remove(SHARED_MEMORY_NAME);
    }
};

TEST_F(SharedMemoryTestuite, ReadWritten) {
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

TEST_F(SharedMemoryTestuite, Exception_FailedToOpenSharedMemory) {
    EXPECT_THROW(
        SharedMemoryTest shared_memory(SHARED_MEMORY_NAME, MemoryRole::OPEN_ONLY),
        FailedToOpenSharedMemory
    );
}

TEST_F(SharedMemoryTestuite, Exception_FailedToMapRegion) {
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
        FailedToMapRegion
    );
}

TEST_F(SharedMemoryTestuite, Exception_SharedMemoryIsEmpty) {
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
        SharedMemoryIsEmpty
    );
}

TEST_F(SharedMemoryTestuite, CheckHeartbeart) {
    int pid = fork();
    ASSERT_NE(-1, pid) << strerror(errno);

    if (pid == 0) {
        // producer logic
        SharedMemoryTest shared_memory(SHARED_MEMORY_NAME, MemoryRole::CREATE_ONLY);
        shared_memory.UpdateHeartbeart();
        _exit(0);
    }

    WaitForProducer();

    SharedMemoryTest shared_memory(SHARED_MEMORY_NAME, MemoryRole::OPEN_ONLY);
    EXPECT_TRUE(shared_memory.IsProducerAlive(1));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_FALSE(shared_memory.IsProducerAlive(1));
}
