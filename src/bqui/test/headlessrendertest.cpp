#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/widget/widget.h>

#include <bq/signal/constant.h>
#include <bq/signal/input.h>

#include <iostream>
#include <string>

using namespace bqui;

// A GPU-only smoke test for App::headless(true): it drives a real-backend
// offscreen run for several frames and checks it renders without opening a
// window or crashing. It needs a live GL backend, which the CI runners do not
// have, so it is built everywhere (to keep it compiling) but never registered as
// a test; run it by hand on a machine with a real GL context.
//
// The frame count is driven the same way apptest drives the dummy loop: the
// 'running' signal advances a step input each frame, so the run steps a fixed
// number of real offscreen frames and then stops.
int main()
{
    App app;
    app.headless(true);

    Window w = window(bq::signal::constant<std::string>("headless"));
    app.addWindow(w, widget::makeWidget());

    constexpr int targetFrames = 8;
    constexpr int maxFrames = 200;

    int frames = 0;
    auto step = bq::signal::makeInput(0);

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames)
                    return false;

                if (i >= targetFrames)
                    return false;

                step.handle.set(i + 1);
                return true;
            });

    int rc = app.run(running);

    bool const ok = rc == 0
        && frames >= targetFrames
        && frames < maxFrames
        && app.getWindows().size() == 1u;

    std::cout << "headless offscreen run: frames=" << frames << " rc=" << rc
        << " -> " << (ok ? "PASS" : "FAIL") << std::endl;

    return ok ? 0 : 1;
}
