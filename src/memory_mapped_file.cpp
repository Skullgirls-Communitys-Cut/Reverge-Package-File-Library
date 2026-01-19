#include "memory_mapped_file.hpp"
#include "archive_exception.hpp"

#ifdef _WIN32
#include <windows.h>
#include <memoryapi.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace RPFL {

    MemoryMappedFile::MemoryMappedFile(const std::string& filepath, Options options) {
        open(filepath, options);
    }

    MemoryMappedFile::~MemoryMappedFile() {
        close();
    }

    MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept
        : file_handle_(other.file_handle_)
        , mapping_handle_(other.mapping_handle_)
        , mapped_data_(other.mapped_data_)
        , mapped_size_(other.mapped_size_)
        , options_(other.options_) {

        other.file_handle_ = nullptr;
        other.mapping_handle_ = nullptr;
        other.mapped_data_ = nullptr;
        other.mapped_size_ = 0;
    }

    MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& other) noexcept {
        if (this != &other) {
            close();

            file_handle_ = other.file_handle_;
            mapping_handle_ = other.mapping_handle_;
            mapped_data_ = other.mapped_data_;
            mapped_size_ = other.mapped_size_;
            options_ = other.options_;

            other.file_handle_ = nullptr;
            other.mapping_handle_ = nullptr;
            other.mapped_data_ = nullptr;
            other.mapped_size_ = 0;
        }
        return *this;
    }

    void MemoryMappedFile::open(const std::string& filepath, Options options) {
        close();
        options_ = options;

#ifdef _WIN32
        DWORD access = GENERIC_READ;
        DWORD share = FILE_SHARE_READ;
        DWORD creation = OPEN_EXISTING;
        DWORD flags = FILE_ATTRIBUTE_NORMAL;

        file_handle_ = CreateFileA(filepath.c_str(), access, share,
            nullptr, creation, flags, nullptr);
        if (file_handle_ == INVALID_HANDLE_VALUE) {
            throw IOError("Failed to open file: " + filepath);
        }

        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(file_handle_, &file_size)) {
            CloseHandle(file_handle_);
            throw IOError("Failed to get file size");
        }
        mapped_size_ = static_cast<std::size_t>(file_size.QuadPart);

        DWORD protect = PAGE_READONLY;
        mapping_handle_ = CreateFileMapping(file_handle_, nullptr, protect,
            0, 0, nullptr);
        if (!mapping_handle_) {
            CloseHandle(file_handle_);
            throw IOError("Failed to create file mapping");
        }

        DWORD view_access = FILE_MAP_READ;
        mapped_data_ = static_cast<std::byte*>(
            MapViewOfFile(mapping_handle_, view_access, 0, 0, 0));

        if (!mapped_data_) {
            CloseHandle(mapping_handle_);
            CloseHandle(file_handle_);
            throw IOError("Failed to map view of file");
        }
#else
        int flags = O_RDONLY;
        file_descriptor_ = ::open(filepath.c_str(), flags);
        if (file_descriptor_ == -1) {
            throw IOError("Failed to open file: " + filepath);
        }

        struct stat st;
        if (fstat(file_descriptor_, &st) == -1) {
            ::close(file_descriptor_);
            throw IOError("Failed to get file size");
        }
        mapped_size_ = static_cast<std::size_t>(st.st_size);

        int prot = PROT_READ;
        mapped_data_ = static_cast<std::byte*>(
            mmap(nullptr, mapped_size_, prot, MAP_PRIVATE, file_descriptor_, 0));

        if (mapped_data_ == MAP_FAILED) {
            ::close(file_descriptor_);
            mapped_data_ = nullptr;
            throw IOError("Failed to mmap file");
        }

        // Опциональная предзагрузка
        if (options_.prefetch) {
            madvise(mapped_data_, mapped_size_, MADV_WILLNEED);
        }
#endif
    }

    void MemoryMappedFile::close() {
#ifdef _WIN32
        if (mapped_data_) {
            UnmapViewOfFile(mapped_data_);
            mapped_data_ = nullptr;
        }
        if (mapping_handle_) {
            CloseHandle(mapping_handle_);
            mapping_handle_ = nullptr;
        }
        if (file_handle_ && file_handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(file_handle_);
            file_handle_ = nullptr;
        }
#else
        if (mapped_data_ && mapped_data_ != MAP_FAILED) {
            munmap(mapped_data_, mapped_size_);
            mapped_data_ = nullptr;
        }
        if (file_descriptor_ != -1) {
            ::close(file_descriptor_);
            file_descriptor_ = -1;
        }
#endif
        mapped_size_ = 0;
    }
} // namespace archive