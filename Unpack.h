//
// Created by sigsegv on 4/19/23.
//

#ifndef BM_UNPACK_H
#define BM_UNPACK_H

#include "Fork.h"

class Unpack : private Fork {
private:
    std::string fileList;
    std::string group;
    std::string name;
    std::string version;
public:
    Unpack(std::string path, std::string destdir);
};


#endif //BM_UNPACK_H
