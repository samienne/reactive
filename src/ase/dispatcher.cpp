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

void Dispatcher::runThread()
{
    while (running_)
    {
        std::vector<std::function<void()> > funcs;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this]
                    {
                        return !this->running_ || !this->funcs_.empty();
                    });

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

} //namespace

