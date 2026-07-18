#include "bqui/agent/introspectionjson.h"

#include "bqui/widget/datavalue.h"

#include <cmath>
#include <cstdio>
#include <sstream>
#include <variant>

namespace bqui::agent
{

namespace
{

void writeString(std::ostream& out, std::string const& value)
{
    out << '"';
    for (char c : value)
    {
        switch (c)
        {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                            static_cast<unsigned char>(c));
                    out << buf;
                }
                else
                {
                    out << c;
                }
                break;
        }
    }
    out << '"';
}

void writeNumber(std::ostream& out, double value)
{
    // JSON has no non-finite literals, so keep the output valid.
    if (!std::isfinite(value))
    {
        out << '0';
        return;
    }

    // "%g" is round-trippable at 9 significant digits and drops the trailing
    // decimal point for integers; the default "C" locale keeps the point a dot.
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.9g", value);
    out << buf;
}

void writeVector(std::ostream& out, char const* firstKey, float first,
        char const* secondKey, float second)
{
    out << '{';
    writeString(out, firstKey);
    out << ':';
    writeNumber(out, first);
    out << ',';
    writeString(out, secondKey);
    out << ':';
    writeNumber(out, second);
    out << '}';
}

void writeDataValue(std::ostream& out, widget::DataValue const& value)
{
    std::visit([&out](auto const& v)
    {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, std::string>)
        {
            writeString(out, v);
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            writeNumber(out, v);
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            out << (v ? "true" : "false");
        }
        else if constexpr (std::is_same_v<T, widget::DataMap>)
        {
            out << '{';
            bool first = true;
            for (auto const& entry : v)
            {
                if (!first)
                    out << ',';
                first = false;

                writeString(out, entry.first);
                out << ':';
                writeDataValue(out, entry.second);
            }
            out << '}';
        }
        else if constexpr (std::is_same_v<T, widget::DataArray>)
        {
            out << '[';
            bool first = true;
            for (auto const& element : v)
            {
                if (!first)
                    out << ',';
                first = false;

                writeDataValue(out, element);
            }
            out << ']';
        }
    }, value.value);
}

void writeObb(std::ostream& out, avg::Obb const& obb)
{
    auto center = obb.getCenter();
    auto size = obb.getSize();

    out << "{";
    writeString(out, "center");
    out << ':';
    writeVector(out, "x", center[0], "y", center[1]);
    out << ',';
    writeString(out, "size");
    out << ':';
    writeVector(out, "w", size[0], "h", size[1]);
    out << ',';
    writeString(out, "angle");
    out << ':';
    writeNumber(out, obb.getTransform().getRotation());
    out << '}';
}

void writeNode(std::ostream& out, widget::Introspection const& node)
{
    out << '{';

    if (node.name)
    {
        writeString(out, "name");
        out << ':';
        writeString(out, *node.name);
        out << ',';
    }

    writeString(out, "role");
    out << ':';
    writeString(out, node.role);

    out << ',';
    writeString(out, "capabilities");
    out << ":[";
    bool firstCapability = true;
    for (auto capability : node.capabilities)
    {
        if (!firstCapability)
            out << ',';
        firstCapability = false;

        writeString(out, widget::toString(capability));
    }
    out << ']';

    out << ',';
    writeString(out, "obb");
    out << ':';
    writeObb(out, node.obb);

    out << ',';
    writeString(out, "data");
    out << ":{";
    bool firstData = true;
    for (auto const& entry : node.data)
    {
        if (!firstData)
            out << ',';
        firstData = false;

        writeString(out, entry.first);
        out << ':';
        writeDataValue(out, entry.second);
    }
    out << '}';

    out << ',';
    writeString(out, "children");
    out << ":[";
    bool firstChild = true;
    for (auto const& child : node.children)
    {
        if (!firstChild)
            out << ',';
        firstChild = false;

        writeNode(out, *child);
    }
    out << ']';

    out << '}';
}

} // namespace

std::string toJson(widget::Introspection const& node)
{
    std::ostringstream out;
    writeNode(out, node);
    return out.str();
}

} // namespace bqui::agent
