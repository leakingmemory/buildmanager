//
// Created by sigsegv on 3/12/23.
//

#include "Port.h"
#include "Build.h"
#include <sstream>

Port::Port(const std::shared_ptr<const PortGroup> &group, path groupRoot, std::string name) : group(group), root(groupRoot / name), name(name) {}

std::vector<Build> Port::GetBuilds() const {
    std::vector<Build> builds{};
    {
        std::filesystem::directory_iterator iterator{root};
        auto shared = shared_from_this();
        for (const auto &item : iterator) {
            std::string filename = item.path().filename();
            if (item.is_regular_file() && filename.ends_with(".build")) {
                builds.emplace_back(shared, item.path());
            }
        }
    }
    {
        auto iterator = builds.begin();
        while (iterator != builds.end()) {
            if (iterator->IsValid()) {
                ++iterator;
            } else {
                builds.erase(iterator);
            }
        }
    }
    return builds;
}

Build Port::GetBuild(const std::string &name) const {
    std::stringstream filename{};
    filename << this->name << "-" << name << ".build";
    Build build{shared_from_this(), root / filename.str()};
    return build;
}

bool Port::IsValid() const {
    return !GetBuilds().empty();
}
