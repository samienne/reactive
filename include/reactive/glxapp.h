#pragma once

#include "app.h"

#include "signal.h"

#include <btl/visibility.h>

#include <vector>

namespace reactive
{
    class BTL_VISIBLE GlxApp : public AppImpl
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

