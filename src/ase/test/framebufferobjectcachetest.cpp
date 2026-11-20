#include "framebufferobjectcache.h"

#include <ase/attachmentframebuffer.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace ase;

namespace
{
    class FakeObjects
    {
    public:
        auto creator()
        {
            return [this](AttachmentFramebuffer::Snapshot const& snapshot)
            {
                versions.push_back(snapshot.version);
                events.push_back("create " + std::to_string(next));
                return next++;
            };
        }

        auto destroyer()
        {
            return [this](int handle)
            {
                events.push_back("destroy " + std::to_string(handle));
            };
        }

        std::vector<std::string> events;
        std::vector<uint64_t> versions;
        int next = 1;
    };

    using Events = std::vector<std::string>;
} // anonymous namespace

TEST(framebufferObjectCache, createsOncePerFramebuffer)
{
    FakeObjects objects;
    FramebufferObjectCache<int> cache;
    auto first = std::make_shared<AttachmentFramebuffer>();
    auto second = std::make_shared<AttachmentFramebuffer>();

    int firstHandle = cache.get(*first, objects.creator(), objects.destroyer());
    EXPECT_EQ(firstHandle,
        cache.get(*first, objects.creator(), objects.destroyer()));
    int secondHandle =
        cache.get(*second, objects.creator(), objects.destroyer());

    EXPECT_NE(firstHandle, secondHandle);
    EXPECT_EQ(2u, cache.size());
    EXPECT_EQ(Events({ "create 1", "create 2" }), objects.events);
}

TEST(framebufferObjectCache, versionChangeDestroysThenCreates)
{
    FakeObjects objects;
    FramebufferObjectCache<int> cache;
    auto framebuffer = std::make_shared<AttachmentFramebuffer>();

    cache.get(*framebuffer, objects.creator(), objects.destroyer());
    framebuffer->unsetDepthTarget();
    EXPECT_EQ(2,
        cache.get(*framebuffer, objects.creator(), objects.destroyer()));

    EXPECT_EQ(Events({ "create 1", "destroy 1", "create 2" }), objects.events);
    EXPECT_EQ(framebuffer->getVersion(), objects.versions.back());
    EXPECT_EQ(1u, cache.size());
}

TEST(framebufferObjectCache, sweepDestroysOnlyExpired)
{
    FakeObjects objects;
    FramebufferObjectCache<int> cache;
    auto kept = std::make_shared<AttachmentFramebuffer>();
    auto dropped = std::make_shared<AttachmentFramebuffer>();

    cache.get(*kept, objects.creator(), objects.destroyer());
    cache.get(*dropped, objects.creator(), objects.destroyer());

    cache.sweep(objects.destroyer());
    EXPECT_EQ(2u, cache.size());

    dropped.reset();
    cache.sweep(objects.destroyer());

    EXPECT_EQ(1u, cache.size());
    EXPECT_EQ(Events({ "create 1", "create 2", "destroy 2" }), objects.events);
    EXPECT_EQ(1, cache.get(*kept, objects.creator(), objects.destroyer()));
}

TEST(framebufferObjectCache, sweepDestroysChangedWithoutOwning)
{
    FakeObjects objects;
    FramebufferObjectCache<int> cache;
    auto kept = std::make_shared<AttachmentFramebuffer>();
    auto changed = std::make_shared<AttachmentFramebuffer>();

    cache.get(*kept, objects.creator(), objects.destroyer());
    cache.get(*changed, objects.creator(), objects.destroyer());

    changed->unsetDepthTarget();
    cache.sweep(objects.destroyer());

    EXPECT_EQ(1u, cache.size());
    EXPECT_EQ(1, changed.use_count());
    EXPECT_EQ(Events({ "create 1", "create 2", "destroy 2" }), objects.events);
    EXPECT_EQ(3, cache.get(*changed, objects.creator(), objects.destroyer()));
    EXPECT_EQ(1, cache.get(*kept, objects.creator(), objects.destroyer()));
}

TEST(framebufferObjectCache, clearDestroysAll)
{
    FakeObjects objects;
    FramebufferObjectCache<int> cache;
    auto first = std::make_shared<AttachmentFramebuffer>();
    auto second = std::make_shared<AttachmentFramebuffer>();

    cache.get(*first, objects.creator(), objects.destroyer());
    cache.get(*second, objects.creator(), objects.destroyer());
    objects.events.clear();

    cache.clear(objects.destroyer());

    EXPECT_TRUE(cache.empty());
    std::sort(objects.events.begin(), objects.events.end());
    EXPECT_EQ(Events({ "destroy 1", "destroy 2" }), objects.events);
}

TEST(framebufferObjectCache, cachesDestroyOnlyTheirOwn)
{
    FakeObjects firstObjects;
    FakeObjects secondObjects;
    secondObjects.next = 100;
    FramebufferObjectCache<int> firstCache;
    FramebufferObjectCache<int> secondCache;
    auto shared = std::make_shared<AttachmentFramebuffer>();
    auto own = std::make_shared<AttachmentFramebuffer>();

    firstCache.get(*shared, firstObjects.creator(), firstObjects.destroyer());
    secondCache.get(*shared,
        secondObjects.creator(),
        secondObjects.destroyer());
    secondCache.get(*own, secondObjects.creator(), secondObjects.destroyer());

    shared.reset();
    firstCache.sweep(firstObjects.destroyer());
    EXPECT_EQ(Events({ "create 1", "destroy 1" }), firstObjects.events);

    secondCache.clear(secondObjects.destroyer());
    std::sort(secondObjects.events.begin(), secondObjects.events.end());
    EXPECT_EQ(
        Events({ "create 100", "create 101", "destroy 100", "destroy 101" }),
        secondObjects.events);
}

TEST(framebufferObjectCache, changeWhileCreatingRebuildsOnNextGet)
{
    FakeObjects objects;
    FramebufferObjectCache<int> cache;
    auto framebuffer = std::make_shared<AttachmentFramebuffer>();
    auto create = objects.creator();

    int handle = cache.get(
        *framebuffer,
        [&](AttachmentFramebuffer::Snapshot const& snapshot)
        {
            framebuffer->unsetDepthTarget();
            return create(snapshot);
        },
        objects.destroyer());

    EXPECT_EQ(1, handle);
    EXPECT_NE(framebuffer->getVersion(), objects.versions.back());
    EXPECT_EQ(2,
        cache.get(*framebuffer, objects.creator(), objects.destroyer()));
    EXPECT_EQ(Events({ "create 1", "destroy 1", "create 2" }), objects.events);
}
