//
// Created by sigsegv on 4/19/23.
//

#include "Unpack.h"
#include <iostream>
extern "C" {
#include <unistd.h>
}
#include "Exec.h"
#include <fstream>

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
    *this << inputStream;
    CloseInput();
    Require();
}
