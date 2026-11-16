#include "remote/png.h"

#include <zlib.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

using bqui::remote::encodePng;

namespace
{
    struct Chunk
    {
        std::string type;
        std::string data;
        std::uint32_t crc = 0;
    };

    std::uint32_t readU32(std::string const& bytes, size_t offset)
    {
        auto byte = [&](size_t i)
        {
            return static_cast<std::uint32_t>(
                    static_cast<unsigned char>(bytes.at(offset + i)));
        };
        return (byte(0) << 24) | (byte(1) << 16) | (byte(2) << 8) | byte(3);
    }

    std::uint32_t crcOf(Chunk const& chunk)
    {
        std::string covered = chunk.type + chunk.data;
        uLong crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, reinterpret_cast<Bytef const*>(covered.data()),
                static_cast<uInt>(covered.size()));
        return static_cast<std::uint32_t>(crc);
    }

    // Splits a PNG file into its chunks after checking the signature.
    std::vector<Chunk> chunks(std::string const& png)
    {
        EXPECT_EQ(std::string("\x89PNG\r\n\x1a\n", 8), png.substr(0, 8));

        std::vector<Chunk> result;
        size_t offset = 8;
        while (offset + 12 <= png.size())
        {
            std::uint32_t length = readU32(png, offset);
            Chunk chunk;
            chunk.type = png.substr(offset + 4, 4);
            chunk.data = png.substr(offset + 8, length);
            chunk.crc = readU32(png, offset + 8 + length);
            result.push_back(std::move(chunk));
            offset += 12 + length;
        }
        EXPECT_EQ(png.size(), offset);
        return result;
    }

    std::vector<std::uint8_t> inflate(std::string const& compressed,
            size_t expected)
    {
        std::vector<std::uint8_t> out(expected + 1);
        uLongf size = static_cast<uLongf>(out.size());
        int status = uncompress(out.data(), &size,
                reinterpret_cast<Bytef const*>(compressed.data()),
                static_cast<uLong>(compressed.size()));
        EXPECT_EQ(Z_OK, status);
        out.resize(size);
        return out;
    }

    int paeth(int a, int b, int c)
    {
        int p = a + b - c;
        int pa = std::abs(p - a);
        int pb = std::abs(p - b);
        int pc = std::abs(p - c);
        if (pa <= pb && pa <= pc)
            return a;
        return pb <= pc ? b : c;
    }

    std::vector<std::uint8_t> unfilter(std::vector<std::uint8_t> const& data,
            std::uint32_t width, std::uint32_t height)
    {
        size_t const stride = size_t(width) * 3;
        std::vector<std::uint8_t> rgb(stride * height);

        for (size_t y = 0; y < height; ++y)
        {
            std::uint8_t filter = data.at(y * (stride + 1));
            for (size_t i = 0; i < stride; ++i)
            {
                int raw = data.at(y * (stride + 1) + 1 + i);
                int a = i >= 3 ? rgb[y * stride + i - 3] : 0;
                int b = y > 0 ? rgb[(y - 1) * stride + i] : 0;
                int c = y > 0 && i >= 3 ? rgb[(y - 1) * stride + i - 3] : 0;

                int predicted = 0;
                switch (filter)
                {
                case 0: break;
                case 1: predicted = a; break;
                case 2: predicted = b; break;
                case 3: predicted = (a + b) / 2; break;
                case 4: predicted = paeth(a, b, c); break;
                default: ADD_FAILURE() << "filter " << int(filter); break;
                }

                rgb[y * stride + i] =
                    static_cast<std::uint8_t>(raw + predicted);
            }
        }

        return rgb;
    }

    // Checks the chunk layout, the CRCs and the header of what encodePng
    // wrote, and returns its pixels.
    std::vector<std::uint8_t> decode(std::string const& png,
            std::uint32_t width, std::uint32_t height)
    {
        auto parts = chunks(png);
        EXPECT_GE(parts.size(), 4u);
        if (parts.size() < 4)
            return {};

        for (auto const& chunk : parts)
            EXPECT_EQ(crcOf(chunk), chunk.crc) << chunk.type;

        EXPECT_EQ("IHDR", parts.front().type);
        EXPECT_EQ("sRGB", parts.at(1).type);
        EXPECT_EQ("IEND", parts.back().type);
        EXPECT_TRUE(parts.back().data.empty());

        auto const& header = parts.front().data;
        EXPECT_EQ(13u, header.size());
        EXPECT_EQ(width, readU32(header, 0));
        EXPECT_EQ(height, readU32(header, 4));
        EXPECT_EQ(std::string({ 8, 2, 0, 0, 0 }), header.substr(8));

        EXPECT_EQ(std::string(1, '\0'), parts.at(1).data);

        std::string compressed;
        for (size_t i = 2; i + 1 < parts.size(); ++i)
        {
            EXPECT_EQ("IDAT", parts[i].type);
            compressed += parts[i].data;
        }

        size_t const filtered = (size_t(width) * 3 + 1) * height;
        auto data = inflate(compressed, filtered);
        EXPECT_EQ(filtered, data.size());
        if (data.size() != filtered)
            return {};

        return unfilter(data, width, height);
    }

    // Pixels whose rows favour different filters.
    std::vector<std::uint8_t> pattern(std::uint32_t width,
            std::uint32_t height, std::uint32_t seed)
    {
        std::vector<std::uint8_t> rgb(size_t(width) * height * 3);
        for (size_t i = 0; i < rgb.size(); ++i)
        {
            size_t pixel = i / 3;
            size_t x = pixel % width;
            size_t y = pixel / width;
            if (y % 4 == 0)
            {
                rgb[i] = static_cast<std::uint8_t>(x * 7 + i % 3);
            }
            else if (y % 4 == 1)
            {
                rgb[i] = static_cast<std::uint8_t>(y * 5);
            }
            else if (y % 4 == 2)
            {
                rgb[i] = static_cast<std::uint8_t>(x * y + seed);
            }
            else
            {
                rgb[i] = static_cast<std::uint8_t>(
                        (i * 2654435761u + seed) >> 13);
            }
        }
        return rgb;
    }
} // namespace

