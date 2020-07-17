#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H


/* serializable interface for read and write operations */
class Serializable
{
public:
    virtual char* write(unsigned int*) = 0;
    virtual void read(char*) = 0;
    virtual ~Serializable() {/* */};
};

#endif