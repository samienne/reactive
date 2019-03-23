#pragma once

#include <functional>

namespace btl
{
    class connection
    {
    public:
        inline connection()
        {
        }

        connection(connection const&) = delete;
        connection(connection&&) noexcept = default;

        connection& operator=(connection const&) = delete;

        connection& operator=(connection&& rhs) noexcept
        {
            disconnect();
            callbacks_ = std::move(rhs.callbacks_);
            return *this;
        }

        inline connection(std::function<void()> callback) :
            callbacks_({std::move(callback)})
        {
        }

        inline static connection on_disconnect(std::function<void()> callback)
        {
            return connection(std::move(callback));
        }

        inline ~connection()
        {
            disconnect();
        }

        inline void disconnect()
        {
            auto callbacks = std::move(callbacks_);
            callbacks_.clear();
            for (auto&& cb : callbacks)
                cb();
        }

        connection& operator+=(connection&& rhs)
        {
            for (auto&& cb : rhs.callbacks_)
                callbacks_.push_back(std::move(cb));
            rhs.callbacks_.clear();
            return *this;
        }

        connection operator+(connection&& rhs)&&
        {
            connection result;

            result.callbacks_ = std::move(callbacks_);
            for (auto&& cb : rhs.callbacks_)
                result.callbacks_.push_back(std::move(cb));
            callbacks_.clear();
            rhs.callbacks_.clear();

            return result;
        }

    private:
        std::vector<std::function<void()>> callbacks_;
    };
} // btl

