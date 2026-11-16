#include "blobstore.h"

#include "base64.h"
#include "dispatcher.h"

#include <algorithm>
#include <string>
#include <utility>

namespace bqui::remote
{

using nlohmann::json;

namespace
{

constexpr char const* kEncoding = "base64";

uint64_t requireIndex(json const& params, char const* key)
{
    auto it = params.find(key);
    if (it == params.end() || !it->is_number_unsigned())
    {
        throw RpcError{ kInvalidParams,
            std::string("'") + key + "' must be a non-negative integer" };
    }

    return it->get<uint64_t>();
}

} // namespace

BlobStore::BlobStore(BlobLimits limits) :
    limits_(limits)
{
}

json BlobStore::add(std::string bytes, std::string contentType)
{
    if (bytes.size() > limits_.maxOutstanding - outstanding_)
    {
        throw RpcError{ kBlobCapReached,
            "unreleased blobs would pass the outstanding cap",
            { { "inUse", outstanding_ }, { "cap", limits_.maxOutstanding } } };
    }

    uint64_t id = nextId_++;
    size_t size = bytes.size();
    json descriptor = {
        { "id", id },
        { "size", size },
        { "contentType", std::move(contentType) },
    };

    outstanding_ += size;
    blobs_.emplace(id, std::move(bytes));

    return descriptor;
}

json BlobStore::read(json const& params) const
{
    uint64_t offset = requireIndex(params, "offset");

    uint64_t length = limits_.defaultRead;
    if (params.contains("length"))
        length = requireIndex(params, "length");

    auto const& bytes = find(params)->second;
    if (offset > bytes.size())
    {
        throw RpcError{ kInvalidParams,
            "'offset' is past the end of the blob" };
    }

    size_t take = static_cast<size_t>(std::min<uint64_t>(
                { length, limits_.maxRead, bytes.size() - offset }));

    return {
        { "data", base64Encode(bytes.data() + offset, take) },
        { "encoding", kEncoding },
        { "end", offset + take == bytes.size() },
    };
}

json BlobStore::release(json const& params)
{
    auto it = find(params);
    outstanding_ -= it->second.size();
    blobs_.erase(it);
    return json::object();
}

json BlobStore::describeLimits() const
{
    return {
        { "maxOutstanding", limits_.maxOutstanding },
        { "maxRead", limits_.maxRead },
        { "defaultRead", limits_.defaultRead },
        { "encodings", json::array({ kEncoding }) },
    };
}

void BlobStore::clear()
{
    blobs_.clear();
    outstanding_ = 0;
}

std::unordered_map<uint64_t, std::string>::const_iterator BlobStore::find(
        json const& params) const
{
    uint64_t id = requireIndex(params, "id");

    auto it = blobs_.find(id);
    if (it == blobs_.end())
        throw RpcError{ kUnknownBlob, "no blob with id " + std::to_string(id) };

    return it;
}

} // namespace bqui::remote
