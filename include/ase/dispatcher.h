#pragma once

#include "asevisibility.h"

#include <optional>
#include <thread>
#include <condition_variable>
#include <functional>
#include <vector>

namespace ase
{
    struct ASE_EXPORT Dispatched {};

    class ASE_EXPORT Dispatcher
    {
    public:
        Dispatcher();
        Dispatcher(Dispatcher const& other) = delete;
        ~Dispatcher();

        Dispatcher& operator=(Dispatcher const& other) = delete;

        void run(std::function<void()>&& func);
        void wait() const;

        void setIdleFunc(Dispatched, std::chrono::duration<float> period,
                std::function<void()> cb);
        void unsetIdleFunc(Dispatched);
        bool hasIdleFunc(Dispatched);

    private:
        void runThread();

    private:
        // Mutable to allow const waiting
        mutable std::mutex mutex_;
        mutable std::condition_variable condition_;
        std::thread thread_;
        std::vector<std::function<void()> > funcs_;
        std::optional<std::function<void()>> idleCallback_;
        std::chrono::duration<float> idlePeriod_;
        bool running_;
        bool idle_;
    };
}

