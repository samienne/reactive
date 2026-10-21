#include "atlasallocator.h"

#include <algorithm>
#include <cassert>

namespace avg
{

namespace
{
    constexpr int padding = 1;
    constexpr int heightGranularity = 4;
    constexpr int none = -1;

    int shelfHeightFor(int glyphHeight)
    {
        int bucket = (glyphHeight + heightGranularity - 1)
            / heightGranularity * heightGranularity;

        return bucket + 2 * padding;
    }
} // anonymous namespace

AtlasAllocator::AtlasAllocator(int width, int height) :
    width_(width),
    height_(height)
{
    assert(width > 0 && height > 0);
}

void AtlasAllocator::beginFrame()
{
    ++frame_;
}

std::optional<AtlasRect> AtlasAllocator::find(GlyphKey const& key)
{
    auto i = map_.find(key);
    if (i == map_.end())
        return std::nullopt;

    touch(i->second);

    return entries_[i->second].rect;
}

std::optional<AtlasRect> AtlasAllocator::insert(
        GlyphKey const& key,
        int width,
        int height)
{
    assert(width > 0 && height > 0);
    assert(map_.find(key) == map_.end());

    int const allocatedWidth = width + 2 * padding;
    int const shelfHeight = shelfHeightFor(height);

    if (allocatedWidth > width_ || shelfHeight > height_)
        return std::nullopt;

    while (true)
    {
        if (auto placement = allocate(allocatedWidth, shelfHeight))
        {
            int index;
            if (freeEntries_.empty())
            {
                index = static_cast<int>(entries_.size());
                entries_.emplace_back();
            }
            else
            {
                index = freeEntries_.back();
                freeEntries_.pop_back();
            }

            Entry& entry = entries_[index];
            entry.key = key;
            entry.rect = AtlasRect{
                placement->x + padding,
                shelves_[placement->shelf].y + padding,
                width,
                height
            };
            entry.allocatedWidth = allocatedWidth;
            entry.shelf = placement->shelf;
            entry.frame = frame_;
            linkFront(index);
            map_.emplace(key, index);

            return entry.rect;
        }

        if (!evictLeastRecentlyUsed())
            return std::nullopt;
    }
}

void AtlasAllocator::clear()
{
    shelves_.clear();
    entries_.clear();
    freeEntries_.clear();
    map_.clear();
    stripY_ = 0;
    head_ = none;
    tail_ = none;
}

int AtlasAllocator::getWidth() const
{
    return width_;
}

int AtlasAllocator::getHeight() const
{
    return height_;
}

size_t AtlasAllocator::getCount() const
{
    return map_.size();
}

std::optional<AtlasAllocator::Placement> AtlasAllocator::allocate(
        int allocatedWidth,
        int shelfHeight)
{
    int bestShelf = none;
    size_t bestRange = 0;
    int bestWidth = 0;

    for (size_t s = 0; s < shelves_.size(); ++s)
    {
        Shelf const& shelf = shelves_[s];
        if (shelf.height != shelfHeight)
            continue;

        for (size_t r = 0; r < shelf.free.size(); ++r)
        {
            int w = shelf.free[r].width;
            if (w >= allocatedWidth && (bestShelf == none || w < bestWidth))
            {
                bestShelf = static_cast<int>(s);
                bestRange = r;
                bestWidth = w;
            }
        }
    }

    if (bestShelf != none)
    {
        auto& free = shelves_[bestShelf].free;
        int x = free[bestRange].x;
        free[bestRange].x += allocatedWidth;
        free[bestRange].width -= allocatedWidth;
        if (free[bestRange].width == 0)
            free.erase(free.begin() + bestRange);

        return Placement{ bestShelf, x };
    }

    if (stripY_ + shelfHeight > height_)
        return std::nullopt;

    Shelf shelf{ stripY_, shelfHeight, {} };
    if (allocatedWidth < width_)
        shelf.free.push_back(Range{ allocatedWidth, width_ - allocatedWidth });

    shelves_.push_back(std::move(shelf));
    stripY_ += shelfHeight;

    return Placement{ static_cast<int>(shelves_.size()) - 1, 0 };
}

bool AtlasAllocator::evictLeastRecentlyUsed()
{
    // Touching moves an entry to the front, so a pinned tail means every
    // entry is pinned.
    if (tail_ == none || entries_[tail_].frame == frame_)
        return false;

    int index = tail_;
    Entry& entry = entries_[index];

    map_.erase(entry.key);
    unlink(index);
    returnRange(entry.shelf, entry.rect.x - padding, entry.allocatedWidth);
    freeEntries_.push_back(index);
    releaseTrailingShelves();

    return true;
}

void AtlasAllocator::returnRange(int shelf, int x, int width)
{
    auto& free = shelves_[shelf].free;
    auto next = std::lower_bound(free.begin(), free.end(), x,
            [](Range const& range, int x)
            {
                return range.x < x;
            });

    bool joinsPrevious = next != free.begin()
        && (next - 1)->x + (next - 1)->width == x;
    bool joinsNext = next != free.end() && x + width == next->x;

    if (joinsPrevious && joinsNext)
    {
        (next - 1)->width += width + next->width;
        free.erase(next);
    }
    else if (joinsPrevious)
    {
        (next - 1)->width += width;
    }
    else if (joinsNext)
    {
        next->x = x;
        next->width += width;
    }
    else
    {
        free.insert(next, Range{ x, width });
    }
}

void AtlasAllocator::releaseTrailingShelves()
{
    while (!shelves_.empty() && isShelfFree(shelves_.back()))
    {
        stripY_ = shelves_.back().y;
        shelves_.pop_back();
    }
}

bool AtlasAllocator::isShelfFree(Shelf const& shelf) const
{
    return shelf.free.size() == 1
        && shelf.free.front().x == 0
        && shelf.free.front().width == width_;
}

void AtlasAllocator::touch(int index)
{
    entries_[index].frame = frame_;

    if (head_ == index)
        return;

    unlink(index);
    linkFront(index);
}

void AtlasAllocator::linkFront(int index)
{
    Entry& entry = entries_[index];
    entry.prev = none;
    entry.next = head_;

    if (head_ != none)
        entries_[head_].prev = index;
    else
        tail_ = index;

    head_ = index;
}

void AtlasAllocator::unlink(int index)
{
    Entry& entry = entries_[index];

    if (entry.prev != none)
        entries_[entry.prev].next = entry.next;
    else
        head_ = entry.next;

    if (entry.next != none)
        entries_[entry.next].prev = entry.prev;
    else
        tail_ = entry.prev;

    entry.prev = none;
    entry.next = none;
}

} // namespace avg

