#pragma once

#include "bqui/remote/remotedriver.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace bqui::remote
{
    inline constexpr int kUnknownBlob = -32001;
    inline constexpr int kBlobCapReached = -32002;

    /**
     * @brief The blobs one remote session has handed out and not released.
     *
     * A producing method passes its bytes to `add` and replies with the
     * descriptor it returns; `read` and `release` serve `blob.read` and
     * `blob.release`. Failures are thrown as `RpcError`.
     */
    class BlobStore
    {
    public:
        explicit BlobStore(BlobLimits limits);

        /**
         * @brief Keep `bytes` and return its descriptor (`id`, `size`,
         * `contentType`).
         *
         * Throws `kBlobCapReached` when the blobs would pass the outstanding
         * cap.
         */
        nlohmann::json add(std::string bytes, std::string contentType);

        nlohmann::json read(nlohmann::json const& params) const;
        nlohmann::json release(nlohmann::json const& params);

        /**
         * @brief The limits as `system.describe` reports them.
         */
        nlohmann::json describeLimits() const;

        void clear();

    private:
        using Blobs = std::unordered_map<uint64_t, std::string>;

        Blobs::const_iterator find(nlohmann::json const& params) const;

        BlobLimits limits_;
        Blobs blobs_;
        uint64_t nextId_ = 1;
        size_t outstanding_ = 0;
    };
} // namespace bqui::remote
