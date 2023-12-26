#include "config.h"

#include <gtest/gtest.h>

TEST(file, getDataPath)
{
    auto p = getDataPath();
    std::cout << p << std::endl;
}
