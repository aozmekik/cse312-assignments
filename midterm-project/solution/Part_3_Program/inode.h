#ifndef INODE_H
#define INODE_H
#include <ctime>
#include "serializable.h"
#include "string"
#include <iostream>
#include <cstring>
#include <vector>
#include <stdint.h>
#include <stdexcept>


/**
 * represents the i-node structure.
 * contains 5 different fields makes up to <input_size>*KB bytes in total. 
 *  -> attributes field             : metadata about the field             : 328 bytes.
 *  -> data blocks field            : references to data blocks            : x - 334 bytes.
 *  -> single indirect block field  : reference to a block of references   : 2 bytes.
 *  -> double indirect block field  : reference to a block of references   : 2 bytes.
 *  -> triple indirect block field  : two level references                 : 2 bytes.
 *                                                                         + ________
 *                                                <input_size>*KB            x bytes.
 * 
 * INode structure itself is 352 bytes in total. since 334 + 8 (data block *) = 352.
 * but of course, we are not interested in this.
 *  
 **/

#define FILENAME_LENGTH_LIMIT 256


class INode : public Serializable
{
public:
    INode();
    ~INode();

    /* forbidding of copying inodes */
    INode(INode &) = delete;
    INode &operator=(INode &) = delete;

    void getBlocks(std::vector<uint16_t> blcks[4]) const;
    void setBlocks(std::vector<uint16_t> blcks[4]);

    /* serialization */
    char *write(uint32_t *s);
    void read(char *);

    std::string info() const;
    static void setSize(uint16_t);

private:
    class FileAttribute : public Serializable
    {
    public:
        /* serialization */
        char *write(uint32_t *);
        void read(char *);

        static struct std::tm &now();
        FileAttribute(uint32_t = 0, std::string = "filename");
        uint32_t getSize() const;
        std::string getLastModification() const;
        std::string getFileName() const;
        void setSize(uint32_t);
        void updateModification();
        void setFileName(std::string);

    private:
        uint32_t size;
        struct std::tm lastModification;
        char fileName[FILENAME_LENGTH_LIMIT];
    };

    uint16_t *directBlock;
    uint16_t indirectBlock[3]; // 3 block for indirect blocks.

    /* those does not count in the inode block structure. (since they're static.) */
    static uint32_t size;
    static uint32_t size_metadata;
    static uint32_t size_directs;
    static uint32_t size_indirects;

public:
    FileAttribute metadata;
    static uint32_t totalDirectBlocks;
};

/* INode Implementation */
uint32_t INode::size = 0;
uint32_t INode::totalDirectBlocks;
uint32_t INode::size_metadata;
uint32_t INode::size_directs;
uint32_t INode::size_indirects;

INode::INode()
    : metadata()
{
    if (size == 0)
        throw std::logic_error("can't create an with uninitialized size!");
    directBlock = new uint16_t[totalDirectBlocks];
}

INode::~INode()
{
    delete[] directBlock;
}

char *INode::write(uint32_t *s)
{
    /* prepare fields */
    uint32_t _size;
    char *s_metadata = metadata.write(&_size);
    char *buf = new char[size];
    *s = size;

    /* merge */
    memcpy(buf, s_metadata, size_metadata);
    memcpy(buf + size_metadata, directBlock, size_directs);
    memcpy(buf + size_metadata + size_directs, indirectBlock, size_indirects);

    delete[] s_metadata;
    return buf;
}

void INode::read(char *buf)
{
    metadata.read(buf);
    memcpy(directBlock, buf + size_metadata, size_directs);
    memcpy(indirectBlock, buf + size_metadata + size_directs, size_indirects);
}

void INode::setSize(uint16_t _size)
{
    if (!(_size > 0 && ((_size & (_size - 1)) == 0)))
        throw std::invalid_argument("size must be power of 2!");
    size = _size;
    totalDirectBlocks = (size - sizeof(metadata) - (sizeof(uint16_t) * 3)) / sizeof(uint16_t);
    size_metadata = sizeof(FileAttribute);
    size_directs = totalDirectBlocks * sizeof(uint16_t);
    size_indirects = 3 * sizeof(uint16_t);
}

