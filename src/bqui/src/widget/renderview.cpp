#include "bqui/widget/renderview.h"

#include "renderviewstate.h"

#include "bqui/modifier/ondraw.h"
#include "bqui/modifier/setsizehint.h"
#include "bqui/simplesizehint.h"

#include <avg/brush.h>
#include <avg/image.h>
#include <avg/shape.h>

#include <bq/signal/constant.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace bqui::widget
{

namespace
{
    constexpr long kMaxViewPixels = 8192;

    avg::Drawing drawView(
            std::shared_ptr<bqui::detail::RenderViewState> const& state,
            avg::DrawContext const& context,
            avg::Vector2f size)
    {
        ase::Vector2i pixels(
                static_cast<int>(std::min(std::lround(size.x()),
                        kMaxViewPixels)),
                static_cast<int>(std::min(std::lround(size.y()),
                        kMaxViewPixels)));

        if (pixels.x() <= 0 || pixels.y() <= 0)
            return context.drawing();

        std::optional<ase::Texture> texture;
        if (auto collector = bqui::detail::RenderViewCollector::current())
        {
            texture = state->prepare(collector->getContext(), pixels);
            collector->add(state);
        }
        else
        {
            texture = state->getTexture();
        }

        if (!texture)
            return context.drawing();

        auto path = context.pathBuilder()
            .start(0.0f, 0.0f)
            .lineTo(size.x(), 0.0f)
            .lineTo(size.x(), size.y())
            .lineTo(0.0f, size.y())
            .close()
            .build();

        return avg::Shape(std::move(path)).fill(avg::Brush(avg::Image(
                        *texture, avg::Image::RowOrder::BottomFirst)));
    }
} // anonymous namespace

AnyWidget renderView(RenderView view)
{
    return makeWidget()
        | modifier::setSizeHint(bq::signal::constant(simpleSizeHint(
                        {{ 0.0f, 100.0f, 100000.0f }},
                        {{ 0.0f, 100.0f, 100000.0f }})))
        | modifier::onDraw(
            [state = bqui::detail::RenderViewAccess::getState(view)](
                avg::DrawContext const& context,
                avg::Vector2f size)
            {
                return drawView(state, context, size);
            });
}

} // namespace bqui::widget
