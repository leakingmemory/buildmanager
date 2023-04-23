//
// Created by sigsegv on 3/9/23.
//

#include "PortGroup.h"
#include "Port.h"

PortGroup::PortGroup(const std::shared_ptr<const Ports> &ports, const path &root, const std::string &name) : ports(ports), name(name), root(root / name) {}

bool PortGroup::ForEachPorts(const std::function<bool(const std::shared_ptr<Port> &)> &func, bool initialReturnValue) const {
    std::shared_ptr<const PortGroup> sharedThis = shared_from_this();
    auto returnValue = initialReturnValue;
    std::filesystem::directory_iterator iterator{root};
    for (const auto &item : iterator) {
        if (item.is_directory()) {
            auto port = std::make_shared<Port>(sharedThis, root, item.path().filename());
            returnValue = func(port);
            if (!returnValue) {
                return false;
            }
        }
    }
    return returnValue;
}

std::vector<std::shared_ptr<Port>> PortGroup::GetPorts() const {
    std::vector<std::shared_ptr<Port>>ports{};
    ForEachPorts([&ports] (const auto &port) {
        if (port->IsValid()) {
            ports.emplace_back(port);
        }
        return true;
    });
    return ports;
}

std::shared_ptr<Port> PortGroup::GetPort(const std::string &portName) const {
    auto port = std::make_shared<Port>(shared_from_this(), root, portName);
    if (port->IsValid()) {
        return port;
    } else {
        return {};
    }
}

bool PortGroup::IsValid() const {
    return !ForEachPorts([] (auto port) { return !port->IsValid(); }, true);
}
