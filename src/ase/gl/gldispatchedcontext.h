#pragma once

#include "glfunctions.h"
#include "dispatcher.h"

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
            dispatcher_.run([this, fn=std::forward<TFunc>(fn)]() mutable
            {
                fn(gl_);
            });
        }

        void wait();

        template <typename TFunc>
        void setIdleFunc(Dispatched d, std::chrono::duration<float> period,
                TFunc&& callback)
        {
            dispatcher_.setIdleFunc(d, period,
                    [this, fn=std::forward<TFunc>(callback)]()
                    {
                        fn(gl_);
                    });
        }

        void unsetIdleFunc(Dispatched);
        bool hasIdleFunc(Dispatched);

        void setGlFunctions(GlFunctions functions);
        GlFunctions const& getGlFunctions() const;

    protected:
        Dispatcher dispatcher_;
        GlFunctions gl_;
    };
} // namespace ase

