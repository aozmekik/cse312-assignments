/**
 * represents the file system management class.
 * disc:
 *  -> [superblock][blocks free bitmap][inodes free bitmap][inodes][rootdir][files and directories] = 1MB
 * 
 * superblock:
 *  -> [block size| number of inodes| number of blocks| inode offset| block offset]
 *  -> [4byte     + 4byte           + 4byte           + 4byte       + 4byte] = 20 bytes.
 * 
 * blocks bitmap:
 *  -> [01010....] = number of blocks / 8 bytes.
 * 
 * inodes :
 *  -> [ inodes bitmap | inodes]  []
 *  ->  inode number/8 + inode number * block_size bytes.
 * 
 * rootdir: @see directory.h
 *  -> [directory entry] = 16 bytes. (14 | 2).
 * 
 * files and directories (blocks):
 *  -> number of blocks * block size bytes. 
**/

// TODO. only check bad file names and directories
// TODO. part3. test operations and write.

#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include "inode.h"
#include "directory.h"
#include "block-manager.h"
#include <sstream>
#include <algorithm>
#include <stdint.h>


#define SUPERBLOCK_SIZE 20
#define PATH_DELIMETER "/"

#define MB 1048576

class FileSystem
{
public:
    FileSystem(FileSystem const &) = delete;
    FileSystem &operator=(FileSystem const &) = delete;

    static void settings(unsigned int size, unsigned int inodes);

    /**
     * creates a file system to given .data file.
     */
    static FileSystem *create(std::string);

    /**
     * opens a file system from given .data file.
     */
    static FileSystem *open(std::string);

    /**
     * closes the file system. 
     */
    static void close();

    /* commands (directly outputed to stdout) */

    void list(std::string);
    void mkdir(std::string);
    void rmdir(std::string);
    void write(std::string, std::string);
    void read(std::string, std::string);
    void del(std::string);
    void dumpe2fs();
    void ln(std::string, std::string);
    void lnsym(std::string, std::string);
    void fsck();

private:
    BlockManager *inodeTable;
    BlockManager *dataBlockTable;
    Directory::DirectoryEntry root;
    static FileSystem *instance;

    static uint32_t blockSize;  /* block size in */
    static uint32_t totalINode; /* total inode */
    static uint32_t totalBlock;
    static uint32_t inodeOffset;
    static uint32_t blockOffset;
    static uint32_t blockBitmapLong;
    static uint32_t inodeBitmapLong;
    static std::string disc;

    FileSystem();

    static void readDisc();
    void writeDisc(bool init = true);

    void readSuperBlock(char *);
    void readFreeBlocks(char *, BlockManager *);

    void writeFreeBlocks(char *, BlockManager *) const;
    void writeSuperBlock(char *) const;

    char *readData(uint16_t);
    void writeData(uint16_t number, char *data, int n);
    void allocData(uint16_t number, int n);
    void deallocData(uint16_t number, int n);
    void readOneBlock(uint16_t number, char *);
    void writeOneBlock(uint16_t number, char *data);

    void singleIndirect(uint16_t number, char *data, int n, bool);
    void doubleIndirect(uint16_t number, char *data, int n, bool);
    void tripleIndirect(uint16_t number, char *data, int n, bool);

    Directory cd(std::string path);
    void readInode(INode &inode, uint16_t number);
    void writeInode(INode &inode, uint16_t number);
    void removeDir(Directory &);
    void removeFile(uint16_t);
    void printTable(BlockManager *table);
};

FileSystem *FileSystem::instance = nullptr;
uint32_t FileSystem::blockSize = 0;
uint32_t FileSystem::totalINode = 0;
uint32_t FileSystem::totalBlock = 0;
uint32_t FileSystem::inodeOffset = 0;
uint32_t FileSystem::blockOffset = 0;
uint32_t FileSystem::blockBitmapLong = 0;
uint32_t FileSystem::inodeBitmapLong = 0;

std::string FileSystem::disc;

FileSystem::FileSystem() : inodeTable(nullptr), dataBlockTable(nullptr), root()
{ /* */
}

