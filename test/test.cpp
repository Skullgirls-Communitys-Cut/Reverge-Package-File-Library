#include "Reverge Package File Library.h"
#include "test.h"
#include <fstream>
#include <iostream>
#include <array>
int main() {
    //Reading
    std::cout << "Reading" << "\n";
    try {
        RPFL::ArchiveReader::Config config;
        config.cache_threshold = 2 * 1024 * 1024; // 2MB caching threshold. Standart it's a 1MB
        config.mmap_options.prefetch = true; //Optional preload. Works only on POSIX.

        RPFL::ArchiveReader archive("ReadTest.gfs", config);

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
            file.release_cache();; //Delete the cache to show that the file is not cached in memory
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

        // Freeing up all caches () 
        archive.release_all_caches();
        // We can do this manually if required, 
        // but it will be done automatically when the object's destructor is called, 
        // or when .close() is called.
        
        archive.close();
    }
    catch (const RPFL::ArchiveException& e) {
        std::cerr << "Reverge Package File Library error: " << e.what() << "\n";
        return 1;
    }
    std::cout << std::endl;

    //Writing
    std::cout << "Writing" << "\n";
    try
    {
        RPFL::ArchiveWriter::Config config;

        config.identifier = "Reverge Package File"; //Standart value, but you can change this
        config.version = "1.1"; //Standart value

        RPFL::ArchiveWriter writer(config);

        //Method 1:
        //You can add data like a string. If you're using char[] instead, be sure that C-style string have zero-terminator
        writer.add_file("TestWriteFromCodeString.txt", "Hello, World!"); 
        //Be carefull! If the file already exsist, throw a error!
        //To prevent this, let's first delete the file:
        writer.remove_file("TestWriteFromCodeString.txt");
        //And now - add:
        writer.add_file("TestWriteFromCodeString.txt", "Hello, World!");

        //Method 2:
        //or like a span<byte>, that means, it's a any container. Here a Array, for exemple:
        std::array<std::byte, 4> testMessage = {
            std::byte{0x1}, std::byte{0x2}, std::byte{0x3}, std::byte{0x4}  // "Hell"
        };
        writer.add_file("TestWriteFromCodeSpanByte.txt", testMessage); 

        //Method 3:
        //Pointer to data + size
        const char* PointerText = "Hello World!";
        writer.add_file("TestWriteFromCodePoineter.txt", reinterpret_cast<const std::byte*>(PointerText), std::strlen(PointerText));

        //Method 4
        //Add file from a file:
        if (writer.add_file_from_disk("TestWriteFromDisk.txt", "TestWriteFromDisk.txt")) {
            std::cout << "The TestWriteFromDisk.txt file was successfully added." << "\n";
        }

        if (!writer.add_file_from_disk("Trash.txt", "Trash.txt"))//Be carefull! If the file does not exist, returns false.
        std::cout << "Try add Trash.txt to archive, but file doesn't exsist" << "\n";       

        //Method 5
        //Add entire folder:
        writer.add_files_from_directory("TestFolder", "TestFolder/"); //You proba

        writer.write("WriteTest.gfs"); //Save to a disk
    }
    catch (const RPFL::ArchiveException& e) {
        std::cerr << "Reverge Package File Library error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}