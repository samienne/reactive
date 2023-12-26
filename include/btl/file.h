#pragma once

#include "buffer.h"
#include "shared.h"

#include "future/future.h"

#include <filesystem>

namespace btl
{
    class FileImpl
    {
    public:
        virtual ~FileImpl() = default;

        virtual std::filesystem::path const& getPath() const = 0;
        virtual future::Future<Buffer> read(size_t size) = 0;
        virtual size_t getSize() const = 0;
        virtual bool hasSize() const = 0;
    };

    class File
    {
    public:
        explicit File(std::shared_ptr<FileImpl> impl);

        std::filesystem::path const& getPath() const;

        future::Future<Buffer> read(size_t size);
        future::Future<Buffer> readAll();
        void close() &&;
        size_t getSize() const;
        bool hasSize() const;

    protected:
        inline FileImpl* d() { return deferred_.get(); }
        inline FileImpl const* d() const { return deferred_.get(); }
    private:
        shared<FileImpl> deferred_;
    };

    future::Future<File> openFile(std::filesystem::path path);
} // namespace btl

