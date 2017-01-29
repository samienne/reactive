#pragma once

#include "rendertarget.h"

#include <string>

namespace ase
{
    class WindowImpl;

    class Window : public RenderTarget
    {
    public:
        Window();
        Window(std::shared_ptr<WindowImpl>&& impl);
        ~Window();

        Window(Window&&) = default;
        Window& operator=(Window&&) = default;

        void setVisible(bool value);
        bool isVisible() const;

        void setTitle(std::string title);
        std::string const& getTitle() const;

        void clear();

    protected:
        inline WindowImpl* d() { return reinterpret_cast<WindowImpl*>(
                RenderTarget::d()); }
        inline WindowImpl const* d() const
        {
            return reinterpret_cast<WindowImpl const*>(RenderTarget::d());
        }
    };
}

