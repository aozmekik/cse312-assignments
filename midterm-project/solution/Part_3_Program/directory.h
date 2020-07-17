/***
 * represents an UNIX-like directory.
 * our UNIX-like directory contains one entry directory for each file in that directory.
 * A directory entry contains only two fields: the file name (14 bytes) and
 * the number of the i-node for that file (2 bytes).
 * 
 * [2byte]|[14byte]
 * */

#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <map>
#include <cstring>
#include <vector>
#include "serializable.h"
#include <stdint.h>


#define CURRENT_PATH "."
#define PARENT_PATH ".."

#define FILE_LENGTH 13

class Directory : Serializable
{
public:
    Directory();
    Directory(uint16_t parent, uint16_t current);
    uint16_t getFile(std::string) const;

    /* serialization */
    char *write(uint32_t *);
    void read(char *);

    void printFiles() const;

    class DirectoryEntry : public Serializable
    {
    public:
        DirectoryEntry(uint16_t = 0, const char * = "unnamed", bool = true);
        std::string getFileName() const;
        uint16_t getInode() const;
        void setFileName(const char *);
        void setInodeNumber(uint16_t);
        static uint16_t getSize();
        bool getIsDir() const;

        /* serialization */
        char *write(uint32_t *);
        void read(char *);

    private:
        uint16_t inode;
        char fileName[FILE_LENGTH];
        char isDir;
    };

    void addFile(Directory::DirectoryEntry &);
    void removeFile(std::string filename);
    std::vector<Directory::DirectoryEntry> getEntries() const;
    bool hasFile(std::string) const;

    static uint32_t blockSize;

private:
    std::map<std::string, DirectoryEntry> files;
};

/** Directory Implementation **/
uint32_t Directory::blockSize = 0;

Directory::Directory(uint16_t parent, uint16_t current)
{
    files.insert(std::pair<std::string, Directory::DirectoryEntry>("..", DirectoryEntry(parent, "..")));
    files.insert(std::pair<std::string, Directory::DirectoryEntry>(".", DirectoryEntry(current, ".")));
}

Directory::Directory()
{ /* */
}

uint16_t Directory::getFile(std::string name) const
{
    if (files.count(name) == 0)
        throw std::logic_error("no such file or directory!");
    return files.at(name).getInode();
}

void Directory::addFile(Directory::DirectoryEntry &e)
{
    files.insert(std::pair<std::string, Directory::DirectoryEntry>(e.getFileName(), e));
}

void Directory::removeFile(std::string filename)
{
    files.erase(filename);
}

char *Directory::write(uint32_t *s)
{
    *s = blockSize;
    char *data = new char[*s];
    memset(data, 0, *s);
    uint32_t size = files.size();
    memcpy(data, &size, sizeof(uint32_t));
    char *p = data + sizeof(uint32_t);
    for (auto &e : files)
    {
        uint32_t _size;
        char *entry = e.second.write(&_size);
        memcpy(p, entry, e.second.getSize());
        p += e.second.getSize();
        delete[] entry;
    }
    return data;
}
void Directory::read(char *data)
{
    files.clear();
    uint32_t size;
    memcpy(&size, data, sizeof(uint32_t));

    char *p = data + sizeof(uint32_t);
    for (uint32_t i = 0; i < size; i++, p += 16)
    {
        DirectoryEntry e;
        e.read(p);
        files.insert(std::pair<std::string, DirectoryEntry>(e.getFileName(), e));
    }
}

std::vector<Directory::DirectoryEntry> Directory::getEntries() const
{
    std::vector<Directory::DirectoryEntry> f;

    for (auto const &i : files)
        if (i.first.compare(".") != 0 && i.first.compare("..") != 0)
            f.push_back(i.second);
    return f;
}

bool Directory::hasFile(std::string filename) const
{
    return files.count(filename) != 0;
}

/** DirectoryEntry Implementation **/

Directory::DirectoryEntry::DirectoryEntry(uint16_t inode, const char *name, bool is_dir)
{
    setFileName(name);
    setInodeNumber(inode);
    memcpy(&isDir, &is_dir, 1);
}

std::string Directory::DirectoryEntry::getFileName() const
{
    return fileName;
}

bool Directory::DirectoryEntry::getIsDir() const
{
    bool is_dir;
    memcpy(&is_dir, &isDir, 1);
    return is_dir;
}

uint16_t Directory::DirectoryEntry::getInode() const
{
    return inode;
}

void Directory::DirectoryEntry::setFileName(const char *name)
{
    strncpy(fileName, name, FILE_LENGTH);
}

void Directory::DirectoryEntry::setInodeNumber(uint16_t inode)
{
    this->inode = inode;
}

char *Directory::DirectoryEntry::write(uint32_t *s)
{
    size_t size = 16;
    *s = size;
    char *buf = new char[size]; // 16 byte.
    memcpy(buf, &inode, sizeof(uint16_t));
    memcpy(buf + sizeof(uint16_t), fileName, FILE_LENGTH);
    memcpy(buf + sizeof(uint16_t) + FILE_LENGTH, &isDir, 1);
    return buf;
}

uint16_t Directory::DirectoryEntry::getSize()
{
    return 16;
}

void Directory::DirectoryEntry::read(char *buf)
{
    memcpy(&inode, buf, sizeof(uint16_t));
    memcpy(fileName, buf + sizeof(uint16_t), FILE_LENGTH);
    memcpy(&isDir, buf + sizeof(uint16_t) + FILE_LENGTH, 1);
}

#endif