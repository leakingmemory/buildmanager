//
// Created by sigsegv on 4/27/23.
//

#ifndef BM_FINDLIB_H
#define BM_FINDLIB_H

#include <string>

class FindLib {
private:
    std::string name;
public:
    FindLib(const std::string &name) : name(name) {}
    std::string Find() const;
};


#endif //BM_FINDLIB_H
