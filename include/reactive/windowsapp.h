#pragma once

#include "app.h"

#include "window.h"
#include "reactivevisibility.h"

#include "signal/signal.h"

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive
{
    class REACTIVE_EXPORT WindowsApp : public AppImpl
    {
    public:
        WindowsApp();
        void addWindows(std::vector<Window> windows) override;
        int run(AnySignal<bool> running) && override;
        int run() && override;

    private:
        std::vector<btl::CloneOnCopy<Window>> windows_;
    };
}

