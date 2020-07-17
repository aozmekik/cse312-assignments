#include <iostream>
#include "file-system.h"
using namespace std;

#define KB 1024

void usage()
{
    std::cout << "Usage: makeFileSystem 4 400 mySystem.dat" << std::endl;
    std::cout << "makeFileSystem <block_size> <inode_count> <disc_name>" << std::endl;
    std::cout << "<block_size> unsigned integer" << std::endl;
    std::cout << "<inode_count> unsigned integer" << std::endl;
}

int main(int argc, char const *argv[])
{
    try
    {
        const int block_size = std::stoi(argv[1]);
        const int inode_count = std::stoi(argv[2]);
        std::string disc_name = argv[3];
        

        if (block_size == 0 || inode_count == 0)
            throw std::logic_error("size can't be zero!");

        FileSystem::settings(KB * block_size, inode_count);
        FileSystem* fs = FileSystem::create(disc_name);
        fs->close();
        std::cout << "Disc is created!" << std::endl;

    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        usage();
    }

    return 0;
}
