#pragma once
#include <cstddef>
#include <memory>
#include <string_view>
#include <span>
#include <system_error>


namespace RPFL {

    class MemoryMappedFile {
    public:
        struct Options {
            bool read_only = true;
            bool prefetch = false;
        };

        MemoryMappedFile() = default;
        explicit MemoryMappedFile(const std::string& filepath,
            Options options = {});

        ~MemoryMappedFile();

        // Запрещаем копирование
        MemoryMappedFile(const MemoryMappedFile&) = delete;
        MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;

        // Разрешаем перемещение
        MemoryMappedFile(MemoryMappedFile&& other) noexcept;
        MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept;

        void open(const std::string& filepath, Options options = {});
        void close();

        bool is_open() const noexcept {
            return mapped_data_ != nullptr;
        }

        std::size_t size() const noexcept {
            return mapped_size_;
        }

        std::span<const std::byte> data() const noexcept {
            return { mapped_data_, mapped_size_ };
        }

        bool is_range_valid(std::size_t offset, std::size_t size) const noexcept {
            return offset <= mapped_size_ && (offset + size) <= mapped_size_;
        }

    private:
#ifdef _WIN32
        void* file_handle_ = nullptr;
        void* mapping_handle_ = nullptr;
#else
        int file_descriptor_ = -1;
#endif
        std::byte* mapped_data_ = nullptr;
        std::size_t mapped_size_ = 0;
        Options options_;
    };

} // namespace archive