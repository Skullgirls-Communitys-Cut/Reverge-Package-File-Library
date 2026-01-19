#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <optional>
#include <variant>
#include <istream>
#include <sstream>
#include <functional>

namespace RPFL {

    class ArchiveReader;

    class ArchiveFile {
    public:
        struct MappedView {
            std::span<const std::byte> data;
        };

        struct CachedData {
            std::unique_ptr<std::byte[]> buffer;
            std::size_t size;
        };

        struct StreamData {
            std::shared_ptr<std::istream> stream;
            std::size_t size;
            std::uint64_t offset;
        };

        using DataHolder = std::variant<MappedView, CachedData, StreamData>;

        ArchiveFile(std::string path,
            std::uint64_t offset,
            std::uint64_t size,
            const std::byte* archive_data,
            std::size_t cache_threshold = 1024 * 1024,
            bool allow_streaming = false);

        // Deny Copy
        ArchiveFile(const ArchiveFile&) = delete;
        ArchiveFile& operator=(const ArchiveFile&) = delete;

        // Accept moving
        ArchiveFile(ArchiveFile&& other) noexcept;
        ArchiveFile& operator=(ArchiveFile&& other) noexcept;

        ~ArchiveFile();

        // Getters
        const std::string& path() const noexcept { return path_; }
        std::uint64_t size() const noexcept { return size_; }
        std::uint64_t offset() const noexcept { return offset_; }

        // Recive data
        std::span<const std::byte> data();
        std::string_view as_string_view();

        // stream reading
        std::shared_ptr<std::istream> open_stream();
        std::vector<std::byte> read_chunk(std::size_t offset, std::size_t size);

        // Checks, cached file or not
        bool is_cached() const noexcept;

        // Checks, support streaming or not
        bool supports_streaming() const noexcept { return supports_streaming_; }

        // exclipt release_cache
        void release_cache() noexcept;

        // Getter data_holder
        const DataHolder& data_holder() const noexcept { return data_holder_; }

    private:
        void ensure_loaded();
        std::vector<std::byte> read_from_stream(std::shared_ptr<std::istream> stream);

        std::string path_;
        std::uint64_t offset_;
        std::uint64_t size_;
        const std::byte* archive_data_;
        std::size_t cache_threshold_;
        DataHolder data_holder_;
        bool is_loaded_ = false;
        bool supports_streaming_ = false;
        std::function<std::shared_ptr<std::istream>()> stream_factory_;
    };

} // namespace RPFL