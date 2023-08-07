//
// Created by sigsegv on 8/2/23.
//

#include "App.h"
#include "basiccmd.h"
#include "Db.h"
#include "Installed.h"
#include "Ports.h"
#include "PortGroup.h"
#include "Port.h"
#include "Build.h"
#include "Fork.h"
#include "Unpack.h"
#include <iostream>
#include <map>
#include <optional>
extern "C" {
#include <unistd.h>
}

const char *rootdir = "/";

class JpkgException : public std::exception {
private:
    const char *error;
public:
    JpkgException(const char *error) : error(error) {}
    const char * what() const noexcept override {
        return error;
    }
};

bool Depstrs(const std::vector<std::string> &world, const std::string &dep, std::string &depstr, std::string &group, std::string &port, std::string &version, std::string &foundPkg) {
    auto depdet = std::filesystem::path(dep);
    auto depiterator = depdet.begin();
    if (depiterator == depdet.end()) {
        throw JpkgException("Invalid (empty) dependency");
    }
    group = *depiterator;
    ++depiterator;
    depstr = group;
    if (depiterator == depdet.end()) {
        throw JpkgException("Invalid (not group/port) dependency");
    }
    port = *depiterator;
    ++depiterator;
    depstr.append("/");
    depstr.append(port);
    depstr.append("/");
    if (depiterator != depdet.end()) {
        version = *depiterator;
        depstr.append(version);
    } else {
        version = "";
    }
    bool found{false};
    for (const auto &w : world) {
        if (version.empty()) {
            if (w.starts_with(depstr)) {
                foundPkg = w;
                found = true;
                break;
            }
        } else {
            if (w == depstr) {
                foundPkg = w;
                found = true;
                break;
            }
        }
    }
    return found;
}

std::map<std::string,Installed> Worldmap(const Db &db) {
    std::map<std::string,Installed> map{};
    auto groups = db.GetGroups();
    for (const auto &group : groups) {
        auto ports = group.GetPorts();
        for (const auto &port : ports) {
            auto versions = port.GetVersions();
            for (const auto &version : versions) {
                std::string name = {version.GetGroup()};
                name.append("/");
                name.append(version.GetName());
                name.append("/");
                name.append(version.GetVersion());
                map.insert_or_assign(name, version);
            }
        }
    }
    return map;
}

std::vector<std::string> World(const std::map<std::string,Installed> &worldmap) {
    std::vector<std::string> w{};
    for (const auto &pair : worldmap) {
        w.push_back(pair.first);
    }
    return w;
}

std::optional<std::string> FindInstalled(const std::vector<std::string> &world, const std::string &name) {
    std::string depstr{};
    std::string group{};
    std::string depname{};
    std::string version{};
    std::string pkgName{};
    bool found = Depstrs(world, name, depstr, group, depname, version, pkgName);
    if (found) {
        return pkgName;
    } else {
        return {};
    }
}

std::map<std::string,std::vector<std::string>> Dependencies(const std::map<std::string,Installed> &worldmap) {
    auto world = World(worldmap);
    std::map<std::string,std::vector<std::string>> dependencies{};
    for (const auto &pair : worldmap) {
        const auto &name = pair.first;
        const auto &pkg = pair.second;
        auto inserted = dependencies.insert_or_assign(name, std::vector<std::string>());
        if (!inserted.second) {
            throw JpkgException("Duplicate in installed list");
        }
        std::vector<std::string> &deplist = inserted.first->second;
        auto rdeps = pkg.GetRdep();
        for (const auto &rdep : rdeps) {
            std::string depstr{};
            {
                auto installed = FindInstalled(world, rdep);
                if (!installed) {
                    std::cerr << "Rdep not satisfied: " << name << " -> " << rdep << "\n";
                    throw JpkgException("Rdep not satisfied");
                }
                depstr = *installed;
            }
            deplist.push_back(depstr);
        }
    }
    return dependencies;
}

