//
// Created by sigsegv on 3/9/23.
//

#ifndef BM_PORTGROUP_H
#define BM_PORTGROUP_H

#include <filesystem>
#include <vector>
#include <functional>

using path = std::filesystem::path;

class Port;
class Ports;

class PortGroup : public std::enable_shared_from_this<PortGroup> {
private:
    std::shared_ptr<const Ports> ports;
    std::string name;
    path root;
public:
    PortGroup(const std::shared_ptr<const Ports> &ports, const path &root, const std::string &name);
    [[nodiscard]] std::shared_ptr<const Ports> GetPortsRoot() const {
        return ports;
    }
    [[nodiscard]] std::string GetName() const {
        return name;
    }
private:
    bool ForEachPorts(const std::function<bool (const std::shared_ptr<Port> &)> &func, bool initialReturnValue = false) const;
public:
    [[nodiscard]] std::vector<std::shared_ptr<Port>> GetPorts() const;
    [[nodiscard]] std::shared_ptr<Port> GetPort(const std::string &portName) const;
    [[nodiscard]] bool IsValid() const;
};


#endif //BM_PORTGROUP_H
