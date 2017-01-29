#include "uniformbuffer.h"

#include "nameduniformbuffer.h"
#include "program.h"

#include "debug.h"

#include <btl/fnv1a.h>
#include <btl/hash.h>
#include <btl/uhash.h>

#include <string>
#include <functional>

namespace ase
{
class UniformBufferDeferred
{
public:
    UniformStack data_;
    size_t hash_ = 0u;
};

UniformBuffer::UniformBuffer()
{
}

UniformBuffer::UniformBuffer(Program const& program,
        NamedUniformBuffer const& buffer) :
    deferred_(std::make_shared<UniformBufferDeferred>())
{
    /*std::map<std::string, std::vector<float> >  uniform1fv =
        buffer.getUniform1fv();
    std::map<std::string, std::vector<float> > const& uniform2fv =
        buffer.getUniform2fv();
    std::map<std::string, std::vector<float> > const& uniform3fv =
        buffer.getUniform3fv();
    std::map<std::string, std::vector<float> > const& uniform4fv =
        buffer.getUniform4fv();

    std::map<std::string, std::vector<int> > const& uniform1iv =
        buffer.getUniform1iv();
    std::map<std::string, std::vector<int> > const& uniform2iv =
        buffer.getUniform2iv();
    std::map<std::string, std::vector<int> > const& uniform3iv =
        buffer.getUniform3iv();
    std::map<std::string, std::vector<int> > const& uniform4iv =
        buffer.getUniform4iv();

    std::map<std::string, std::vector<unsigned int> > const& uniform1uiv =
        buffer.getUniform1uiv();
    std::map<std::string, std::vector<unsigned int> > const& uniform2uiv =
        buffer.getUniform2uiv();
    std::map<std::string, std::vector<unsigned int> > const& uniform3uiv =
        buffer.getUniform3uiv();
    std::map<std::string, std::vector<unsigned int> > const& uniform4uiv =
        buffer.getUniform4uiv();

    auto const& uniformMatrix4fv = buffer.getUniformMatrix4fv();

    for (std::map<std::string, std::vector<float> >::const_iterator i =
            uniform1fv.begin(); i != uniform1fv.end(); ++i)
    {
        UniformBuffer::uniform1fv(program.getUniformLocation(i->first),
                i->second.size(), &i->second[0]);
    }

    for (std::map<std::string, std::vector<float> >::const_iterator i =
            uniform2fv.begin(); i != uniform2fv.end(); ++i)
    {
        UniformBuffer::uniform2fv(program.getUniformLocation(i->first),
                i->second.size() / 2, &i->second[0]);
    }

    for (std::map<std::string, std::vector<float> >::const_iterator i =
            uniform3fv.begin(); i != uniform3fv.end(); ++i)
    {
        UniformBuffer::uniform3fv(program.getUniformLocation(i->first),
                i->second.size() / 3, &i->second[0]);
    }

    for (std::map<std::string, std::vector<float> >::const_iterator i =
            uniform4fv.begin(); i != uniform4fv.end(); ++i)
    {
        UniformBuffer::uniform4fv(program.getUniformLocation(i->first),
                i->second.size() / 4, &i->second[0]);
    }

    //

    for (std::map<std::string, std::vector<int> >::const_iterator i =
            uniform1iv.begin(); i != uniform1iv.end(); ++i)
    {
        UniformBuffer::uniform1iv(program.getUniformLocation(i->first),
                i->second.size(), &i->second[0]);
    }

    for (std::map<std::string, std::vector<int> >::const_iterator i =
            uniform2iv.begin(); i != uniform2iv.end(); ++i)
    {
        UniformBuffer::uniform2iv(program.getUniformLocation(i->first),
                i->second.size() / 2, &i->second[0]);
    }

    for (std::map<std::string, std::vector<int> >::const_iterator i =
            uniform3iv.begin(); i != uniform3iv.end(); ++i)
    {
        UniformBuffer::uniform3iv(program.getUniformLocation(i->first),
                i->second.size() / 3, &i->second[0]);
    }

    for (std::map<std::string, std::vector<int> >::const_iterator i =
            uniform4iv.begin(); i != uniform4iv.end(); ++i)
    {
        UniformBuffer::uniform4iv(program.getUniformLocation(i->first),
                i->second.size() / 4, &i->second[0]);
    }

    //


    for (std::map<std::string, std::vector<unsigned int> >::const_iterator i =
            uniform1uiv.begin(); i != uniform1uiv.end(); ++i)
    {
        UniformBuffer::uniform1uiv(program.getUniformLocation(i->first),
                i->second.size(), &i->second[0]);
    }

    for (std::map<std::string, std::vector<unsigned int> >::const_iterator i =
            uniform2uiv.begin(); i != uniform2uiv.end(); ++i)
    {
        UniformBuffer::uniform2uiv(program.getUniformLocation(i->first),
                i->second.size() / 2, &i->second[0]);
    }

    for (std::map<std::string, std::vector<unsigned int> >::const_iterator i =
            uniform3uiv.begin(); i != uniform3uiv.end(); ++i)
    {
        UniformBuffer::uniform3uiv(program.getUniformLocation(i->first),
                i->second.size() / 3, &i->second[0]);
    }

    for (std::map<std::string, std::vector<unsigned int> >::const_iterator i =
            uniform4uiv.begin(); i != uniform4uiv.end(); ++i)
    {
        UniformBuffer::uniform4uiv(program.getUniformLocation(i->first),
                i->second.size() / 4, &i->second[0]);
    }

    for (auto const& uniform : uniformMatrix4fv)
    {
        UniformBuffer::uniformMatrix4fv(
                program.getUniformLocation(uniform.first),
                uniform.second.size() / 16, uniform.second.data());
    }*/

    for (auto&& u : buffer)
    {
        int location = program.getUniformLocation(u.getLocation());
        uniform(location, u.getType(), u.getCount(), u.getData());
    }

    d()->hash_ = calculateHash();
}

UniformBuffer::~UniformBuffer()
{
}

bool UniformBuffer::operator==(UniformBuffer const& rhs) const
{
    return d()->hash_ == rhs.d()->hash_;
}

bool UniformBuffer::operator!=(UniformBuffer const& rhs) const
{
    return d()->hash_ != rhs.d()->hash_;
}

bool UniformBuffer::operator<(UniformBuffer const& rhs) const
{
    return d()->hash_ < rhs.d()->hash_;
}

size_t UniformBuffer::getHash() const
{
    if (!d())
        return 0;

    return d()->hash_;
}

void UniformBuffer::uniform(int location, UniformType type, size_t count,
        void const* data)
{
    d()->data_.push(UniformHeader(type, count, location));
    auto len = getUniformSize(type, count);
    auto const* p = (uint8_t const*)data;
    for (size_t i = 0; i < len; ++i)
        d()->data_.push(p[i]);
}

void UniformBuffer::uniform1f(int location, float v0)
{
    uniform1fv(location, 1, &v0);
}

void UniformBuffer::uniform2f(int location, float v0, float v1)
{
    float v[2] = { v0, v1 };
    uniform2fv(location, 1, v);
}

void UniformBuffer::uniform3f(int location, float v0, float v1, float v2)
{
    float v[3] = { v0, v1, v2 };
    uniform3fv(location, 1, v);
}

void UniformBuffer::uniform4f(int location, float v0, float v1, float v2,
        float v3)
{
    float v[4] = { v0, v1, v2, v3 };
    uniform4fv(location, 1, v);
}

void UniformBuffer::uniform1i(int location, int v0)
{
    uniform1iv(location, 1, &v0);
}

void UniformBuffer::uniform2i(int location, int v0, int v1)
{
    int v[2] = { v0, v1 };
    uniform2iv(location, 1, v);
}

void UniformBuffer::uniform3i(int location, int v0, int v1, int v2)
{
    int v[3] = { v0, v1, v2 };
    uniform3iv(location, 1, v);
}

void UniformBuffer::uniform4i(int location, int v0, int v1, int v2, int v3)
{
    int v[4] = { v0, v1, v2, v3 };
    uniform4iv(location, 1, v);
}

void UniformBuffer::uniform1ui(int location, unsigned int v0)
{
    uniform1uiv(location, 1, &v0);
}

void UniformBuffer::uniform2ui(int location, unsigned int v0, unsigned int v1)
{
    unsigned int v[2] = { v0, v1 };
    uniform2uiv(location, 1, v);
}

void UniformBuffer::uniform3ui(int location, unsigned int v0, unsigned int v1,
        unsigned int v2)
{
    unsigned int v[3] = { v0, v1, v2 };
    uniform3uiv(location, 1, v);
}

void UniformBuffer::uniform4ui(int location, unsigned int v0, unsigned int v1,
        unsigned int v2, unsigned int v3)
{
    unsigned int v[4] = { v0, v1, v2, v3 };
    uniform4uiv(location, 1, v);
}

void UniformBuffer::uniform1fv(int location, int count, float const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform1fv, count, location));
    for (int i = 0; i < count; ++i)
        d()->data_.push(value[i]);
}

