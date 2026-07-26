#pragma once

#include <bq/signal/signal.h>

#include <btl/uniqueid.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace bqui
{
    class AppDeferred;

    /** @brief A window's persistent state, shared by every copy of a Window.
     *
     * Holds the window's identity, title and close callbacks — but none of its
     * contents: the widget is supplied to App::addWindow at mount and lives in
     * the app-owned impl, so a widget may capture the Window that owns it
     * without a retain cycle. The link back to the app is weak, so close() is a
     * no-op for a window that was never added or whose app is gone.
     *
     * The mutable parts (the app reference and the close callbacks) are guarded
     * by a mutex, so a Window is safe to read and to close from any thread.
     */
    class WindowData
    {
    public:
        explicit WindowData(bq::signal::AnySignal<std::string> const& title) :
            title_(title.share())
        {
        }

        WindowData(WindowData const&) = delete;
        WindowData& operator=(WindowData const&) = delete;

        btl::UniqueId getId() const
        {
            return id_;
        }

        bq::signal::AnySignal<std::string> const& getTitle() const
        {
            return title_;
        }

        void addCloseCallback(std::function<void()> const& cb)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closeCallbacks_.push_back(cb);
        }

        void invokeOnClose() const
        {
            std::vector<std::function<void()>> cbs;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                cbs = closeCallbacks_;
            }

            for (auto const& cb : cbs)
                cb();
        }

        /** @brief Removes the window from the app that owns it.
         *
         * A no-op if the window is in no app, or if that app is gone.
         */
        void close() const;

        bool hasApp() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return !app_.expired();
        }

        void setApp(std::weak_ptr<AppDeferred> app) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            app_ = std::move(app);
        }

        void clearApp() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            app_.reset();
        }

    private:
        btl::UniqueId const id_ = btl::makeUniqueId();
        bq::signal::AnySignal<std::string> title_;

        mutable std::mutex mutex_;
        mutable std::weak_ptr<AppDeferred> app_;
        std::vector<std::function<void()>> closeCallbacks_;
    };
} // namespace bqui
