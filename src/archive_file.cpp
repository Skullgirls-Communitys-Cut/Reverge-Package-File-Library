#include "archive_file.hpp"
#include <cstring>
#include <algorithm>

namespace RPFL {

    ArchiveFile::ArchiveFile(std::string path,
        std::uint64_t offset,
        std::uint64_t size,
        const std::byte* archive_data,
        std::size_t cache_threshold)
        : path_(std::move(path))
        , offset_(offset)
        , size_(size)
        , archive_data_(archive_data)
        , cache_threshold_(cache_threshold) {
    }

    ArchiveFile::~ArchiveFile() {
        release_cache();
    }

    ArchiveFile::ArchiveFile(ArchiveFile&& other) noexcept
        : path_(std::move(other.path_))
        , offset_(other.offset_)
        , size_(other.size_)
        , archive_data_(other.archive_data_)
        , cache_threshold_(other.cache_threshold_)
        , data_holder_(std::move(other.data_holder_))
        , is_loaded_(other.is_loaded_) {

        other.archive_data_ = nullptr;
        other.is_loaded_ = false;
    }

    ArchiveFile& ArchiveFile::operator=(ArchiveFile&& other) noexcept {
        if (this != &other) {
            release_cache();

            path_ = std::move(other.path_);
            offset_ = other.offset_;
            size_ = other.size_;
            archive_data_ = other.archive_data_;
            cache_threshold_ = other.cache_threshold_;
            data_holder_ = std::move(other.data_holder_);
            is_loaded_ = other.is_loaded_;

            other.archive_data_ = nullptr;
            other.is_loaded_ = false;
        }
        return *this;
    }

    void ArchiveFile::ensure_loaded() {
        if (is_loaded_) {
            return;
        }

        const std::byte* file_data = archive_data_ + offset_;

        if (size_ <= cache_threshold_) {
            // Кэшируем маленькие файлы
            auto buffer = std::make_unique<std::byte[]>(size_);
            std::memcpy(buffer.get(), file_data, size_);
            data_holder_ = CachedData{ std::move(buffer), size_ };
        }
        else {
            // Большие файлы читаем напрямую из mmap
            data_holder_ = MappedView{ std::span<const std::byte>(file_data, size_) };
        }

        is_loaded_ = true;
    }

    std::span<const std::byte> ArchiveFile::data() {
        ensure_loaded();

        return std::visit([](auto&& holder) -> std::span<const std::byte> {
            using T = std::decay_t<decltype(holder)>;
            if constexpr (std::is_same_v<T, MappedView>) {
                return holder.data;
            }
            else if constexpr (std::is_same_v<T, CachedData>) {
                return { holder.buffer.get(), holder.size };
            }
            }, data_holder_);
    }

    std::string_view ArchiveFile::as_string_view() {
        auto span = data();
        return { reinterpret_cast<const char*>(span.data()), span.size() };
    }

    bool ArchiveFile::is_cached() const noexcept {
        return std::holds_alternative<CachedData>(data_holder_);
    }

    void ArchiveFile::release_cache() noexcept {
        if (is_cached()) {
            data_holder_ = MappedView{ std::span<const std::byte>() };
            is_loaded_ = false;
        }
    }

} // namespace RPFL