void FileSystem::settings(unsigned int size, unsigned int inodes)
{
    if (size == 0 || inodes == 0)
        throw std::invalid_argument("size can't be zero!");
    blockSize = size;
    // DataBlock::setSize(size);
    INode::setSize(size);
    Directory::blockSize = size;
    totalINode = inodes;

    if (inodes * size >= MB)
        throw std::logic_error("bad size! can't be greater than 1MB");
    if (8 * (MB - (SUPERBLOCK_SIZE + (totalINode / 8) + totalINode * blockSize + 16)) / (blockSize + 1) <= 0)
        throw std::logic_error("bad size! no place left for blocks!");

    /* calculating the offsets and counts */
    inodeBitmapLong = totalINode / 8 + ((totalINode % 8) ? 1 : 0);
    totalBlock = ((8 * (MB - (SUPERBLOCK_SIZE + inodeBitmapLong + (totalINode * blockSize) + 16))) - 8) / (8 * blockSize + 1);
    blockBitmapLong = totalBlock / 8 + ((totalBlock % 8) ? 1 : 0);
    inodeOffset = SUPERBLOCK_SIZE + blockBitmapLong + inodeBitmapLong;
    blockOffset = inodeOffset + (totalINode * blockSize) + 16; // TODO. check this.

    if (blockOffset + blockSize * totalBlock > MB)
        throw std::logic_error("bad size!");
}

FileSystem *FileSystem::create(std::string discName)
{
    if (instance != nullptr)
        throw std::logic_error("can't initialize file system once more!");
    if (blockSize == 0 || totalINode == 0)
        throw std::logic_error("uninitialized file system state!");
    disc = discName;

    instance = new FileSystem();
    instance->inodeTable = new BlockManager(totalINode, blockSize, inodeOffset, discName);
    instance->dataBlockTable = new BlockManager(totalBlock, blockSize, blockOffset, discName);

    // allocate inode for root.
    instance->root.setFileName(PATH_DELIMETER);
    instance->root.setInodeNumber(instance->inodeTable->allocateBlock());

    instance->writeDisc();

    // allocate place for root directory in block table.
    INode inode_root;
    inode_root.metadata.setFileName(PATH_DELIMETER);
    inode_root.metadata.setSize(0);
    uint32_t n;
    char *data = inode_root.write(&n);
    instance->inodeTable->setBlock(instance->root.getInode(), data);
    delete[] data;
    Directory root_dir(instance->root.getInode(), instance->root.getInode());
    data = root_dir.write(&n);
    instance->writeData(instance->root.getInode(), data, n);
    delete[] data;

    return instance;
}

FileSystem *FileSystem::open(std::string discName)
{
    if (instance != nullptr)
        throw std::logic_error("can't initialize file system once more!");
    disc = discName;

    instance = new FileSystem();
    instance->readDisc();
    INode::setSize(instance->blockSize);
    Directory::blockSize = instance->blockSize;
    return instance;
}

void FileSystem::close()
{
    instance->writeDisc(false);
    delete instance->inodeTable;
    delete instance->dataBlockTable;
    delete instance;
    instance = nullptr;
}

void FileSystem::writeSuperBlock(char *raw) const
{
    /* prepare the superblock */
    /* assign fields */
    size_t each_size = sizeof(uint32_t);
    memcpy(raw, &blockSize, each_size);
    memcpy(raw + each_size, &totalINode, each_size);
    memcpy(raw + each_size * 2, &totalBlock, each_size);
    memcpy(raw + each_size * 3, &inodeOffset, each_size);
    memcpy(raw + each_size * 4, &blockOffset, each_size);
}

void FileSystem::readSuperBlock(char *raw)
{
    /* assign fields */
    size_t each_size = sizeof(uint32_t);
    memcpy(&blockSize, raw, each_size);
    memcpy(&totalINode, raw + each_size, each_size);
    memcpy(&totalBlock, raw + each_size * 2, each_size);
    memcpy(&inodeOffset, raw + each_size * 3, each_size);
    memcpy(&blockOffset, raw + each_size * 4, each_size);
}

bool getBit(unsigned char byte, unsigned int pos)
{
    return (byte >> pos) & 0x1;
}