std::map<std::string,std::vector<std::string>> Revdeps(std::map<std::string,std::vector<std::string>> deps) {
    std::map<std::string,std::vector<std::string>> revdeps{};
    for (const auto &pair : deps) {
        const auto &revdep = pair.first;
        const auto &deps = pair.second;
        for (const auto &dep : deps) {
            auto iterator = revdeps.find(dep);
            if (iterator != revdeps.end()) {
                auto &list = iterator->second;
                if (std::find(list.begin(), list.end(), revdep) == list.end()) {
                    list.push_back(revdep);
                }
            } else {
                std::vector<std::string> list{};
                list.push_back(revdep);
                revdeps.insert_or_assign(dep, list);
            }
        }
    }
    return revdeps;
}

std::vector<std::string> Leafs(const std::map<std::string,Installed> &worldmap) {
    std::vector<std::string> leafs{};
    {
        auto revdeps = Revdeps(Dependencies(worldmap));
        for (const auto &wpair: worldmap) {
            const auto &name = wpair.first;
            if (revdeps.find(name) == revdeps.end()) {
                leafs.push_back(name);
            }
        }
    }
    return leafs;
}

enum class DeporderingStage {
    INITIAL,
    RDEPS,
    BDEPS
};

std::vector<std::string> GenerateDepordering(Ports &ports, std::vector<std::string> add) {
    constexpr std::vector<std::string>::size_type fuse = 100000;
    std::vector<std::string> buildOrder{};
    std::vector<std::string> world{};
    std::vector<std::string> pendingDeps{};
    std::map<std::string,DeporderingStage> trail{};
    try {
        std::vector<std::tuple<std::string,std::string>> rdeps{};
        Db db{rootdir};
        auto groups = db.GetGroups();
        for (const auto &group: groups) {
            auto installedPorts = group.GetPorts();
            for (const auto &port: installedPorts) {
                auto versions = port.GetVersions();
                for (const auto &version: versions) {
                    std::string w{version.GetGroup()};
                    w.append("/");
                    w.append(version.GetName());
                    w.append("/");
                    w.append(version.GetVersion());
                    world.push_back(w);
                    auto deps = version.GetRdep();
                    for (const auto &dep: deps) {
                        rdeps.emplace_back(w, dep);
                    }
                }
            }
        }
        for (const auto &deptuple : rdeps) {
            const auto &from = std::get<0>(deptuple);
            const auto &dep = std::get<1>(deptuple);
            auto found = FindInstalled(world, dep);
            if (!found) {
                pendingDeps.push_back(dep);
            }
        }
    } catch (const DbNotFound &dbNotFound) {
    }
    for (const auto &dep : add) {
        if (pendingDeps.size() > fuse || buildOrder.size() > fuse) {
            throw JpkgException("Fuse blown");
        }
        auto found = FindInstalled(world, dep);
        if (!found) {
            pendingDeps.push_back(dep);
        }
    }
    while (pendingDeps.size() > 0) {
        auto dep = *(pendingDeps.begin());
        std::string depstr{};
        std::string group{};
        std::string port{};
        std::string version{};
        std::string pkgName{};
        bool found = Depstrs(world, dep, depstr, group, port, version, pkgName);
        if (found) {
            pendingDeps.erase(pendingDeps.begin());
        } else {
            auto portGroup = ports.GetGroup(group);
            if (!portGroup->IsValid()) {
                std::cerr << depstr << " not found\n";
                throw JpkgException("Dependency not found");
            }
            auto portObj = portGroup->GetPort(port);
            if (!portObj) {
                std::cerr << depstr << " not found\n";
                throw JpkgException("Dependency not found");
            }
            Build build{};
            if (!version.empty()) {
                build = portObj->GetBuild(version, {});
            } else {
                auto builds = portObj->GetBuilds();
                auto iterator = builds.begin();
                if (iterator == builds.end()) {
                    std::cerr << depstr << " not found\n";
                    throw JpkgException("Dependency not found");
                }
                build = *iterator;
            }
            if (!build.IsValid()) {
                std::cerr << depstr << " not found\n";
                throw JpkgException("Dependency not found");
            }
            std::string buildpath{group};
            buildpath.append("/");
            buildpath.append(port);
            buildpath.append("/");
            buildpath.append(build.GetVersion());
            DeporderingStage stage{DeporderingStage::INITIAL};
            {
                auto iterator = trail.find(buildpath);
                if (iterator != trail.end()) {
                    stage = iterator->second;
                }
            }

            std::vector<std::string> notFounds{};
            auto rdeps = build.GetRdeps();
            for (const auto &dep : rdeps) {
                auto found = FindInstalled(world, dep);
                if (!found) {
                    notFounds.push_back(dep);
                }
            }
            if (!notFounds.empty()) {
                if (stage != DeporderingStage::INITIAL) {
                    std::cerr << "error: Circular dependency detected at " << buildpath << "\n";
                    throw JpkgException("Circular dependency");
                }
                stage = DeporderingStage::RDEPS;
            } else {
                auto bdeps = build.GetBdeps();
                for (const auto &dep : bdeps) {
                    auto found = FindInstalled(world, dep);
                    if (!found) {
                        notFounds.push_back(dep);
                    }
                }
                if (!notFounds.empty()) {
                    if (stage == DeporderingStage::BDEPS) {
                        std::cerr << "error: Circular dependency detected at " << buildpath << "\n";
                        throw JpkgException("Circular dependency");
                    }
                }
                stage = DeporderingStage::BDEPS;
            }
            trail.insert_or_assign(buildpath, stage);
            if (notFounds.empty()) {
                buildOrder.push_back(buildpath);
                world.push_back(buildpath);
                pendingDeps.erase(pendingDeps.begin());
            } else {
                for (const auto &dep : notFounds) {
                    pendingDeps.insert(pendingDeps.begin(), dep);
                }
            }
        }
    }
    return buildOrder;
}

