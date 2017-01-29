#pragma once

#include "rendertargetimpl.h"

#include <string>

namespace ase
{
    class WindowImpl : public RenderTargetImpl
    {
    public:
        virtual void setVisible(bool value) = 0;
        virtual bool isVisible() const = 0;

        virtual void setTitle(std::string&& title) = 0;
        virtual std::string const& getTitle() const = 0;

        virtual void clear() = 0;
    };
}

