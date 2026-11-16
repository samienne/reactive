#include <avg/rendercounters.h>
#include <avg/painter.h>
#include <avg/drawing.h>
#include <avg/targetimage.h>
#include <avg/builtineffects.h>
#include <avg/effect.h>
#include <avg/brush.h>
#include <avg/color.h>
#include <avg/pathbuilder.h>
#include <avg/shape.h>
#include <avg/transform.h>
#include <avg/vector.h>

#include <ase/dummyplatform.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/format.h>

#include <btl/runloop.h>

#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    avg::Vector2i const screenSize(800, 600);
    avg::Color const white(1.0f, 1.0f, 1.0f, 1.0f);
    avg::Color const grey(0.5f, 0.5f, 0.5f, 1.0f);

    avg::Shape box(float x, float y, float width, float height)
    {
        return avg::Shape(avg::PathBuilder(pmr::new_delete_resource())
                .start(x, y)
                .lineTo(x + width, y)
                .lineTo(x + width, y + height)
                .lineTo(x, y + height)
                .close()
                .build());
    }

    avg::Drawing::Element fill(float x, float y, float width, float height,
            avg::Color const& color)
    {
        return avg::Drawing::ShapeElement{ box(x, y, width, height),
            avg::Brush(color), std::nullopt };
    }

    avg::Drawing::Element panel(float x, float y, float width, float height)
    {
        avg::FrostedGlassOptions options;
        options.radius = 24.0f;
        return avg::Drawing::EffectElement{ box(x, y, width, height),
            std::make_shared<avg::Effect const>(avg::frostedGlass(options)),
            avg::Transform() };
    }

    avg::Drawing::Element brushPanel(float x, float y, float width,
            float height)
    {
        avg::FrostedGlassOptions options;
        options.radius = 24.0f;
        return avg::Drawing::ShapeElement{ box(x, y, width, height),
            avg::Brush(avg::frostedGlass(options)), std::nullopt };
    }

    avg::Drawing drawing(std::vector<avg::Drawing::Element> elements)
    {
        avg::Drawing result(pmr::new_delete_resource());
        for (auto& element : elements)
            result += std::move(element);

        return result;
    }

    avg::Drawing::Element scope(std::vector<avg::Drawing::Element> elements)
    {
        return drawing(std::move(elements)).scope().getElements().front();
    }

    // A background and two stripes for the panels to blur
    std::vector<avg::Drawing::Element> backdrop()
    {
        std::vector<avg::Drawing::Element> result;
        result.push_back(fill(0.0f, 0.0f, 800.0f, 600.0f, white));
        result.push_back(fill(0.0f, 100.0f, 800.0f, 50.0f, grey));
        result.push_back(fill(0.0f, 400.0f, 800.0f, 50.0f, grey));
        return result;
    }

    avg::Drawing scene(std::vector<avg::Drawing::Element> over)
    {
        auto elements = backdrop();
        for (auto& element : over)
            elements.push_back(std::move(element));

        return drawing(std::move(elements));
    }

    struct Scene
    {
        std::string name;
        std::function<avg::Drawing()> make;
        avg::RenderCounters expected;
    };

    // passes, draws, snapshots, scratchTargets, scratchBytes, pixelsShaded
    std::vector<Scene> const scenes = {
        { "noEffect",
            []() { return scene({}); },
            { 0, 1, 0, 0, 0, 0 } },
        { "onePanel",
            []() { return scene({ scope({ panel(100, 100, 200, 150) }) }); },
            { 4, 6, 1, 3, 3'840'000, 144'330 } },
        { "twoPanelsSideBySide",
            []()
            {
                return scene({ scope({ panel(50, 100, 200, 150),
                        panel(550, 100, 200, 150) }) });
            },
            { 8, 10, 1, 3, 3'840'000, 338'660 } },
        { "twoPanelsOverlapping",
            []()
            {
                return scene({ scope({ panel(100, 100, 200, 150) }),
                        scope({ panel(200, 175, 200, 150) }) });
            },
            { 8, 11, 2, 3, 3'840'000, 287'682 } },
        { "onePanelAsABrush",
            []()
            {
                return scene({ scope({ brushPanel(100, 100, 200, 150) }) });
            },
            { 4, 6, 1, 3, 3'840'000, 144'330 } },
        { "oneLargePanel",
            []() { return scene({ scope({ panel(20, 20, 760, 560) }) }); },
            { 4, 6, 1, 3, 3'840'000, 1'253'324 } },
    };
} // anonymous namespace

TEST(RenderCounters, theSceneSetRecordsItsExpectedCounters)
{
    for (Scene const& s : scenes)
    {
        SCOPED_TRACE(s.name);

        btl::RunLoop loop;
        ase::Platform platform = ase::makeDummyPlatform(loop);
        ase::RenderContext context = platform.makeRenderContext();
        avg::Painter painter(pmr::new_delete_resource(), context);
        avg::TargetImage target(context, screenSize, ase::FORMAT_SRGBA);

        painter.clearImage(target);
        EXPECT_EQ(s.expected, painter.paintToImage(target, 1.0f, s.make()));
        painter.flush();
    }
}

TEST(RenderCounters, aSecondPaintInOneFrameCountsOnlyItsOwnDraws)
{
    btl::RunLoop loop;
    ase::Platform platform = ase::makeDummyPlatform(loop);
    ase::RenderContext context = platform.makeRenderContext();
    avg::Painter painter(pmr::new_delete_resource(), context);
    avg::TargetImage target(context, screenSize, ase::FORMAT_SRGBA);
    Scene const& onePanel = scenes[1];

    painter.clearImage(target);
    EXPECT_EQ(onePanel.expected,
            painter.paintToImage(target, 1.0f, onePanel.make()));
    EXPECT_EQ(onePanel.expected,
            painter.paintToImage(target, 1.0f, onePanel.make()));
    painter.flush();
}
