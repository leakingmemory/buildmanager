//
// Created by sigsegv on 7/29/23.
//

#include "Installed.h"
#include <fstream>
#include <nlohmann/json.hpp>

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
