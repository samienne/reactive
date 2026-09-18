#include <bqui/app.h>
#include <bqui/window.h>

#include <bqui/widget/button.h>
#include <bqui/widget/label.h>
#include <bqui/widget/vbox.h>

#include <ase/dummyplatform.h>

#include <btl/runloop.h>

#include <bq/signal/constant.h>
#include <bq/signal/input.h>

#include <avg/animationoptions.h>
#include <avg/curve/curves.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

using namespace bqui;

// A button whose onClick calls withAnimation used to crash with a use-after-free:
// withAnimation re-enters WindowBridge::makeTransaction, which rebuilds the
// widget instance and move-assigns fresh InputAreas over the stored ones --
// freeing the very InputArea whose handler is still executing on the button-up
// dispatch stack. The dispatch now runs from a copy, so the click must complete
// cleanly (and ASan-clean under the Sanitize profile).
//
// A local App is used with App::withAnimation so the handler transacts this
// window without touching the process-wide app() singleton. Injection goes
// through the genuine hit-test -> onClick path (App::testInjectClick).
TEST(inputReentrancy, withAnimationClickIsReentrancySafe)
{
    btl::RunLoop loop;
    App app;
    app.platform(ase::makeDummyPlatform(loop));
    App* appPtr = &app;

    auto counter = bq::signal::makeInput(0);
    auto clicks = std::make_shared<int>(0);

    // Clicking "Go" animates and changes the counter, forcing a widget rebuild
    // so makeTransaction re-maps areas_ while the Go handler is still running.
    auto root = widget::vbox({
            widget::label(counter.signal.map([](int n)
                    {
                        return std::string("n=") + std::to_string(n);
                    })),
            widget::button(bq::signal::constant<std::string>("Go"),
                bq::signal::constant(
                    [appPtr, handle = counter.handle, clicks]() mutable
                    {
                        auto guard = appPtr->withAnimation(avg::AnimationOptions{
                                std::chrono::milliseconds(300),
                                avg::curve::linear });
                        handle.set(++*clicks);
                    }))
        });

    Window w = window(bq::signal::constant<std::string>("reentrancy"));
    app.addWindow(w, std::move(root));

    constexpr int maxFrames = 100;
    int frames = 0;

    auto step = bq::signal::makeInput(0);
    auto running = step.signal.map([&](int i) -> bool
        {
            if (++frames > maxFrames)
                return false;

            if (i >= 2 && app.testWindowCount() > 0)
            {
                float x = 0.0f;
                float y = 0.0f;
                if (app.testFindText(0, "Go", x, y))
                    app.testInjectClick(0, x, y);
            }

            if (i >= 8)
            {
                w.close();
                return false;
            }

            step.handle.set(i + 1);
            return true;
        });

    int rc = app.run(running);

    EXPECT_EQ(0, rc);
    EXPECT_LT(frames, maxFrames);
    // The click reached the real onClick handler, and dispatching it (with a
    // re-entrant makeTransaction inside) did not use-after-free.
    EXPECT_GT(*clicks, 0);
}
