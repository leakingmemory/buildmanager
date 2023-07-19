//
// Created by sigsegv on 6/14/23.
//

#include "Sysconfig.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <filesystem>
extern "C" {
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <sys/stat.h>
}

using path = std::filesystem::path;

Sysconfig::Sysconfig() : cflags(), cxxflags(), ldflags(), cc(), cxx() {
    std::string configfile{"/etc/buildmanager.conf"};
    path configfilePath{};
    configfilePath = configfile;
    if (!exists(configfilePath)) {
        return;
    }
    std::ifstream configfileInput{configfilePath};
    nlohmann::json jsonData{};
    try {
        jsonData = nlohmann::json::parse(configfileInput);
        {
            auto iterator = jsonData.find("cflags");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    cflags = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("cxxflags");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    cxxflags = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("ldflags");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    ldflags = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("cc");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    cc = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("cxx");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    cxx = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("username");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    username = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("groupname");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    groupname = item;
                }
            }
        }
    } catch (std::exception &e) {
        std::cerr << "warning: parse config file: " << configfile << ": " << e.what() << "\n";
    }
}

pid_t Sysconfig::GetUid() {
    if (username.empty()) {
        return 0;
    }
    constexpr size_t size = 2048;
    passwd pwbuf;
    char buf[size];
    passwd *pw;
    if (getpwnam_r(username.c_str(), &pwbuf, buf, size, &pw) == 0 && pw != nullptr) {
        return pw->pw_uid;
    }
    return 0;
}

gid_t Sysconfig::GetGid() {
    if (groupname.empty()) {
        return 0;
    }
    constexpr size_t size = 2048;
    group grbuf;
    char buf[size];
    group *gr;
    if (getgrnam_r(groupname.c_str(), &grbuf, buf, size, &gr) == 0 && gr != nullptr) {
        return gr->gr_gid;
    }
    return 0;
}

class SysconfigChownOpenFile {
public:
    int fd;
    SysconfigChownOpenFile(const std::filesystem::path &p) {
        std::string fname = p;
        fd = open(fname.c_str(), 0);
        if (fd < 0) {
            std::cerr << "warning: failed to open file for owner/permissions check\n";
            return;
        }
    }
    ~SysconfigChownOpenFile() {
        if (fd >= 0) {
            if (close(fd) != 0) {
                std::cerr << "warning: failed to close file for owner/permissions check\n";
            }
        }
    }
    SysconfigChownOpenFile(const SysconfigChownOpenFile &) = delete;
    SysconfigChownOpenFile(SysconfigChownOpenFile &&) = delete;
    SysconfigChownOpenFile &operator =(const SysconfigChownOpenFile &) = delete;
    SysconfigChownOpenFile &operator =(SysconfigChownOpenFile &&) = delete;
};

void Sysconfig::Chown(const std::filesystem::path &p) {
    if (geteuid() != 0) {
        return;
    }
    SysconfigChownOpenFile f{p};
    uid_t uid;
    gid_t gid;
    uid = GetUid();
    gid = GetGid();
    if (uid <= 0 || gid <= 0) {
        struct stat stbuf;
        if (fstat(f.fd, &stbuf) != 0) {
            std::cerr << "warning: file permissions check fstat failed\n";
            return;
        }
        if (uid <= 0) {
            uid = stbuf.st_uid;
        }
        if (gid <= 0) {
            gid = stbuf.st_gid;
        }
    }
    if (fchown(f.fd, uid, gid) != 0) {
        std::cerr << "warning: failed to set owner/group of file\n";
    }
}

void Sysconfig::ChownDirTree(const std::filesystem::path &p) {
    if (!is_directory(p)) {
        return;
    }
    for (const auto &subitem : std::filesystem::directory_iterator{p}) {
        auto subpath = subitem.path();
        std::string filename = subpath.filename();
        if (filename == "." || filename == "..") {
            continue;
        }
        ChownDirTree(subpath);
    }
    Chown(p);
}

void Sysconfig::SetUserAndGroup() {
    if (geteuid() != 0) {
        return;
    }
    auto gid = GetGid();
    if (gid > 0) {
        setgid(gid);
    }
    auto uid = GetUid();
    if (uid > 0) {
        setuid(uid);
    }
}
