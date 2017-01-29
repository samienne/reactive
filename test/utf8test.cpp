#include <utf8/utf8.h>

#include <gtest/gtest.h>

#include <vector>
#include <string>

using namespace utf8;

TEST(Utf8, viewConstruct)
{
    std::string str("tättäröö");
    std::vector<uint32_t> v;

    for (auto c : asUtf8(str))
        v.push_back(c);

    EXPECT_EQ(8, v.size());

    EXPECT_EQ('t', v[0]);
    EXPECT_EQ(0xe4, v[1]);
    EXPECT_EQ('t', v[2]);
    EXPECT_EQ('t', v[3]);
    EXPECT_EQ(0xe4, v[4]);
    EXPECT_EQ('r', v[5]);
    EXPECT_EQ(0xf6, v[6]);
    EXPECT_EQ(0xf6, v[7]);
}

TEST(Utf8, longCharacters)
{
    std::string str("tst࿑ஔ𐆖");

    std::vector<uint32_t> v;

    for (auto c : asUtf8(str))
        v.push_back(c);

    EXPECT_EQ(6, v.size());

    EXPECT_EQ('t', v[0]);
    EXPECT_EQ('s', v[1]);
    EXPECT_EQ('t', v[2]);
    EXPECT_EQ(0x0fd1, v[3]);
    EXPECT_EQ(0x0b94, v[4]);
    EXPECT_EQ(0x10196, v[5]);
}

TEST(Utf8, encode)
{
    EXPECT_EQ(std::string("ä"), encode(0xe4));
}

TEST(Utf8, encodeDecode)
{
    auto str = encode(0x20) + encode(0x88) + encode(0x0800) + encode(0x10000)
        + encode(0x200000) + encode(0x4000000);

    EXPECT_EQ(21, str.size());

    std::vector<uint32_t> v;

    for (auto c : asUtf8(str))
        v.push_back(c);

    EXPECT_EQ(6, v.size());

    EXPECT_EQ(0x20, v[0]);
    EXPECT_EQ(0x88, v[1]);
    EXPECT_EQ(0x0800, v[2]);
    EXPECT_EQ(0x10000, v[3]);
    EXPECT_EQ(0x200000, v[4]);
    EXPECT_EQ(0x4000000, v[5]);
}

TEST(Utf8, emptyString)
{
    std::string str;
    auto ustr = asUtf8(str);
    int n = 0;
    for (auto i = ustr.begin(); i != ustr.end(); ++i)
        ++n;

    EXPECT_EQ(0, n);
}

TEST(Utf8, garbage)
{
    std::string str(10, (char)0x80);
    std::vector<uint32_t> v;
    for (auto c : asUtf8(str))
        v.push_back(c);

    EXPECT_EQ(10, v.size());
    for (int n = 0; n < 10; ++n)
    {
        EXPECT_EQ(0x80, v[n]);
    }
}

TEST(Utf8, garbagePadding)
{
    auto str = std::string(10, (char)0x80) + std::string("ätestö")
        + std::string(10, (char)0x80);
    std::vector<uint32_t> v;
    for (auto&& c : asUtf8(str))
        v.push_back(c);

    EXPECT_EQ(26, v.size());

    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(0x80, v[i]);

    EXPECT_EQ(0xe4, v[10]);
    EXPECT_EQ('t', v[11]);
    EXPECT_EQ('e', v[12]);
    EXPECT_EQ('s', v[13]);
    EXPECT_EQ('t', v[14]);
    EXPECT_EQ(0xf6, v[15]);

    for (int i = 16; i < 26; ++i)
        EXPECT_EQ(0x80, v[i]);
}

