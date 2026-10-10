#pragma once

#include "widget/constraintlayout.h"

#include <bqui/modifier/buildermodifier.h>
#include <bqui/modifier/constraintsize.h>
#include <bqui/modifier/instancemodifier.h>
#include <bqui/modifier/widgetmodifier.h>

#include <bqui/widget/filler.h>
#include <bqui/widget/instance.h>
#include <bqui/widget/layoutspec.h>
#include <bqui/widget/widget.h>

#include <bqui/buildparams.h>
#include <bqui/inputarea.h>
#include <bqui/sizehint.h>

#include <bq/signal/constant.h>
#include <bq/signal/frameinfo.h>
#include <bq/signal/signalcontext.h>

#include <avg/vector.h>

#include <btl/uniqueid.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

// Geometry read-back helpers shared by the pure-solver layout tests: probe
// leaves tagged with an InputArea, realised through a SignalContext, and read
// back in window space.
namespace bqui::test
{

using namespace bqui::widget;
using namespace bq::signal;

struct Geometry
{
    avg::Vector2f position;
    avg::Vector2f size;
};

// Tags a widget's realised instance with an InputArea keyed by @p id, so its
// window-space geometry can be read back through the accumulated transform.
inline AnyWidget withArea(AnyWidget widget, btl::UniqueId id)
{
    return std::move(widget)
        | modifier::makeWidgetModifier(modifier::makeInstanceModifier(
                    [](Instance instance, btl::UniqueId id)
                    {
                        auto areas = instance.getInputAreas();
                        areas.push_back(makeInputArea(id, instance.getObb()));

                        return std::move(instance)
                            .setInputAreas(std::move(areas));
                    }, constant(id)));
}

// A content leaf sized by native size words read off one Band per axis: the
// natural at content strength, a min below or a max above it, and a flex where it
// grows. A max at the 10000 ceiling states no cap. An explicit size word applied
// after overrides it. Tagged for geometry read-back.
inline AnyWidget probe(btl::UniqueId id, Band width, Band height)
{
    float const uncapped = 10000.0f;

    AnyWidget widget = makeWidget()
        | modifier::defaultSize(avg::Vector2f(width.natural, height.natural));

    if (width.min < width.natural)
        widget = AnyWidget(std::move(widget) | modifier::minWidth(width.min));
    if (height.min < height.natural)
        widget = AnyWidget(std::move(widget) | modifier::minHeight(height.min));
    if (width.max > width.natural && width.max < uncapped)
        widget = AnyWidget(std::move(widget) | modifier::maxWidth(width.max));
    if (height.max > height.natural && height.max < uncapped)
        widget = AnyWidget(std::move(widget) | modifier::maxHeight(height.max));
    if (width.grow > 0.0f)
        widget = AnyWidget(std::move(widget) | modifier::growWidth());
    if (height.grow > 0.0f)
        widget = AnyWidget(std::move(widget) | modifier::growHeight());

    return withArea(std::move(widget), id);
}

// A pure-solver filler tagged for geometry read-back.
inline AnyWidget fillerProbe(btl::UniqueId id)
{
    return withArea(filler(), id);
}

inline Geometry readProbe(Instance const& instance, btl::UniqueId id)
{
    for (auto const& area : instance.getInputAreas())
        if (area.getId() == id)
            return Geometry{
                area.getTransform().getTranslation(),
                area.getObbs().front().getSize()
            };

    ADD_FAILURE() << "probe was not realised";
    return {};
}

// A region owner's solve settles a pass behind the build, so a few update passes
// are driven before the geometry is read.
inline Instance realiseConverged(AnyWidget widget, avg::Vector2f size)
{
    auto instanceSignal = std::move(widget)(BuildParams())(constant(size))
        .getInstance();

    auto context = makeSignalContext(std::move(instanceSignal));
    Instance instance = context.evaluate<0>().get<0>();

    for (uint64_t frame = 1; frame <= 5; ++frame)
    {
        context.update(bq::signal::FrameInfo(frame,
                    std::chrono::microseconds(0)));
        instance = context.evaluate<0>().get<0>();
    }

    return instance;
}

// A single evaluate with NO update passes. The forward-only pure solver settles
// on the first frame -- its constraints ride the builders and its solution is a
// build argument, not a tee'd input -- so the geometry read here has had no
// chance to settle behind.
inline Instance realiseOnce(AnyWidget widget, avg::Vector2f size)
{
    auto instanceSignal = std::move(widget)(BuildParams())(constant(size))
        .getInstance();

    auto context = makeSignalContext(std::move(instanceSignal));
    return context.evaluate<0>().get<0>();
}

Band const fixed100 = { 100.0f, 100.0f, 100.0f };
Band const fixed40 = { 40.0f, 40.0f, 40.0f };

// A content leaf whose natural height is inversely proportional to its resolved
// width -- narrower means taller (height == area / width) -- so it reflows only
// when phase 2 reads the resolved width rather than the natural width. It fills
// its row so its resolved width tracks the window. Tagged for geometry read-back.
inline AnyWidget reflowProbe(btl::UniqueId id, float area, float fallbackWidth)
{
    auto natural = [](float value)
    {
        Constraints c;
        c.natural = BandNatural{ value, contentStrength() };
        return c;
    };

    return withArea(makeWidget()
            | modifier::makeWidgetModifier(modifier::makeBuilderModifier(
                    [area, fallbackWidth, natural](widget::AnyBuilder builder)
                        -> widget::AnyBuilder
                    {
                        widget::BoxVariables box = builder.getBoxVariables();
                        builder.setPureLayout(widget::simplePureLayout(
                            constant(natural(fallbackWidth)),
                            [box, area, fallbackWidth, natural](
                                bq::signal::AnySignal<LayoutSolution> ws)
                            {
                                return bq::signal::AnySignal<Constraints>(
                                    std::move(ws).map(
                                    [box, area, fallbackWidth, natural](
                                            LayoutSolution const& sol)
                                    {
                                        float w = readObb(sol, box)
                                            .getSize()[0];
                                        return natural(area
                                                / (w > 0.0f ? w : fallbackWidth));
                                    }));
                            }));
                        return builder;
                    }))
            | modifier::fill(),
            id);
}

} // namespace bqui::test
