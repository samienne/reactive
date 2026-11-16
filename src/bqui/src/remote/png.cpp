#include "png.h"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

namespace bqui::remote
{

namespace
{

constexpr size_t kBytesPerPixel = 3;
constexpr size_t kMaxIdatChunk = size_t(1) << 20;
constexpr std::uint32_t kMaxDimension = (std::uint32_t(1) << 31) - 1;

enum Filter : std::uint8_t { None, Sub, Up, Average, Paeth };

void putU32(std::string& out, std::uint32_t value)
{
    out.push_back(static_cast<char>((value >> 24) & 0xff));
    out.push_back(static_cast<char>((value >> 16) & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
    out.push_back(static_cast<char>(value & 0xff));
}

void putChunk(std::string& out, char const (&type)[5], char const* data,
        size_t size)
{
    putU32(out, static_cast<std::uint32_t>(size));

    size_t const start = out.size();
    out.append(type, 4);
    if (size > 0)
        out.append(data, size);

    auto const* bytes = reinterpret_cast<Bytef const*>(out.data() + start);
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, bytes, static_cast<uInt>(4 + size));
    putU32(out, static_cast<std::uint32_t>(crc));
}

std::uint8_t paeth(int a, int b, int c)
{
    int const p = a + b - c;
    int const pa = std::abs(p - a);
    int const pb = std::abs(p - b);
    int const pc = std::abs(p - c);
    if (pa <= pb && pa <= pc)
        return static_cast<std::uint8_t>(a);
    if (pb <= pc)
        return static_cast<std::uint8_t>(b);
    return static_cast<std::uint8_t>(c);
}

void filterRow(Filter filter, std::uint8_t const* row,
        std::uint8_t const* prior, size_t length, std::uint8_t* out)
{
    for (size_t i = 0; i < length; ++i)
    {
        int const a = i >= kBytesPerPixel ? row[i - kBytesPerPixel] : 0;
        int const b = prior ? prior[i] : 0;
        int const c = prior && i >= kBytesPerPixel
            ? prior[i - kBytesPerPixel] : 0;

        int predicted = 0;
        switch (filter)
        {
        case None: predicted = 0; break;
        case Sub: predicted = a; break;
        case Up: predicted = b; break;
        case Average: predicted = (a + b) / 2; break;
        case Paeth: predicted = paeth(a, b, c); break;
        }

        out[i] = static_cast<std::uint8_t>(row[i] - predicted);
    }
}

size_t filterCost(std::uint8_t const* filtered, size_t length)
{
    size_t cost = 0;
    for (size_t i = 0; i < length; ++i)
    {
        cost += static_cast<size_t>(std::abs(static_cast<std::int8_t>(
                        filtered[i])));
    }
    return cost;
}

// Filters every row with the filter whose output has the smallest sum of
// absolute signed bytes, the heuristic the PNG specification recommends.
std::vector<std::uint8_t> filterRows(std::uint32_t width,
        std::uint32_t height, std::uint8_t const* rgb)
{
    size_t const stride = size_t(width) * kBytesPerPixel;
    std::vector<std::uint8_t> out((stride + 1) * height);
    std::vector<std::uint8_t> candidate(stride);

    for (size_t y = 0; y < height; ++y)
    {
        std::uint8_t const* row = rgb + y * stride;
        std::uint8_t const* prior = y > 0 ? row - stride : nullptr;
        std::uint8_t* dest = out.data() + y * (stride + 1);

        size_t bestCost = (std::numeric_limits<size_t>::max)();
        for (auto filter : { None, Sub, Up, Average, Paeth })
        {
            filterRow(filter, row, prior, stride, candidate.data());
            size_t const cost = filterCost(candidate.data(), stride);
            if (cost < bestCost)
            {
                bestCost = cost;
                dest[0] = filter;
                std::copy(candidate.begin(), candidate.end(), dest + 1);
            }
        }
    }

    return out;
}

std::string deflate(std::vector<std::uint8_t> const& data)
{
    if (data.size() > (std::numeric_limits<uLong>::max)())
        throw std::invalid_argument("png: the image is too large to encode");

    uLongf size = compressBound(static_cast<uLong>(data.size()));
    std::string out(size, '\0');
    int const status = compress2(reinterpret_cast<Bytef*>(out.data()), &size,
            data.data(), static_cast<uLong>(data.size()), 6);
    if (status != Z_OK)
        throw std::runtime_error("png: deflate failed");

    out.resize(size);
    return out;
}

} // namespace

std::string encodePng(std::uint32_t width, std::uint32_t height,
        std::uint8_t const* rgb)
{
    if (width == 0 || height == 0 || width > kMaxDimension
            || height > kMaxDimension)
    {
        throw std::invalid_argument("png: width and height must be in "
                "[1, 2^31 - 1]");
    }

    std::string const compressed = deflate(filterRows(width, height, rgb));

    std::string out("\x89PNG\r\n\x1a\n", 8);

    std::string header;
    putU32(header, width);
    putU32(header, height);
    header += std::string{
        8, // bit depth
        2, // colour type: truecolour
        0, // compression: deflate
        0, // filter method: adaptive
        0, // interlace: none
    };
    putChunk(out, "IHDR", header.data(), header.size());

    char const perceptual = 0;
    putChunk(out, "sRGB", &perceptual, 1);

    for (size_t offset = 0; offset < compressed.size(); offset += kMaxIdatChunk)
    {
        putChunk(out, "IDAT", compressed.data() + offset,
                (std::min)(kMaxIdatChunk, compressed.size() - offset));
    }

    putChunk(out, "IEND", nullptr, 0);
    return out;
}

} // namespace bqui::remote