void FileSystem::readFreeBlocks(char *raw, BlockManager *table)
{
    size_t size = table->getTotal() / 8 + ((table->getTotal() % 8) ? 1 : 0);
    bool *bitmap = new bool[size * 8];
    for (size_t byte = 0; byte < size; byte++)
        for (size_t bit = 0; bit < 8; bit++)
            bitmap[byte * 8 + bit] = getBit(raw[byte], bit);

    table->read(bitmap);
    delete[] bitmap;
}

void setBit(char *byte, unsigned int pos)
{
    *byte |= 1UL << pos;
}

void FileSystem::writeFreeBlocks(char *raw, BlockManager *table) const
{
    bool *bitmap = table->write();
    size_t size = table->getTotal() / 8 + ((table->getTotal() % 8) ? 1 : 0);
    memset(raw, 0, size);
    for (size_t byte = 0; byte < size; byte++)
    {
        for (size_t bit = 0; bit < 8; bit++)
            if (bitmap[byte * 8 + bit])
                setBit(&raw[byte], bit);
    }
    delete[] bitmap;
}

void FileSystem::readDisc()
{
    /* read the whole disc raw */
    char raw[MB];
    std::ifstream in;
    in.open(disc, std::ios::in | std::ios::binary);
    in.read(raw, MB);
    in.close();

    /* read the sections */
    instance->readSuperBlock(raw); // superblock
    blockBitmapLong = totalBlock / 8 + ((totalBlock % 8) ? 1 : 0);

    instance->inodeTable = new BlockManager(totalINode, blockSize, inodeOffset, disc);
    instance->dataBlockTable = new BlockManager(totalBlock, blockSize, blockOffset, disc);

    instance->readFreeBlocks(raw + SUPERBLOCK_SIZE, instance->dataBlockTable);               // free blocks
    instance->readFreeBlocks(raw + SUPERBLOCK_SIZE + blockBitmapLong, instance->inodeTable); // free inodes
    instance->root.read(raw + inodeOffset + (blockSize * totalINode));                       // root dir.
}

void FileSystem::writeDisc(bool init)
{
    char raw[MB];

    if (!init)
    {
        std::ifstream in;
        in.open(disc, std::ios::out | std::ios::binary);
        in.read(raw, MB);
        in.close();
    }

    /* write the sections */
    writeSuperBlock(raw);                                                 // superblock
    writeFreeBlocks(raw + SUPERBLOCK_SIZE, dataBlockTable);               // free blocks
    writeFreeBlocks(raw + SUPERBLOCK_SIZE + blockBitmapLong, inodeTable); // free inodes
    uint32_t _size;
    char *root_buf = root.write(&_size);
    memcpy(raw + inodeOffset + (blockSize * totalINode), root_buf, root.getSize()); // root dir.
    delete[] root_buf;

    /* write the whole raw to disc */
    std::ofstream out;
    out.open(disc, std::ios::out | std::ios::binary);
    out.write(raw, MB);
    out.close();
}

void FileSystem::readOneBlock(uint16_t number, char *data)
{
    char *block = (char *)dataBlockTable->getBlock(number);
    memcpy(data, block, blockSize);
    delete[] block;
}

void FileSystem::writeOneBlock(uint16_t number, char *data)
{
    dataBlockTable->setBlock(number, data);
}

void FileSystem::singleIndirect(uint16_t number, char *data, int n, bool read = true)
{
    uint16_t *blocks = new uint16_t[blockSize];
    readOneBlock(number, (char *)blocks);
    for (int i = 0; i < n; i++, data += blockSize)
    {
        if (read)
            readOneBlock(blocks[i], data);
        else
            writeOneBlock(blocks[i], data);
    }
    delete[] blocks;
}

void FileSystem::doubleIndirect(uint16_t number, char *data, int n, bool read = true)
{
    uint32_t ref_per_block = (blockSize / sizeof(uint16_t));

    int advance = ref_per_block;
    uint16_t *blocks = new uint16_t[blockSize];
    readOneBlock(number, (char *)blocks);
    for (int i = 0; i < n; i += advance, data += advance * blockSize)
    {
        if (read)
            singleIndirect(blocks[i], data, advance);
        else
            singleIndirect(blocks[i], data, advance, false);
    }
    delete[] blocks;
}

