//
// Created by sigsegv on 4/10/23.
//

#ifndef BM_BUILDENV_H
#define BM_BUILDENV_H

#include <string>
#include <map>

class Buildenv {
private:
    std::string cxxflags;
public:
    Buildenv(const std::string &cxxflags);
    void FilterEnv(std::map<std::string,std::string> &env);
};



#endif //BM_BUILDENV_H
