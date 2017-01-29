#include "app.h"
#include "glxapp.h"

#include "rendering.h"

#include "debug.h"

#include "reactive/signal/input.h"

#include <ase/rendercontext.h>

#include <chrono>

namespace reactive
{

App::App() :
    impl_(std::make_unique<GlxApp>())
{
}

App App::windows(std::initializer_list<Window> windows) &&
{
    std::vector<Window> ws;
    ws.reserve(windows.size());
    for (auto const& w : windows)
        ws.push_back(btl::clone(w));

    impl_->addWindows(std::move(ws));

    return std::move(*this);
}

int App::run(Signal<bool> running) &&
{
    return std::move(*impl_).run(running);
}

int App::run() &&
{
    return std::move(*impl_).run();
}

} // namespace