TEST(png, roundTripsThroughZlib)
{
    std::uint32_t const width = 37;
    std::uint32_t const height = 23;
    auto rgb = pattern(width, height, 11);

    auto png = encodePng(width, height, rgb.data());

    EXPECT_EQ(rgb, decode(png, width, height));
}

TEST(png, encodesASinglePixel)
{
    std::vector<std::uint8_t> rgb{ 255, 128, 1 };

    auto png = encodePng(1, 1, rgb.data());

    EXPECT_EQ(rgb, decode(png, 1, 1));
}

TEST(png, encodesSingleRowsAndColumns)
{
    auto row = pattern(97, 1, 3);
    EXPECT_EQ(row, decode(encodePng(97, 1, row.data()), 97, 1));

    auto column = pattern(1, 61, 5);
    EXPECT_EQ(column, decode(encodePng(1, 61, column.data()), 1, 61));
}

TEST(png, splitsLargeDataAcrossSeveralIdatChunks)
{
    std::uint32_t const width = 1024;
    std::uint32_t const height = 600;
    std::vector<std::uint8_t> rgb(size_t(width) * height * 3);
    std::uint32_t state = 12345;
    for (auto& byte : rgb)
    {
        state = state * 1664525u + 1013904223u;
        byte = static_cast<std::uint8_t>(state >> 24);
    }

    auto png = encodePng(width, height, rgb.data());

    size_t idats = 0;
    for (auto const& chunk : chunks(png))
    {
        if (chunk.type == "IDAT")
            ++idats;
    }
    EXPECT_GT(idats, 1u);
    EXPECT_EQ(rgb, decode(png, width, height));
}

TEST(png, rejectsAnEmptyImage)
{
    std::uint8_t pixel[3] = {};
    EXPECT_THROW(encodePng(0, 1, pixel), std::invalid_argument);
    EXPECT_THROW(encodePng(1, 0, pixel), std::invalid_argument);
}
