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
            std::uint32_t align = 1; // выравнивание файла (1 = без выравнивания)
            std::uint64_t offset = 0; // смещение в архиве (вычисляется при записи)
        };

        ArchiveWriter(
            std::string identifier = "Reverge Package File",
            std::string version = "1.1",
            Endianness endianness = Endianness::Big,
            std::uint32_t default_alignment = 1
        );

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

        template<std::size_t N>
        void add_file(const std::string& path, const std::array<std::byte, N>& data,
            std::uint32_t alignment = 0) {
            add_file(path, std::span<const std::byte>(data.data(), N), alignment);
        }

        void add_file(const std::string& path, const std::vector<std::byte>& data,
            std::uint32_t alignment = 0) {
            add_file(path, std::span<const std::byte>(data.data(), data.size()), alignment);
        }

        void add_file(const std::string& path, const std::byte* data, std::size_t size,
            std::uint32_t alignment = 0) {
            add_file(path, std::span<const std::byte>(data, size), alignment);
        }

        // Добавление файла с диска
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

        // Пакетные операции
        void add_files_from_directory(const std::filesystem::path& dir,
            const std::string& prefix = "",
            std::function<bool(const std::filesystem::path&)> filter = nullptr);

        // Модификация файлов
        bool update_file(const std::string& path, std::span<const std::byte> new_data);

        // Установка параметров
        void set_identifier(const std::string& identifier) { identifier_ = identifier; }
        void set_version(const std::string& version) { version_ = version; }
        void set_endianness(Endianness endianness) { endianness_ = endianness; }
        void set_default_alignment(std::uint32_t alignment) { default_alignment_ = alignment; }

    private:
        std::string identifier_ = "Reverge Package File";
        std::string version_ = "1.1";
        Endianness endianness_ = Endianness::Big;
        std::uint32_t default_alignment_ = 1;
        std::unordered_map<std::string, FileEntry> files_;

        std::uint64_t calculate_header_size() const;
        void write_header(std::byte* buffer, std::uint64_t& offset) const;
        void write_file_table(std::byte* buffer, std::uint64_t& offset) const;
        void write_file_data(std::byte* buffer, std::uint64_t& offset) const;
    };

} // namespace RPFL