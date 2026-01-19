#include "archive_reader.hpp"
#include <cstring>
#include <algorithm>
#include <format>
#include <bit>
#include <array>
#include <ranges>
#include <type_traits>

template<std::integral T>
constexpr T byteswap(T value) noexcept
{
    static_assert(std::has_unique_object_representations_v<T>,
        "T may not have padding bits");
    auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    std::ranges::reverse(value_representation);
    return std::bit_cast<T>(value_representation);
}

namespace RPFL {

    ArchiveReader::ArchiveReader(const std::string& filepath, Config config)
        : config_(std::move(config)) {
        open(filepath, config_);
    }

    ArchiveReader::~ArchiveReader() {
        close();
    }

    void ArchiveReader::open(const std::string& filepath, Config config) {
        close();
        config_ = std::move(config);

        try {
            mmap_file_.open(filepath, config_.mmap_options);
            auto data = mmap_file_.data();

            parse_header(data);
            parse_file_table(data, header_.data_offset);

            is_open_ = true;
        }
        catch (const IOError&) {
            close();
            throw;
        }
    }

    void ArchiveReader::close() {
        files_.clear();
        file_map_.clear();
        mmap_file_.close();
        is_open_ = false;
    }

    bool ArchiveReader::is_open() const noexcept {
        return is_open_;
    }

    void ArchiveReader::parse_header(std::span<const std::byte> data) {
        if (data.size() < 4) {
            throw ArchiveFormatException("File too small for header");
        }

        const std::byte* ptr = data.data();

        // data_offset
        std::memcpy(&header_.data_offset, ptr, sizeof(header_.data_offset));
        header_.data_offset = byteswap(header_.data_offset); // Endian issue
        ptr += sizeof(header_.data_offset);

        // Проверяем, что есть место для идентификатора
        if (static_cast<std::size_t>(ptr - data.data()) + 8 > data.size()) {
            throw ArchiveFormatException("Incomplete identifier length");
        }

        // file_identifier_length
        std::uint64_t ident_length;
        std::memcpy(&ident_length, ptr, sizeof(ident_length));
        ident_length = byteswap(ident_length); // Endian issue
        ptr += sizeof(ident_length);

        if (static_cast<std::size_t>(ptr - data.data()) + ident_length > data.size()) {
            throw ArchiveFormatException("Identifier extends beyond file");
        }

        header_.identifier.assign(reinterpret_cast<const char*>(ptr), ident_length);
        ptr += ident_length;

        // file_version_length
        if (static_cast<std::size_t>(ptr - data.data()) + 8 > data.size()) {
            throw ArchiveFormatException("Incomplete version length");
        }

        std::uint64_t version_length;
        std::memcpy(&version_length, ptr, sizeof(version_length));
        version_length = byteswap(version_length); // Endian issue
        ptr += sizeof(version_length);

        if (static_cast<std::size_t>(ptr - data.data()) + version_length > data.size()) {
            throw ArchiveFormatException("Version extends beyond file");
        }

        header_.version.assign(reinterpret_cast<const char*>(ptr), version_length);
        ptr += version_length;
    }

    void ArchiveReader::parse_file_table(std::span<const std::byte> data,
        std::uint32_t data_offset) {
        const std::byte* ptr =
            data.data() //Pointer
            + sizeof(data_offset)
            + sizeof(uint64_t) //header.identifier lenght
            + header_.identifier.size()
            + 8 //header.version lenght
            + header_.version.size();

        // n_of_files
        std::uint64_t num_files;
        std::memcpy(&num_files, ptr, sizeof(num_files));
        num_files = byteswap(num_files); // Endian issue
        ptr += sizeof(num_files);

        std::uint64_t current_offset = data_offset;

        for (std::uint64_t i = 0; i < num_files; ++i) {
            // file_path_length
            if (static_cast<std::size_t>(ptr - data.data()) + 8 > data.size()) {
                throw ArchiveFormatException("Incomplete file path length");
            }

            std::uint64_t path_length;
            std::memcpy(&path_length, ptr, sizeof(path_length));
            path_length = byteswap(path_length); // Endian issue
            ptr += sizeof(path_length);

            if (static_cast<std::size_t>(ptr - data.data()) + path_length > data.size()) {
                throw ArchiveFormatException("File path extends beyond file");
            }

            std::string file_path(reinterpret_cast<const char*>(ptr), path_length);
            ptr += path_length;

            // file_length
            if (static_cast<std::size_t>(ptr - data.data()) + 8 > data.size()) {
                throw ArchiveFormatException("Incomplete file length");
            }

            std::uint64_t file_size;
            std::memcpy(&file_size, ptr, sizeof(file_size));
            file_size = byteswap(file_size); // Endian issue
            ptr += sizeof(file_size);

            // file_align (ignoring for)
            ptr += sizeof(uint32_t);

            // Check that file data not extends beyond archive
            if (current_offset + file_size > data.size()) {
                throw ArchiveFormatException(
                    std::format("File '{}' extends beyond archive", file_path));
            }

            auto archive_file = std::make_unique<ArchiveFile>(
                file_path, current_offset, file_size,
                data.data(), config_.cache_threshold);

            file_map_[archive_file->path()] = archive_file.get();
            files_.push_back(std::move(archive_file));

            current_offset += file_size;
        }
    }

    std::string_view ArchiveReader::identifier() const noexcept {
        return header_.identifier;
    }

    std::string_view ArchiveReader::version() const noexcept {
        return header_.version;
    }

    std::size_t ArchiveReader::file_count() const noexcept {
        return files_.size();
    }

    ArchiveFile& ArchiveReader::get_file(const std::string& path) {
        auto it = file_map_.find(path);
        if (it == file_map_.end()) {
            throw FileNotFoundException(path);
        }
        return *it->second;
    }

    const ArchiveFile& ArchiveReader::get_file(const std::string& path) const {
        auto it = file_map_.find(path);
        if (it == file_map_.end()) {
            throw FileNotFoundException(path);
        }
        return *it->second;
    }

    bool ArchiveReader::contains(const std::string& path) const noexcept {
        return file_map_.find(path) != file_map_.end();
    }

    const std::vector<std::unique_ptr<ArchiveFile>>& ArchiveReader::files() const noexcept {
        return files_;
    }

    void ArchiveReader::release_all_caches() noexcept {
        for (auto& file : files_) {
            file->release_cache();
        }
    }

    std::size_t ArchiveReader::cache_size() const noexcept {
        std::size_t total = 0;
        for (const auto& file : files_) {
            if (file->is_cached()) {
                total += file->size();
            }
        }
        return total;
    }

    std::span<const std::byte> ArchiveReader::read_raw(const std::string& path) const {
        auto it = file_map_.find(path);
        if (it == file_map_.end()) {
            throw FileNotFoundException(path);
        }

        const ArchiveFile* file = it->second;
        const std::byte* data = mmap_file_.data().data() + file->offset();
        return { data, file->size() };
    }

} // namespace RPFL