void FileSystem::tripleIndirect(uint16_t number, char *data, int n, bool read = true)
{
    uint32_t ref_per_block = (blockSize / sizeof(uint16_t));

    int advance = ref_per_block * ref_per_block;
    uint16_t *blocks = new uint16_t[blockSize];
    readOneBlock(number, (char *)blocks);
    for (int i = 0; i < n; i += advance, data += advance * blockSize)
    {
        if (read)
            doubleIndirect(blocks[i], data, advance);
        else
            doubleIndirect(blocks[i], data, advance, false);
    }
    delete[] blocks;
}

void FileSystem::deallocData(uint16_t number, int n)
{
    if (n == 0)
        return;
    INode inode;
    readInode(inode, number);
    std::vector<uint16_t> blcks[4];
    inode.getBlocks(blcks);

    uint32_t total_blocks = (n / blockSize) + ((n % blockSize == 0) ? 0 : 1);
    uint32_t ref_per_block = (blockSize / sizeof(uint16_t));

    /* total number of blocks indirects references */
    uint32_t single_indirect_blocks = ref_per_block;
    uint32_t double_indirect_blocks = ref_per_block * single_indirect_blocks;

    /* find the used blocks */
    const bool single_indirect_used = total_blocks > INode::totalDirectBlocks;
    const bool double_indirect_used = total_blocks > single_indirect_blocks;
    const bool triple_indirect_used = total_blocks > double_indirect_blocks;

    /* dealloc the direct blocks */
    uint32_t blk_total = std::min(total_blocks, INode::totalDirectBlocks);
    for (unsigned int i = 0; i < blk_total; i++)
        dataBlockTable->deallocateBlock(blcks[0][i]);

    /* dealloc the indirect blocks */
    if (single_indirect_used)
    {
        /* single indirect */
        blk_total = std::min(total_blocks - blk_total, single_indirect_blocks);
        uint16_t single_indirect = blcks[1][0];
        dataBlockTable->deallocateBlock(single_indirect);
        char *data = new char[blockSize];

        readOneBlock(single_indirect, data);

        for (unsigned i = 0; i < blk_total / blockSize; i++)
        {
            uint16_t address;
            memcpy(&address, &data[i], sizeof(uint16_t));
            dataBlockTable->deallocateBlock(address);
        }

        if (double_indirect_used)
        {
            /* double indirect */
            blk_total = std::min(total_blocks - blk_total, double_indirect_blocks);
            uint16_t double_indirect = blcks[2][0];
            dataBlockTable->deallocateBlock(double_indirect);
            char *data2 = new char[blockSize];
            memset(data2, 0, blockSize);

            readOneBlock(double_indirect, data);
            for (size_t i = 0; i < blk_total / (blockSize * blockSize); i++)
            {
                uint16_t address;
                memcpy(&address, &data[i], sizeof(uint16_t));
                dataBlockTable->deallocateBlock(address);

                readOneBlock(address, data2);
                for (unsigned j = 0; j < blk_total / blockSize; j++)
                {
                    uint16_t address2;
                    memcpy(&address2, &data2[j], sizeof(uint16_t));
                    dataBlockTable->deallocateBlock(address2);
                }
            }
            if (triple_indirect_used)
            {
                /* triple indirect */
                blk_total = std::min(total_blocks - blk_total, double_indirect_blocks);
                uint16_t triple_indirect = blcks[3][0];
                dataBlockTable->deallocateBlock(triple_indirect);
                char *data3 = new char[blockSize];

                readOneBlock(triple_indirect, data3);
                for (size_t k = 0; k < blk_total / (blockSize * blockSize * blockSize); k++)
                {

                    uint16_t address3;
                    memcpy(&address3, &data3[k], sizeof(uint16_t));
                    dataBlockTable->deallocateBlock(address3);

                    readOneBlock(address3, data2);
                    for (size_t i = 0; i < blk_total / (blockSize * blockSize); i++)
                    {
                        uint16_t address;
                        memcpy(&address, &data2[i], sizeof(uint16_t));
                        dataBlockTable->deallocateBlock(address);

                        readOneBlock(address, data);
                        for (unsigned j = 0; j < blk_total / blockSize; j++)
                        {
                            uint16_t address2;
                            memcpy(&address2, &data[j], sizeof(uint16_t));
                            dataBlockTable->deallocateBlock(address2);
                        }
                    }
                }
                delete[] data3;
            }
            delete[] data2;
        }
        delete[] data;
    }
}