int Depordering(Ports &ports, std::vector<std::string> add) {
    auto depordering = GenerateDepordering(ports, add);
    for (const auto &dep : depordering) {
        std::cout << dep << "\n";
    }
    return 0;
}

int Selected() {
    Db db{rootdir};
    auto selected = db.GetSelectedList();
    for (const auto &sel : selected) {
        std::cout << sel << "\n";
    }
    return 0;
}

int Revdeps(const std::string &port) {
    Db db{rootdir};
    auto worldmap = Worldmap(db);
    std::string name{};
    {
        auto world = World(worldmap);
        {
            std::string depstr{};
            std::string group{};
            std::string portname{};
            std::string version{};
            if (!Depstrs(world, port, depstr, group, portname, version, name)) {
                throw JpkgException("Pkg not found");
            }
        }
        if (worldmap.find(name) == worldmap.end()) {
            throw JpkgException("Inconsistent db/algo, pkg not found");
        }
    }
    auto revdeps = Revdeps(Dependencies(worldmap));
    auto iterator = revdeps.find(name);
    if (iterator != revdeps.end()) {
        for (const auto &revdep : iterator->second) {
            std::cout << revdep << "\n";
        }
    }
    return 0;
}

int Leafs() {
    Db db{rootdir};
    auto worldmap = Worldmap(db);
    auto leafs = Leafs(worldmap);
    for (const auto &leaf : leafs) {
        std::cout << leaf << "\n";
    }
    return 0;
}

class Tempdir {
private:
    std::string dirname{};
public:
    Tempdir() : dirname("/tmp/pkgbuildXXXXXX") {
        if (mkdtemp(dirname.data()) != dirname.c_str()) {
            dirname = "";
            throw JpkgException("Failed to create tempdir");
        }
    }
    ~Tempdir() {
        if (!dirname.empty()) {
            if (rmdir(dirname.c_str()) != 0) {
                std::cerr << "Failed to remove tmp directory: " << dirname << "\n";
            }
        }
    }
    [[nodiscard]] std::string GetName() const {
        return dirname;
    }
};

