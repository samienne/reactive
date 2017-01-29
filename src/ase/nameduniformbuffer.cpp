#include "nameduniformbuffer.h"

#include <cstring>

namespace ase
{

NamedUniformBuffer::NamedUniformBuffer()
{
}

NamedUniformBuffer::~NamedUniformBuffer()
{
}

NamedUniformIterator NamedUniformBuffer::begin() const
{
    return NamedUniformIterator(data_, offsets_.begin());
}

NamedUniformIterator NamedUniformBuffer::end() const
{
    return NamedUniformIterator(data_, offsets_.end());
}

void NamedUniformBuffer::uniform1f(std::string const& location, float v0)
{
    uniform1fv(location, 1, &v0);
}

void NamedUniformBuffer::uniform2f(std::string const& location, float v0,
        float v1)
{
    float v[2] = { v0, v1 };
    uniform2fv(location, 1, v);
}

void NamedUniformBuffer::uniform3f(std::string const& location, float v0,
        float v1, float v2)
{
    float v[3] = { v0, v1, v2 };
    uniform3fv(location, 1, v);
}

void NamedUniformBuffer::uniform4f(std::string const& location, float v0,
        float v1, float v2, float v3)
{
    float v[4] = { v0, v1, v2, v3 };
    uniform4fv(location, 1, v);
}

void NamedUniformBuffer::uniform1i(std::string const& location, int v0)
{
    uniform1iv(location, 1, &v0);
}

void NamedUniformBuffer::uniform2i(std::string const& location, int v0, int v1)
{
    int v[2] = { v0, v1 };
    uniform2iv(location, 1, v);
}

void NamedUniformBuffer::uniform3i(std::string const& location, int v0, int v1,
        int v2)
{
    int v[3] = { v0, v1, v2 };
    uniform3iv(location, 1, v);
}

void NamedUniformBuffer::uniform4i(std::string const& location, int v0, int v1,
        int v2, int v3)
{
    int v[4] = { v0, v1, v2, v3 };
    uniform4iv(location, 1, v);
}

void NamedUniformBuffer::uniform1ui(std::string const& location,
        unsigned int v0)
{
    uniform1uiv(location, 1, &v0);
}

void NamedUniformBuffer::uniform2ui(std::string const& location,
        unsigned int v0, unsigned int v1)
{
    unsigned int v[2] = { v0, v1 };
    uniform2uiv(location, 1, v);
}

void NamedUniformBuffer::uniform3ui(std::string const& location,
        unsigned int v0, unsigned int v1, unsigned int v2)
{
    unsigned int v[3] = { v0, v1, v2 };
    uniform3uiv(location, 1, v);
}

void NamedUniformBuffer::uniform4ui(std::string const& location,
        unsigned int v0, unsigned int v1, unsigned int v2, unsigned int v3)
{
    unsigned int v[4] = { v0, v1, v2, v3 };
    uniform4uiv(location, 1, v);
}

void NamedUniformBuffer::uniform1fv(std::string const& location, int count,
        float const* value)
{
    uniform(location, UniformType::uniform1fv, count, value);
}

void NamedUniformBuffer::uniform2fv(std::string const& location, int count,
        float const* value)
{
    uniform(location, UniformType::uniform2fv, count, value);
}

void NamedUniformBuffer::uniform3fv(std::string const& location, int count,
        float const* value)
{
    uniform(location, UniformType::uniform3fv, count, value);
}

void NamedUniformBuffer::uniform4fv(std::string const& location, int count,
        float const* value)
{
    uniform(location, UniformType::uniform4fv, count, value);
}

void NamedUniformBuffer::uniform1iv(std::string const& location, int count,
        int const* value)
{
    uniform(location, UniformType::uniform1iv, count, value);
}

void NamedUniformBuffer::uniform2iv(std::string const& location, int count,
        int const* value)
{
    uniform(location, UniformType::uniform2iv, count, value);
}

void NamedUniformBuffer::uniform3iv(std::string const& location, int count,
        int const* value)
{
    uniform(location, UniformType::uniform3iv, count, value);
}

void NamedUniformBuffer::uniform4iv(std::string const& location, int count,
        int const* value)
{
    uniform(location, UniformType::uniform4iv, count, value);
}

void NamedUniformBuffer::uniform1uiv(std::string const& location, int count,
        unsigned int const* value)
{
    uniform(location, UniformType::uniform1uiv, count, value);
}

void NamedUniformBuffer::uniform2uiv(std::string const& location, int count,
        unsigned int const* value)
{
    uniform(location, UniformType::uniform2uiv, count, value);
}

void NamedUniformBuffer::uniform3uiv(std::string const& location, int count,
        unsigned int const* value)
{
    uniform(location, UniformType::uniform3uiv, count, value);
}

void NamedUniformBuffer::uniform4uiv(std::string const& location, int count,
        unsigned int const* value)
{
    uniform(location, UniformType::uniform4uiv, count, value);
}

void NamedUniformBuffer::uniformMatrix4fv(std::string const& location,
        size_t count, float const* value)
{
    uniform(location, UniformType::uniformMatrix4fv, count, value);
}

void NamedUniformBuffer::uniform(std::string const& location, UniformType type,
        size_t count, void const* data)
{
    auto r = offsets_.insert(std::make_pair(location, data_.size()));
    auto len = getUniformSize(type, count);
    uint8_t const* p = nullptr;
    if (r.second)
    {
        auto t = (uint16_t)type;
        p = (uint8_t const*)&t;
        data_.push_back(p[0]);
        data_.push_back(p[1]);

        auto c = (uint16_t)count;
        p = (uint8_t const*)&c;
        data_.push_back(p[0]);
        data_.push_back(p[1]);

        auto const* p = (uint8_t const*)data;
        for (size_t i = 0; i < len; ++i)
            data_.push_back(*(p++));
    }
    else
    {
        assert((UniformType)*(uint16_t const*)(&*r.first) == type);
        assert(*((uint16_t const*)(&*r.first) + 1) == count);
        memcpy(((uint8_t*)(&*r.first)) + 4, data, len);
    }
}

} // namespace

