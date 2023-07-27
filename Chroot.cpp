//
// Created by sigsegv on 7/27/23.
//

#include "Chroot.h"
#include "Mount.h"
#include "Fork.h"
#include <iostream>
extern "C" {
#include <unistd.h>
}

class MountCheckException : public std::exception {
private:
    const char *error;
public:
    MountCheckException(const char *error) : error(error) {}
    const char * what() const noexcept override {
        return error;
    }
};

class MountCheck {
public:
    MountCheck(std::filesystem::path mountpoint) {
        if (!exists(mountpoint) || !is_directory(mountpoint)) {
            throw MountCheckException("Mountpoint does not exist");
        }
    }
};

class CheckedMount : private MountCheck, public Mount
{
public:
    CheckedMount(const std::string &source, const std::string &target, const std::string &filesystemtype, unsigned long mountflags, const void *data)
        : MountCheck(target), Mount(source, target, filesystemtype, mountflags, data) {}
};

class ChrootMounts {
private:
    CheckedMount tmpmount;
    CheckedMount devmount;
    CheckedMount sysmount;
    CheckedMount procmount;
public:
    ChrootMounts(std::filesystem::path root) :
        tmpmount("none", root / "tmp", "tmpfs", 0, nullptr),
        devmount("/dev", root / "dev", "", MS_BIND, nullptr),
        sysmount("/sys", root / "sys", "", MS_BIND, nullptr),
        procmount("/proc", root / "proc", "", MS_BIND, nullptr) {}
};

Chroot::Chroot(const std::filesystem::path &root, const std::function<void()> &func) : mounts() {
    mounts = std::make_unique<ChrootMounts>(root);
    Fork chrootFork{[&root, &func] () {
        std::string rootStr = root;
        if (chdir(rootStr.c_str()) != 0) {
            std::cerr << "chdir: failed dir: " << rootStr << "\n";
            return 1;
        }
        if (chroot(".") != 0) {
            std::cerr << "chroot: failed dir: " << rootStr << "\n";
            return 1;
        }
        func();
        return 0;
    }};
    chrootFork.Require();
}

Chroot::~Chroot() {
}