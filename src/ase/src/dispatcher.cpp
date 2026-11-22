#include "dispatcher.h"

#include "debug.h"

#include <tracy/Tracy.hpp>

namespace ase
{

Dispatcher::Dispatcher(std::string name) :
    name_(name.empty() ? "Dispatcher" : std::move(name)),
    idleGeneration_(0),
    running_(true),
    idle_(true)
{
    thread_ = std::thread([this]()
            {
                this->runThread();
            });
}

Dispatcher::~Dispatcher()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        running_ = false;
        condition_.notify_all();
    }
    thread_.join();
}

void Dispatcher::run(std::function<void(Dispatched)>&& func)
{
    std::unique_lock<std::mutex> lock(mutex_);
    funcs_.push_back(std::move(func));
    condition_.notify_all();
}

void Dispatcher::wait() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this]{ return this->idle_
            && this->funcs_.empty(); });
}

void Dispatcher::setIdleFunc(Dispatched,
        std::chrono::duration<float> period,
        std::function<void(Dispatched)> cb)
{
    idleCallback_ = std::move(cb);
    idlePeriod_ = period;
    ++idleGeneration_;
}

void Dispatcher::unsetIdleFunc(Dispatched)
{
    idleCallback_.reset();
    ++idleGeneration_;
}

bool Dispatcher::hasIdleFunc(Dispatched)
{
    return idleCallback_.has_value();
}

void Dispatcher::runThread()
{
    ZoneScoped;
    ZoneName(name_.c_str(), name_.length());

    auto predicate = [this]()
    {
        return !this->running_ || !this->funcs_.empty();
    };

    while (running_)
    {
        ZoneScopedN("Dispatcher main loop");
        std::vector<std::function<void(Dispatched)> > funcs;

        {
            ZoneScopedN("Dispatcher handle idle");
            std::unique_lock<std::mutex> lock(mutex_);
            if (!predicate())
            {
                if (idleCallback_.has_value())
                {
                    ZoneScopedN("Dispatcher idle wait");
                    if (!condition_.wait_for(lock, idlePeriod_, predicate))
                    {
                        ZoneScopedN("Dispatcher idle callback");
                        // The callback may replace or unset itself, so
                        // run it moved out and restore it only if it did not.
                        auto callback = std::move(*idleCallback_);
                        auto const generation = idleGeneration_;
                        callback(Dispatched());
                        if (idleGeneration_ == generation)
                        {
                            idleCallback_ = std::move(callback);
                        }
                    }
                }
                else
                {
                    ZoneScopedN("Dispatcher wait");
                    condition_.wait(lock, predicate);
                }
            }

            funcs.swap(funcs_);
            idle_ = false;
        }

        for (auto i = funcs.begin(); i != funcs.end(); ++i)
        {
            ZoneScopedN("Dispatcher run task");
            (*i)(Dispatched());
        }

        // Release the tasks' captures before wait() can return.
        funcs.clear();

        {
            ZoneScopedN("Dispatcher notify");
            std::unique_lock<std::mutex> lock(mutex_);
            idle_ = true;
            condition_.notify_all();
        }
    }
}

} //namespace ase
