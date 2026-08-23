#include "rendertree/snapshot.h"

#include "drawing.h"
#include "obb.h"
#include "rendertree/rendertreenode.h"
#include "textentry.h"
#include "transform.h"

#include <algorithm>
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

} // namespace avg