int Install(Ports &ports, const std::vector<std::string> &installs) {
    std::vector<Build> requestedBuilds{};
    for (const auto &install : installs) {
        std::filesystem::path p{install};
        auto iterator = p.begin();
        if (iterator == p.end()) {
            std::cerr << "Not found: " << install << "\n";
            throw JpkgException("Port not found");
        }
        auto group = ports.GetGroup(*iterator);
        ++iterator;
        if (iterator == p.end() || !group->IsValid()) {
            std::cerr << "Not found: " << install << "\n";
            throw JpkgException("Port not found");
        }
        auto port = group->GetPort(*iterator);
        ++iterator;
        if (!port->IsValid()) {
            std::cerr << "Not found: " << install << "\n";
            throw JpkgException("Port not found");
        }
        Build build{};
        if (iterator != p.end()) {
            build = port->GetBuild(*iterator, {});
            ++iterator;
            if (iterator != p.end() || !build.IsValid()) {
                std::cerr << "Not found: " << install << "\n";
                throw JpkgException("Port not found");
            }
        } else {
            auto builds = port->GetBuilds();
            build = *(builds.begin());
            if (!build.IsValid()) {
                std::cerr << "Not found: " << install << "\n";
                throw JpkgException("Port not found");
            }
        }
        requestedBuilds.push_back(build);
    }
    auto depordering = GenerateDepordering(ports, installs);
    std::vector<Build> builds{};
    for (const auto &dep : depordering) {
        auto build = GetBuild(ports, dep, {});
        if (!build.IsValid()) {
            throw JpkgException("Port not found");
        }
        builds.push_back(build);
    }
    for (auto &build : builds) {
        build.Fetch();
    }
    for (auto &build : builds) {
        build.Clean();
    }
    {
        Tempdir tempdir{};
        std::string tempdirName = tempdir.GetName();
        Fork fork{[&builds, &tempdirName]() {
            if (chdir(tempdirName.c_str()) != 0) {
                throw JpkgException("Failed to changedir to tmpdir");
            }
            for (auto &build: builds) {
                build.Package();
                build.Clean();
                std::string packageFilename{build.GetName()};
                packageFilename.append("-");
                packageFilename.append(build.GetVersion());
                packageFilename.append(".pkg");
                Unpack unpack{packageFilename, "/"};
                unpack.Register("/");
                if (unlink(packageFilename.c_str()) != 0) {
                    throw JpkgException("Failed to delete tmp pkg");
                }
            }
            return 0;
        }};
        fork.Require();
    }
    Db db{"/"};
    auto selected = db.GetSelectedList();
    for (const auto &build : requestedBuilds) {
        bool found{false};
        std::string sel{build.GetGroup()};
        sel.append("/");
        sel.append(build.GetName());
        for (const auto &existing : selected) {
            if (existing == sel) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << " ==> Selecting " << sel << "\n";
            selected.push_back(sel);
        } else {
            std::cout << " ==> Already selected " << sel << "\n";
        }
    }
    db.WriteSelectedList(selected);
    return 0;
}

enum class Command {
    NONE,
    LIST_GROUPS,
    LIST_PORTS,
    LIST_BUILDS,
    LIST_INSTALLED,
    DEPORDERING,
    SELECTED,
    REVDEPS,
    LEAFS,
    INSTALL
};

