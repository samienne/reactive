#pragma once

#include <GL/glx.h>

#include <ostream>
#include <mutex>

namespace ase
{
    class GlxWindow;
    class GlxPlatform;

    /**
     * @brief Wrapper class for GLXContext
     */
    class GlxContext
    {
    public:
        typedef std::mutex Mutex;
        typedef std::unique_lock<Mutex> Lock;

        GlxContext();
        GlxContext(GlxPlatform& platform);
        GlxContext(GlxContext&& other);
        GlxContext(GlxContext const&) = delete;
        ~GlxContext();

        GlxContext& operator=(GlxContext const&) = delete;
        GlxContext& operator=(GlxContext&& other);

        void makeCurrent(Lock const& lock, GLXDrawable drawable) const;

    private:
        friend std::ostream& operator<<(std::ostream& stream,
                ase::GlxContext const& context);
        GlxPlatform* platform_;
        GLXContext context_;
    };

    std::ostream& operator<<(std::ostream& stream,
            ase::GlxContext const& context);
}


