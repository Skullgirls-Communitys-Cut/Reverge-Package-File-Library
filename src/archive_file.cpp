#include "archive_file.hpp"
#include <cstring>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <memory>

namespace RPFL {

    ArchiveFile::ArchiveFile(std::string path,
        std::uint64_t offset,
        std::uint64_t size,
        const std::byte* archive_data,
        std::size_t cache_threshold,
        bool allow_streaming)
        : path_(std::move(path))
        , offset_(offset)
        , size_(size)
        , archive_data_(archive_data)
        , cache_threshold_(cache_threshold)
        , supports_streaming_(allow_streaming&& size_ > cache_threshold_) {

        // Если поддерживается потоковое чтение для больших файлов
        if (supports_streaming_) {
            stream_factory_ = [this]() -> std::shared_ptr<std::istream> {
                // Создаем строковый поток из данных в памяти
                auto data_span = std::span<const std::byte>(archive_data_ + offset_, size_);
                auto ss = std::make_shared<std::stringstream>();
                ss->write(reinterpret_cast<const char*>(data_span.data()), data_span.size());
                ss->seekg(0);
                return ss;
                };
        }
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
        , is_loaded_(other.is_loaded_)
        , supports_streaming_(other.supports_streaming_)
        , stream_factory_(std::move(other.stream_factory_)) {

        other.archive_data_ = nullptr;
        other.is_loaded_ = false;
        other.supports_streaming_ = false;
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
            supports_streaming_ = other.supports_streaming_;
            stream_factory_ = std::move(other.stream_factory_);

            other.archive_data_ = nullptr;
            other.is_loaded_ = false;
            other.supports_streaming_ = false;
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
        else if (supports_streaming_) {
            // Для больших файлов создаем потоковый доступ
            auto stream = stream_factory_();
            data_holder_ = StreamData{ std::move(stream), size_, 0 };
        }
        else {
            // Большие файлы читаем напрямую из mmap (только для чтения)
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
            else if constexpr (std::is_same_v<T, StreamData>) {
                // Для потоковых данных мы должны либо:
                // 1. Прочитать всё в буфер (неэффективно для больших файлов)
                // 2. Вернуть пустой span и требовать использования open_stream()
                // Я выбираю вариант 2 для больших файлов
                static std::vector<std::byte> empty;
                return { empty.data(), 0 };
            }
            else {
                // Для полноты, хотя этот случай никогда не должен произойти
                static std::vector<std::byte> empty;
                return { empty.data(), 0 };
            }
            }, data_holder_);
    }

    std::string_view ArchiveFile::as_string_view() {
        auto span = data();
        // Проверяем, не пустой ли span (например, для потоковых данных)
        if (span.empty() && std::holds_alternative<StreamData>(data_holder_)) {
            // Для потоковых данных читаем всё в строку
            auto stream = open_stream();
            if (!stream) return "";

            std::stringstream ss;
            ss << stream->rdbuf();
            thread_local std::string cached_string;
            cached_string = ss.str();
            return cached_string;
        }
        return { reinterpret_cast<const char*>(span.data()), span.size() };
    }

    std::shared_ptr<std::istream> ArchiveFile::open_stream() {
        if (supports_streaming_) {
            return stream_factory_();
        }

        // Если потоковое чтение не поддерживается, но файл маленький,
        // создаем поток из кэшированных данных
        ensure_loaded();

        return std::visit([](auto&& holder) -> std::shared_ptr<std::istream> {
            using T = std::decay_t<decltype(holder)>;
            if constexpr (std::is_same_v<T, StreamData>) {
                return holder.stream;
            }
            else if constexpr (std::is_same_v<T, CachedData>) {
                auto ss = std::make_shared<std::stringstream>();
                ss->write(reinterpret_cast<const char*>(holder.buffer.get()), holder.size);
                ss->seekg(0);
                return ss;
            }
            else if constexpr (std::is_same_v<T, MappedView>) {
                auto ss = std::make_shared<std::stringstream>();
                ss->write(reinterpret_cast<const char*>(holder.data.data()), holder.data.size());
                ss->seekg(0);
                return ss;
            }
            else {
                return nullptr;
            }
            }, data_holder_);
    }

    std::vector<std::byte> ArchiveFile::read_chunk(std::size_t offset, std::size_t size) {
        if (offset >= size_) {
            return {};
        }

        size = std::min(size, size_ - offset);
        std::vector<std::byte> chunk(size);

        if (is_cached() || !supports_streaming_) {
            // Для кэшированных или отображенных файлов копируем напрямую
            auto span = data();
            if (offset < span.size()) {
                size_t copy_size = std::min(size, span.size() - offset);
                std::memcpy(chunk.data(), span.data() + offset, copy_size);
            }
        }
        else {
            // Для потоковых файлов читаем через поток
            auto stream = open_stream();
            if (!stream) return {};

            stream->seekg(offset);
            stream->read(reinterpret_cast<char*>(chunk.data()), size);
        }

        return chunk;
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

    std::vector<std::byte> ArchiveFile::read_from_stream(std::shared_ptr<std::istream> stream) {
        if (!stream) return {};

        stream->seekg(0, std::ios::end);
        std::size_t size = stream->tellg();
        stream->seekg(0, std::ios::beg);

        std::vector<std::byte> buffer(size);
        stream->read(reinterpret_cast<char*>(buffer.data()), size);

        return buffer;
    }

} // namespace RPFL