void FileSystem::allocData(uint16_t number, int n)
{
    INode inode;
    readInode(inode, number);
    deallocData(number, inode.metadata.getSize());
    std::vector<uint16_t> blcks[4];

    uint32_t total_blocks = (n / blockSize) + ((n % blockSize == 0) ? 0 : 1);
    uint32_t ref_per_block = (blockSize / sizeof(uint16_t));

    /* total number of blocks indirects references */
    uint32_t single_indirect_blocks = ref_per_block;
    uint32_t double_indirect_blocks = ref_per_block * single_indirect_blocks;
    uint32_t triple_indirect_blocks = ref_per_block * double_indirect_blocks;

    /* find the used blocks */
    const bool single_indirect_used = total_blocks > INode::totalDirectBlocks;
    const bool double_indirect_used = total_blocks > single_indirect_blocks;
    const bool triple_indirect_used = total_blocks > double_indirect_blocks;

    /* alloc the direct blocks */
    uint32_t blk_total = std::min(total_blocks, INode::totalDirectBlocks);
    for (unsigned int i = 0; i < blk_total; i++)
        blcks[0].push_back(dataBlockTable->allocateBlock());

    /* alloc the indirect blocks */
    if (single_indirect_used)
    {
        /* single indirect */
        blk_total = std::min(total_blocks - blk_total, single_indirect_blocks);
        uint16_t single_indirect = dataBlockTable->allocateBlock();
        blcks[1].push_back(single_indirect);
        char *data = new char[blockSize];
        memset(data, 0, blockSize);

        for (unsigned i = 0; i < blk_total / blockSize; i++)
        {
            uint16_t address = dataBlockTable->allocateBlock();
            memcpy(&data[i], &address, sizeof(uint16_t));
        }
        writeOneBlock(single_indirect, data);

        if (double_indirect_used)
        {
            /* double indirect */
            blk_total = std::min(total_blocks - blk_total, double_indirect_blocks);
            uint16_t double_indirect = dataBlockTable->allocateBlock();
            blcks[2].push_back(double_indirect);
            char *data2 = new char[blockSize];
            memset(data2, 0, blockSize);

            for (size_t i = 0; i < blk_total / (blockSize * blockSize); i++)
            {
                uint16_t address = dataBlockTable->allocateBlock();

                for (unsigned j = 0; j < blk_total / blockSize; j++)
                {
                    uint16_t address2 = dataBlockTable->allocateBlock();
                    memcpy(&data[j], &address2, sizeof(uint16_t));
                }
                writeOneBlock(address, data);
                memcpy(&data2[i], &address, sizeof(uint16_t));
            }

            writeOneBlock(double_indirect, data2);

            if (triple_indirect_used)
            {
                /* triple indirect */
                blk_total = std::min(total_blocks - blk_total, double_indirect_blocks);
                uint16_t triple_indirect = dataBlockTable->allocateBlock();
                blcks[3].push_back(triple_indirect_blocks);
                char *data3 = new char[blockSize];
                memset(data3, 0, blockSize);

                for (size_t k = 0; k < blk_total / (blockSize * blockSize * blockSize); k++)
                {
                    uint16_t address3 = dataBlockTable->allocateBlock();

                    for (size_t i = 0; i < blk_total / (blockSize * blockSize); i++)
                    {
                        uint16_t address = dataBlockTable->allocateBlock();

                        for (unsigned j = 0; j < blk_total / blockSize; j++)
                        {
                            uint16_t address2 = dataBlockTable->allocateBlock();
                            memcpy(&data[j], &address2, sizeof(uint16_t));
                        }
                        writeOneBlock(address, data);
                        memcpy(&data2[i], &address, sizeof(uint16_t));
                    }

                    writeOneBlock(address3, data2);
                    memcpy(&data3[k], &address3, sizeof(uint16_t));
                }

                writeOneBlock(triple_indirect, data3);
                delete[] data3;
            }
            delete[] data2;
        }
        delete[] data;
    }

    readInode(inode, number);
    inode.metadata.setSize(n);
    inode.setBlocks(blcks);
    writeInode(inode, number);
}

