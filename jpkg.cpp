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
#include "versioncmp.h"
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

std::vector<std::string> GenerateDepordering(Ports &ports, std::vector<std::string> add, std::vector<std::tuple<std::string,std::string>> replace) {
    constexpr std::vector<std::string>::size_type fuse = 100000;
    std::vector<std::string> buildOrder{};
    std::vector<std::string> world{};
    std::vector<std::tuple<std::string,std::string>> pendingDeps{};
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
                pendingDeps.emplace_back("", dep);
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
            pendingDeps.emplace_back("", dep);
        }
    }
    for (const auto &repl : replace) {
        if (pendingDeps.size() > fuse || buildOrder.size() > fuse) {
            throw JpkgException("Fuse blown");
        }
        auto found = FindInstalled(world, std::get<0>(repl));
        if (!found) {
            throw JpkgException("Requested replace of not installed");
        }
        pendingDeps.emplace_back(std::get<0>(repl), std::get<1>(repl));
    }
    while (pendingDeps.size() > 0) {
        auto dep = *(pendingDeps.begin());
        std::string depstr{};
        std::string group{};
        std::string port{};
        std::string version{};
        std::string pkgName{};
        bool found = Depstrs(world, std::get<1>(dep), depstr, group, port, version, pkgName);
        if (found && std::get<1>(dep) != std::get<0>(dep)) {
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
                std::sort(builds.begin(), builds.end(), [](Build &a, Build &b) {
                    auto cmp = versioncmp(b.GetVersion(), a.GetVersion());
                    return cmp < 0;
                });
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
                    pendingDeps.insert(pendingDeps.begin(), std::make_tuple("", dep));
                }
            }
        }
    }
    return buildOrder;
}

