#include <gtest/gtest.h>

#include <lib/objects_handler.hpp>

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

using ObjectsHandlerCreate = ObjectsHandler<MemoryRole::CREATE_ONLY, ALIGNMENT, Object4b, Object8b, Object13b>;
using ObjectsHandlerOpen = ObjectsHandler<MemoryRole::OPEN_ONLY, ALIGNMENT, Object4b, Object8b, Object13b>;

TEST(ObjectsHandler, Sizes) {
    EXPECT_EQ(sizeof(Object4b), 4);
    EXPECT_EQ(sizeof(Object8b), 8);
    EXPECT_EQ(sizeof(Object13b), 13);
    // Object4b: 8 bytes (4 sizeof + 4 padding)
    // Object8b: 8 bytes (8 sizeof + 0 padding)
    // Object13b: 16 bytes (13 sizeof + 3 padding)
    EXPECT_EQ(ObjectsHandlerCreate::DataSize(), 32);
}

TEST(ObjectsHandler, OpenCreated) {
    void* const buffer = malloc(ObjectsHandlerCreate::DataSize());

    {
        ObjectsHandlerCreate create_handler(buffer);
        auto [object_4b, object_8b, object_13b] = create_handler.GetObjects();

        object_4b->value = 42;
        object_13b->value[5] = 'h';
    }

    {
        ObjectsHandlerOpen open_handler(buffer);
        auto [object_4b, object_8b, object_13b] = open_handler.GetObjects();

        EXPECT_EQ(object_4b->value, 42);
        EXPECT_EQ(object_13b->value[5], 'h');
    }

    free(buffer);
}
