#include "rendertree/snapshot.h"

#include "drawing.h"
#include "obb.h"
#include "rendertree/rendertreenode.h"
#include "textentry.h"
#include "transform.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

namespace avg
{

namespace
{

void collectText(
        pmr::vector<Drawing::Element> const& elements,
        Transform const& transform,
        std::vector<SnapshotText>& result
        )
{
    for (auto const& element : elements)
    {
        if (auto const* text = std::get_if<TextEntry>(&element))
        {
            result.push_back(SnapshotText {
                    text->getText(),
                    transform * text->getControlObb()
                    });
        }
        else if (auto const* clip = std::get_if<Drawing::ClipElement>(&element))
        {
            collectText(
                    (*clip->subDrawing).elements,
                    transform * clip->transform,
                    result
                    );
        }
    }
}

void writeNumber(std::ostringstream& out, float value)
{
    out << (std::isfinite(value) ? value : 0.0f);
}

void writeString(std::ostringstream& out, std::string const& value)
{
    out << '"';

    for (char c : value)
    {
        switch (c)
        {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\b':
            out << "\\b";
            break;
        case '\f':
            out << "\\f";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x",
                        static_cast<unsigned int>(
                            static_cast<unsigned char>(c)));
                out << buffer;
            }
            else
            {
                out << c;
            }
        }
    }

    out << '"';
}

void writeObb(std::ostringstream& out, Obb const& obb)
{
    auto center = obb.getCenter();
    auto scale = obb.getTransform().getScale();
    auto size = obb.getSize();

    out << "{\"center\":{\"x\":";
    writeNumber(out, center[0]);
    out << ",\"y\":";
    writeNumber(out, center[1]);
    out << "},\"size\":{\"w\":";
    writeNumber(out, size[0] * scale);
    out << ",\"h\":";
    writeNumber(out, size[1] * scale);
    out << "},\"angle\":";
    writeNumber(out, obb.getTransform().getRotation());
    out << '}';
}

void writeNode(std::ostringstream& out, SnapshotNode const& node)
{
    out << "{\"type\":";
    writeString(out, node.type);

    if (node.id)
        out << ",\"id\":" << node.id->getValue();

    out << ",\"obb\":";
    writeObb(out, node.obb);

    if (node.leaving)
        out << ",\"leaving\":true";

    out << ",\"text\":[";

    bool first = true;
    for (auto const& text : node.text)
    {
        if (!first)
            out << ',';
        first = false;

        out << "{\"text\":";
        writeString(out, text.text);
        out << ",\"obb\":";
        writeObb(out, text.obb);
        out << '}';
    }

    out << "],\"children\":[";

    first = true;
    for (auto const& child : node.children)
    {
        if (!first)
            out << ',';
        first = false;

        writeNode(out, child);
    }

    out << "]}";
}

} // anonymous namespace

SnapshotNode makeSnapshotNode(
        std::string type,
        RenderTreeNode const& node,
        Obb const& parentObb,
        std::chrono::milliseconds time
        )
{
    SnapshotNode result;

    result.type = std::move(type);
    result.id = node.getId();
    result.obb = parentObb.getTransform() * node.getObbAt(time);

    return result;
}

SnapshotNode makeLeafSnapshotNode(
        std::string type,
        RenderTreeNode const& node,
        DrawContext const& context,
        Obb const& parentObb,
        std::chrono::milliseconds time
        )
{
    auto result = makeSnapshotNode(std::move(type), node, parentObb, time);

    auto drawing = node.draw(context, parentObb, time).first;

    collectText(drawing.getElements(), Transform(), result.text);

    return result;
}

void clipSnapshotText(SnapshotNode& node, Obb const& clip)
{
    auto bounds = clip.getBoundingRect();

    node.text.erase(
            std::remove_if(node.text.begin(), node.text.end(),
                [&](SnapshotText const& text)
                {
                    return !text.obb.getBoundingRect().overlaps(bounds);
                }),
            node.text.end()
            );

    for (auto& child : node.children)
        clipSnapshotText(child, clip);
}

std::string toJson(Snapshot const& snapshot)
{
    std::ostringstream out;

    // snprintf would take the decimal separator from the process locale.
    out.imbue(std::locale::classic());
    out << std::setprecision(9);

    out << "{\"version\":" << Snapshot::version
        << ",\"time\":" << snapshot.time.count()
        << ",\"obb\":";

    writeObb(out, snapshot.obb);

    out << ",\"root\":";

    if (snapshot.root)
        writeNode(out, *snapshot.root);
    else
        out << "null";

    out << '}';

    return out.str();
}

} // namespace avg
