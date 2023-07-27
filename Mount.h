//
// Created by sigsegv on 7/27/23.
//

#ifndef BM_MOUNT_H
#define BM_MOUNT_H

#include <string>
extern "C" {
#include <sys/mount.h>
}

class Mount {
private:
    std::string target;
public:
    Mount(const std::string &source, const std::string &target, const std::string &filesystemtype, unsigned long mountflags, const void *data);
    ~Mount();
    Mount(const Mount &) = delete;
    Mount(Mount &&) = delete;
    Mount &operator=(const Mount &) = delete;
    Mount &operator=(Mount &&) = delete;
};


#endif //BM_MOUNT_H