int Depordering(Ports &ports, std::vector<std::string> add) {
    auto depordering = GenerateDepordering(ports, add, {});
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
    auto depordering = GenerateDepordering(ports, installs, {});
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

std::string GetPkgNameWithoutVersion(const std::string &pkgname) {
    std::filesystem::path p{pkgname};
    auto iterator = p.begin();
    if (iterator == p.end()) {
        std::cerr << "Invalid pkg name: " << pkgname << "\n";
        throw JpkgException("Invalid pkg name");
    }
    std::string group = *iterator;
    ++iterator;
    if (iterator == p.end()) {
        std::cerr << "Invalid pkg name: " << pkgname << "\n";
        throw JpkgException("Invalid pkg name");
    }
    std::string name = *iterator;
    ++iterator;
    if (iterator == p.end()) {
        std::cerr << "Invalid pkg name: " << pkgname << "\n";
        throw JpkgException("Invalid pkg name");
    }
    ++iterator;
    if (iterator != p.end()) {
        std::cerr << "Invalid pkg name: " << pkgname << "\n";
        throw JpkgException("Invalid pkg name");
    }
    std::string res{group};
    res.append("/");
    res.append(name);
    return res;
}

int Uninstall(const std::vector<std::string> &uninstalls) {
    Db db{rootdir};
    auto worldmap = Worldmap(db);
    std::vector<std::string> unselect{};
    {
        std::vector<std::string> fullids{};
        for (const auto &uninst : uninstalls) {
            if (worldmap.find(uninst) != worldmap.end()) {
                fullids.push_back(uninst);
            } else {
                unselect.push_back(uninst);
            }
        }
        for (const auto &uninst : fullids) {
            unselect.push_back(GetPkgNameWithoutVersion(uninst));
        }
    }
    auto selected = db.GetSelectedList();
    for (const auto &unsel : unselect) {
        if (std::find(selected.begin(), selected.end(), unsel) == selected.end()) {
            std::cerr << "Not selected: " << unsel << " - can't uninstall\n";
            throw JpkgException("Requested uninstall is not selected");
        }
    }
    for (const auto &unsel : unselect) {
        std::cout << " ===> Deselecting " << unsel << "\n";
        auto iterator = std::find(selected.begin(), selected.end(), unsel);
        if (iterator == selected.end()) {
            std::cerr << "Not selected: " << unsel << " - can't uninstall (rolled back)\n";
            throw JpkgException("Requested uninstall is not selected");
        }
        selected.erase(iterator);
    }
    db.WriteSelectedList(selected);
    bool cont{true};
    do {
        std::vector<Installed> uninstalls{};
        auto leafs = Leafs(worldmap);
        for (const auto &leaf : leafs) {
            auto selname = GetPkgNameWithoutVersion(leaf);
            if (std::find(selected.begin(), selected.end(), selname) == selected.end()) {
                auto iterator = worldmap.find(leaf);
                if (iterator == worldmap.end()) {
                    std::cerr << "Pkg " << leaf << " not found in transaction world set\n";
                    throw JpkgException("Unexpectedly gone pkg");
                }
                uninstalls.push_back(iterator->second);
                worldmap.erase(iterator);
            }
        }
        for (const auto &installed : uninstalls) {
            std::cout << " ===> Uninstalling " << installed.GetGroup() << "/"
                      << installed.GetName() << "/" << installed.GetVersion() << "\n";
            installed.Uninstall(rootdir);
            installed.Unregister();
        }
        cont = !uninstalls.empty();
    } while (cont);
    return 0;
}

class UnsolvableException : public std::exception {
};

struct PotentialUpdateWithScore {
    int score{0};
    std::map<std::string,Build> resultingUpdates{};
    std::vector<std::string> resultingDeporder{};
};

std::vector<std::string> TrySolve(Ports &ports, const std::map<std::string,Installed> &origWorld, const std::map<std::string,Build> &acceptedUpdates) {
    std::vector<std::string> mainOrder{};
    {
        std::vector<std::vector<std::string>> depshells;
        std::map<std::string, Installed> world{origWorld};
        while (!world.empty()) {
            auto leafs = Leafs(world);
            if (!leafs.empty()) {
                for (const auto &leaf: leafs) {
                    auto iterator = world.find(leaf);
                    if (iterator == world.end()) {
                        throw JpkgException("world leaf not a leaf");
                    }
                    world.erase(iterator);
                }
                depshells.push_back(leafs);
            } else {
                std::vector<std::string> remaining{};
                for (const auto &rem: world) {
                    remaining.push_back(rem.first);
                }
                world.clear();
                depshells.push_back(remaining);
            }
        }
        std::reverse(depshells.begin(), depshells.end());
        for (const auto &shell: depshells) {
            for (const auto &pkg: shell) {
                if (acceptedUpdates.find(pkg) != acceptedUpdates.end()) {
                    mainOrder.push_back(pkg);
                }
            }
        }
    }
    std::vector<std::tuple<std::string,std::string>> replace{};
    for (const auto &pkg : mainOrder) {
        auto iterator = acceptedUpdates.find(pkg);
        if (iterator == acceptedUpdates.end()) {
            throw JpkgException("In main order list, but was not in the accepted list");
        }
        auto &build = iterator->second;
        std::string name{build.GetGroup()};
        name.append("/");
        name.append(build.GetName());
        name.append("/");
        name.append(build.GetVersion());
        replace.emplace_back(pkg, name);
    }
    return GenerateDepordering(ports, {}, replace);
}

int TryUpdate(Ports &ports, const std::map<std::string,Installed> &origWorld, const std::map<std::string,std::vector<Build>> &origUpdates, const std::map<std::string,Build> &acceptedUpdates, std::map<std::string,Build> &resultingUpdates, std::vector<std::string> &resultingDeporder) {
    std::vector<PotentialUpdateWithScore> potentialUpdatesWithScore{};
    {
        std::map<std::string, std::vector<Build>> updates{origUpdates};
        std::vector<Build> potentialUpdates{};
        std::string pkgName{};
        {
            auto iterator = updates.begin();
            if (iterator == updates.end()) {
                resultingDeporder = TrySolve(ports, origWorld, acceptedUpdates);
                resultingUpdates = acceptedUpdates;
                return 0;
            }
            pkgName = iterator->first;
            potentialUpdates = iterator->second;
            updates.erase(iterator);
        }
        for (const auto &potentialUpdate: potentialUpdates) {
            PotentialUpdateWithScore potentialUpdateWithScore{};
            std::map<std::string,Build> newAcceptedUpdates{acceptedUpdates};
            {
                newAcceptedUpdates.insert_or_assign(pkgName, potentialUpdate);
            }
            try {
                potentialUpdateWithScore.score = 1 + TryUpdate(ports, origWorld, updates, newAcceptedUpdates, potentialUpdateWithScore.resultingUpdates, potentialUpdateWithScore.resultingDeporder);
                potentialUpdateWithScore.resultingUpdates.insert_or_assign(pkgName, potentialUpdate);
                potentialUpdatesWithScore.push_back(potentialUpdateWithScore);
            } catch (UnsolvableException &e) {
                continue;
            }
        }
    }
    if (potentialUpdatesWithScore.empty()) {
        throw UnsolvableException();
    }
    std::sort(potentialUpdatesWithScore.begin(), potentialUpdatesWithScore.end(), [] (PotentialUpdateWithScore &a, PotentialUpdateWithScore &b) {
        return a.score > b.score;
    });
    auto result = *(potentialUpdatesWithScore.begin());
    resultingUpdates = result.resultingUpdates;
    resultingDeporder = result.resultingDeporder;
    return result.score;
}

class UpdateOperation {
protected:
    Build build;
public:
    UpdateOperation(const Build &build) : build(build) {}
    void Clean();
    void Fetch();
    virtual void DoUpdate() = 0;
};

class UpdateReplaceOperation : public UpdateOperation {
private:
    Installed old;
public:
    UpdateReplaceOperation(const Installed &old, const Build &newBuild) : UpdateOperation(newBuild), old(old) {}
    void DoUpdate() override;
};

class UpdateInstallOperation : public UpdateOperation {
private:
public:
    UpdateInstallOperation(const Build &build) : UpdateOperation(build) {}
    void DoUpdate() override;
};

void UpdateOperation::Clean() {
    build.Clean();
}

void UpdateOperation::Fetch() {
    build.Fetch();
}

void UpdateReplaceOperation::DoUpdate() {
    Tempdir tempdir{};
    std::string tempdirName = tempdir.GetName();
    Fork fork{[this, &tempdirName]() {
        if (chdir(tempdirName.c_str()) != 0) {
            throw JpkgException("Failed to changedir to tmpdir");
        }
        build.Package();
        build.Clean();
        std::string packageFilename{build.GetName()};
        packageFilename.append("-");
        packageFilename.append(build.GetVersion());
        packageFilename.append(".pkg");
        std::cout << " ==> Replacing " << old.GetGroup() << "/" << old.GetName() << "/"
            << old.GetVersion() << " with " << packageFilename << "\n";
        Unpack unpack{packageFilename};
        unpack.Replace(old, "/");
        if (unlink(packageFilename.c_str()) != 0) {
            throw JpkgException("Failed to delete tmp pkg");
        }
        return 0;
    }};
    fork.Require();
}

void UpdateInstallOperation::DoUpdate() {
    Tempdir tempdir{};
    std::string tempdirName = tempdir.GetName();
    Fork fork{[this, &tempdirName]() {
        if (chdir(tempdirName.c_str()) != 0) {
            throw JpkgException("Failed to changedir to tmpdir");
        }
        build.Package();
        build.Clean();
        std::string packageFilename{build.GetName()};
        packageFilename.append("-");
        packageFilename.append(build.GetVersion());
        packageFilename.append(".pkg");
        std::cout << " ==> Installing " << packageFilename << "\n";
        Unpack unpack{packageFilename, "/"};
        unpack.Register("/");
        if (unlink(packageFilename.c_str()) != 0) {
            throw JpkgException("Failed to delete tmp pkg");
        }
        return 0;
    }};
    fork.Require();
}

int Update(Ports &ports) {
    Db db{rootdir};
    auto worldmap = Worldmap(db);
    std::vector<std::shared_ptr<UpdateOperation>> updateOps{};
    {
        std::map<std::string, std::vector<Build>> updates{};
        for (const auto &pair: worldmap) {
            std::string groupName{};
            std::string portName{};
            std::string installedVersion{};
            {
                const auto &installed = pair.second;
                groupName = installed.GetGroup();
                portName = installed.GetName();
                installedVersion = installed.GetVersion();
            }
            std::shared_ptr<Port> port{};
            {
                auto group = ports.GetGroup(groupName);
                if (!group) {
                    std::cerr << "warning: corresponding port for " << pair.first << " does not exist\n";
                    continue;
                }
                port = group->GetPort(portName);
                if (!port) {
                    std::cerr << "warning: corresponding port for " << pair.first << " does not exist\n";
                    continue;
                }
            }
            auto versions = port->GetBuilds();
            if (versions.empty()) {
                std::cerr << "warning: corresponding builds for " << pair.first << " does not exist\n";
                continue;
            }
            std::sort(versions.begin(), versions.end(), [](Build &a, Build &b) {
                auto cmp = versioncmp(b.GetVersion(), a.GetVersion());
                return cmp < 0;
            });
            std::vector<Build> potentialUpdates{};
            for (const auto &build: versions) {
                auto cmp = versioncmp(build.GetVersion(), installedVersion);
                if (cmp <= 0) {
                    break;
                }
                std::cout << " ===> Potential update found: " << pair.first << " to " << build.GetVersion() << "\n";
                potentialUpdates.push_back(build);
            }
            if (!potentialUpdates.empty()) {
                updates.insert_or_assign(pair.first, potentialUpdates);
            }
        }
        if (!updates.empty()) {
            std::map<std::string, Build> resultingUpdates{};
            std::vector<std::string> deporder{};
            try {
                if (TryUpdate(ports, worldmap, updates, {}, resultingUpdates, deporder) == 0) {
                    std::cerr << "warning: updates found, but couldn't solve dependencies\n";
                }
                std::map<std::string, std::string> newToOldMap{};
                for (const auto &update: resultingUpdates) {
                    auto &build = update.second;
                    std::string newName{build.GetGroup()};
                    newName.append("/");
                    newName.append(build.GetName());
                    newName.append("/");
                    newName.append(build.GetVersion());
                    newToOldMap.insert_or_assign(newName, update.first);
                }
                for (const auto &dep: deporder) {
                    std::string oldName{};
                    {
                        auto iterator = newToOldMap.find(dep);
                        if (iterator != newToOldMap.end()) {
                            oldName = iterator->second;
                        }
                    }
                    if (!oldName.empty()) {
                        std::cout << " * Update " << oldName << " -> " << dep << "\n";
                        Installed old{};
                        {
                            auto iterator = worldmap.find(oldName);
                            if (iterator == worldmap.end()) {
                                throw JpkgException("Old pkg not found");
                            }
                            old = iterator->second;
                        }
                        Build build{};
                        {
                            auto iterator = resultingUpdates.find(oldName);
                            if (iterator == resultingUpdates.end()) {
                                throw JpkgException("Old build not in map");
                            }
                            build = iterator->second;
                        }
                        if (!build.IsValid()) {
                            throw JpkgException("Build not valid");
                        }
                        updateOps.push_back(std::make_shared<UpdateReplaceOperation>(old, build));
                    } else {
                        std::cout << " * Install " << dep << "\n";
                        auto build = GetBuild(ports, dep, {});
                        if (!build.IsValid()) {
                            throw JpkgException("Build not found");
                        }
                        updateOps.push_back(std::make_shared<UpdateInstallOperation>(build));
                    }
                }
            } catch (UnsolvableException &e) {
                std::cerr << "warning: updates found, but couldn't solve dependencies (unsolvable)\n";
            }
        }
    }
    for (const auto &update : updateOps) {
        update->Clean();
    }
    for (const auto &update : updateOps) {
        update->Fetch();
    }
    for (const auto &update : updateOps) {
        update->DoUpdate();
    }
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
    INSTALL,
    UNINSTALL,
    UPDATE
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
    cmdMap.insert_or_assign("uninstall", Command::UNINSTALL);
    cmdMap.insert_or_assign("update", Command::UPDATE);
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
    std::cerr << " " << cmd << " uninstall [<pkgs...>]\n";
    std::cerr << " " << cmd << " update\n";
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
        case Command::UNINSTALL: {
            std::vector<std::string> deselect{};
            {
                auto iterator = args.begin();
                while (iterator != args.end()) {
                    deselect.push_back(*iterator);
                    ++iterator;
                }
            }
            return Uninstall(deselect);
        }
        case Command::UPDATE: {
            if (args.begin() != args.end()) {
                return usage();
            }
            return Update(ports);
        }
    }
    return 0;
}

int main(int argc, const char *argv[]) {
    JpkgApp app{argc, argv};
    return app.main();
}
