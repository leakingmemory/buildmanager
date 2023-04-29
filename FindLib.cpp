//
// Created by sigsegv on 4/27/23.
//

#include "FindLib.h"
#include <filesystem>

std::string FindFile(const std::string &filename, const char *cpath) {
    std::filesystem::path search{cpath};
    std::filesystem::path p = search / filename;
    if (exists(p)) {
        std::string str = p;
        return str;
    } else {
        return "";
    }
}

template <typename T> concept constcharptr = requires (T item) {
    { item } -> std::convertible_to<const char *>;
};

template <constcharptr... Args> std::string FindFile(const std::string &filename, const char *cpath, Args... a) {
    std::string first = FindFile(filename, cpath);
    if (!first.empty()) {
        return first;
    }
    return FindFile(filename, a...);
}

std::string FindLib::Find() const {
    return FindFile(name, "/lib64", "/usr/lib64", "/lib", "/usr/lib");
}
