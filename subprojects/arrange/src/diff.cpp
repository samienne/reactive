#include <arrange/diff.h>

#include <unordered_set>

namespace arrange
{

ConstraintDiff diffConstraints(
    const std::vector<Constraint>& before, const std::vector<Constraint>& after)
{
    std::unordered_set<Id> beforeIds;
    beforeIds.reserve(before.size());
    for (const auto& c : before)
    {
        if (c.id() != nullId)
        {
            beforeIds.insert(c.id());
        }
    }

    std::unordered_set<Id> afterIds;
    afterIds.reserve(after.size());
    for (const auto& c : after)
    {
        if (c.id() != nullId)
        {
            afterIds.insert(c.id());
        }
    }

    ConstraintDiff diff;
    diff.removed.reserve(before.size());
    diff.added.reserve(after.size());

    for (const auto& c : before)
    {
        if (c.id() == nullId || afterIds.find(c.id()) == afterIds.end())
        {
            diff.removed.push_back(c);
        }
    }
    for (const auto& c : after)
    {
        if (c.id() == nullId || beforeIds.find(c.id()) == beforeIds.end())
        {
            diff.added.push_back(c);
        }
    }

    return diff;
}

}  // namespace arrange
