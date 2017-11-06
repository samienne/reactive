#pragma once

#include "window.h"

#include "signal.h"

#include <ase/platform.h>

#include <btl/unique.h>

namespace reactive
{
    class AppImpl
    {
    public:
        virtual ~AppImpl() = default;
        virtual void addWindows(std::vector<Window> windows) = 0;
        virtual int run(Signal<bool> running) && = 0;
        virtual int run() && = 0;
    };

    class App
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

