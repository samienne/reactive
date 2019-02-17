#pragma once

#include <btl/visibility.h>

#include <string>
#include <memory>

namespace ase
{
    class WindowImpl;

    class BTL_VISIBLE Window
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

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    protected:
        std::shared_ptr<WindowImpl> deferred_;
        inline WindowImpl* d() { return deferred_.get(); }
        inline WindowImpl const* d() const { return deferred_.get(); }
    };
}

