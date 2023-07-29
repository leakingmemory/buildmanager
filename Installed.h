//
// Created by sigsegv on 7/29/23.
//

#ifndef BM_INSTALLED_H
#define BM_INSTALLED_H

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

struct InstalledFile {
    std::string hashOrType;
    std::string filename;
};

enum class FileMatch {
    OK,
    NOT_FOUND,
    NOT_MATCHING
};

class Installed {
private:
    std::string group;
    std::string name;
    std::string version;
    std::string files;
    std::vector<std::string> rdep;
    std::filesystem::path path;
public:
    Installed(const std::filesystem::path &dbpath, const std::string &group, const std::string &name, const std::string &version);
    [[nodiscard]] std::vector<InstalledFile> GetFiles() const;
    void Verify(const std::filesystem::path &rootPath, const std::function<void (const std::filesystem::path &, FileMatch)> &) const;
    void Uninstall(const std::filesystem::path &rootPath) const;
    void Unregister() const;
    [[nodiscard]] std::string GetGroup() const {
        return group;
    }
    [[nodiscard]] std::string GetName() const {
        return name;
    }
    [[nodiscard]] std::string GetVersion() const {
        return version;
    }
};


#endif //BM_INSTALLED_H
