#pragma once

#include "signaltraits.h"

#include "reactive/connection.h"

#include <memory>

namespace reactive::signal
{
    class FrameInfo;

    template <typename T>
    class SignalBase
    {
    public:
        virtual ~SignalBase() = default;
        virtual T evaluate() const = 0;
        virtual bool hasChanged() const = 0;
        virtual Connection observe(std::function<void()> const& callback) = 0;
        virtual UpdateResult updateBegin(FrameInfo const&) = 0;
        virtual UpdateResult updateEnd(FrameInfo const&) = 0;
        virtual Annotation annotate() const = 0;
        virtual bool isCached() const = 0;
    };
} // reactive::signal

