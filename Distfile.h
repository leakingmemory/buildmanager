//
// Created by sigsegv on 3/15/23.
//

#ifndef BM_DISTFILE_H
#define BM_DISTFILE_H

#include <string>
#include <vector>
#include <filesystem>

class Ports;

class Distfile {
private:
    std::string name;
    std::vector<std::string> source;
public:
    Distfile(const std::string &name, const std::vector<std::string> &source) : name(name), source(source) {}
    [[nodiscard]] std::string GetName() const {
        return name;
    }
    [[nodiscard]] std::vector<std::string> GetSource() const {
        return source;
    }
    void Fetch(const Ports &ports) const;
    void Extract(const Ports &ports, const std::filesystem::path &buildDir) const;
};


#endif //BM_DISTFILE_H
