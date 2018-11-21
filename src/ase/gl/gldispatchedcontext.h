#pragma once

#include "glfunctions.h"
#include "dispatcher.h"

#include <functional>

namespace ase
{
    class GlDispatchedContext
    {
    public:
        GlDispatchedContext();
        virtual ~GlDispatchedContext() = default;

        template <typename TFunc>
        void dispatch(TFunc&& fn)
        {
            dispatcher_.run([this, fn=std::move(fn)]() mutable
            {
                fn(gl_);
            });
        }

        void wait();

        GlFunctions const& getGlFunctions() const;

    protected:
        Dispatcher dispatcher_;
        GlFunctions gl_;
    };
} // namespace ase

