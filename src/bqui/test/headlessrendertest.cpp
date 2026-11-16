#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/widget/widget.h>

#include <avg/brush.h>
#include <avg/color.h>
#include <avg/drawing.h>
#include <avg/painter.h>
#include <avg/pathbuilder.h>
#include <avg/shape.h>

#include <ase/platform.h>
#include <ase/readback.h>
#include <ase/rendercontext.h>
#include <ase/window.h>

#include <bq/signal/constant.h>
#include <bq/signal/input.h>

#include <btl/runloop.h>

#include <pmr/new_delete_resource.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <string>

using namespace bqui;

// GPU-only checks for headless rendering: pixel readback from a real-backend
// offscreen window, then a smoke test for App::headless(true) that drives an
// offscreen run for several frames and checks it renders without opening a
// window or crashing. It needs a live GL backend, which the CI runners do not
// have, so it is built everywhere (to keep it compiling) but never registered as
// a test; run it by hand on a machine with a real GL context.
namespace
{
    using Rgba = std::array<int, 4>;

    avg::Drawing fillRect(pmr::memory_resource* memory, float x, float y,
            float width, float height, avg::Color color)
    {
        return avg::Shape(avg::PathBuilder(memory)
                .start(x, y)
                .lineTo(x + width, y)
                .lineTo(x + width, y + height)
                .lineTo(x, y + height)
                .close()
                .build())
            .fill(avg::Brush(color));
    }

    Rgba pixelAt(ase::ReadbackImage const& image, int x, int y)
    {
        auto const* p = image.pixels.data()
            + (static_cast<size_t>(y) * static_cast<size_t>(image.size[0])
                    + static_cast<size_t>(x)) * 4u;
        return { p[0], p[1], p[2], p[3] };
    }

    bool matchesRgb(Rgba const& actual, Rgba const& expected)
    {
        for (size_t i = 0; i < 3; ++i)
        {
            if (std::abs(actual[i] - expected[i]) > 2)
                return false;
        }

        return true;
    }

    // Paints four solid quadrants into a real-backend offscreen window and
    // reads the frame back. avg's y axis points up, so the quadrants drawn at
    // high y must come back in the first rows.
    bool captureQuadrants()
    {
        btl::RunLoop loop;
        auto platform = ase::makeDefaultPlatform(loop);
        auto context = platform.makeRenderContext();

        ase::Vector2i const size(64, 32);
        auto window = platform.makeWindow(context, size, true);

        if (!window.canCapture())
        {
            std::cout << "capture: window cannot capture -> FAIL" << std::endl;
            return false;
        }

        auto* memory = pmr::new_delete_resource();
        float const w = 32.0f;
        float const h = 16.0f;

        avg::Drawing drawing(memory);
        drawing += fillRect(memory, 0.0f, h, w, h, avg::Color(1, 0, 0, 1));
        drawing += fillRect(memory, w, h, w, h, avg::Color(0, 1, 0, 1));
        drawing += fillRect(memory, 0.0f, 0.0f, w, h, avg::Color(0, 0, 1, 1));
        drawing += fillRect(memory, w, 0.0f, w, h,
                avg::Color(0.5f, 0.5f, 0.5f, 1));

        auto promise = std::make_shared<std::promise<ase::CaptureResult>>();
        auto result = promise->get_future();

        window.captureNextFrame([promise](ase::CaptureResult capture)
            {
                promise->set_value(std::move(capture));
            });

        avg::Painter painter(memory, context);
        painter.setSize(window.getSize());
        painter.clearWindow(window);
        painter.paintToWindow(window, drawing);
        painter.presentWindow(window);

        if (result.wait_for(std::chrono::seconds(10))
                != std::future_status::ready)
        {
            std::cout << "capture: no result -> FAIL" << std::endl;
            return false;
        }

        auto capture = result.get();
        if (!capture.valid())
        {
            std::cout << "capture: error "
                << static_cast<int>(capture.err()) << " -> FAIL" << std::endl;
            return false;
        }

        auto const& image = capture.value().image;

        Rgba const topLeft = pixelAt(image, 8, 4);
        Rgba const topRight = pixelAt(image, 56, 4);
        Rgba const bottomLeft = pixelAt(image, 8, 28);
        Rgba const bottomRight = pixelAt(image, 56, 28);

        bool const sizeOk = image.size == size
            && image.pixels.size() == static_cast<size_t>(64 * 32 * 4);

        // 0.5 linear encodes to 188 in sRGB.
        bool const ok = sizeOk
            && capture.value().presentIndex == 1u
            && matchesRgb(topLeft, { 255, 0, 0, 255 })
            && matchesRgb(topRight, { 0, 255, 0, 255 })
            && matchesRgb(bottomLeft, { 0, 0, 255, 255 })
            && matchesRgb(bottomRight, { 188, 188, 188, 255 });

        auto print = [](char const* name, Rgba const& c)
        {
            std::cout << " " << name << "=(" << c[0] << "," << c[1] << ","
                << c[2] << "," << c[3] << ")";
        };

        std::cout << "capture: size=" << image.size[0] << "x" << image.size[1]
            << " bytes=" << image.pixels.size();
        print("topLeft", topLeft);
        print("topRight", topRight);
        print("bottomLeft", bottomLeft);
        print("bottomRight", bottomRight);
        std::cout << " -> " << (ok ? "PASS" : "FAIL") << std::endl;

        return ok;
    }
} // anonymous namespace

int main()
{
    if (!captureQuadrants())
        return 1;

    App app;
    app.headless(true);

    Window w = window(bq::signal::constant<std::string>("headless"));
    app.addWindow(w, widget::makeWidget());

    // The frame count is driven the same way apptest drives the dummy loop:
    // the 'running' signal advances a step input each frame, so the run steps a
    // fixed number of real offscreen frames and then stops.
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
