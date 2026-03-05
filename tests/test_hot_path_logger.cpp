#include <gtest/gtest.h>
#include <tests/utils.hpp>

#include <lib/interprocess/hot_path_logger.hpp>

using namespace hft;

constexpr uint32_t TRUE_SIZE = ObserverRingBufferData::message_size - 1;

TEST(HotPathLogger, MessageLength) {
    EXPECT_EQ(TRUE_SIZE, 118);
}

TEST(HotPathLogger, StringLiterals) {
    ObserverRingBuffer buffer;
    HotPathLogger::Init(&buffer);

    ObserverRingBufferData result;
    {
        HOT_INFO << "foo";
        // message would not be logged without "Endl"
        EXPECT_FALSE(buffer.Read(result));

        HOT_INFO << "bar" << Endl;
        EXPECT_TRUE(buffer.Read(result));
        EXPECT_EQ(std::string(result.message), std::string("bar"));
    }
    {
        std::string str = test::GenerateRandomString(TRUE_SIZE);
        HOT_INFO << str.c_str() << Endl;
        EXPECT_TRUE(buffer.Read(result));
        EXPECT_EQ(std::string(result.message), str);
    }
    {
        std::string str = test::GenerateRandomString(TRUE_SIZE);
        std::string too_large_str = str + "overflow";
        HOT_INFO << too_large_str.c_str() << Endl;
        EXPECT_TRUE(buffer.Read(result));
        EXPECT_EQ(std::string(result.message), str);
    }
    {
        HOT_INFO << "foo" << "bar" << "baz" << Endl;
        EXPECT_TRUE(buffer.Read(result));
        EXPECT_EQ(std::string(result.message), std::string("foobarbaz"));
    }
}

TEST(HotPathLogger, Integers) {
    ObserverRingBuffer buffer;
    HotPathLogger::Init(&buffer);

    ObserverRingBufferData result;
    {
        HOT_INFO << 123 << Endl;
        EXPECT_TRUE(buffer.Read(result));
        EXPECT_EQ(std::string(result.message), std::string("123"));
    }
    {
        std::string str = test::GenerateRandomString(TRUE_SIZE - 2);
        HOT_INFO << str.c_str() << 123 << Endl;
        EXPECT_TRUE(buffer.Read(result));
        EXPECT_EQ(std::string(result.message), str);
    }
}

TEST(HotPathLogger, Doubles) {
    ObserverRingBuffer buffer;
    HotPathLogger::Init(&buffer);

    ObserverRingBufferData result;
    {
        HOT_INFO << 123.45 << Endl;
        EXPECT_TRUE(buffer.Read(result));
        EXPECT_EQ(std::string(result.message), std::string("123.45"));
    }
    {
        std::string str = test::GenerateRandomString(TRUE_SIZE - 5);
        HOT_INFO << str.c_str() << 123.45 << Endl;
        EXPECT_TRUE(buffer.Read(result));
        EXPECT_EQ(std::string(result.message), str);
    }
}

TEST(HotPathLogger, Common) {
    ObserverRingBuffer buffer;
    HotPathLogger::Init(&buffer);

    ObserverRingBufferData result;
    HOT_INFO << "Int = " << 42 << ", double = " << 36.6 << Endl;
    EXPECT_TRUE(buffer.Read(result));
    EXPECT_EQ(std::string(result.message), std::string("Int = 42, double = 36.6"));
}
