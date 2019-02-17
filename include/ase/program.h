#pragma once

#include <btl/visibility.h>

#include <string>
#include <memory>

namespace ase
{
    class ProgramImpl;
    class VertexShader;
    class FragmentShader;
    class RenderContext;
    class Platform;

    class BTL_VISIBLE Program
    {
    public:
        Program(std::shared_ptr<ProgramImpl> impl);
        ~Program();

        int getUniformLocation(std::string const& name) const;
        int getAttribLocation(std::string const& name) const;

        bool operator==(Program const& rhs) const;
        bool operator!=(Program const& rhs) const;
        bool operator<(Program const& rhs) const;

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    private:
        std::shared_ptr<ProgramImpl> makeImpl(RenderContext& context,
                VertexShader const& vertexShader,
                FragmentShader const& fragmentShader);

    private:
        std::shared_ptr<ProgramImpl> deferred_;
        inline ProgramImpl* d() { return deferred_.get(); }
        inline ProgramImpl const* d() const { return deferred_.get(); }
    };
}

