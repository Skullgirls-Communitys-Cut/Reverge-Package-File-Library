#include "Reverge Package File Library.h"
#include <iostream>
#include <fstream>

int main() {
    try {
        RPFL::ArchiveReader::Config config;
        config.cache_threshold = 2 * 1024 * 1024; // 2MB caching threshold. Standart it's a 1MB
        config.mmap_options.prefetch = true; //Optional preload. Works only on POSIX.

        RPFL::ArchiveReader archive("test.gfs", config);

        //Reading Header
        std::cout << "Header:" << "\n";
        std::cout << "Archive identifier: " << archive.identifier() << "\n";
        std::cout << "Archive version: " << archive.version() << "\n";
        std::cout << "Files count: " << archive.file_count() << "\n";
        std::cout << std::endl;
        // Working with file
        std::cout << "Working with file:" << "\n";
        if (archive.contains("test_text_file.txt")) { //Check if archive have a file
            
            auto& file = archive.get_file("test_text_file.txt"); 
            // Automatic cache, if file is smaller that caching threshold. 
            // If the file is large, we will read from the archive file itself (mmap method)
            //Now it will return false because we haven't tried to read the file yet.
            std::cout << "Is cached: " << (file.is_cached() ? "yes" : "no") << "\n";
            std::cout << std::endl;


            //Method 1: C++20 std::span<std::byte> view    
            std::cout << "Method 1: C++20 std::span<std::byte> view:" << "\n";
            auto data = file.data();
            
            std::cout << "File size: " << data.size() << " bytes\n";
            std::cout << "File data: " << std::string(reinterpret_cast<const char*>(data.data()), data.size()) << "\n"; //Example for a string.
            //Now it will return true, since we read the file when we used file.data();
            //for all subsequent file.data(); or archive.get_file("test_text_file.txt"); we will use the cache
            std::cout << "Is cached: " << (file.is_cached() ? "yes" : "no") << "\n";
            std::cout << std::endl;

            //Method 2: C++17 std::string_view
            std::cout << "Method 2: C++17 std::string_view:" << "\n";
            auto text = file.as_string_view();
            std::cout << "File size: " << text.size() << " bytes\n";
            std::cout << "File data: " << text << "\n"; 
            std::cout << std::endl;

            //Method 3: from the archive file itself by mmap method (Don't even try to cache or not).
            //Works the same as get_file func, returns std::span<std::byte>
            archive.release_all_caches(); //Delete the cache to show that the file is not cached in memory
            std::cout << "Method 3: Read raw data from the archive file" << "\n";
            auto raw_data = archive.read_raw("test_text_file.txt");

            std::cout << "File size: " << raw_data.size() << " bytes\n";
            std::cout << "File data: " << std::string(reinterpret_cast<const char*>(raw_data.data()), raw_data.size()) << "\n"; //Exemple for string
            
            //Returns false only because we use read_raw() func
            std::cout << "Is cached: " << (file.is_cached() ? "yes" : "no") << "\n";
            std::cout << std::endl;
        }

        // Iterate over all files
        std::cout << "Iterate over all files:" << "\n";

        for (const auto& file : archive.files()) {
            std::cout << "File: " << file->path() << " Size: " << file->size() << "\n";
        }

        // Freeing up all caches
        archive.release_all_caches();

    }
    catch (const RPFL::ArchiveException& e) {
        std::cerr << "Reverge Package File Library error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}