void UniformBuffer::uniform2fv(int location, int count, float const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform2fv, count, location));
    for (int i = 0; i < (count*2); ++i)
        d()->data_.push(value[i]);

}

void UniformBuffer::uniform3fv(int location, int count, float const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform3fv, count, location));
    for (int i = 0; i < (count*3); ++i)
        d()->data_.push(value[i]);
}

void UniformBuffer::uniform4fv(int location, int count, float const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform4fv, count, location));
    for (int i = 0; i < (count*4); ++i)
        d()->data_.push(value[i]);
}

void UniformBuffer::uniform1iv(int location, int count, int const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform1iv, count, location));
    for (int i = 0; i < (count*1); ++i)
        d()->data_.push(value[i]);
}

void UniformBuffer::uniform2iv(int location, int count, int const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform2iv, count, location));
    for (int i = 0; i < (count*2); ++i)
        d()->data_.push(value[i]);
}

void UniformBuffer::uniform3iv(int location, int count, int const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform3iv, count, location));
    for (int i = 0; i < (count*3); ++i)
        d()->data_.push(value[i]);
}

void UniformBuffer::uniform4iv(int location, int count, int const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform4iv, count, location));
    for (int i = 0; i < (count*4); ++i)
        d()->data_.push(value[i]);
}

void UniformBuffer::uniform1uiv(int location, int count,
        unsigned int const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform1uiv, count, location));
    for (int i = 0; i < (count*1); ++i)
        d()->data_.push(value[i]);
}

