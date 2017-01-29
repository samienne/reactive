#pragma once

#include <thread>
#include <condition_variable>
#include <functional>
#include <vector>

namespace ase
{
    struct Dispatched {};

    class Dispatcher
    {
    public:
        Dispatcher();
        Dispatcher(Dispatcher const& other) = delete;
        ~Dispatcher();

        Dispatcher& operator=(Dispatcher const& other) = delete;

        void run(std::function<void()>&& func);
        void wait() const;

    private:
        void runThread();

    private:
        // Mutable to allow const waiting
        mutable std::mutex mutex_;
        mutable std::condition_variable condition_;
        std::thread thread_;
        std::vector<std::function<void()> > funcs_;
        bool running_;
        bool idle_;
    };
}

