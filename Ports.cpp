//
// Created by sigsegv on 3/9/23.
//

#include "Ports.h"
#include "PortGroup.h"

Ports::Ports(const char *root) : root(root) {
}

class PortsImpl : public Ports {
public:
    PortsImpl(const char *root) : Ports(root) {}
};

std::shared_ptr<Ports> Ports::Create(const char *root) {
    return std::make_shared<PortsImpl>(root);
}

std::vector<std::shared_ptr<PortGroup>> Ports::GetGroups() const {
    std::vector<std::shared_ptr<PortGroup>> groups{};
    {
        std::filesystem::directory_iterator iterator{root};
        for (const auto &item : iterator) {
            if (item.is_directory()) {
                auto group = std::make_shared<PortGroup>(shared_from_this(), root, item.path().filename());
                if (group->IsValid()) {
                    groups.emplace_back(group);
                }
            }
        }
    }
    return groups;
}

std::shared_ptr<PortGroup> Ports::GetGroup(const std::string &groupName) const {
    return std::make_shared<PortGroup>(shared_from_this(), root, groupName);
}

std::shared_ptr<Port> Ports::GetPort(const std::string &portName) const {
    std::string groupName{};
    std::string pname{};
    {
        path portPath{portName};
        auto iterator = portPath.begin();
        if (iterator == portPath.end()) {
            return {};
        }
        groupName = *iterator;
        ++iterator;
        if (iterator == portPath.end()) {
            return {};
        }
        pname = *iterator;
        ++iterator;
        if (iterator != portPath.end()) {
            return {};
        }
    }
    auto group = GetGroup(groupName);
    return group->GetPort(pname);
}
