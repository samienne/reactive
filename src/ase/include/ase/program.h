#pragma once

#include "asevisibility.h"

#include <optional>
#include <string>
#include <memory>

namespace ase
{
    class ProgramImpl;
    class ShaderInterface;

    /**
     * @brief A linked shader program together with its declared interface.
     */
    class ASE_EXPORT Program
    {
    public:
        Program(std::shared_ptr<ProgramImpl> impl);
        ~Program();

        int getUniformLocation(std::string const& name) const;
        int getAttribLocation(std::string const& name) const;

        /**
         * @brief The interface the program was created with.
         */
        ShaderInterface const& getInterface() const;

        /**
         * @brief The interface the backend actually compiled, if it can tell.
         *
         * Backends without program introspection return nothing.
         */
        std::optional<ShaderInterface> reflect() const;

        bool operator==(Program const& rhs) const;
        bool operator!=(Program const& rhs) const;
        bool operator<(Program const& rhs) const;

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    private:
        std::shared_ptr<ProgramImpl> deferred_;
        inline ProgramImpl* d() { return deferred_.get(); }
        inline ProgramImpl const* d() const { return deferred_.get(); }
    };
}
