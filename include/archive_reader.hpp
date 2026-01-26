#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <span>

#include "memory_mapped_file.hpp"
#include "archive_file.hpp"
#include "archive_exception.hpp"
#include "archive_common.hpp"

namespace RPFL {

    class ArchiveReader {
    public:
        ArchiveReader() = default;
        explicit ArchiveReader(
            const std::string& filepath,
            std::size_t cache_threshold = 1024 * 1024, // 1MB
            bool lazy_load = true,
            bool allow_streaming = true,
            MemoryMappedFile::Options mmap_options = {},
            Endianness file_endianness = Endianness::Big
        );
        ~ArchiveReader();

        // Открытие и закрытие
        void open(
            const std::string& filepath,
            std::size_t cache_threshold = 1024 * 1024,
            bool lazy_load = true,
            bool allow_streaming = true,
            MemoryMappedFile::Options mmap_options = {},
            Endianness file_endianness = Endianness::Big
        );
        void close();
        bool is_open() const noexcept;

        // Информация об архиве
        std::string_view identifier() const noexcept;
        std::string_view version() const noexcept;
        std::size_t file_count() const noexcept;
        Endianness endianness() const noexcept { return file_endianness_; }

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

        // Установка параметров
        void set_cache_threshold(std::size_t threshold) { cache_threshold_ = threshold; }
        void set_lazy_load(bool lazy_load) { lazy_load_ = lazy_load; }
        void set_allow_streaming(bool allow_streaming) { allow_streaming_ = allow_streaming; }
        void set_mmap_options(const MemoryMappedFile::Options& options) { mmap_options_ = options; }
        void set_file_endianness(Endianness endianness) { file_endianness_ = endianness; }

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
        bool is_open_ = false;

        // Параметры чтения
        std::size_t cache_threshold_ = 1024 * 1024;
        bool lazy_load_ = true;
        bool allow_streaming_ = true;
        MemoryMappedFile::Options mmap_options_;
        Endianness file_endianness_ = Endianness::Big;

        friend class ArchiveWriter;
    };

} // namespace RPFL