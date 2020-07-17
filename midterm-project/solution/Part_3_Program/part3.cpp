#include <iostream>
#include "file-system.h"
#define KB 1024

void usage()
{
    std::cout << "Usage: fileSystemOper fileSystem.data operation parameters" << std::endl;
}

int main(int argc, char const *argv[])
{
    try
    {
        std::string disc_name = argv[1];
        std::string op = argv[2];
        FileSystem *fs = FileSystem::open(disc_name);

        std::string op1;
        std::string op2;
        if (argv[3] != NULL)
        {
            op1 = argv[3];
            op1.erase(std::remove(op1.begin(), op1.end(), '"'), op1.end());
        }
        if (argv[4] != NULL)
        {
            op2 = argv[4];
            op2.erase(std::remove(op2.begin(), op2.end(), '"'), op2.end());
        }

        /* parse operations */
        if (op.compare("list") == 0)
        {
            if (op1.back() != '/')
                op1 += "/";
            fs->list(op1);
        }
        else if (op.compare("mkdir") == 0)
            fs->mkdir(op1);
        else if (op.compare("rmdir") == 0)
            fs->rmdir(op1);
        else if (op.compare("dumpe2fs") == 0)
            fs->dumpe2fs();
        else if (op.compare("write") == 0)
            fs->write(op1, op2);
        else if (op.compare("read") == 0)
            fs->read(op1, op2);
        else if (op.compare("del") == 0)
            fs->del(op1);
        else if (op.compare("ln") == 0)
            fs->ln(op1, op2);
        else if (op.compare("lnsym") == 0)
            fs->lnsym(op1, op2);
        else if (op.compare("fsck") == 0)
            fs->fsck();
        else
            throw std::logic_error("invalid operation!");
        fs->close();
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        usage();
    }

    return 0;
}
