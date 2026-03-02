#include <gtest/gtest.h>

#include <lib/shared_memory.hpp>

using namespace hft;

const char* const SHARED_MEMORY_NAME = "shm_test";
constexpr uint32_t ALIGNMENT = 8;

struct SharedInt {
    uint32_t value;
};

struct SharedChar {
    char value;
};

void RunProducer() {
    SharedMemory<
        MemoryRole::CREATE_ONLY,
        ALIGNMENT,
        SharedInt,
        SharedChar
    > shared_memory(SHARED_MEMORY_NAME);

    auto [shared_int, shared_char] = shared_memory.GetObjects();
    shared_int->value = 42;
    shared_char->value = 'h';

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    exit(0);
}

class SharedMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    
    void TearDown() override {
        boost::interprocess::shared_memory_object::remove(SHARED_MEMORY_NAME);
    }
};

TEST_F(SharedMemoryTest, ReadWritten) {
    int pid = fork();
    ASSERT_NE(-1, pid) << strerror(errno);

    if (pid == 0) {
        RunProducer();
        _exit(1);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SharedMemory<
        MemoryRole::OPEN_ONLY,
        ALIGNMENT,
        SharedInt,
        SharedChar
    > shared_memory(SHARED_MEMORY_NAME);

    auto [shared_int, shared_char] = shared_memory.GetObjects();
    EXPECT_EQ(shared_int->value, 42);
    EXPECT_EQ(shared_char->value, 'h');
}
