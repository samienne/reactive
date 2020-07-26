#pragma once

#include "app.h"

#include "reactivevisibility.h"

#include "signal/signal.h"

#include <btl/visibility.h>

namespace reactive
{
    class REACTIVE_EXPORT DummyApp : public AppImpl
    {
    public:
        void addWindows(std::vector<Window> windows) override;
        int run(AnySignal<bool> running) && override;
        int run() && override;
    };
}

