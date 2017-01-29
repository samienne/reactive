#include <reactive/rendering.h>
#include <reactive/signal/constant.h>
#include <reactive/signal/input.h>
#include <reactive/signal.h>

#include <avg/shape.h>
#include <avg/pen.h>
#include <avg/brush.h>
#include <avg/path.h>
#include <avg/pathspec.h>
#include <avg/painter.h>

#include <ase/rendercontext.h>
#include <ase/glxwindow.h>
#include <ase/glxplatform.h>

#include <iostream>
#include <chrono>

using namespace reactive;

int main()
{
    ase::GlxPlatform platform;
    ase::RenderContext& bgContext = platform.getDefaultContext();
    ase::RenderContext context(platform);

    avg::Painter painter(bgContext);

    bool running = true;
    ase::GlxWindow window(platform, ase::Vector2i(800, 600));
    window.setVisible(true);
    window.setTitle("Functional test");
    window.setCloseCallback([&running]() { running = false; });

    avg::Pen pen(avg::Color(), 4.0f);
    avg::Brush brush(avg::Color(0.2f, 0.8f, 0.2f));

    //Afui::Shape s(path, pen, brush);
    //avg::Shape drawing(reactive::makeRect(0.5f, 0.5f), pen, brush);

    std::cout << "sizeof(avg::Path) = " << sizeof(avg::Path) << std::endl;
    std::cout << "sizeof(avg::Pen) = " << sizeof(avg::Pen) << std::endl;
    std::cout << "sizeof(avg::Brush) = " << sizeof(avg::Brush) << std::endl;
    std::cout << "sizeof(avg::Shape) = " << sizeof(avg::Shape) << std::endl;
    std::cout << "sizeof(avg::Drawing) = " << sizeof(avg::Drawing) << std::endl;

    auto makeSquare = [](int frame)
    {
        float w = (float)(std::abs(frame % 60 - 30)) * 0.01 + 0.40;

        return makeRect(w * 200.0f, w * 200.0f);
    };

    auto translate = [](avg::Path const& path, avg::Vector2f v)
    {
        return path + v;
    };

    auto circle = [](int frame)
    {
        auto f = (float)(frame % 60) / 60.0f;
        auto x = std::cos(f * 2.0f * 3.14f) * 160.0f;
        auto y = std::sin(f * 2.0f * 3.14f) * 120.0f;
        return avg::Vector2f(x, y);
    };

    auto input = signal::input(0.1f);
    auto path = signal::map(makeSquare, input.signal);
    auto offset = signal::map(circle, input.signal);
    auto offsetPath = signal::map(translate, path, offset);
    auto shape = signal::map(&makeShape, offsetPath,
            signal::constant(brush), signal::constant(pen));

    int n = 0;

    std::chrono::steady_clock clock;
    auto startTime = clock.now();

    std::cout << "Signal size: " << sizeof(shape) << std::endl;

    RenderCache cache;

    while (running)
    {
        auto events = platform.getEvents();
        window.handleEvents(events);
        window.clear();

        avg::Drawing drawing;

        drawing = std::move(drawing) + shape.evaluate();

        cache = render(context, cache, window, painter, drawing);

        window.submitAll(context);
        context.present(window);

        n = (n + 1);// % 60;
        shape.beginTransaction();
        input.handle.set(n);
        shape.endTransaction(std::chrono::microseconds(16667));
    }

    auto endTime = clock.now();

    std::chrono::duration<double> time = endTime - startTime;

    std::cout << "Fps: " << (double)n / time.count() << std::endl;

    return 0;
}

