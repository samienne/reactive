#pragma once

#include "app.h"

#include "signal.h"

#include <vector>

namespace reactive
{
    class GlxApp : public AppImpl
    {
    public:
        GlxApp();
        void addWindows(std::vector<Window> windows) override;
        int run(Signal<bool> running) && override;
        int run() && override;

    private:
        std::vector<Window> windows_;
    };
}

