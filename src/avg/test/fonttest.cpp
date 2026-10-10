#include "gtest/gtest.h"

#include <avg/font.h>
#include <avg/fontmanager.h>
#include <avg/path.h>
#include <avg/pathbuilder.h>
#include <ase/vector.h>

#include <pmr/new_delete_resource.h>

#include <utf8/utf8.h>

#include <iostream>
#include <stdexcept>
#include <utility>
#include <string>

const std::string fontPath = "data/fonts/OpenSans-Regular.ttf";

TEST(Font, ToPath)
{
    avg::Font font(fontPath, 0);

    avg::Path path = font.textToPath(pmr::new_delete_resource(),
            utf8::asUtf8("jepulis"), 18.0f / 800.0f,
            ase::Vector2f(-0.5f, -0.5f));

    //std::cout << path << std::endl;

    EXPECT_EQ(false, path.isEmpty());
}

TEST(Font, compare)
{
    avg::Font font(fontPath, 0);

    avg::Font font2(fontPath, 0);

    EXPECT_EQ(font, font2);
}

TEST(Font, missingFileThrows)
{
    std::string const missing = "data/fonts/does-not-exist.ttf";

    try
    {
        avg::Font font(missing, 0);
        FAIL() << "expected std::runtime_error";
    }
    catch (std::runtime_error const& e)
    {
        EXPECT_NE(std::string::npos, std::string(e.what()).find(missing));
    }
}

TEST(Font, reloadAfterRelease)
{
    {
        avg::Font font(fontPath, 0);
        EXPECT_FALSE(font.isEmpty());
    }

    avg::Font font(fontPath, 0);

    EXPECT_FALSE(font.isEmpty());
    EXPECT_GT(font.getAscender(1.0f), 0.0f);
    EXPECT_FALSE(font.textToPath(pmr::new_delete_resource(),
            utf8::asUtf8("jepulis"), 1.0f, ase::Vector2f(0.0f, 0.0f))
            .isEmpty());
}
