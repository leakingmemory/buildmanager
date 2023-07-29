//
// Created by sigsegv on 7/29/23.
//

#include "Db.h"
#include "Installed.h"

class DbException : public std::exception {
    const char *error;
public:
    DbException(const char *error) : error(error) {}
    const char * what() const noexcept override {
        return error;
    }
};

Db::Db(const std::filesystem::path &rootpath) {
    path = rootpath / "var" / "lib" / "jpkg";
    if (!exists(path) || !is_directory(path)) {
        throw DbException("Pkg DB not found");
    }
    std::filesystem::directory_iterator iterator{path};
    for (const auto &item : iterator) {
        if (is_directory(item)) {
            auto filename = item.path().filename();
            if (filename != "." && filename != "..") {
                groups.push_back(filename);
            }
        }
    }
}

DbGroup::DbGroup(const std::filesystem::path &dbpath, const std::string &group) : dbpath(dbpath), group(group) {
    auto path = dbpath / group;
    if (!exists(path) || !is_directory(path)) {
        throw DbException("Pkg DB group not found");
    }
    std::filesystem::directory_iterator iterator{path};
    for (const auto &item : iterator) {
        if (is_directory(item)) {
            auto filename = item.path().filename();
            if (filename != "." && filename != "..") {
                ports.push_back(filename);
            }
        }
    }
}

DbPort::DbPort(const std::filesystem::path &dbpath, const std::string &group, const std::string &port) : dbpath(dbpath), group(group), port(port) {
    auto path = dbpath / group / port;
    if (!exists(path) || !is_directory(path)) {
        throw DbException("Pkg DB port not found");
    }
    std::filesystem::directory_iterator iterator{path};
    for (const auto &item : iterator) {
        if (is_directory(item)) {
            auto filename = item.path().filename();
            if (filename != "." && filename != "..") {
                versions.push_back(filename);
            }
        }
    }
}

std::vector<DbGroup> Db::GetGroups() {
    std::vector<DbGroup> groups{};
    for (const auto &group : this->groups) {
        groups.emplace_back(path, group);
    }
    return groups;
}

std::vector<DbPort> DbGroup::GetPorts() {
    std::vector<DbPort> ports{};
    for (const auto &port : this->ports) {
        ports.emplace_back(dbpath, group, port);
    }
    return ports;
}

std::vector<Installed> DbPort::GetVersions() const {
    std::vector<Installed> versions{};
    for (const auto &version : this->versions) {
        {
            auto path = dbpath / group / port / version;
            if (!exists(path / "info.json") || !exists(path / "files")) {
                continue;
            }
        }
        versions.emplace_back(dbpath, group, port, version);
    }
    return versions;
}

Installed Db::Find(const std::string &port) const {
    std::vector<std::string> path{};
    {
        std::filesystem::path pathit{port};
        for (const auto &item : pathit) {
            path.push_back(item);
        }
    }
    if (path.size() == 0 || path.size() > 3) {
        throw PkgNotFound(port);
    }
    auto iterator = path.begin();
    auto lookFor = *iterator;
    ++iterator;
    std::string remaining{};
    if (iterator != path.end()) {
        remaining.append(*iterator);
        ++iterator;
        while (iterator != path.end()) {
            remaining.append("/", 1);
            remaining.append(*iterator);
            ++iterator;
        }
    }
    for (const auto &group : groups) {
        if (lookFor == group) {
            if (remaining.empty()) {
                throw PkgNotFound(port);
            }
            DbGroup groupObj{this->path, group};
            try {
                return groupObj.Find(remaining);
            } catch (const PkgAmbiguous &amb) {
                throw PkgAmbiguous(port);
            } catch (const PkgNotFound &notFound) {
                throw PkgNotFound(port);
            }
        }
    }
    if (path.size() > 2) {
        throw PkgNotFound(port);
    }
    std::unique_ptr<Installed> installed{};
    for (const auto &group: groups) {
        DbGroup groupObj{this->path, group};
        try {
            auto pkg = groupObj.Find(port);
            if (installed) {
                throw PkgAmbiguous(port);
            }
            installed = std::make_unique<Installed>(pkg);
        } catch (const PkgAmbiguous &amb) {
            throw PkgAmbiguous(port);
        } catch (const PkgNotFound &notFound) {
        }
    }
    if (!installed) {
        throw PkgNotFound(port);
    }
    return *installed;
}

Installed DbGroup::Find(const std::string &port) const {
    std::vector<std::string> path{};
    {
        std::filesystem::path pathit{port};
        for (const auto &item : pathit) {
            path.push_back(item);
        }
    }
    if (path.size() == 0 || path.size() > 2) {
        throw PkgNotFound(port);
    }
    auto portName = path.at(0);
    auto fspath = dbpath / group / portName;
    if (!exists(fspath) || !is_directory(fspath)) {
        throw PkgNotFound(port);
    }
    DbPort portObj{dbpath, group, portName};
    try {
        if (path.size() > 1) {
            return portObj.Find(path.at(1));
        } else {
            return portObj.Find();
        }
    } catch (const PkgAmbiguous &amb) {
        throw PkgAmbiguous(port);
    } catch (const PkgNotFound &notFound) {
        throw PkgNotFound(port);
    }
}

Installed DbPort::Find(const std::string &version) const {
    auto path = dbpath / group / port / version;
    if (!exists(path / "info.json") || !exists(path / "files")) {
        throw PkgNotFound(version);
    }
    Installed retv{dbpath, group, port, version};
    return retv;
}

Installed DbPort::Find() const {
    if (versions.size() == 0) {
        throw PkgNotFound("");
    }
    if (versions.size() > 1) {
        auto versions = GetVersions();
        if (versions.size() > 1) {
            throw PkgAmbiguous("");
        }
        return versions.at(0);
    }
    auto version = versions.at(0);
    auto path = dbpath / group / port / version;
    if (!exists(path / "info.json") || !exists(path / "files")) {
        throw PkgNotFound(version);
    }
    Installed retv{dbpath, group, port, version};
    return retv;
}

