#pragma once

#include "uniformtype.h"

#include <string>
#include <vector>
#include <map>
#include <unordered_map>

namespace ase
{
    class NamedUniformView
    {
    public:
        inline NamedUniformView(void const* data,
                std::string const& location) :
            data_(data),
            location_(location)
        {
        }

        inline UniformType getType() const
        {
            return (UniformType)*(uint16_t const*)data_;
        }

        inline void const* getData() const
        {
            return (uint8_t const*)data_ + 4;
        }

        inline uint16_t getCount() const
        {
            return *(uint16_t const*)((char const*)data_ + 2);
        }

        inline size_t getElementSize() const
        {
            return 4 + getUniformSize(getType(), getCount());
        }

        inline std::string const& getLocation() const
        {
            return location_;
        }

    private:
        void const* data_;
        std::string const& location_;
    };

    class NamedUniformIterator
    {
    public:
        inline NamedUniformIterator(
                std::vector<uint8_t> const& data,
                std::unordered_map<std::string, size_t>::const_iterator iter) :
            data_(data),
            iter_(iter)
        {
        }

        inline NamedUniformView operator*() const
        {
            return NamedUniformView(&data_[iter_->second], iter_->first);
        }

        inline NamedUniformView operator->() const
        {
            return NamedUniformView(&data_[iter_->second], iter_->first);
        }

        inline NamedUniformIterator& operator++()
        {
            ++iter_;

            return *this;
        }

        inline NamedUniformIterator operator++(int)
        {
            return NamedUniformIterator(data_, iter_++);
        }

        inline bool operator==(NamedUniformIterator const& rhs) const
        {
            return iter_ == rhs.iter_;
        }

        inline bool operator!=(NamedUniformIterator const& rhs) const
        {
            return iter_ != rhs.iter_;
        }

    private:
        std::vector<uint8_t> const& data_;
        std::unordered_map<std::string, size_t>::const_iterator iter_;
    };

    class NamedUniformBuffer
    {
    public:
        NamedUniformBuffer();
        NamedUniformBuffer(NamedUniformBuffer const& other) = default;
        NamedUniformBuffer(NamedUniformBuffer&& other) = default;
        ~NamedUniformBuffer();

        NamedUniformBuffer& operator=(
                NamedUniformBuffer const& other) = default;
        NamedUniformBuffer& operator=(NamedUniformBuffer&& other) = default;

        NamedUniformIterator begin() const;
        NamedUniformIterator end() const;

        void uniform1f(std::string const& location, float v0);
        void uniform2f(std::string const& location, float v0, float v1);
        void uniform3f(std::string const& location, float v0, float v1,
                float v2);
        void uniform4f(std::string const& location, float v0, float v1,
                float v2, float v3);

        void uniform1i(std::string const& location, int v0);
        void uniform2i(std::string const& location, int v0, int v1);
        void uniform3i(std::string const& location, int v0, int v1, int v2);
        void uniform4i(std::string const& location, int v0, int v1, int v2,
                int v3);

        void uniform1ui(std::string const& location, unsigned int v0);
        void uniform2ui(std::string const& location, unsigned int v0,
                unsigned int v1);
        void uniform3ui(std::string const& location, unsigned int v0,
                unsigned int v1, unsigned int v2);
        void uniform4ui(std::string const& location, unsigned int v0,
                unsigned int v1, unsigned int v2, unsigned int v3);

        void uniform1fv(std::string const& location, int count,
                float const* value);
        void uniform2fv(std::string const& location, int count,
                float const* value);
        void uniform3fv(std::string const& location, int count,
                float const* value);
        void uniform4fv(std::string const& location, int count,
                float const* value);

        void uniform1iv(std::string const& location, int count,
                int const* value);
        void uniform2iv(std::string const& location, int count,
                int const* value);
        void uniform3iv(std::string const& location, int count,
                int const* value);
        void uniform4iv(std::string const& location, int count,
                int const* value);

        void uniform1uiv(std::string const& location, int count,
                unsigned int const* value);
        void uniform2uiv(std::string const& location, int count,
                unsigned int const* value);
        void uniform3uiv(std::string const& location, int count,
                unsigned int const* value);
        void uniform4uiv(std::string const& location, int count,
                unsigned int const* value);

        void uniformMatrix4fv(std::string const& location, size_t count,
                float const* value);

    private:
        void uniform(std::string const& location, UniformType type,
                size_t count, void const* data);

    private:
        std::unordered_map<std::string, size_t> offsets_;
        std::vector<uint8_t> data_;
        /*std::map<std::string, std::vector<float> > uniform1fv_;
        std::map<std::string, std::vector<float> > uniform2fv_;
        std::map<std::string, std::vector<float> > uniform3fv_;
        std::map<std::string, std::vector<float> > uniform4fv_;

        std::map<std::string, std::vector<int> > uniform1iv_;
        std::map<std::string, std::vector<int> > uniform2iv_;
        std::map<std::string, std::vector<int> > uniform3iv_;
        std::map<std::string, std::vector<int> > uniform4iv_;

        std::map<std::string, std::vector<unsigned int> > uniform1uiv_;
        std::map<std::string, std::vector<unsigned int> > uniform2uiv_;
        std::map<std::string, std::vector<unsigned int> > uniform3uiv_;
        std::map<std::string, std::vector<unsigned int> > uniform4uiv_;

        std::map<std::string, std::vector<float>> uniformMatrix4fv_;*/
    };
}

