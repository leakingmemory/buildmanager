//
// Created by sigsegv on 4/19/23.
//

#include "Unpack.h"
#include "PkgHdr.h"
#include "Exec.h"
#include "Installed.h"
#include <iostream>
#include <nlohmann/json.hpp>
extern "C" {
#include <unistd.h>
}

class UnpackException : public std::exception {
private:
    const char *error;
public:
    UnpackException(const char *error) : error(error) {}
    const char * what() const noexcept override {
        return error;
    }
};

Unpack::Unpack(std::string path) : fork() {
    inputStream.open(path, std::ios_base::in | std::ios_base::binary);
    if (!inputStream.is_open()) {
        throw UnpackException("Failed to open input file");
    }
    uint32_t fileListLen;
    inputStream.read((char *) &fileListLen, sizeof(fileListLen));
    if (!inputStream) {
        throw UnpackException("Expected package to start with package list size");
    }
    std::string pkginfo{"{}"};
    if (fileListLen == PKG_MAGIC) {
        PkgHdr hdr{};
        hdr.magic = fileListLen;
        inputStream.read((char *) &(hdr.jsonLength), sizeof(hdr) - sizeof(fileListLen));
        if (!inputStream) {
            throw UnpackException("Expected package to start with package header");
        }
        fileListLen = hdr.listLength;
        char *buf = (char *) malloc(hdr.jsonLength);
        inputStream.read(buf, hdr.jsonLength);
        if (!inputStream) {
            free(buf);
            throw UnpackException("Truncated pkg info");
        }
        pkginfo = "";
        pkginfo.append(buf, hdr.jsonLength);
        free(buf);
    }
    {
        char *buf = (char *) malloc(fileListLen);
        inputStream.read(buf, fileListLen);
        if (!inputStream) {
            free(buf);
            throw UnpackException("Truncated file list");
        }
        fileList.append(buf, fileListLen);
        free(buf);
    }
    nlohmann::json pkgData{};
    pkgData = nlohmann::json::parse(pkginfo);
    {
        auto iterator = pkgData.find("group");
        if (iterator != pkgData.end() && iterator->is_string()) {
            group = *iterator;
            std::cout << "Group: " << group << "\n";
        }
    }
    {
        auto iterator = pkgData.find("name");
        if (iterator != pkgData.end() && iterator->is_string()) {
            name = *iterator;
            std::cout << "Name: " << name << "\n";
        }
    }
    {
        auto iterator = pkgData.find("version");
        if (iterator != pkgData.end() && iterator->is_string()) {
            version = *iterator;
            std::cout << "Version: " << version << "\n";
        }
    }
    {
        auto iterator = pkgData.find("rdep");
        if (iterator != pkgData.end() && iterator->is_array()) {
            auto jsonArray = *iterator;
            auto iterator = jsonArray.begin();
            while (iterator != jsonArray.end()) {
                if (iterator->is_string()) {
                    rdep.push_back(*iterator);
                }
                ++iterator;
            }
        }
    }
}

Unpack::Unpack(std::string path, std::string destdir) : Unpack(path) {
    std::cout << "==> Unpacking " << path << " into " << destdir << "\n";
    fork = {[destdir] () {
        if (chdir(destdir.c_str()) != 0) {
            std::cerr << "chdir: build dir: " << destdir << "\n";
            return 1;
        }
        Exec exec{"cpio"};
        std::vector<std::string> args{};
        args.emplace_back("--extract");
        args.emplace_back("-u");
        auto env = Exec::getenv();
        exec.exec(args, env);
        return 0;
    }, ForkInputOutput::INPUT};
    fork << inputStream;
    fork.CloseInput();
    fork.Require();
}

