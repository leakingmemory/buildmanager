//
// Created by sigsegv on 4/19/23.
//

#include "Unpack.h"
#include "PkgHdr.h"
#include "Exec.h"
#include <fstream>
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

Unpack::Unpack(std::string path, std::string destdir) : Fork([destdir] () {
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
}, ForkInputOutput::INPUT), fileList() {
    std::cout << "==> Unpacking " << path << " into " << destdir << "\n";
    std::fstream inputStream{};
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
    *this << inputStream;
    CloseInput();
    Require();
}