void INode::getBlocks(std::vector<uint16_t> blcks[4]) const
{
    uint32_t total_blocks = (metadata.getSize() / size) + ((metadata.getSize() %size == 0)? 0: 1);
    uint32_t ref_per_block = (size / sizeof(uint16_t));

    /* total number of blocks indirects references */
    uint32_t single_indirect_blocks = ref_per_block;
    uint32_t double_indirect_blocks = ref_per_block * single_indirect_blocks;

    /* find the used blocks */
    const bool single_indirect_used = total_blocks > totalDirectBlocks;
    const bool double_indirect_used = total_blocks > single_indirect_blocks;
    const bool triple_indirect_used = total_blocks > double_indirect_blocks;

    /* collect the direct blocks */
    uint32_t blk_total = std::min(total_blocks, totalDirectBlocks);
    for (unsigned int i = 0; i < blk_total; i++)
        blcks[0].push_back(directBlock[i]);
    blcks[0].push_back(blk_total);

    /* collect the indirect blocks */
    if (single_indirect_used)
    {
        /* single indirect */
        blk_total = std::min(total_blocks - blk_total, single_indirect_blocks);
        blcks[1].push_back(indirectBlock[0]);
        blcks[1].push_back(blk_total);

        if (double_indirect_used)
        {
            /* double indirect */
            blk_total = std::min(total_blocks - blk_total, double_indirect_blocks);
            blcks[2].push_back(indirectBlock[1]);
            blcks[2].push_back(blk_total);
            if (triple_indirect_used)
            {
                /* triple indirect */
                blk_total = std::min(total_blocks - blk_total, double_indirect_blocks);
                blcks[3].push_back(indirectBlock[2]);
                blcks[3].push_back(blk_total);
            }
        }
    }
}

std::string INode::info() const
{
    return std::to_string(metadata.getSize()) + "\t" + metadata.getLastModification() + "\t";
}

void INode::setBlocks(std::vector<uint16_t> blcks[4])
{
    uint32_t total_blocks = (metadata.getSize() / size) + ((metadata.getSize() %size == 0)? 0: 1);
    uint32_t ref_per_block = (size / sizeof(uint16_t));

    /* total number of blocks indirects references */
    uint32_t single_indirect_blocks = ref_per_block;
    uint32_t double_indirect_blocks = ref_per_block * single_indirect_blocks;

    /* find the used blocks */
    const bool single_indirect_used = total_blocks > totalDirectBlocks;
    const bool double_indirect_used = total_blocks > single_indirect_blocks;
    const bool triple_indirect_used = total_blocks > double_indirect_blocks;

    /* set the direct blocks */
    uint32_t blk_total = std::min(total_blocks, totalDirectBlocks);
    for (unsigned int i = 0; i < blk_total; i++)
        directBlock[i] = blcks[0][i];

    if (single_indirect_used)
    {
        indirectBlock[0] = blcks[1][0];
        if (double_indirect_used)
        {
            indirectBlock[1] = blcks[2][0];
            if (triple_indirect_used)
                indirectBlock[2] = blcks[3][0];
        }
    }
}

/* FileAttribute Implementation */

char *INode::FileAttribute::write(uint32_t *s)
{
    size_t mysize = sizeof(FileAttribute);
    *s = mysize;
    char *buf = new char[mysize];
    memcpy(buf, this, mysize);

    return buf;
}

void INode::FileAttribute::read(char *buf)
{
    *this = *((FileAttribute*) buf);
}

struct std::tm &INode::FileAttribute::now()
{
    time_t now;
    time(&now);
    return *localtime(&now);
}

uint32_t INode::FileAttribute::getSize() const
{
    return size;
}

std::string INode::FileAttribute::getLastModification() const
{
    char buffer[80];
    strftime(buffer, 80, "%c", &lastModification);
    return buffer;
}

std::string INode::FileAttribute::getFileName() const
{
    return fileName;
}

void INode::FileAttribute::setSize(uint32_t _size)
{
    size = _size;
}

void INode::FileAttribute::updateModification()
{
    lastModification = now();
}

void INode::FileAttribute::setFileName(std::string source)
{
    if (source.length() >= FILENAME_LENGTH_LIMIT)
        throw std::invalid_argument("filename can't be greater than 255 byte!");
    strncpy(fileName, source.c_str(), FILENAME_LENGTH_LIMIT);
}

INode::FileAttribute::FileAttribute(uint32_t _size, std::string _fileName)
{
    updateModification();
    setSize(_size);
    setFileName(_fileName);
}

#endif