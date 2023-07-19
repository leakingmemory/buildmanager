//
// Created by sigsegv on 6/14/23.
//

#ifndef BM_SYSCONFIG_H
#define BM_SYSCONFIG_H

#include <string>
#include <filesystem>
extern "C" {
#include <grp.h>
};

class Sysconfig {
private:
    std::string cflags;
    std::string cxxflags;
    std::string ldflags;
    std::string cc;
    std::string cxx;
    std::string username;
    std::string groupname;
public:
    Sysconfig();
    void AppendCflags(std::string &cflags) const {
        if (cflags.empty()) {
            cflags = this->cflags;
        } else if (!this->cflags.empty()) {
            cflags.append(" ");
            cflags.append(this->cflags);
        }
    }
    void AppendCxxflags(std::string &cxxflags) const {
        if (cxxflags.empty()) {
            cxxflags = this->cxxflags;
        } else if (!this->cxxflags.empty()) {
            cxxflags.append(" ");
            cxxflags.append(this->cxxflags);
        }
    }
    void AppendLdflags(std::string &ldflags) const {
        if (ldflags.empty()) {
            ldflags = this->ldflags;
        } else if (!this->ldflags.empty()) {
            ldflags.append(" ");
            ldflags.append(this->ldflags);
        }
    }
    std::string GetCc() const {
        return cc;
    }
    std::string GetCxx() const {
        return cxx;
    }
    pid_t GetUid();
    gid_t GetGid();
    void Chown(const std::filesystem::path &);
    void ChownDirTree(const std::filesystem::path &);
    void SetUserAndGroup();
};


#endif //BM_SYSCONFIG_H
