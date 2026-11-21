#include "base64.h"

#include <cstdint>

namespace bqui::remote
{

std::string base64Encode(void const* data, size_t size)
{
    static char const alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    auto const* bytes = static_cast<unsigned char const*>(data);

    std::string out;
    out.reserve((size + 2) / 3 * 4);

    size_t i = 0;
    for (; i + 3 <= size; i += 3)
    {
        uint32_t group = (uint32_t(bytes[i]) << 16)
            | (uint32_t(bytes[i + 1]) << 8) | uint32_t(bytes[i + 2]);
        out.push_back(alphabet[(group >> 18) & 0x3f]);
        out.push_back(alphabet[(group >> 12) & 0x3f]);
        out.push_back(alphabet[(group >> 6) & 0x3f]);
        out.push_back(alphabet[group & 0x3f]);
    }

    size_t rest = size - i;
    if (rest == 0)
        return out;

    uint32_t group = uint32_t(bytes[i]) << 16;
    if (rest == 2)
        group |= uint32_t(bytes[i + 1]) << 8;

    out.push_back(alphabet[(group >> 18) & 0x3f]);
    out.push_back(alphabet[(group >> 12) & 0x3f]);
    out.push_back(rest == 2 ? alphabet[(group >> 6) & 0x3f] : '=');
    out.push_back('=');

    return out;
}

} // namespace bqui::remote
