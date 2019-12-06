#pragma once

#include "window.h"

#include "signal.h"
#include "reactivevisibility.h"

#include <ase/platform.h>

#include <btl/unique.h>
#include <btl/visibility.h>

namespace reactive
{
    class REACTIVE_EXPORT AppImpl
    {
    public:
        virtual ~AppImpl() = default;
        virtual void addWindows(std::vector<Window> windows) = 0;
        virtual int run(AnySignal<bool> running) && = 0;
        virtual int run() && = 0;
    };

    class REACTIVE_EXPORT App
    {
    public:
        App();

        App windows(std::initializer_list<Window> windows) &&;

        int run(AnySignal<bool> running) &&;
        int run() &&;

    private:
        btl::unique<AppImpl> impl_;
    };
}

