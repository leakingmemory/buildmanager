//
// Created by sigsegv on 7/29/23.
//

#include "Installed.h"
#include "sha2alg.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

class InstalledException : public std::exception {
private:
    const char *error;
public:
    InstalledException(const char *error) : error(error) {}
    const char * what() const noexcept override {
        return error;
    }
};

Installed::Installed(const std::filesystem::path &dbpath, const std::string &group, const std::string &name,
                     const std::string &version) :
                     group(group), name(name), version(version), files(), rdep(), path(dbpath / group / name / version) {
    if (!exists(path / "info.json") || !exists(path / "files")) {
        throw InstalledException("Package install files in DB not found");
    }
    {
        nlohmann::json infoJson{};
        {
            std::ifstream jsonStream{};
            jsonStream.open(path / "info.json", std::ios_base::in | std::ios_base::binary);
            infoJson = nlohmann::json::parse(jsonStream);
            jsonStream.close();
            if (!jsonStream) {
                throw InstalledException("Read error for info.json");
            }
        }
        {
            auto iterator = infoJson.find("rdep");
            if (iterator != infoJson.end() && iterator->is_array()) {
                auto rdeps = *iterator;
                auto iterator = rdeps.begin();
                while (iterator != rdeps.end()) {
                    auto rdep = *iterator;
                    if (rdep.is_string()) {
                        this->rdep.push_back(rdep);
                    }
                    ++iterator;
                }
            }
        }
    }
    {
        std::ifstream jsonStream{};
        jsonStream.open(path / "files", std::ios_base::in | std::ios_base::binary);
        jsonStream.seekg(0, std::ios::end);
        auto length = jsonStream.tellg();
        jsonStream.seekg(0, std::ios::beg);
        {
            char *buffer = (char *) malloc(length);
            jsonStream.read(buffer, length);
            if (!jsonStream) {
                free(buffer);
                jsonStream.close();
                throw InstalledException("Read file list error");
            }
            files.append(buffer, length);
            free(buffer);
        }
        jsonStream.close();
    }
}

std::vector<std::string> SplitLines(const std::string &str) {
    std::vector<std::string> lns{};
    std::string::size_type pos = 0;
    while (pos < str.size()) {
        auto sep = str.find('\n', pos);
        std::string ln{};
        if (sep < str.size()) {
            ln = str.substr(pos, sep - pos);
            pos = sep + 1;
        } else {
            ln = str.substr(pos);
            pos = sep;
        }
        lns.emplace_back(ln);
    }
    return lns;
}

std::vector<InstalledFile> Installed::GetFiles() const {
    std::vector<InstalledFile> list{};
    std::vector<std::string> lns = SplitLines(files);
    for (const auto &ln : lns) {
        if (ln.empty()) {
            continue;
        }
        auto pos = ln.find(' ');
        if (pos > 0 && pos < std::string::npos) {
            std::string hash = ln.substr(0, pos);
            std::string filename = ln.substr(pos + 1);
            if (hash.empty()) {
                throw InstalledException("File hash or type empty");
            }
            if (filename.empty()) {
                throw InstalledException("File list item empty filename");
            }
            InstalledFile file{.hashOrType = hash, .filename = filename};
            list.emplace_back(file);
        } else {
            throw InstalledException("File list format error");
        }
    }
    return list;
}

void Installed::Verify(const std::filesystem::path &rootPath, const std::function<void(const std::filesystem::path &, FileMatch)> &func) const {
    auto files = GetFiles();
    for (const auto &file : files) {
        std::filesystem::path filePath{file.filename};
        std::filesystem::path path = rootPath / filePath;
        if (exists(path) || is_symlink(path)) {
            std::string hashValue{};
            if (is_symlink(path)) {
                hashValue = "link";
            } else if (is_directory(path)) {
                hashValue = "dir";
            } else {
                std::fstream inputStream{};
                inputStream.open(path, std::ios_base::in | std::ios_base::binary);
                if (!inputStream.is_open()) {
                    throw InstalledException("Failed to open file for hashing");
                }
                sha256 hash{};
                while (true) {
                    sha256::chunk_type chunk;
                    inputStream.read((char *) &(chunk[0]), sizeof(chunk));
                    if (inputStream) {
                        hash.Consume(chunk);
                    } else {
                        auto sz = inputStream.gcount();
                        if (sz > 0) {
                            hash.Final((uint8_t *) &(chunk[0]), sz);
                        }
                        break;
                    }
                }
                hashValue = hash.Hex();
            }
            if (hashValue == file.hashOrType) {
                func(filePath, FileMatch::OK);
            } else {
                func(filePath, FileMatch::NOT_MATCHING);
            }
        } else {
            func(filePath, FileMatch::NOT_FOUND);
        }
    }
}

void Installed::Uninstall(const std::filesystem::path &rootPath) const {
    std::vector<std::filesystem::path> directories{};
    Verify(rootPath, [&rootPath, &directories] (auto path, auto match) {
        switch (match) {
            case FileMatch::OK: {
                auto fullPath = rootPath / path;
                if (!is_directory(fullPath)) {
                    std::string fullPathStr = fullPath;
                    if (unlink(fullPathStr.c_str()) != 0) {
                        std::cerr << "Failed to remove file: " << fullPathStr << "\n";
                    }
                } else {
                    directories.push_back(fullPath);
                }
                break;
            }
            case FileMatch::NOT_MATCHING:
                std::cerr << "Modified: " << path << " (not deleted)\n";
                break;
            case FileMatch::NOT_FOUND:
                std::cerr << "Not found: " << path << "\n";
                break;
        }
    });
    int n = 1;
    while (n > 0) {
        n = 0;
        auto iterator = directories.begin();
        while (iterator != directories.end()) {
            std::error_code ec;
            if (std::filesystem::remove(*iterator, ec)) {
                ++n;
                directories.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }
}

void Installed::Unregister() const {
    auto filesPath = path / "files";
    std::error_code ec;
    {
        auto infoPath = path / "info.json";
        if (!std::filesystem::remove(infoPath, ec)) {
            if (!std::filesystem::remove(filesPath, ec)) {
                std::filesystem::remove(path, ec);
                throw InstalledException("Unregister info.json and files");
            }
            throw InstalledException("Unregister info.json");
        }
    }
    if (!std::filesystem::remove(filesPath, ec)) {
        std::filesystem::remove(path, ec);
        throw InstalledException("Unregister files");
    }
    std::filesystem::remove(path, ec);
}
