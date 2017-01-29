#include <btl/future/future.h>

#include <btl/result.h>

#include <gtest/gtest.h>

#include <string>
#include <memory>

TEST(Result, constructWithValue)
{
    btl::Result<int, std::string> r1(10);

    EXPECT_TRUE(r1.valid());
    EXPECT_EQ(10, r1.value());
}

TEST(Result, constructWithErr)
{
    btl::Result<int, std::string> r1("error");

    EXPECT_FALSE(r1.valid());
    EXPECT_EQ(std::string("error"), r1.err());
}

TEST(Result, fmapWithValue)
{
    btl::Result<int, std::string> r1(10);

    auto r2 = r1.fmap([](int i)
    {
        return (float)i * 2.0f;
    });

    EXPECT_TRUE(r2.valid());
    EXPECT_EQ((float)10 * 2.f, r2.value());
}

TEST(Result, fmapWithUniquePtr)
{
    btl::Result<std::unique_ptr<int>, std::string> r1(std::make_unique<int>(10));

    auto r2 = std::move(r1).fmap([](std::unique_ptr<int> i)
    {
        return (float)*i * 2.0f;
    });

    EXPECT_TRUE(r2.valid());
    EXPECT_EQ((float)10 * 2.f, r2.value());
}

TEST(Result, fmapWithErr)
{
    btl::Result<int, std::string> r1("error");

    auto r2 = r1.fmap([](int i)
    {
        return (float)i * 2.0f;
    });

    EXPECT_FALSE(r2.valid());
    EXPECT_EQ(std::string("error"), r2.err());
}

TEST(Result, errMapWithValue)
{
    btl::Result<int, std::string> r1(10);

    auto r2 = r1.errMap([](std::string const&)
    {
        return 30u;
    });

    EXPECT_TRUE(r2.valid());
    EXPECT_EQ(10, r2.value());
}

TEST(Result, errMapWithErr)
{
    btl::Result<int, std::string> r1("error");

    auto r2 = r1.errMap([](std::string const&)
    {
        return 30u;
    });

    EXPECT_FALSE(r2.valid());
    EXPECT_EQ(30u, r2.err());
}

TEST(Result, errMapWithUniquePtr)
{
    btl::Result<int, std::unique_ptr<std::string>> r1(
            std::make_unique<std::string>("error"));

    auto r2 = std::move(r1).errMap([](std::unique_ptr<std::string>)
    {
        return 30u;
    });

    EXPECT_FALSE(r2.valid());
    EXPECT_EQ(30u, r2.err());
}