void UniformBuffer::uniform2uiv(int location, int count,
        unsigned int const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform2uiv, count, location));
    for (int i = 0; i < (count*2); ++i)
        d()->data_.push(value[i]);
}

void UniformBuffer::uniform3uiv(int location, int count,
        unsigned int const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform3uiv, count, location));
    for (int i = 0; i < (count*3); ++i)
        d()->data_.push(value[i]);
}

void UniformBuffer::uniform4uiv(int location, int count,
        unsigned int const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniform4uiv, count, location));
    for (int i = 0; i < (count*4); ++i)
        d()->data_.push(value[i]);
}

void UniformBuffer::uniformMatrix4fv(int location, int count,
        float const* value)
{
    if (location == -1)
        return;

    d()->data_.push(UniformHeader(UniformType::uniformMatrix4fv, count,
                location));

    for (int i = 0; i < (count*16); ++i)
        d()->data_.push(value[i]);
}

size_t UniformBuffer::calculateHash()
{
    btl::uhash<btl::fnv1a> h;

    return h(d()->data_);
}

void UniformBuffer::hashAppend(btl::deferred_hash<size_t>& h) const noexcept
{
    hash_append(h, d()->data_);
}

UniformIterator UniformBuffer::begin() const
{
    return d()->data_.begin();
}

UniformIterator UniformBuffer::end() const
{
    return d()->data_.end();
}

} // namespace

