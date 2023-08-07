//
// Created by sigsegv on 7/29/23.
//

#ifndef BM_DB_H
#define BM_DB_H

#include <string>
#include <vector>
#include <filesystem>

class DbGroup;
class DbPort;
class Installed;

class DbNotFound : public std::exception {
public:
    const char * what() const noexcept override {
        return "Pkg DB not found";
    }
};

class PkgNotFound : public std::exception {
private:
    std::string error;
    std::string name;
public:
    PkgNotFound(const std::string &name) : error("Pkg not found: "), name(name) {
        error.append(name);
    }
    const char * what() const noexcept override {
        return error.c_str();
    }
};

class PkgAmbiguous : public std::exception {
private:
    std::string error;
    std::string name;
public:
    PkgAmbiguous(const std::string &name) : error("Pkg ambiguous match: "), name(name) {
        error.append(name);
    }
    const char * what() const noexcept override {
        return error.c_str();
    }
};

class Db {
private:
    std::filesystem::path path;
    std::vector<std::string> groups;
public:
    Db(const std::filesystem::path &rootpath);
    std::vector<DbGroup> GetGroups() const ;
    [[nodiscard]] Installed Find(const std::string &port) const;
    [[nodiscard]] std::vector<std::string> GetSelectedList() const;
    void WriteSelectedList(const std::vector<std::string> &list) const;
};

class DbGroup {
private:
    std::filesystem::path dbpath;
    std::string group;
    std::vector<std::string> ports;
public:
    DbGroup(const std::filesystem::path &dbpath, const std::string &group);
    std::vector<DbPort> GetPorts() const;
    [[nodiscard]] Installed Find(const std::string &port) const;
};

class DbPort {
private:
    std::filesystem::path dbpath;
    std::string group;
    std::string port;
    std::vector<std::string> versions;
public:
    DbPort(const std::filesystem::path &dbpath, const std::string &group, const std::string &port);
    [[nodiscard]] std::vector<Installed> GetVersions() const;
    [[nodiscard]] Installed Find(const std::string &version) const;
    [[nodiscard]] Installed Find() const;
};


#endif //BM_DB_H