void Unpack::Register(std::string destdir) {
    if (group.empty() || name.empty() || version.empty()) {
        throw UnpackException("Invalid package for register (missing group, name and/or version)");
    }
    std::filesystem::path dbdir = destdir;
    if (!exists(dbdir)) {
        std::cerr << "error: destination root for register pkg not found: " << destdir << "\n";
        throw UnpackException("Destination root");
    }
    dbdir = dbdir / "var";
    if (!exists(dbdir) && !create_directory(dbdir)) {
        std::cerr << "error: destination root /var for register pkg not found: " << destdir << "\n";
        throw UnpackException("Destination root");
    }
    dbdir = dbdir / "lib";
    if (!exists(dbdir) && !create_directory(dbdir)) {
        std::cerr << "error: destination root /var/lib for register pkg not found: " << destdir << "\n";
        throw UnpackException("Destination root");
    }
    dbdir = dbdir / "jpkg";
    if (!exists(dbdir) && !create_directory(dbdir)) {
        std::cerr << "error: destination root /var/lib/jpkg for register pkg not found: " << destdir << "\n";
        throw UnpackException("Destination root");
    }
    auto pkgdir = dbdir / group;
    if (!exists(pkgdir) && !create_directory(pkgdir)) {
        std::cerr << "error: unable to create pkgdir in /var/lib/jpkg for group: " << group << "\n";
        throw UnpackException("Destination root");
    }
    pkgdir = pkgdir / name;
    if (!exists(pkgdir) && !create_directory(pkgdir)) {
        std::cerr << "error: unable to create pkgdir in /var/lib/jpkg in group " << group << " for: " << name << "\n";
        throw UnpackException("Destination root");
    }
    pkgdir = pkgdir / version;
    if (!exists(pkgdir) && !create_directory(pkgdir)) {
        std::cerr << "error: unable to create pkgdir for version " << version << " in group " << group << " and pkg " << name << "\n";
        throw UnpackException("Destination root");
    }
    if (exists(pkgdir / "info.json") && exists(pkgdir / "files")) {
        std::cerr << "error: " << group << "/" << name << "/" << version << " already indstalled\n";
        throw UnpackException("Already installed");
    }
    {
        std::string clob{};
        {
            nlohmann::json pkgData{};
            pkgData.emplace("group", group);
            pkgData.emplace("name", name);
            pkgData.emplace("version", version);
            {
                auto jsonArray = nlohmann::json::array();
                for (const auto &dep: rdep) {
                    jsonArray.push_back(dep);
                }
                pkgData.emplace("rdep", jsonArray);
            }
            clob = pkgData.dump();
        }
        std::ofstream infoOut{};
        {
            std::string filename = pkgdir / "info.json";
            infoOut.open(filename, std::ios_base::out | std::ios_base::binary);
        }
        infoOut.write(clob.c_str(), (std::streamsize) clob.size());
        infoOut.close();
        if (!infoOut) {
            throw UnpackException("Unable to register package as installed (info.json)");
        }
    }
    {
        std::ofstream filesOut{};
        {
            std::string filename = pkgdir / "files";
            filesOut.open(filename, std::ios_base::out | std::ios_base::binary);
            filesOut.write(fileList.c_str(), fileList.size());
        }
        filesOut.close();
        if (!filesOut) {
            throw UnpackException("Unable to register package as installed (files)");
        }
    }
}

void Unpack::Replace(Installed &installed, std::string destdir) {
    auto oldFiles = installed.GetFiles();
    {
        auto newFiles = Installed::GetFiles(fileList);
        {
            auto iterator = oldFiles.begin();
            while (iterator != oldFiles.end()) {
                const auto &oldFile = *iterator;
                bool found{false};
                for (const auto &newFile: newFiles) {
                    if (oldFile.filename == newFile.filename) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    oldFiles.erase(iterator);
                } else {
                    ++iterator;
                }
            }
        }
    }
    std::vector<std::filesystem::path> directories{};
    for (const auto &oldFile : oldFiles) {
        std::filesystem::path rootPath{destdir};
        std::filesystem::path filePath{oldFile.filename};
        auto path = rootPath / filePath;
        auto verify = Installed::Verify(destdir, oldFile);
        switch (verify) {
            case FileMatch::OK:
                if (!is_directory(path)) {
                    std::cout << "Removing old file " << path << "\n";
                    std::string fullPathStr = path;
                    if (unlink(fullPathStr.c_str()) != 0) {
                        std::cerr << "Failed to remove file: " << fullPathStr << "\n";
                    }
                } else {
                    directories.push_back(path);
                }
                break;
            case FileMatch::NOT_MATCHING:
                std::cerr << "Modified: " << path << " (not deleted)\n";
                break;
            case FileMatch::NOT_FOUND:
                std::cerr << "Not found: " << path << "\n";
                break;
        }
    }
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
    installed.Unregister();
    Register(destdir);
}
