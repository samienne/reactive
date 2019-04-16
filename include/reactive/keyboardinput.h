#pragma once

#include "inputresult.h"

#include "signal/input.h"
#include "stream/handle.h"
#include "reactivevisibility.h"

#include <avg/obb.h>

#include <ase/keyevent.h>

#include <btl/option.h>
#include <btl/function.h>

namespace reactive
{
    using KeyEvent = ase::KeyEvent;
    /** keyboard input
     *
     * Request focus (onClick + on some random event)
     * Track focus (onFocus)
     * Get keypress events to focused handle
     * Get the matching key release to the same handle as pressed
     * Keyboard navigation
     */

    class REACTIVE_EXPORT KeyboardInput
    {
    public:
        using FocusHandle = signal::InputHandle<bool>;
        using Handler = btl::Function<InputResult(KeyEvent const& e)>;

        KeyboardInput(avg::Obb obb);
        KeyboardInput(avg::Obb obb,
                btl::option<FocusHandle> focusHandle,
                btl::option<Handler> handlers,
                bool requestFocus,
                bool hasFocus);
        KeyboardInput requestFocus(bool focus) &&;
        KeyboardInput setFocus(bool focus) &&;
        KeyboardInput onKeyEvent(Handler handler) &&;
        KeyboardInput setFocusHandle(FocusHandle handle) &&;
        KeyboardInput setFocusHandle(btl::option<FocusHandle> handle) &&;
        KeyboardInput setFocusable(bool focusable) &&;
        KeyboardInput transform(avg::Transform const& t) &&;
        KeyboardInput setObb(avg::Obb obb) &&;

        bool getRequestFocus() const;
        bool hasFocus() const;
        bool isFocusable() const;
        avg::Obb const& getObb() const;

        btl::option<FocusHandle> const& getFocusHandle() const;
        btl::option<Handler> const& getHandler() const;

    private:
        avg::Obb obb_;

        // Handle to send the focus status to
        btl::option<FocusHandle> focusHandle_;

        btl::option<Handler> handler_;

        bool requestFocus_ = false;
        bool hasFocus_ = false;
        bool focusable_ = false;
    };
} // reactive