void FileSystem::readInode(INode &inode, uint16_t number)
{
    char *inode_data = (char *)inodeTable->getBlock(number);
    inode.read(inode_data);
    delete[] inode_data;
}

void FileSystem::writeInode(INode &inode, uint16_t number)
{
    uint32_t size;
    char *inode_data = inode.write(&size);
    inodeTable->setBlock(number, inode_data);
    delete[] inode_data;
}

void FileSystem::writeData(uint16_t number, char *data, int n)
{

    INode inode;
    readInode(inode, number);

    allocData(number, n);
    readInode(inode, number);

    /* read all direct and undirect blocks */
    std::vector<uint16_t> blks[4];
    inode.getBlocks(blks);

    uint32_t total_blocks = (n / blockSize) + ((n % blockSize == 0) ? 0 : 1);
    uint32_t ref_per_block = (blockSize / sizeof(uint16_t));

    /* total number of blocks indirects references */
    uint32_t single_indirect_blocks = ref_per_block;
    uint32_t double_indirect_blocks = ref_per_block * single_indirect_blocks;

    /* find the used blocks */
    const bool single_indirect_used = total_blocks > INode::totalDirectBlocks;
    const bool double_indirect_used = total_blocks > single_indirect_blocks;
    const bool triple_indirect_used = total_blocks > double_indirect_blocks;

    /* write directs */
    char *p = data;
    for (size_t i = 0; i < blks[0].back(); i++, p += blockSize)
        writeOneBlock(blks[0][i], p);

    /* write single indirects */
    if (single_indirect_used)
    {
        singleIndirect(blks[1].at(0), p, blks[1].back(), false);
        p += blks[1].back() * blockSize;

        /* read double indirects */
        if (double_indirect_used)
        {
            doubleIndirect(blks[2].at(0), p, blks[2].back(), false);
            p += blks[2].back() * blockSize;

            /* read triple indirects */
            if (triple_indirect_used)
                tripleIndirect(blks[3].at(0), p + blks[0].back() * blockSize, blks[2].back(), false);
        }
    }
}

char *FileSystem::readData(uint16_t number)
{

    INode inode;
    readInode(inode, number);
    uint32_t file_size = inode.metadata.getSize();

    /* read all direct and undirect blocks */
    std::vector<uint16_t> blks[4];
    inode.getBlocks(blks);

    uint32_t total_blocks = (file_size / blockSize) + ((file_size % blockSize == 0) ? 0 : 1);
    char *data = new char[total_blocks * blockSize];

    uint32_t ref_per_block = (blockSize / sizeof(uint16_t));

    /* total number of blocks indirects references */
    uint32_t single_indirect_blocks = ref_per_block;
    uint32_t double_indirect_blocks = ref_per_block * single_indirect_blocks;

    /* find the used blocks */
    const bool single_indirect_used = total_blocks > INode::totalDirectBlocks;
    const bool double_indirect_used = total_blocks > single_indirect_blocks;
    const bool triple_indirect_used = total_blocks > double_indirect_blocks;

    /* read directs */
    char *p = data;
    for (size_t i = 0; i < blks[0].back(); i++, p += blockSize)
        readOneBlock(blks[0][i], p);

    /* read single indirects */
    if (single_indirect_used)
    {
        singleIndirect(blks[1].at(0), p, blks[1].back());
        p += blks[1].back() * blockSize;

        /* read double indirects */
        if (double_indirect_used)
        {
            doubleIndirect(blks[2].at(0), p, blks[2].back());
            p += blks[2].back() * blockSize;

            /* read triple indirects */
            if (triple_indirect_used)
                tripleIndirect(blks[3].at(0), p + blks[0].back() * blockSize, blks[2].back());
        }
    }

    return data;
}

