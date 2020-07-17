
/**
 * controller class for data-blocks in file system.
 ***/

#include <fstream>
#include <string>
#include <iostream>
#include <stack>
#include <set>
#include <stdint.h>

#ifndef BLOCKMANAGER_H
#define BLOCKMANAGER_H

class IOCommunicator
{
public:
    /*
     * creates an input-output communicator.
     **/
    IOCommunicator(bool = false);

    /**
     * destroys the connection io disc. must be called at some point to free the resources.
     **/
    void closeDisc();

    /**
     * gets the size of block in bytes.
     **/
    unsigned int getSize();

    /**
     * gets the index of block in block table.
     **/
    uint16_t getIndex();

    /**
     * sets the index of block in block table.
     **/
    void setIndex(unsigned int);

    void setData(void *);
    void *getData() const;

    /**
     * factory functions for io-communicator.
     */
    IOCommunicator &initDisc(std::string);
    IOCommunicator &initSize(unsigned int);
    IOCommunicator &initOffset(unsigned int);

    void checkInitialized() const;

    void readFromDisc();
    void writeToDisc();
    void setUpCursor();

private:
    void *data; // there won't any allocation. only a reference.
    std::fstream discIO;
    unsigned int size;   // block size, will be same for all block in one session;
    unsigned int offset; // data blocks start offset in disc.
    uint16_t index;
    bool binary;

    bool sizeInitialized;
    bool offsetInitialized;
    bool discInitialized;
};

IOCommunicator::IOCommunicator(bool _binary)
    : size(0),
      offset(0),
      index(0),
      binary(_binary),
      sizeInitialized(false),
      offsetInitialized(false),
      discInitialized(false)
{ /* intentionally left blank */
}

void IOCommunicator::setUpCursor()
{
    checkInitialized();
    discIO.seekp(offset + (getIndex() * getSize()));
}

void IOCommunicator::readFromDisc()
{
    setUpCursor();
    discIO.read((char *)getData(), getSize());
}

void IOCommunicator::writeToDisc()
{
    setUpCursor();
    discIO.write((char *)getData(), getSize());
}

IOCommunicator &IOCommunicator::initSize(unsigned int _size)
{
    if (sizeInitialized)
        throw std::logic_error("can't initialize size once more!");
    if (_size == 0)
        throw std::invalid_argument("size can't be zero!");
    size = _size;
    sizeInitialized = true;
    return *this;
}

IOCommunicator &IOCommunicator::initOffset(unsigned int _offset)
{
    if (offsetInitialized)
        throw std::logic_error("can't initialize offset once more!");
    offset = _offset;
    offsetInitialized = true;
    return *this;
}

IOCommunicator &IOCommunicator::initDisc(std::string discName)
{
    if (discInitialized)
        throw std::logic_error("can't initialize disc once more!");
    std::ios_base::openmode mode = std::ios::in | std::ios::out;
    discIO.open(discName, binary ? mode | std::ios::binary : mode);
    discInitialized = true;
    return *this;
}

void IOCommunicator::closeDisc()
{
    checkInitialized();
    discIO.close();
}

unsigned int IOCommunicator::getSize()
{
    return size;
}

void IOCommunicator::setIndex(unsigned int _index)
{
    index = _index;
}

uint16_t IOCommunicator::getIndex()
{
    return index;
}

void IOCommunicator::checkInitialized() const
{
    if (!(sizeInitialized && offsetInitialized && discInitialized))
        throw std::logic_error("unitialized state in communicator!");
}

void *IOCommunicator::getData() const
{
    return data;
}

void IOCommunicator::setData(void *_data)
{
    data = _data;
}

class BlockManager
{
public:
    /**
     * creates a block manager with given total block size.
     **/
    BlockManager(uint16_t _total, unsigned int size, unsigned int offset, std::string discName);
    ~BlockManager();

    void initialize(unsigned int, unsigned int, std::string);
    uint32_t getTotal() const;
    uint32_t freeTotal() const;
    std::set<uint16_t> occupied() const;

    void read(bool *); /* builds from and existing disc */
    bool *write() const;
    bool inFree(uint16_t);

    /** model: block creator and destroyers */

    uint16_t allocateBlock();
    void deallocateBlock(uint16_t);

    /** controller: block mutator and accessors of the disc. **/

    void setBlock(uint16_t index, void *);
    void *getBlock(uint16_t index);

private:
    std::stack<uint16_t> free;
    std::set<uint16_t> full;
    IOCommunicator communicator;
    uint16_t total; // total block size;
};

BlockManager::BlockManager(uint16_t _total, unsigned int size, unsigned int offset, std::string discName)
    : communicator(true), total(_total)
{

    if (total == 0)
        throw std::invalid_argument("block number can't be zero!");

    communicator.initSize(size).initOffset(offset).initDisc(discName);

    // fill the free list with all blocks.
    for (uint16_t i = 0; i < total; i++)
        free.push((total - 1) - i);
}

BlockManager::~BlockManager()
{
    communicator.closeDisc();
}

void BlockManager::initialize(unsigned int size, unsigned int offset, std::string discName)
{
    communicator.initSize(size).initOffset(offset).initDisc(discName);
}

uint32_t BlockManager::getTotal() const
{
    return total;
}

uint32_t BlockManager::freeTotal() const
{
    return free.size();
}

std::set<uint16_t> BlockManager::occupied() const
{
    std::set<uint16_t> occupied(full);
    return occupied;
}

void BlockManager::read(bool *table)
{
    communicator.checkInitialized(); /* communicator must be initialized before build */

    while (!free.empty())
        free.pop();

    // fill the free list with all blocks.
    for (uint16_t i = 0; i < total; i++)
    {
        if (table[(total - 1) - i])
            full.insert((total - 1) - i);
        else
            free.push((total - 1) - i);
    }
}

bool *BlockManager::write() const
{
    communicator.checkInitialized();
    size_t size = total / 8 + ((total % 8) ? 1 : 0);
    bool *table = new bool[size * 8];
    for (size_t i = 0; i < size; i++)
        table[i] = (full.count(i) != 0);
    return table;
}

void BlockManager::setBlock(uint16_t index, void *data)
{
    if (inFree(index))
        throw std::logic_error("desired block in the index is in free list!");
    communicator.checkInitialized();
    communicator.setIndex(index);
    communicator.setData(data);
    communicator.writeToDisc();
}

void *BlockManager::getBlock(uint16_t index)
{
    if (inFree(index))
        throw std::logic_error("desired block in the index is in free list!");
    communicator.checkInitialized();
    communicator.setIndex(index);
    void *data = new char[communicator.getSize()];
    communicator.setData(data);
    communicator.readFromDisc();
    return data;
}

uint16_t BlockManager::allocateBlock()
{
    communicator.checkInitialized();
    uint16_t next = free.top();
    free.pop();
    full.insert(next);
    return next;
}

void BlockManager::deallocateBlock(uint16_t index)
{
    communicator.checkInitialized();
    full.erase(index);
    free.push(index);
}

bool BlockManager::inFree(uint16_t index)
{
    if (index >= total)
        std::logic_error("index out of boundaries!");
    communicator.checkInitialized();
    const bool in_full = full.find(index) != full.end();
    return !in_full;
}

#endif