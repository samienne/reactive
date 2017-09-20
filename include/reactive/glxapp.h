#pragma once

#include "app.h"

#include <vector>

namespace reactive
{
    class GlxApp : public AppImpl
    {
    public:
        GlxApp();
        void addWindows(std::vector<Window> windows) override;
        int run(signal2::Signal<bool> running) && override;
        int run() && override;

    private:
        std::vector<Window> windows_;
    };
}

