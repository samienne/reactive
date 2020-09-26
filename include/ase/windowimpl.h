#pragma once

#include "asevisibility.h"

#include <string>

namespace ase
{
    class ASE_EXPORT WindowImpl
    {
    public:
        virtual ~WindowImpl() = default;

        virtual void setVisible(bool value) = 0;
        virtual bool isVisible() const = 0;

        virtual void setTitle(std::string&& title) = 0;
        virtual std::string const& getTitle() const = 0;

        virtual void clear() = 0;
    };
}

