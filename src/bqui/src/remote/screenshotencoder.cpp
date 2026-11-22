#include "screenshotencoder.h"

#include "png.h"

#include <btl/threadpool.h>

#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <utility>

namespace bqui::remote
{

struct ScreenshotEncoder::Jobs
{
    std::mutex mutex;
    std::condition_variable done;
    int running = 0;
    bool stopped = false;

    bool start()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopped)
            return false;

        ++running;
        return true;
    }

    void finish()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            --running;
        }
        done.notify_all();
    }
};

ScreenshotEncoder::ScreenshotEncoder(btl::RunLoop& loop) :
    loop_(loop),
    jobs_(std::make_shared<Jobs>())
{
}

ScreenshotEncoder::~ScreenshotEncoder()
{
    std::unique_lock<std::mutex> lock(jobs_->mutex);
    jobs_->stopped = true;
    jobs_->done.wait(lock, [this] { return jobs_->running == 0; });
}

void ScreenshotEncoder::encode(ase::ReadbackImage image,
        ScreenshotPlan const& plan, Done done)
{
    btl::ThreadPool::getInstance().async(
            [loop = &loop_, jobs = jobs_, plan, image = std::move(image),
            done = std::move(done)]() mutable
            {
                if (!jobs->start())
                    return;

                struct Finish
                {
                    Jobs& jobs;
                    ~Finish() { jobs.finish(); }
                } finish{ *jobs };

                std::string png;
                std::string error;
                try
                {
                    auto rgb = renderScreenshot(image, plan);
                    png = encodePng(
                            static_cast<std::uint32_t>(plan.outputSize.x()),
                            static_cast<std::uint32_t>(plan.outputSize.y()),
                            rgb.data());
                }
                catch (std::exception const& e)
                {
                    error = e.what();
                }

                loop->post([done = std::move(done), png = std::move(png),
                        error = std::move(error)](
                            btl::RunLoop::Controller& controller) mutable
                    {
                        done(controller, std::move(png), error);
                    });
            });
}

} // namespace bqui::remote