Directory FileSystem::cd(std::string path)
{
    std::string token;
    char *data;

    size_t occurence = std::count(path.begin(), path.end(), '/');

    Directory *dirs = new Directory[occurence];

    data = (char *)readData(root.getInode());
    dirs[0].read(data);
    delete[] data;

    size_t pos = 0;
    path.erase(0, path.find(PATH_DELIMETER) + 1);
    for (size_t i = 1; i < occurence; i++)
    {
        pos = path.find(PATH_DELIMETER);
        token = path.substr(0, pos);
        data = (char *)readData(dirs[i - 1].getFile(token));
        dirs[i].read(data);
        delete[] data;
        path.erase(0, path.find(PATH_DELIMETER) + 1);
    }

    Directory dir = dirs[occurence - 1];
    delete[] dirs;

    return dir;
}

void FileSystem::list(std::string path)
{
    std::vector<Directory::DirectoryEntry> files = cd(path).getEntries();

    for (auto const &t : files)
    {
        INode inode;
        readInode(inode, t.getInode());
        std::cout << inode.info() << t.getFileName() << (t.getIsDir() ? "\t(dir)" : " ")
                  << std::endl;
    }
}

std::string getFile(std::string path)
{
    size_t occurence = std::count(path.begin(), path.end(), '/');
    for (size_t i = 0; i < occurence; i++)
        path = path.substr(path.find(PATH_DELIMETER) + 1);
    return path;
}

std::string getPath(std::string path)
{
    std::string file = getFile(path);
    return path.substr(0, path.find(file));
}

void FileSystem::mkdir(std::string path)
{
    std::string file = getFile(path);
    path = getPath(path);
    Directory current_dir = cd(path);

    /* create the new directory */
    uint16_t new_dir_inode = inodeTable->allocateBlock();
    Directory::DirectoryEntry new_dir(new_dir_inode, file.c_str());
    INode new_inode;
    new_inode.metadata.setFileName(file);
    uint32_t inode_size;
    char *inode_data = new_inode.write(&inode_size);
    inodeTable->setBlock(new_dir_inode, inode_data);
    delete[] inode_data;
    Directory dir(current_dir.getFile("."), new_dir_inode);
    inode_data = dir.write(&inode_size);
    writeData(new_dir_inode, inode_data, inode_size);
    delete[] inode_data;

    /* insert to the given directory */
    uint32_t dir_size;
    current_dir.addFile(new_dir);
    char *data = current_dir.write(&dir_size);
    writeData(current_dir.getFile("."), data, dir_size);
    delete[] data;
}

void FileSystem::removeFile(uint16_t index)
{
    INode inode;
    readInode(inode, index);
    deallocData(index, inode.metadata.getSize());
    inodeTable->deallocateBlock(index);
}

void FileSystem::removeDir(Directory &dir)
{
    for (auto &i : dir.getEntries())
    {
        if (i.getIsDir()) // recursively deleting files.
        {
            char *dir_data = readData(i.getInode());
            Directory dir;
            dir.read(dir_data);
            removeDir(dir);
            delete[] dir_data;
        }
        else
            removeFile(i.getInode());
        dir.removeFile(i.getFileName());
    }

    removeFile(dir.getFile("."));
}

void FileSystem::rmdir(std::string path)
{
    Directory current_dir = cd(path + "/");

    removeDir(current_dir);

    std::string file = getFile(path);
    path = getPath(path);
    Directory parent_dir = cd(path);

    uint32_t dir_size;
    parent_dir.removeFile(file);
    char *data = parent_dir.write(&dir_size);
    writeData(parent_dir.getFile("."), data, dir_size);
    delete[] data;
}

inline std::string readLinuxFile(const std::string &path)
{
    std::ostringstream buf;
    std::ifstream input(path.c_str());
    buf << input.rdbuf();
    return buf.str();
}

inline void writeLinuxFile(char *data, uint32_t size, const std::string &path)
{
    std::ofstream out;
    out.open(path, std::ios::out | std::ios::binary);
    out.write(data, size);
    out.close();
}

