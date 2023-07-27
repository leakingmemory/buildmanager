//
// Created by sigsegv on 7/27/23.
//

#include "Mount.h"
#include <iostream>

class MountException : public std::exception {
private:
    const char *error;
public:
    MountException(const char *error) : error(error) {}
    const char * what() const noexcept override {
        return error;
    }
};

Mount::Mount(const std::string &source, const std::string &target, const std::string &filesystemtype,
             unsigned long mountflags, const void *data) : target(target){
    if (mount(source.c_str(), target.c_str(), filesystemtype.c_str(), mountflags, data) != 0) {
        std::cerr << "error: failed to mount " << target << "\n";
        throw new MountException("Mount failed");
    }
}

Mount::~Mount() {
    if (umount(target.c_str()) != 0) {
        std::cerr << "error: failed to unmount " << target << "\n";
    }
}
