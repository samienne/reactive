// A minimal headless, agentic bqui app for an external inspector-protocol
// client (e.g. a Python bridge) to launch and drive. It builds one window with
// a labelled button that bumps a counter shown in a label, so a client can
// `window.introspect` to see the button and counter and `window.inject` a
// click to watch the counter change.
//
// The app launches paused and connects back to the agent endpoint, taken from
// argv[1] or, failing that, the REACTIVE_AGENT_ENDPOINT environment variable
// the app already uses. A TCP endpoint (`tcp://host:port`, `host:port`, or
// `:port`) makes it reachable from any cross-platform client.

#include <bqui/widget/button.h>
#include <bqui/widget/label.h>
#include <bqui/widget/vbox.h>

#include <bqui/modifier/setwidgetintrospection.h>
#include <bqui/modifier/focusgroup.h>

#include <bqui/window.h>
#include <bqui/app.h>

#include <bq/signal/signal.h>
#include <bq/signal/input.h>
#include <bq/signal/constant.h>

#include <ase/platform.h>
#include <ase/dummyplatform.h>

#include <cstdlib>
#include <iostream>
#include <string>

using namespace bqui;

int main(int argc, char** argv)
{
    // Endpoint precedence: an explicit argv[1] wins, else the same env var the
    // app reads for the agent endpoint.
    std::string endpoint;
    if (argc > 1)
        endpoint = argv[1];
    else if (char const* env = std::getenv("REACTIVE_AGENT_ENDPOINT"))
        endpoint = env;

    if (endpoint.empty())
    {
        std::cerr << "inspectorapp: no agent endpoint (pass argv[1] or set "
                     "REACTIVE_AGENT_ENDPOINT); running without a client.\n";
    }

    // A counter the button bumps and the label displays. bindToFunction reads
    // the current value at click time, so no separate mirror of the state.
    auto counter = bq::signal::makeInput(0);

    auto counterLabel = widget::label(counter.signal.map(
                [](int c) { return std::string("Count: ") + std::to_string(c); }))
        | modifier::setName("counterLabel");

    auto incrementButton = widget::button(
            std::string("Increment"),
            counter.signal.bindToFunction(
                [handle = counter.handle](int c) mutable { handle.set(c + 1); }))
        | modifier::setName("incrementButton");

    auto content = widget::vbox({
            widget::label(std::string("Inspector Demo"))
                | modifier::setName("title"),
            std::move(counterLabel),
            std::move(incrementButton),
        })
        | modifier::focusGroup();

    return App()
        .platform(ase::makeDummyPlatform())
        .agentic(true)
        .agentEndpoint(endpoint)
        .addWindow(window(
                    bq::signal::constant<std::string>("Inspector Demo"),
                    std::move(content)))
        .run();
}
