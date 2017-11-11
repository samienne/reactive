#pragma once

#include "inputresult.h"

#include "signal/input.h"
#include "stream/handle.h"

#include <avg/obb.h>

#include <ase/keyevent.h>

#include <btl/option.h>
#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

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

    class BTL_CLASS_VISIBLE KeyboardInput
    {
    public:
        using FocusHandle = signal::InputHandle<bool>;
        using Handler = std::function<InputResult(KeyEvent const& e)>;

        BTL_VISIBLE KeyboardInput(avg::Obb obb);
        BTL_VISIBLE KeyboardInput(avg::Obb obb,
                btl::option<FocusHandle> focusHandle,
                btl::option<Handler> handlers,
                bool requestFocus,
                bool hasFocus);
        BTL_VISIBLE KeyboardInput requestFocus(bool focus) &&;
        BTL_VISIBLE KeyboardInput setFocus(bool focus) &&;
        BTL_VISIBLE KeyboardInput onKeyEvent(Handler handler) &&;
        BTL_VISIBLE KeyboardInput setFocusHandle(FocusHandle handle) &&;
        BTL_VISIBLE KeyboardInput setFocusHandle(btl::option<FocusHandle> handle) &&;
        BTL_VISIBLE KeyboardInput setFocusable(bool focusable) &&;
        BTL_VISIBLE KeyboardInput transform(avg::Transform const& t) &&;
        BTL_VISIBLE KeyboardInput setObb(avg::Obb obb) &&;

        BTL_VISIBLE bool getRequestFocus() const;
        BTL_VISIBLE bool hasFocus() const;
        BTL_VISIBLE bool isFocusable() const;
        BTL_VISIBLE avg::Obb const& getObb() const;

        BTL_VISIBLE btl::option<FocusHandle> const& getFocusHandle() const;
        BTL_VISIBLE btl::option<Handler> const& getHandler() const;

    private:
        avg::Obb obb_;

        // Handle to send the focus status to
        btl::option<FocusHandle> focusHandle_;

        btl::option<Handler> handler_;

        bool requestFocus_ = false;
        bool hasFocus_ = false;
        bool focusable_ = false;
    };
}

BTL_VISIBILITY_POP

