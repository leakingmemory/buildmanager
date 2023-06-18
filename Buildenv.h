//
// Created by sigsegv on 4/10/23.
//

#ifndef BM_BUILDENV_H
#define BM_BUILDENV_H

#include <string>
#include <map>

class Sysconfig;

class Buildenv {
private:
    std::string cflags;
    std::string cxxflags;
    std::string ldflags;
    std::string sysrootCxxflags;
    std::string sysrootLdflags;
    std::string nosysrootLdflags;
    std::string nobootstrapLdflags;
    std::string sysroot;
    std::string cc;
    std::string cxx;
    bool requiresClang;
    bool bootstrappingBuild;
public:
    Buildenv(const Sysconfig &sysconfig, const std::string &cxxflags, const std::string &ldflags, const std::string &sysrootCxxflags, const std::string &sysrootLdflags, const std::string &nosysrootLdflags, const std::string &bobootstrapLdflags, bool requiresClang, bool bootstrappingBuild);
    void FilterEnv(std::map<std::string,std::string> &env);
    std::string Sysroot() {
        return sysroot;
    }
};



#endif //BM_BUILDENV_H
