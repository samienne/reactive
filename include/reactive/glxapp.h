#pragma once

#include "app.h"

#include "reactivevisibility.h"

#include "signal/signal.h"

#include <btl/visibility.h>

#include <vector>

namespace reactive
{
    class REACTIVE_EXPORT GlxApp : public AppImpl
    {
    public:
        GlxApp();
        void addWindows(std::vector<Window> windows) override;
        int run(AnySignal<bool> running) && override;
        int run() && override;

    private:
        std::vector<Window> windows_;
    };
}

