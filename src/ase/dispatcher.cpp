#include "dispatcher.h"

#include "debug.h"

namespace ase
{

Dispatcher::Dispatcher() :
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

void Dispatcher::run(std::function<void()>&& func)
{
    std::unique_lock<std::mutex> lock(mutex_);
    funcs_.push_back(func);
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
        std::function<void()> cb)
{
    idleCallback_ = std::move(cb);
    idlePeriod_ = period;
}

void Dispatcher::unsetIdleFunc(Dispatched)
{
    idleCallback_.reset();
}

bool Dispatcher::hasIdleFunc(Dispatched)
{
    return idleCallback_.has_value();
}

void Dispatcher::runThread()
{
    auto predicate = [this]()
    {
        return !this->running_ || !this->funcs_.empty();
    };

    while (running_)
    {
        std::vector<std::function<void()> > funcs;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (idleCallback_.has_value())
            {
                if (!condition_.wait_for(lock, idlePeriod_, predicate))
                {
                    (*idleCallback_)();
                }
            }
            else
            {
                condition_.wait(lock, predicate);
            }

            funcs.swap(funcs_);
            idle_ = false;
        }

        for (auto i = funcs.begin(); i != funcs.end(); ++i)
        {
            (*i)();
        }

        std::unique_lock<std::mutex> lock(mutex_);
        idle_ = true;
        condition_.notify_all();
    }
}

} //namespace ase

