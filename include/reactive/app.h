#pragma once

#include "window.h"

#include "signal.h"

#include <ase/platform.h>

#include <btl/unique.h>
#include <btl/visibility.h>

namespace reactive
{
    class BTL_VISIBLE AppImpl
    {
    public:
        virtual ~AppImpl() = default;
        virtual void addWindows(std::vector<Window> windows) = 0;
        virtual int run(Signal<bool> running) && = 0;
        virtual int run() && = 0;
    };

    class BTL_VISIBLE App
    {
    public:
        App();

        App windows(std::initializer_list<Window> windows) &&;

        int run(Signal<bool> running) &&;
        int run() &&;

    private:
        btl::unique<AppImpl> impl_;
    };
}

