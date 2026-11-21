#include "remote/base64.h"

#include <gtest/gtest.h>

#include <string>

using bqui::remote::base64Encode;

namespace
{
    std::string encode(std::string const& text)
    {
        return base64Encode(text.data(), text.size());
    }
} // namespace

TEST(base64, encodesTheRfc4648Vectors)
{
    EXPECT_EQ("", encode(""));
    EXPECT_EQ("Zg==", encode("f"));
    EXPECT_EQ("Zm8=", encode("fo"));
    EXPECT_EQ("Zm9v", encode("foo"));
    EXPECT_EQ("Zm9vYg==", encode("foob"));
    EXPECT_EQ("Zm9vYmE=", encode("fooba"));
    EXPECT_EQ("Zm9vYmFy", encode("foobar"));
}

TEST(base64, usesBothHighCharactersAndKeepsZeroBytes)
{
    std::string const bytes{ '\xfb', '\xff', '\xbf', '\0', '\0' };
    EXPECT_EQ("+/+/AAA=", encode(bytes));
}
