#pragma once
#include <stdexcept>
#include <string>

namespace RPFL {

    class ArchiveException : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class ArchiveFormatException : public ArchiveException {
    public:
        explicit ArchiveFormatException(const std::string& what)
            : ArchiveException("Archive format error: " + what) {
        }
    };

    class FileNotFoundException : public ArchiveException {
    public:
        explicit FileNotFoundException(const std::string& path)
            : ArchiveException("File not found in archive: " + path) {
        }
    };

    class IOError : public ArchiveException {
    public:
        explicit IOError(const std::string& what)
            : ArchiveException("IO error: " + what) {
        }
    };

} // namespace RPFL