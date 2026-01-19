#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <optional>
#include <variant>

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

        using DataHolder = std::variant<MappedView, CachedData>;

        ArchiveFile(std::string path,
            std::uint64_t offset,
            std::uint64_t size,
            const std::byte* archive_data,
            std::size_t cache_threshold = 1024 * 1024); // 1MB по умолчанию

        // Запрещаем копирование
        ArchiveFile(const ArchiveFile&) = delete;
        ArchiveFile& operator=(const ArchiveFile&) = delete;

        // Разрешаем перемещение
        ArchiveFile(ArchiveFile&& other) noexcept;
        ArchiveFile& operator=(ArchiveFile&& other) noexcept;

        ~ArchiveFile();

        // Геттеры
        const std::string& path() const noexcept { return path_; }
        std::uint64_t size() const noexcept { return size_; }
        std::uint64_t offset() const noexcept { return offset_; }

        // Получение данных
        std::span<const std::byte> data();
        std::string_view as_string_view();

        // Проверка, закэширован ли файл
        bool is_cached() const noexcept;

        // Явное освобождение кэша
        void release_cache() noexcept;

        // Получение владельца данных
        const DataHolder& data_holder() const noexcept { return data_holder_; }

    private:
        void ensure_loaded();

        std::string path_;
        std::uint64_t offset_;
        std::uint64_t size_;
        const std::byte* archive_data_;
        std::size_t cache_threshold_;
        DataHolder data_holder_;
        bool is_loaded_ = false;
    };

} // namespace RPFL