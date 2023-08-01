//
// Created by sigsegv on 4/19/23.
//

#ifndef BM_UNPACK_H
#define BM_UNPACK_H

#include "Fork.h"
#include <fstream>

class Installed;

class Unpack  {
private:
    Fork fork;
    std::fstream inputStream;
    std::string fileList;
    std::string group;
    std::string name;
    std::string version;
    std::vector<std::string> rdep;
public:
    Unpack(std::string path);
    Unpack(std::string path, std::string destdir);
    void Register(std::string destdir);
    void Replace(Installed &installed, std::string destdir);
};


#endif //BM_UNPACK_H
