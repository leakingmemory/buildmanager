//
// Created by sigsegv on 7/27/23.
//

#ifndef BM_CHROOT_H
#define BM_CHROOT_H

#include <functional>
#include <filesystem>
#include <memory>

class ChrootMounts;

class Chroot {
private:
    std::unique_ptr<ChrootMounts> mounts;
public:
    Chroot(const std::filesystem::path &root, const std::function<void ()> &func);
    ~Chroot();
    Chroot(const Chroot &) = delete;
    Chroot(Chroot &&) = delete;
    Chroot &operator =(const Chroot &) = delete;
    Chroot &operator =(Chroot &&) = delete;
};


#endif //BM_CHROOT_H
