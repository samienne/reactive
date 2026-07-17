#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/widget/label.h>
#include <bqui/widget/introspection.h>

#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <ase/platform.h>
#include <ase/dummyplatform.h>

#include <gtest/gtest.h>

using namespace bqui;
using namespace bqui::widget;

namespace
{
    // A headless platform capped to a few frames, so a run is bounded and fast.
    ase::Platform makeBoundedHeadless(uint64_t frames)
    {
        auto platform = ase::makeDummyPlatform();
        platform.getImpl<ase::DummyPlatform>().setMaxFrames(frames);
        return platform;
    }
} // namespace

TEST(headlessApp, runsAndExitsWithNoWindow)
{
    // A minimal app driven by the headless backend must build, tick a bounded
    // number of frames, and return cleanly without ever opening an OS window.
    auto widget = label("Headless");

    int result = app()
        .platform(makeBoundedHeadless(5))
        .addWindow(
                window(bq::signal::constant<std::string>("Test"),
                    std::move(widget)))
        .run();

    EXPECT_EQ(0, result);
}

TEST(headlessApp, introspectionResolvesWithNoWindow)
{
    // Introspection is available headless: build a widget, realise it at a
    // concrete size, and read a resolved snapshot with sane window-space obbs —
    // no platform or window involved at all.
    auto widget = label("Snapshot");

    auto sig = std::move(widget)(BuildParams{})(
                bq::signal::constant(avg::Vector2f(200.0f, 100.0f)))
            .getIntrospection();

    auto node = bq::signal::makeSignalContext(std::move(sig))
        .evaluate<0>().get<0>();

    EXPECT_EQ("Label", node.role);
    EXPECT_GT(node.obb.getSize()[0], 0.0f);
    EXPECT_GT(node.obb.getSize()[1], 0.0f);
}