void FileSystem::write(std::string path, std::string file)
{
    Directory current_dir = cd(getPath(path));
    std::string file_name = getFile(path);

    if (current_dir.hasFile(file_name))
        del(path);

    Directory::DirectoryEntry new_file(inodeTable->allocateBlock(), file_name.c_str(), false);

    std::string content = readLinuxFile(file);

    // create inode.
    INode inode;
    inode.metadata.setFileName(file_name);
    writeInode(inode, new_file.getInode());

    // write the data into inode.
    uint32_t file_size = (content.length() / blockSize) * blockSize + blockSize;
    char *data = new char[file_size];
    memset(data, 0, file_size);
    memcpy(data, content.c_str(), content.length());

    writeData(new_file.getInode(), data, content.length());
    delete[] data;

    // insert the file into directory
    current_dir.addFile(new_file);
    uint32_t dir_size;
    data = current_dir.write(&dir_size);
    writeData(current_dir.getFile("."), data, dir_size);
    delete[] data;
}

void FileSystem::read(std::string path, std::string linux_file)
{
    std::string file = getFile(path);
    Directory current_dir = cd(path);

    uint16_t file_inode = current_dir.getFile(file);

    INode inode;
    readInode(inode, file_inode);

    char *data = readData(file_inode);
    writeLinuxFile(data, inode.metadata.getSize(), linux_file);
    delete[] data;
}

void FileSystem::del(std::string path)
{
    std::string file = getFile(path);
    path = getPath(path);

    Directory current_dir = cd(path);

    // delete inode and data.
    removeFile(current_dir.getFile(file));

    // delete the directory entry from directory
    current_dir.removeFile(file);
    uint32_t dir_size;
    char *data = current_dir.write(&dir_size);
    writeData(current_dir.getFile("."), data, dir_size);
    delete[] data;
}

void FileSystem::dumpe2fs()
{
    // print general info
    std::cout << "Inode count:\t\t" << inodeTable->getTotal() << std::endl;
    std::cout << "Block count:\t\t" << dataBlockTable->getTotal() << std::endl;
    std::cout << "Free inodes:\t\t" << inodeTable->freeTotal() << std::endl;
    std::cout << "Free blocks:\t\t" << dataBlockTable->freeTotal() << std::endl;
    std::cout << "Block size:\t\t" << blockSize << std::endl;
    std::cout << "Inode size:\t\t" << blockSize << std::endl;

    // print occupied inodes and blocks.
    std::cout << "Occupied inodes:" << std::endl;
    for (auto &index : inodeTable->occupied())
    {
        INode inode;
        std::vector<uint16_t> blcks[4];
        readInode(inode, index);
        inode.getBlocks(blcks);
        std::cout << "Index: " << index << "\tFilename: " << inode.metadata.getFileName() << std::endl;
        std::cout << "Occopied blocks: { ";

        for (int i = 0; i < 4; i++)
            for (auto &b : blcks[i])
                std::cout << b << ",";
        std::cout << "\b\b\b";
        std::cout << " }" << std::endl;
    }
}

void FileSystem::printTable(BlockManager *table)
{
    bool *bitmap = table->write();
    std::cout << "Block in use:" << std::endl;
    for (size_t i = 0; i < table->getTotal(); i++)
    {
        std::cout << std::to_string(bitmap[i] ? 1 : 0) << " ";
        if (i % 35 == 0)
            std::cout << std::endl;
    }
    std::cout << "]" << std::endl;

    std::cout << "\t\tFree blocks[" << std::endl;
    for (size_t i = 0; i < table->getTotal(); i++)
    {
        std::cout << std::to_string(bitmap[i] ? 0 : 1) << " ";
        if (i % 35 == 0)
            std::cout << std::endl;
    }
    std::cout << std::endl;
}
void FileSystem::ln(std::string file1, std::string file2)
{
    read(file1, "temp_file.txt");
    write(file2, "temp_file.txt");
    remove("temp_file.txt");
}
void FileSystem::lnsym(std::string file1, std::string file2)
{
    Directory dir1 = cd(getPath(file1));
    Directory dir2 = cd(getPath(file2));

    uint16_t inode = dir1.getFile(getFile(file1));

    Directory::DirectoryEntry entry(inode, getFile(file2).c_str(), false);

    dir2.addFile(entry);
    uint32_t size;
    char *data = dir2.write(&size);
    writeData(dir2.getFile("."), data, size);
}

void FileSystem::fsck()
{
    std::cout << "Blocks:";
    printTable(dataBlockTable);
    std::cout << "Inodes:";
    printTable(inodeTable);
}

#endif