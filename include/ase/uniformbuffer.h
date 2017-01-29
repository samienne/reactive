#pragma once

#include "uniformtype.h"

#include <btl/hash.h>
#include <btl/uhash.h>

#include <map>
#include <vector>
#include <memory>
#include <cassert>

namespace ase
{
    class Program;
    class NamedUniformBuffer;
    class UniformBufferDeferred;

    class UniformView
    {
    public:
        inline UniformView(void const* data) :
            data_(data)
        {
        }

        inline UniformType getType() const
        {
            return (UniformType)*(uint16_t const*)data_;
        }

        inline void const* getData() const
        {
            return (uint8_t const*)data_ + 8;
        }

        inline uint16_t getCount() const
        {
            return *(uint16_t const*)((char const*)data_ + 2);
        }

        inline int32_t getLocation() const
        {
            return *(int32_t const*)((char const*)data_ + 4);
        }

        inline size_t getElementSize() const
        {
            return 8 + getUniformSize(getType(), getCount());
        }

    private:
        void const* data_;
    };

    class UniformIterator
    {
    public:
        inline UniformIterator(void const* data) :
            data_(data)
        {
        }

        inline UniformView operator*() const
        {
            return UniformView(data_);
        }

        inline UniformView operator->() const
        {
            return UniformView(data_);
        }

        inline UniformIterator& operator++()
        {
            data_ = (char const*)data_ + UniformView(data_).getElementSize();

            return *this;
        }

        inline UniformIterator operator++(int)
        {
            auto* oldData = data_;
            data_ = (char const*)data_ + UniformView(data_).getElementSize();

            return UniformIterator(oldData);
        }

        inline bool operator==(UniformIterator const& rhs) const
        {
            return data_ == rhs.data_;
        }

        inline bool operator!=(UniformIterator const& rhs) const
        {
            return data_ != rhs.data_;
        }

    private:
        void const* data_;
    };

    struct UniformHeader
    {
        inline UniformHeader(UniformType type, uint16_t count,
                int32_t location) :
            type(type),
            count(count),
            location(location)
        {
            assert(count > 0);
        }

        UniformType type;
        uint16_t count;
        int32_t location;
    };

    class UniformStack
    {
    public:
        inline void push(UniformHeader const& uniform)
        {
            push((uint16_t)uniform.type);
            push((uint16_t)uniform.count);
            push(uniform.location);
        }

        inline void push(void const* data, size_t len)
        {
            uint8_t const* p = (uint8_t const*)data;
            for (size_t i = 0; i < len; ++i)
            {
                data_.push_back(*p);
                ++p;
            }
        }

        template <typename T>
        inline void push(T const& t)
        {
            push((void const*)&t, sizeof(T));
        }

        inline size_t getLen() const
        {
            return data_.size();
        }

        inline void* getData() const
        {
            return (void*)data_.data();
        }

        inline UniformIterator begin() const
        {
            return UniformIterator(data_.data());
        }

        inline UniformIterator end() const
        {
            return UniformIterator(data_.data() + data_.size());
        }

        template <typename THash>
        friend void hash_append(THash& h,
                ase::UniformStack const& stack) noexcept
        {
            using btl::hash_append;
            h(stack.data_.data(), stack.data_.size());
        }

    private:
    private:
        std::vector<uint8_t> data_;
    };

    class UniformBuffer
    {
    public:
        UniformBuffer();
        UniformBuffer(UniformBuffer const& other) = default;
        UniformBuffer(UniformBuffer&& other) = default;
        UniformBuffer(Program const& program,
                NamedUniformBuffer const& buffer);
        ~UniformBuffer();

        UniformBuffer& operator=(UniformBuffer const& other) = default;
        UniformBuffer& operator=(UniformBuffer&& other) = default;

        bool operator==(UniformBuffer const& rhs) const;
        bool operator!=(UniformBuffer const& rhs) const;
        bool operator<(UniformBuffer const& rhs) const;

        size_t getHash() const;

        UniformIterator begin() const;
        UniformIterator end() const;

    private:
        void uniform(int location, UniformType type, size_t count,
                void const* data);
        void uniform1f(int location, float v0);
        void uniform2f(int location, float v0, float v1);
        void uniform3f(int location, float v0, float v1, float v2);
        void uniform4f(int location, float v0, float v1, float v2, float v3);

        void uniform1i(int location, int v0);
        void uniform2i(int location, int v0, int v1);
        void uniform3i(int location, int v0, int v1, int v2);
        void uniform4i(int location, int v0, int v1, int v2, int v3);

        void uniform1ui(int location, unsigned int v0);
        void uniform2ui(int location, unsigned int v0, unsigned int v1);
        void uniform3ui(int location, unsigned int v0, unsigned int v1,
                unsigned int v2);
        void uniform4ui(int location, unsigned int v0, unsigned int v1,
                unsigned int v2, unsigned int v3);

        void uniform1fv(int location, int count, float const* value);
        void uniform2fv(int location, int count, float const* value);
        void uniform3fv(int location, int count, float const* value);
        void uniform4fv(int location, int count, float const* value);

        void uniform1iv(int location, int count, int const* value);
        void uniform2iv(int location, int count, int const* value);
        void uniform3iv(int location, int count, int const* value);
        void uniform4iv(int location, int count, int const* value);

        void uniform1uiv(int location, int count, unsigned int const* value);
        void uniform2uiv(int location, int count, unsigned int const* value);
        void uniform3uiv(int location, int count, unsigned int const* value);
        void uniform4uiv(int location, int count, unsigned int const* value);

        void uniformMatrix4fv(int location, int count, float const* value);

        size_t calculateHash();

        void hashAppend(btl::deferred_hash<size_t>& h) const noexcept;

        template <typename THash>
        friend void hash_append(THash& h, UniformBuffer const& buffer) noexcept
        {
            btl::deferred_hash<typename THash::result_type> d(h);
            buffer.hashAppend(d);
            h = std::move(*d.template target<THash>());
        }

    private:
        std::shared_ptr<UniformBufferDeferred> deferred_;
        inline UniformBufferDeferred* d() { return deferred_.get(); }
        inline UniformBufferDeferred const* d() const
        {
            return deferred_.get();
        }
    };
}

namespace std
{
    template<>
    struct hash<ase::UniformBuffer>
    {
        typedef ase::UniformBuffer argument_type;
        typedef std::size_t value_type;

        value_type operator()(argument_type const& s)
        {
            return s.getHash();
        }
    };
}

