#include "archive_writer.hpp"
#include <fstream>
#include <format>
#include <algorithm>

namespace RPFL {

    ArchiveWriter::ArchiveWriter(
        std::string identifier,
        std::string version,
        Endianness endianness,
        std::uint32_t default_alignment
    ) : identifier_(std::move(identifier)),
        version_(std::move(version)),
        endianness_(endianness),
        default_alignment_(default_alignment) {
    }

    void ArchiveWriter::add_file(const std::string& path, std::span<const std::byte> data,
        std::uint32_t alignment) {
        if (path.empty()) {
            throw ArchiveException("File path cannot be empty");
        }

        if (contains(path)) {
            throw ArchiveException(std::format("File '{}' already exists in archive", path));
        }

        FileEntry entry;
        entry.path = path;
        entry.data.assign(data.begin(), data.end());
        entry.align = alignment > 0 ? alignment : default_alignment_;

        files_[path] = std::move(entry);
    }

    void ArchiveWriter::add_file(const std::string& path, const std::string& data,
        std::uint32_t alignment) {
        add_file(path, std::as_bytes(std::span{ data }), alignment);
    }

    bool ArchiveWriter::add_file_from_disk(const std::filesystem::path& filepath,
        const std::string& archive_path,
        std::uint32_t alignment) {
        if (!std::filesystem::exists(filepath)) {
            return false;
        }

        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file) {
            return false;
        }

        std::size_t size = file.tellg();
        file.seekg(0);

        std::vector<std::byte> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            return false;
        }

        std::string path = archive_path.empty() ? filepath.filename().string() : archive_path;
        add_file(path, buffer, alignment);
        return true;
    }

    bool ArchiveWriter::remove_file(const std::string& path) {
        return files_.erase(path) > 0;
    }

    void ArchiveWriter::clear() {
        files_.clear();
    }

    std::size_t ArchiveWriter::total_size() const noexcept {
        std::size_t total = calculate_header_size();
        for (const auto& [path, entry] : files_) {
            if (entry.align > 1) {
                total = (total + entry.align - 1) & ~(entry.align - 1);
            }
            total += entry.data.size();
        }
        return total;
    }

    bool ArchiveWriter::contains(const std::string& path) const noexcept {
        return files_.find(path) != files_.end();
    }

    std::uint64_t ArchiveWriter::calculate_header_size() const {
        std::uint64_t size = 0;

        // data_offset (4 байта)
        size += sizeof(std::uint32_t);

        // identifier length + identifier
        size += sizeof(std::uint64_t) + identifier_.size();

        // version length + version
        size += sizeof(std::uint64_t) + version_.size();

        // number of files
        size += sizeof(std::uint64_t);

        // file table entries
        for (const auto& [path, entry] : files_) {
            size += sizeof(std::uint64_t); // path length
            size += path.size();
            size += sizeof(std::uint64_t); // file size
            size += sizeof(std::uint32_t); // alignment
        }

        return size;
    }

    void ArchiveWriter::write_header(std::byte* buffer, std::uint64_t& offset) const {
        std::uint64_t header_size = calculate_header_size();

        // Write data_offset (начало данных файлов)
        write_with_endianness(buffer + offset, static_cast<std::uint32_t>(header_size), endianness_);
        offset += sizeof(std::uint32_t);

        // Write identifier
        std::uint64_t ident_length = identifier_.size();
        write_with_endianness(buffer + offset, ident_length, endianness_);
        offset += sizeof(ident_length);
        std::memcpy(buffer + offset, identifier_.data(), ident_length);
        offset += ident_length;

        // Write version
        std::uint64_t version_length = version_.size();
        write_with_endianness(buffer + offset, version_length, endianness_);
        offset += sizeof(version_length);
        std::memcpy(buffer + offset, version_.data(), version_length);
        offset += version_length;

        // Write number of files
        std::uint64_t num_files = files_.size();
        write_with_endianness(buffer + offset, num_files, endianness_);
        offset += sizeof(num_files);
    }

    void ArchiveWriter::write_file_table(std::byte* buffer, std::uint64_t& offset) const {
        for (const auto& [path, entry] : files_) {
            // Write path length and path
            std::uint64_t path_length = path.size();
            write_with_endianness(buffer + offset, path_length, endianness_);
            offset += sizeof(path_length);
            std::memcpy(buffer + offset, path.data(), path_length);
            offset += path_length;

            // Write file size
            std::uint64_t file_size = entry.data.size();
            write_with_endianness(buffer + offset, file_size, endianness_);
            offset += sizeof(file_size);

            // Write alignment
            write_with_endianness(buffer + offset, entry.align, endianness_);
            offset += sizeof(entry.align);
        }
    }

    void ArchiveWriter::write_file_data(std::byte* buffer, std::uint64_t& offset) const {
        for (const auto& [path, entry] : files_) {
            // Apply alignment
            if (entry.align > 1) {
                offset = (offset + entry.align - 1) & ~(entry.align - 1);
            }

            // Write file data
            std::memcpy(buffer + offset, entry.data.data(), entry.data.size());
            offset += entry.data.size();
        }
    }

    void ArchiveWriter::write(const std::string& filepath) {
        std::ofstream file(filepath, std::ios::binary);
        if (!file) {
            throw IOError("Failed to open file for writing: " + filepath);
        }
        write(file);
    }

    void ArchiveWriter::write(std::ostream& stream) {
        std::vector<std::byte> data = write_to_memory();
        stream.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    std::vector<std::byte> ArchiveWriter::write_to_memory() {
        std::size_t total = total_size();
        std::vector<std::byte> buffer(total);

        std::uint64_t offset = 0;
        write_header(buffer.data(), offset);
        write_file_table(buffer.data(), offset);
        write_file_data(buffer.data(), offset);

        return buffer;
    }

    void ArchiveWriter::add_files_from_directory(const std::filesystem::path& dir,
        const std::string& prefix,
        std::function<bool(const std::filesystem::path&)> filter) {
        if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                if (filter && !filter(entry.path())) {
                    continue;
                }

                std::string archive_path = prefix + entry.path().lexically_relative(dir).string();
                std::replace(archive_path.begin(), archive_path.end(), '\\', '/');

                add_file_from_disk(entry.path(), archive_path);
            }
        }
    }

    bool ArchiveWriter::update_file(const std::string& path, std::span<const std::byte> new_data) {
        auto it = files_.find(path);
        if (it == files_.end()) {
            return false;
        }

        it->second.data.assign(new_data.begin(), new_data.end());
        return true;
    }

} // namespace RPFL