static std::map<std::string,Command> GetInitialCmdMap() {
    std::map<std::string,Command> cmdMap{};
    cmdMap.insert_or_assign("list-groups", Command::LIST_GROUPS);
    cmdMap.insert_or_assign("list-ports", Command::LIST_PORTS);
    cmdMap.insert_or_assign("list-builds", Command::LIST_BUILDS);
    cmdMap.insert_or_assign("list-installed", Command::LIST_INSTALLED);
    cmdMap.insert_or_assign("depordering", Command::DEPORDERING);
    cmdMap.insert_or_assign("selected", Command::SELECTED);
    cmdMap.insert_or_assign("revdeps", Command::REVDEPS);
    cmdMap.insert_or_assign("leafs", Command::LEAFS);
    cmdMap.insert_or_assign("install", Command::INSTALL);
    return cmdMap;
}

static std::map<std::string,Command> cmdMap = GetInitialCmdMap();

Command GetCmd(std::vector<std::string> &args)
{
    auto cmd = args.begin();
    if (cmd == args.end()) {
        return Command::NONE;
    }
    auto iter = cmdMap.find(*cmd);
    if (iter != cmdMap.end()) {
        args.erase(cmd);
        return iter->second;
    }
    return Command::NONE;
}

class JpkgApp : public App {
private:
    Command command;
public:
    JpkgApp(int argc, const char * const argv[]) : App(argc, argv) {}
    int usage() override;
    void ConsumeCmd(std::vector<std::string> &args) override;
    bool IsValidCmd() override;
    int RunCmd(Ports &ports, std::vector<std::string> &args) override;
};

int JpkgApp::usage() {
    std::cerr << "Usage:\n";
    std::cerr << " " << cmd << " list-groups\n " << cmd << " list-ports <group>\n";
    std::cerr << " " << cmd << " list-builds <group/port>\n " << cmd << " list-installed\n";
    std::cerr << " " << cmd << " depordering [<ports...>]\n";
    std::cerr << " " << cmd << " selected\n";
    std::cerr << " " << cmd << " revdeps <group/port>\n";
    std::cerr << " " << cmd << " leafs\n";
    std::cerr << " " << cmd << " install [<ports...>]\n";
    return 1;
}

void JpkgApp::ConsumeCmd(std::vector<std::string> &args) {
    command = GetCmd(args);
}

bool JpkgApp::IsValidCmd() {
    return command != Command::NONE;
}

int JpkgApp::RunCmd(Ports &ports, std::vector<std::string> &args) {
    switch (command) {
        case Command::NONE:
            break;
        case Command::LIST_GROUPS:
            if (args.begin() != args.end()) {
                return usage();
            }
            return ListGroups(ports);
        case Command::LIST_PORTS: {
            std::string groupName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage();
                }
                groupName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage();
                }
            }
            return ListPorts(ports, groupName);
        }
        case Command::LIST_BUILDS: {
            std::string portName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage();
                }
                portName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage();
                }
            }
            return ListBuilds(ports, portName);
        }
        case Command::LIST_INSTALLED: {
            if (args.begin() != args.end()) {
                return usage();
            }
            return ListInstalled("/");
        }
        case Command::DEPORDERING: {
            std::vector<std::string> add{};
            {
                auto iterator = args.begin();
                while (iterator != args.end()) {
                    add.push_back(*iterator);
                    ++iterator;
                }
            }
            return Depordering(ports, add);
        }
        case Command::SELECTED: {
            if (args.begin() != args.end()) {
                return usage();
            }
            return Selected();
        }
        case Command::REVDEPS: {
            auto iterator = args.begin();
            if (iterator == args.end()) {
                return usage();
            }
            std::string port = *iterator;
            ++iterator;
            if (iterator != args.end()) {
                return usage();
            }
            return Revdeps(port);
        }
        case Command::LEAFS: {
            if (args.begin() != args.end()) {
                return usage();
            }
            return Leafs();
        }
        case Command::INSTALL: {
            std::vector<std::string> add{};
            {
                auto iterator = args.begin();
                while (iterator != args.end()) {
                    add.push_back(*iterator);
                    ++iterator;
                }
            }
            return Install(ports, add);
        }
    }
    return 0;
}

int main(int argc, const char *argv[]) {
    JpkgApp app{argc, argv};
    return app.main();
}
