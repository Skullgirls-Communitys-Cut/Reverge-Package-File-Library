#pragma once
#include "archive_common.hpp"
#include "archive_exception.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <span>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <functional>

namespace RPFL {

    class ArchiveWriter {
    public:
        struct FileEntry {
            std::string path;
            std::vector<std::byte> data;
            std::uint32_t align = 1; // file_aligned? (1 = нет выравнивания)
            std::uint64_t offset = 0; // offset in archive (calc via writing)
        };

        struct Config {
            std::string identifier = "Reverge Package File";
            std::string version = "1.1";
            Endianness endianness = Endianness::Big;
            std::uint32_t default_alignment = 1;
        };

        ArchiveWriter() = default;
        explicit ArchiveWriter(Config config = {});

        // Добавление файлов
        void add_file(const std::string& path, std::span<const std::byte> data,
            std::uint32_t alignment = 0);
        void add_file(const std::string& path, const std::string& data,
            std::uint32_t alignment = 0);

        template<std::size_t N>
        void add_file(const std::string& path, const std::byte(&data)[N],
            std::uint32_t alignment = 0) {
            add_file(path, std::span<const std::byte>(data, N), alignment);
        }

        // Adding a new file from std::array<std::byte, N>
        template<std::size_t N>
        void add_file(const std::string& path, const std::array<std::byte, N>& data,
            std::uint32_t alignment = 0) {
            add_file(path, std::span<const std::byte>(data.data(), N), alignment);
        }

        // Adding a new file from std::vector<std::byte>
        void add_file(const std::string& path, const std::vector<std::byte>& data,
            std::uint32_t alignment = 0) {
            add_file(path, std::span<const std::byte>(data.data(), data.size()), alignment);
        }

        // Adding a new file from pointer + we need size
        void add_file(const std::string& path, const std::byte* data, std::size_t size,
            std::uint32_t alignment = 0) {
            add_file(path, std::span<const std::byte>(data, size), alignment);
        }

        // Adding a new file from disk
        bool add_file_from_disk(const std::filesystem::path& filepath,
            const std::string& archive_path = "",
            std::uint32_t alignment = 0);
        
        // Удаление файлов
        bool remove_file(const std::string& path);
        void clear();

        // Получение информации
        std::size_t file_count() const noexcept { return files_.size(); }
        std::size_t total_size() const noexcept;
        bool contains(const std::string& path) const noexcept;

        // Запись архива
        void write(const std::string& filepath);
        void write(std::ostream& stream);
        std::vector<std::byte> write_to_memory();

        // Batch операции
        void add_files_from_directory(const std::filesystem::path& dir,
            const std::string& prefix = "",
            std::function<bool(const std::filesystem::path&)> filter = nullptr);

        // Модификация файлов
        bool update_file(const std::string& path, std::span<const std::byte> new_data);

    private:
        Config config_;
        std::unordered_map<std::string, FileEntry> files_;

        std::uint64_t calculate_header_size() const;
        void write_header(std::byte* buffer, std::uint64_t& offset) const;
        void write_file_table(std::byte* buffer, std::uint64_t& offset) const;
        void write_file_data(std::byte* buffer, std::uint64_t& offset) const;
    };

} // namespace RPFL