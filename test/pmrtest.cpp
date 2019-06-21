#include <pmr/monotonic_buffer_resource.h>
#include <pmr/polymorphic_allocator.h>
#include <pmr/new_delete_resource.h>

#include <gtest/gtest.h>

#include <vector>
#include <list>
#include <iostream>

using namespace pmr;

class PrintResource : public memory_resource
{
public:
    explicit PrintResource(memory_resource* upstream) :
        upstream_(upstream)
    {
    }

private:
    void* do_allocate(std::size_t size, std::size_t alignment) override
    {
        auto* p = upstream_->allocate(size, alignment);
        std::cout << "allocated: " << size << "(" << alignment << ") "
            << p << std::endl;

        return p;
    }

    void do_deallocate(void* p, std::size_t size, std::size_t alignment) override
    {
        std::cout << "deallocate: " << size << "(" << alignment << ") "
            << p << std::endl;
        return upstream_->deallocate(p, size, alignment);
    }

    bool do_is_equal(memory_resource const& rhs) const override
    {
        return this == &rhs;
    }

    memory_resource* upstream_;
};

class TraceResource : public memory_resource
{
public:
    TraceResource(memory_resource* upstream) :
        upstream_(upstream)
    {
    }

    ~TraceResource()
    {
        assert(allocations_.empty());
    }

    std::size_t getAllocationCount() const
    {
        return allocationCount_;
    }

    std::size_t getTotalAllocation() const
    {
        return totalAllocation_;
    }

private:
    void* do_allocate(std::size_t size, std::size_t alignment) override
    {
        totalAllocation_ += size;
        allocationCount_ += 1;

        void* p = upstream_->allocate(size, alignment);
        allocations_.push_back(Allocation{ p, size, alignment });

        return p;
    }

    void do_deallocate(void* p, std::size_t size, std::size_t alignment) override
    {
        bool found = false;
        for (auto i = allocations_.begin(); i != allocations_.end(); ++i)
        {
            Allocation& a = *i;
            if (a.p == p)
            {
                if (a.size != size)
                {
                    std::cout << "size mismatch: " << a.size << " != " << size
                        << std::endl;
                }

                if (a.alignment != alignment)
                {
                    std::cout << "alignment mismatch: "
                        << a.alignment << " != " << alignment << std::endl;
                }

                assert(a.size == size && a.alignment == alignment);
                found = true;
                allocations_.erase(i);
                break;
            }
        }

        //assert(found);
        if (!found)
        {
            std::cout << "didn't find allocation " << p << std::endl;
        }

        return upstream_->deallocate(p, size, alignment);
    }

    bool do_is_equal(memory_resource const& rhs) const override
    {
        return this == &rhs;
    }

    memory_resource* upstream_;

    struct Allocation
    {
        void* p;
        std::size_t size;
        std::size_t alignment;
    };

    std::vector<Allocation> allocations_;
    std::size_t allocationCount_ = 0;
    std::size_t totalAllocation_ = 0;
};

TEST(pmr, asdf)
{
    auto tr = TraceResource(new_delete_resource());
    auto& r = tr;

    std::vector<std::string, polymorphic_allocator<std::string>> v(&r);;

    for (int i = 0; i < 4096; ++i)
        v.push_back("test");

    for (auto const& s : v)
        EXPECT_EQ("test", s);
}

TEST(pmr, monotonic_buffer_resource_without_buffer)
{
    auto tr = TraceResource(new_delete_resource());

    auto mono = monotonic_buffer_resource(&tr);
    auto& r = mono;

    std::list<std::string, polymorphic_allocator<std::string>> v(&r);;

    for (int i = 0; i < 4096; ++i)
        v.push_back("test");

    for (auto const& s : v)
        EXPECT_EQ("test", s);
}

TEST(pmr, monotonic_buffer_resource_with_buffer)
{
    std::array<char, 512> buf;

    auto tr = TraceResource(new_delete_resource());

    auto mono = monotonic_buffer_resource(buf.data(), buf.size(), &tr);
    auto& r = mono;

    std::list<std::string, polymorphic_allocator<std::string>> v(&r);;

    for (int i = 0; i < 4096; ++i)
        v.push_back("test");

    for (auto const& s : v)
        EXPECT_EQ("test", s);

    std::cout << "allocations: " <<  tr.getAllocationCount()
        << " (" << tr.getTotalAllocation() << " bytes)"
        << std::endl;
}

TEST(pmr, monotonic_buffer_resource_alignment)
{
    auto mono = monotonic_buffer_resource(new_delete_resource());

    for (int i = 0; i < 256; ++i)
    {
        auto* p = mono.allocate(6, 8);
        EXPECT_EQ(0u, reinterpret_cast<std::uintptr_t>(p) % 8);
        mono.deallocate(p, 14, 16);

        p = mono.allocate(14, 16);
        EXPECT_EQ(0u, reinterpret_cast<std::uintptr_t>(p) % 16);
        mono.deallocate(p, 14, 16);
    }
}

