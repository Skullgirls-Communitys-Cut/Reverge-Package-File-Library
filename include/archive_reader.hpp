#pragma once
#include "memory_mapped_file.hpp"
#include "archive_file.hpp"
#include "archive_exception.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <span>

namespace RPFL {

    class ArchiveReader {
    public:
        struct Config {
            std::size_t cache_threshold = 1024 * 1024; // 1MB
            bool lazy_load = true;
            MemoryMappedFile::Options mmap_options = {};
        };

        ArchiveReader() = default;
        explicit ArchiveReader(const std::string& filepath,
            Config config = {});
        ~ArchiveReader();

        // Открытие/закрытие
        void open(const std::string& filepath, Config config = {});
        void close();
        bool is_open() const noexcept;

        // Информация об архиве
        std::string_view identifier() const noexcept;
        std::string_view version() const noexcept;
        std::size_t file_count() const noexcept;

        // Работа с файлами
        ArchiveFile& get_file(const std::string& path);
        const ArchiveFile& get_file(const std::string& path) const;

        bool contains(const std::string& path) const noexcept;

        // Итерация по файлам
        const std::vector<std::unique_ptr<ArchiveFile>>& files() const noexcept;

        // Управление памятью
        void release_all_caches() noexcept;
        std::size_t cache_size() const noexcept; // Общий размер кэшированных данных

        // Быстрое чтение без кэширования
        std::span<const std::byte> read_raw(const std::string& path) const;

    private:
        struct Header {
            std::uint32_t data_offset;
            std::string identifier;
            std::string version;
        };

        void parse_header(std::span<const std::byte> data);
        void parse_file_table(std::span<const std::byte> data,
            std::uint32_t data_offset);

        MemoryMappedFile mmap_file_;
        Header header_;
        std::vector<std::unique_ptr<ArchiveFile>> files_;
        std::unordered_map<std::string_view, ArchiveFile*> file_map_;
        Config config_;
        bool is_open_ = false;
    };

} // namespace RPFL