//
// Created by sigsegv on 3/9/23.
//

#ifndef BM_PORTS_H
#define BM_PORTS_H

#include <filesystem>
#include <vector>
#include <memory>

using path = std::filesystem::path;

class PortGroup;
class Port;

class Ports : public std::enable_shared_from_this<Ports> {
private:
    path root;
protected:
    Ports(const char *root);
public:
    static std::shared_ptr<Ports> Create(const char *root);
    [[nodiscard]] path GetRoot() const {
        return root;
    }
    [[nodiscard]] std::vector<std::shared_ptr<PortGroup>> GetGroups() const;
    [[nodiscard]] std::shared_ptr<PortGroup> GetGroup(const std::string &groupName) const;
    [[nodiscard]] std::shared_ptr<Port> GetPort(const std::string &portName) const;
};


#endif //BM_PORTS_H
