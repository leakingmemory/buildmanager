//
// Created by sigsegv on 3/15/23.
//

#include "Distfile.h"
#include "Ports.h"
#include "CurlEasy.h"
#include "Fork.h"
#include "Exec.h"
#include <iostream>
#include <fstream>

using path = std::filesystem::path;

class DistfileException : public std::exception {
private:
    const char *error;
public:
    DistfileException(const char *error) : error(error) {}
    const char * what() const noexcept override {
        return error;
    }
};

void Distfile::Fetch(const Ports &ports) const {
    path distfiles = ports.GetRoot() / "distfiles";
    if (!exists(distfiles)) {
        if (!create_directory(distfiles)) {
            throw DistfileException("Could not create distfiles directory (<portsdir>/distfiles)");
        }
    } else if (!is_directory(distfiles)) {
        throw DistfileException("Distfiles is not a directory (<portsdir>/distfiles)");
    }
    path filename = distfiles / name;
    if (exists(filename)) {
        std::cout << "Distfile " << filename << " exists.\n";
        return;
    }
    std::cout << "Downloading to " << filename << "\n";
    for (const auto &src : source) {
        std::cout << "Downloading from " << src << "\n";
        std::string url{src};
        int count = 10;
        while (--count > 0) {
            std::fstream outputStream{};
            CurlEasy curl{};
            outputStream.open(filename, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
            if (!outputStream.is_open()) {
                throw new DistfileException("Failed to open distfile for writing");
            }
            curl.SetUrl(url);
            curl.SetWriteCallback([&outputStream](const void *buf, size_t size) {
                outputStream.write((const char *) buf, size);
                return size;
            });
            if (!curl.Perform()) {
                break;
            }
            auto code = curl.GetResponseCode();
            if (code == 200) {
                std::cout << "Downloaded successfully.\n";
                return;
            }
            auto redir = curl.PotentialRedirectUrl();
            if (redir) {
                url = *redir;
                std::cout << "Redirected to " << url << "\n";
                continue;
            }
            std::cerr << "Error code " << code << "\n";
            break;
        }
    }
    throw DistfileException("Distfile download failed");
}

void Distfile::Extract(const Ports &ports, const path &iBuildDir) const {
    path distfiles = ports.GetRoot() / "distfiles";
    path filename = distfiles / name;
    path buildDir = iBuildDir / "work";
    std::cout << "Extracting " << filename << "\n";
    if (!exists(filename)) {
        throw DistfileException("Distfile does not exist");
    }
    if (!exists(buildDir)) {
        if (!create_directory(buildDir)) {
            throw DistfileException("Unable to create work directory");
        }
    }
    if (name.ends_with(".tar.gz")) {
        Fork fork{[filename, buildDir] () {
            {
                std::string dir = buildDir.string();
                if (chdir(dir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << dir << "\n";
                    return 1;
                }
            }
            Exec exec{"tar"};
            std::vector<std::string> args{};
            args.push_back("xzf");
            args.push_back(filename);
            auto env = Exec::getenv();
            exec.exec(args, env);
            return 0;
        }};
        return;
    }
    if (name.ends_with(".tar.xz")) {
        Fork fork{[filename, buildDir] () {
            {
                std::string dir = buildDir.string();
                if (chdir(dir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << dir << "\n";
                    return 1;
                }
            }
            Exec exec{"tar"};
            std::vector<std::string> args{};
            args.push_back("xf");
            args.push_back(filename);
            auto env = Exec::getenv();
            exec.exec(args, env);
            return 0;
        }};
        return;
    }
    throw DistfileException("Unknown format to extract");
}
