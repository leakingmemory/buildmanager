//
// Created by sigsegv on 8/2/23.
//

#include "Ports.h"
#include "PortGroup.h"
#include "Port.h"
#include "Build.h"
#include "Db.h"
#include "Installed.h"
#include <iostream>

int ListGroups(Ports &ports) {
    auto groups = ports.GetGroups();
    for (const auto &group : groups) {
        std::cout << group->GetName() << "\n";
    }
    return 0;
}

int ListPorts(Ports &ports, const std::string &groupName) {
    auto group = ports.GetGroup(groupName);
    auto portList = group->GetPorts();
    for (const auto &port : portList) {
        std::cout << port->GetName() << "\n";
    }
    return 0;
}

int ListBuilds(Ports &ports, const std::string &portName) {
    auto port = ports.GetPort(portName);
    if (!port) {
        std::cerr << "Error: Port was not found: " << portName << "\n";
        return 1;
    }
    auto builds = port->GetBuilds();
    for (const auto &build : builds) {
        std::cout << build.GetName() << " " << build.GetVersion() << "\n";
    }
    return 0;
}

int ListInstalled(const std::string &rootDir) {
    Db db{rootDir};
    auto groups = db.GetGroups();
    for (const auto &group : groups) {
        auto ports = group.GetPorts();
        for (const auto &port : ports) {
            auto versions = port.GetVersions();
            for (const auto &version : versions) {
                std::cout << version.GetGroup() << "/" << version.GetName() << "/" << version.GetVersion() << "\n";
            }
        }
    }
    return 0;
}
