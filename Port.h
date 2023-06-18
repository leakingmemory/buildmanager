//
// Created by sigsegv on 3/12/23.
//

#ifndef BM_PORT_H
#define BM_PORT_H

#include <string>
#include <vector>
#include <memory>
#include <filesystem>

using path = std::filesystem::path;

class PortGroup;
class Build;

class Port : public std::enable_shared_from_this<Port> {
private:
    std::shared_ptr<const PortGroup> group;
    std::string name;
    path root;
public:
    Port(const std::shared_ptr<const PortGroup> &group, path groupRoot, std::string name);
    [[nodiscard]] std::vector<Build> GetBuilds() const;
    [[nodiscard]] Build GetBuild(const std::string &name, const std::vector<std::string> &flags) const;
    [[nodiscard]] std::string GetName() const {
        return name;
    }
    [[nodiscard]] std::shared_ptr<const PortGroup> GetGroup() const {
        return group;
    }
    [[nodiscard]] path GetRoot() const {
        return root;
    }
    [[nodiscard]] bool IsValid() const;
};


#endif //BM_PORT_H
