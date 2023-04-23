//
// Created by sigsegv on 4/19/23.
//

#ifndef BM_UNPACK_H
#define BM_UNPACK_H

#include "Fork.h"

class Unpack : private Fork {
public:
    Unpack(std::string path, std::string destdir);
};


#endif //BM_UNPACK_H
