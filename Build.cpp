//
// Created by sigsegv on 3/13/23.
//

#include "Build.h"
#include "Port.h"
#include "PortGroup.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include "Fork.h"
#include "Shell.h"
#include "Buildenv.h"
#include "Ports.h"
#include "Unpack.h"

Build GetBuild(Ports &ports, const std::string &buildName) {
    std::string groupName{};
    std::string portName{};
    std::string version{};
    {
        path buildpath{buildName};
        auto iterator = buildpath.begin();
        if (iterator == buildpath.end()) {
            std::cerr << "Invalid build name: " << buildName << "\n";
            return {};
        }
        groupName = *iterator;
        ++iterator;
        if (iterator == buildpath.end()) {
            std::cerr << "Invalid build name: " << buildName << "\n";
            return {};
        }
        portName = *iterator;
        ++iterator;
        if (iterator == buildpath.end()) {
            std::cerr << "Invalid build name: " << buildName << "\n";
            return {};
        }
        version = *iterator;
        ++iterator;
        if (iterator != buildpath.end()) {
            std::cerr << "Invalid build name: " << buildName << "\n";
            return {};
        }
    }
    auto group = ports.GetGroup(groupName);
    if (!group || !group->IsValid()) {
        std::cerr << buildName << ": Group " << groupName << " not found.\n";
        return {};
    }
    auto port = group->GetPort(portName);
    if (!port || !port->IsValid()) {
        std::cerr << buildName << ": Port " << portName << " not found.\n";
        return {};
    }
    auto build = port->GetBuild(version);
    if (!build.IsValid()) {
        std::cerr << buildName << ": Build " << version << " not fount.\n";
        return {};
    }
    return build;
}

class BuildException : public std::exception {
private:
    const char *error;
public:
    BuildException(const char *error) : error(error) {}
    const char * what() const noexcept override {
        return error;
    }
};

Build::Build(const std::shared_ptr<const Port> &port, path buildfile) : port(port), buildfile(buildfile), version(), distfiles(), prefix("/usr"), tooling("configure"), libc(), bootstrap(), staticBootstrap(), cxxflags(), buildTargets(), installTargets(), configureParams(), patches(), configureDefaultParameters(true), valid(false) {
    std::string filename{buildfile.filename()};
    std::string portName{port->GetName()};
    const std::string end{".build"};
    if (filename.starts_with(portName + "-") && filename.ends_with(end)) {
        std::string substr{filename.c_str() + portName.size() + 1, filename.size() - portName.size() - end.size() - 1};
        version = substr;
    }
    std::ifstream buildfileInput{buildfile};
    nlohmann::json jsonData{};
    try {
        jsonData = nlohmann::json::parse(buildfileInput);
        {
            auto iterator = jsonData.find("distfiles");
            if (iterator != jsonData.end()) {
                auto distfilesObject = iterator.value();
                auto iterator = distfilesObject.begin();
                while (iterator != distfilesObject.end()) {
                    std::string filename = iterator.key();
                    std::vector<std::string> source{};
                    auto filenameObject = iterator.value();
                    {
                        auto iterator = filenameObject.find("src");
                        if (iterator != filenameObject.end()) {
                            auto sources = iterator.value();
                            if (sources.is_array()) {
                                for (auto &item : sources) {
                                    if (item.is_string()) {
                                        std::string s = item;
                                        source.push_back(s);
                                    }
                                }
                            }
                        }
                    }
                    distfiles.emplace_back(filename, source);
                    ++iterator;
                }
            }
        }
        {
            auto iterator = jsonData.find("builddir");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    builddir = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("prefix");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    prefix = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("tooling");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    tooling = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("libc");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    libc = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("bootstrap");
            if (iterator != jsonData.end()) {
                auto &bootstrapItem = *iterator;
                if (bootstrapItem.is_array()) {
                    auto iterator = bootstrapItem.begin();
                    while (iterator != bootstrapItem.end()) {
                        auto bootstrapElement = *iterator;
                        if (bootstrapElement.is_string()) {
                            std::string bootstrap = bootstrapElement;
                            this->bootstrap.emplace_back(bootstrap);
                        }
                        ++iterator;
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("cxxflags");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    cxxflags = item;
                    ReplaceVars(cxxflags);
                }
            }
        }
        {
            auto iterator = jsonData.find("buildTargets");
            if (iterator != jsonData.end()) {
                auto &buildTargets = *iterator;
                if (buildTargets.is_array()) {
                    auto iterator = buildTargets.begin();
                    while (iterator != buildTargets.end()) {
                        auto &buildTarget = *iterator;
                        if (buildTarget.is_string()) {
                            std::string target = buildTarget;
                            this->buildTargets.emplace_back(target);
                        }
                        ++iterator;
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("installTargets");
            if (iterator != jsonData.end()) {
                auto &installTargets = *iterator;
                if (installTargets.is_array()) {
                    auto iterator = installTargets.begin();
                    while (iterator != installTargets.end()) {
                        auto &installTarget = *iterator;
                        if (installTarget.is_string()) {
                            std::string target = installTarget;
                            this->installTargets.emplace_back(target);
                        }
                        ++iterator;
                    }
                }
            } else {
                installTargets.emplace_back("install");
            }
        }
        {
            auto iterator = jsonData.find("configure");
            if (iterator != jsonData.end()) {
                auto &configure = *iterator;
                {
                    auto iterator = configure.find("parameters");
                    if (iterator != configure.end()) {
                        auto &parameters = *iterator;
                        if (parameters.is_array()) {
                            auto iterator = parameters.begin();
                            while (iterator != parameters.end()) {
                                auto &value = *iterator;
                                if (value.is_string()) {
                                    std::string str = value;
                                    configureParams.emplace_back(str);
                                }
                                ++iterator;
                            }
                        }
                    }
                }
                {
                    auto iterator = configure.find("defaultParameters");
                    if (iterator != configure.end()) {
                        auto &defparams = *iterator;
                        if (defparams.is_boolean()) {
                            bool defparamsValue = defparams;
                            configureDefaultParameters = defparamsValue;
                        }
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("static");
            if (iterator != jsonData.end()) {
                auto &staticItem = *iterator;
                if (staticItem.is_object()) {
                    auto iterator = staticItem.find("configure");
                    if (iterator != staticItem.end()) {
                        auto &configure = *iterator;
                        {
                            auto iterator = configure.find("parameters");
                            if (iterator != configure.end()) {
                                auto &parameters = *iterator;
                                if (parameters.is_array()) {
                                    auto iterator = parameters.begin();
                                    while (iterator != parameters.end()) {
                                        auto &value = *iterator;
                                        if (value.is_string()) {
                                            std::string str = value;
                                            staticConfigureParams.emplace_back(str);
                                        }
                                        ++iterator;
                                    }
                                }
                            }
                        }
                    }
                }
                if (staticItem.is_array()) {
                    auto iterator = staticItem.begin();
                    while (iterator != staticItem.end()) {
                        auto &staticBuild = *iterator;
                        if (staticBuild.is_string()) {
                            std::string str = staticBuild;
                            staticBootstrap.emplace_back(str);
                        }
                        ++iterator;
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("patches");
            if (iterator != jsonData.end()) {
                auto &patches = *iterator;
                if (patches.is_array()) {
                    auto iterator = patches.begin();
                    while (iterator != patches.end()) {
                        auto &patch = *iterator;
                        if (patch.is_string()) {
                            std::string patchFile = patch;
                            this->patches.emplace_back(patchFile);
                        }
                        ++iterator;
                    }
                }
            }
        }
    } catch (std::exception &e) {
        std::cerr << buildfile << ": json parse: " << e.what() << "\n";
        return;
    }
    valid = true;
}

void Build::ReplaceVars(std::string &str) const {
    std::string workdirStr;
    {
        path workdir = port->GetRoot() / "work";
        workdirStr = workdir;
    }
    std::string::size_type pos = 0;
    std::string key = "{WORKDIR}";
    std::string value = workdirStr;
    while (true) {
        pos = str.find(key, pos);
        if (pos == std::string::npos) {
            break;
        }
        str.erase(pos, key.length());
        str.insert(pos, value);
        pos += value.length();
    }
}

std::string Build::GetName() const {
    return port->GetName();
}

void Build::Fetch() {
    for (const auto &distfile : distfiles) {
        std::cout << "Fetching " << distfile.GetName() << "\n";
        distfile.Fetch(*(port->GetGroup()->GetPortsRoot()));
    }
}

void Build::Extract() {
    Fetch();
    path workdir = port->GetRoot() / "work";
    if (exists(workdir)) {
        return;
    }
    std::cout << "==> Extracting " << GetName() << "-" << GetVersion() << "\n";
    for (const auto &distfile : distfiles) {
        distfile.Extract(*(port->GetGroup()->GetPortsRoot()), port->GetRoot());
    }
    path builddir = workdir / this->builddir;
    if (!patches.empty() && exists(builddir)) {
        std::cout << "==> Applying patches to " << GetName() << "-" << GetVersion() << "\n";
        for (const auto &patch : patches) {
            std::cout << "*** " << patch << "\n";
            std::fstream inputStream{};
            inputStream.open(this->port->GetRoot() / "patches" / patch, std::ios_base::in | std::ios_base::binary);
            if (!inputStream.is_open()) {
                throw BuildException("Failed to open patch file");
            }
            Fork patcher{[&builddir] () {
                if (chdir(builddir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                Exec exec{"patch"};
                std::vector<std::string> args{};
                args.emplace_back("-p1");
                exec.exec(args, Exec::getenv());
                return 0;
            }, ForkInputOutput::INPUT};
            patcher << inputStream;
            patcher.CloseInput();
            patcher.Require();
        }
    }
}

enum class Tooling {
    CONFIGURE,
    CMAKE,
    BOOTSTRAP,
    STATIC_FILES
};

Tooling Build::GetTooling() const {
    if (this->tooling == "configure") {
        return Tooling::CONFIGURE;
    } else if (this->tooling == "cmake") {
        return Tooling::CMAKE;
    } else if (this->tooling == "bootstrap") {
        return Tooling::BOOTSTRAP;
    } else if (this->tooling == "static-files") {
        return Tooling::STATIC_FILES;
    }
    throw BuildException("Tooling not found");
}

void Build::Clean() {
    path workdir = port->GetRoot() / "work";
    if (exists(workdir)) {
        std::cout << "==> Cleaning " << GetName() << "-" << GetVersion() << "\n";
        Fork fork{[this] () {
            if (chdir(port->GetRoot().c_str()) != 0) {
                std::cerr << "chdir: build dir: " << port->GetRoot() << "\n";
                return 1;
            }
            Exec exec{"rm"};
            std::vector<std::string> args{};
            args.emplace_back("-rf");
            args.emplace_back("work");
            auto env = Exec::getenv();
            exec.exec(args, env);
            return 0;
        }};
    }
}

void Build::Configure(const std::vector<std::string> &flags) {
    path workdir = port->GetRoot() / "work";
    if (exists(workdir / "configured")) {
        return;
    }
    Extract();
    bool staticBuild = (std::find(flags.begin(), flags.end(), "static") != flags.end());
    if (!exists(workdir)) {
        if (!create_directory(workdir)) {
            throw BuildException("Create workdir");
        }
    }
    path builddir = workdir / this->builddir;
    if (!exists(builddir)) {
        if (!create_directory(builddir)) {
            throw BuildException("Create builddir");
        }
    }
    path cmakeDir;
    {
        std::string cm{this->builddir};
        cm.append("-cmake");
        cmakeDir = port->GetRoot() / "work" / cm;
    }
    std::cout << "==> Configuring " << GetName() << "-" << GetVersion() << "\n";
    auto tooling = GetTooling();
    if (tooling == Tooling::CMAKE) {
        if (!exists(cmakeDir)) {
            if (!create_directory(cmakeDir)) {
                throw BuildException("Unable to create cmake directory");
            }
        }
        if (!is_directory(cmakeDir)) {
            throw BuildException("Cmake directory: Is not a directory");
        }
    }
    if (tooling == Tooling::BOOTSTRAP) {
        return;
    }
    if (tooling == Tooling::STATIC_FILES) {
        return;
    }
    if (exists(builddir) && is_directory(builddir)) {
        Fork f{[this, builddir, cmakeDir, tooling, staticBuild] () {
            if (tooling == Tooling::CONFIGURE) {
                if (chdir(builddir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                Shell sh{};
                std::vector<std::string> conf{};
                conf.emplace_back("./configure");
                if (configureDefaultParameters) {
                    std::string prefix{"--prefix="};
                    prefix.append(this->prefix);
                    conf.emplace_back(prefix);
                }
                for (const std::string &param: configureParams) {
                    conf.emplace_back(param);
                }
                if (staticBuild) {
                    for (const std::string &param: staticConfigureParams) {
                        conf.emplace_back(param);
                    }
                }
                auto env = Exec::getenv();
                Buildenv buildenv{cxxflags};
                buildenv.FilterEnv(env);
                sh.exec(conf, env);
            } else if (tooling == Tooling::CMAKE) {
                if (chdir(cmakeDir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                Exec exec{"cmake"};
                std::vector<std::string> cmk{};
                if (configureDefaultParameters) {
                    {
                        std::string prefix{"-DCMAKE_INSTALL_PREFIX="};
                        prefix.append(this->prefix);
                        cmk.emplace_back(prefix);
                    }
                    cmk.emplace_back("-DCMAKE_BUILD_TYPE=Release");
                }
                for (const std::string &param: configureParams) {
                    cmk.emplace_back(param);
                }
                if (configureDefaultParameters) {
                    cmk.emplace_back(builddir);
                }
                auto env = Exec::getenv();
                Buildenv buildenv{cxxflags};
                buildenv.FilterEnv(env);
                exec.exec(cmk, env);
            }
            return 0;
        }};
        f.Require();
    } else {
        throw BuildException("Build dir not found");
    }
    create_directory(workdir / "configured");
}

std::vector<std::filesystem::path> ListFiles(const std::filesystem::path &root,
                                             const std::function<bool (const std::filesystem::path &)> &match = [] (const std::filesystem::path &) {return true;}) {
    std::vector<std::filesystem::path> subitems{};
    for (const auto &subitem : std::filesystem::directory_iterator{root}) {
        std::string filename = subitem.path().filename();
        if (filename == "." || filename == "..") {
            continue;
        }
        if (!match(subitem.path())) {
            continue;
        }
        subitems.emplace_back(filename);
        if (is_directory(subitem)) {
            for (const auto &subsubitem : ListFiles(subitem, match)) {
                path p = filename / subsubitem;
                subitems.emplace_back(p);
            }
        }
    }
    return subitems;
}

std::string FileListString(const std::vector<std::filesystem::path> &paths) {
    std::string fileListStr{};
    {
        for (const auto &file : paths) {
            if (!fileListStr.empty()) {
                fileListStr.append("\n");
            }
            fileListStr.append(file);
        }
    }
    return fileListStr;
}

void Build::Make(const std::vector<std::string> &flags) {
    path workdir = port->GetRoot() / "work";
    if (exists(workdir / "built")) {
        return;
    }
    Configure(flags);
    std::cout << "==> Building " << GetName() << "-" << GetVersion() << "\n";
    path builddir = workdir / this->builddir;
    auto tooling = GetTooling();
    if (tooling == Tooling::CMAKE) {
        std::string cm{this->builddir};
        cm.append("-cmake");
        builddir = port->GetRoot() / "work" / cm;
    }
    if (exists(builddir) && is_directory(builddir)) {
        Fork f{[this, builddir, tooling] () {
            if (chdir(builddir.c_str()) != 0) {
                std::cerr << "chdir: build dir: " << builddir << "\n";
                return 1;
            }
            if (tooling == Tooling::BOOTSTRAP) {
                auto ports = Ports::Create(port->GetGroup()->GetPortsRoot()->GetRoot().c_str());
                auto build = GetBuild(*ports, libc);
                if (!build.IsValid()) {
                    std::cerr << "Bootstrap libc was not found: " << libc << "\n";
                    return 1;
                }
                std::vector<Build> bootstrapBuilds{};
                std::vector<Build> bootstrapStaticBuilds{};
                for (const auto &bootstrapName : this->bootstrap) {
                    auto build = GetBuild(*ports, bootstrapName);
                    if (!build.IsValid()) {
                        std::cerr << "Build " << bootstrapName << " not found.\n";
                        throw BuildException("Build not found");
                    }
                    bootstrapBuilds.emplace_back(build);
                }
                for (const auto &bootstrapName : this->staticBootstrap) {
                    auto build = GetBuild(*ports, bootstrapName);
                    if (!build.IsValid()) {
                        std::cerr << "Build " << bootstrapName << " not found.\n";
                        throw BuildException("Build not found");
                    }
                    bootstrapStaticBuilds.emplace_back(build);
                }
                std::cout << "==> Fetching distfiles for bootstrapping\n";
                for (auto &bb : bootstrapBuilds) {
                    bb.Fetch();
                }
                std::vector<std::string> staticFlags{};
                staticFlags.emplace_back("static");
                for (auto &bb : bootstrapStaticBuilds) {
                    bb.Clean();
                    bb.Package(staticFlags);
                    bb.Clean();
                }
                build.Clean();
                build.Package();
                build.Clean();
                path bootstrapdir = builddir / "bootstrap";
                if (!exists(bootstrapdir)) {
                    if (!create_directory(bootstrapdir)) {
                        throw BuildException("Create bootstrapdir");
                    }
                }
                if (!is_directory(bootstrapdir)) {
                    throw BuildException("Bootstrapdir is not directory");
                }
                {
                    std::cout << "==> Copying ports dir to booststrap\n";
                    path bootstrapPortsdir = bootstrapdir;
                    for (const auto &part : ports->GetRoot()) {
                        std::string p = part;
                        if (p == "/") {
                            continue;
                        }
                        bootstrapPortsdir = bootstrapPortsdir / p;
                    }
                    if (!create_directories(bootstrapPortsdir)) {
                        throw BuildException("Create bootstrap ports dir");
                    }
                    std::string fileList = FileListString(ListFiles(ports->GetRoot(), [] (const auto &p) {
                        std::string filename = p.filename();
                        return filename != "work";
                    }));
                    Fork unpacking{[bootstrapPortsdir] () {
                        std::string dir = bootstrapPortsdir;
                        if (chdir(dir.c_str()) != 0) {
                            std::cerr << "chdir: build dir: " << dir << "\n";
                            return 1;
                        }
                        Exec exec{"cpio"};
                        std::vector<std::string> args{};
                        args.emplace_back("--extract");
                        auto env = Exec::getenv();
                        exec.exec(args, env);
                        return 0;
                    }, ForkInputOutput::INPUT};
                    Fork packing{[ports] () {
                        if (chdir(ports->GetRoot().c_str()) != 0) {
                            std::cerr << "chdir: build dir: " << ports->GetRoot() << "\n";
                            return 1;
                        }
                        Exec exec{"cpio"};
                        std::vector<std::string> args{};
                        args.emplace_back("--create");
                        auto env = Exec::getenv();
                        exec.exec(args, env);
                        return 0;
                    }, ForkInputOutput::INPUTOUTPUT};
                    Fork submit{[&packing, &fileList] () {
                        packing << fileList;
                        packing.CloseInput();
                        return 0;
                    }};
                    packing.CloseInput();
                    unpacking << packing;
                    unpacking.CloseInput();
                    packing.Require();
                    unpacking.Require();
                }
                std::string bootstrapdirStr = bootstrapdir;
                {
                    path bootstrapdirToRoot = "..";
                    {
                        auto iterator = builddir.begin();
                        if (iterator != builddir.end()) {
                            ++iterator;
                        }
                        if (iterator != builddir.end()) {
                            ++iterator;
                        }
                        while (iterator != builddir.end()) {
                            bootstrapdirToRoot = bootstrapdirToRoot / "..";
                            ++iterator;
                        }
                    }
                    path bootstrapdirBootstrapLink = bootstrapdir;
                    for (auto &part : bootstrapdir) {
                        std::string p = part;
                        if (p == "/") {
                            continue;
                        }
                        std::cout << p << "\n";
                        bootstrapdirBootstrapLink = bootstrapdirBootstrapLink / p;
                    }
                    {
                        path crdir = bootstrapdir;
                        for (auto &part : builddir) {
                            std::string p = part;
                            if (p == "/") {
                                continue;
                            }
                            std::cout << p << "\n";
                            crdir = crdir / p;
                        }
                        if (!create_directories(crdir)) {
                            std::cerr << "Bootstrap link failed: mkdir: " << crdir << "\n";
                            return 1;
                        }
                    }
                    create_directory_symlink(bootstrapdirToRoot, bootstrapdirBootstrapLink);
                }
                {
                    std::cout << "==> Unpacking libc to bootstrap\n";
                    std::string pkg{build.GetName()};
                    pkg.append("-");
                    pkg.append(build.GetVersion());
                    pkg.append(".pkg");
                    pkg = builddir / pkg;
                    Unpack unpack{pkg, bootstrapdirStr};
                }
                {
                    std::cout << "==> Unpacking static builds to bootstrap\n";
                    for (auto &bb : bootstrapStaticBuilds) {
                        std::string pkg{bb.GetName()};
                        pkg.append("-");
                        pkg.append(bb.GetVersion());
                        pkg.append(".pkg");
                        pkg = builddir / pkg;
                        Unpack unpack{pkg, bootstrapdirStr};
                    }
                    auto originalEnv = Exec::getenv();
                    auto env = originalEnv;
                    env.insert_or_assign("MUSL_BOOTSTRAP", bootstrapdirStr);
                    Exec::setenv(env);
                    std::cout << "==> Building and installing bootstrapping builds\n";
                    for (auto &bb : bootstrapBuilds) {
                        bb.Clean();
                        bb.Package();
                        bb.Clean();
                        {
                            std::string pkg{bb.GetName()};
                            pkg.append("-");
                            pkg.append(bb.GetVersion());
                            pkg.append(".pkg");
                            pkg = builddir / pkg;
                            Unpack unpack{pkg, bootstrapdirStr};
                        }
                    }
                    std::cout << "==> Bootstrap build complete\n";
                    Exec::setenv(originalEnv);
                }
                return 0;
            } else if (tooling == Tooling::STATIC_FILES) {
                Fork extract([] () {
                    Exec exec{"cpio"};
                    std::vector<std::string> args{};
                    args.emplace_back("--extract");
                    exec.exec(args, Exec::getenv());
                    return 0;
                }, ForkInputOutput::INPUT);
                path files = port->GetRoot() / "files";
                Fork packing{[&files] () {
                    if (chdir(files.c_str()) != 0) {
                        std::cerr << "chdir: build dir: " << files << "\n";
                        return 1;
                    }
                    Exec exec{"cpio"};
                    std::vector<std::string> args{};
                    args.emplace_back("--create");
                    exec.exec(args, Exec::getenv());
                    return 0;
                }, ForkInputOutput::INPUTOUTPUT};
                std::string list = FileListString(ListFiles(files));
                Fork submit{[&packing, &list] () {
                    packing << list;
                    packing.CloseInput();
                    return 0;
                }};
                packing.CloseInput();
                extract << packing;
                extract.CloseInput();
                submit.Require();
                packing.Require();
                extract.Require();
                return 0;
            } else {
                Exec make{"make"};
                std::vector<std::string> args{};
                if (tooling == Tooling::CMAKE) {
                    args.emplace_back("VERBOSE=1");
                }
                for (auto &target: buildTargets) {
                    args.emplace_back(target);
                }
                auto env = Exec::getenv();
                Buildenv buildenv{cxxflags};
                buildenv.FilterEnv(env);
                make.exec(args, env);
                return 0;
            }
        }};
        f.Require();
    } else {
        throw BuildException("Build dir not found");
    }
    create_directory(workdir / "built");
}

void Build::Install(const std::vector<std::string> &flags) {
    path workdir = port->GetRoot() / "work";
    if (exists(workdir / "installed")) {
        return;
    }
    Make(flags);
    std::cout << "==> Installing " << GetName() << "-" << GetVersion() << "\n";
    path builddir = workdir / this->builddir;
    auto tooling = GetTooling();
    if (tooling == Tooling::CMAKE) {
        std::string cm{this->builddir};
        cm.append("-cmake");
        builddir = port->GetRoot() / "work" / cm;
    }
    path installdir = port->GetRoot() / "work" / "install";
    if (!exists(installdir)) {
        if (!create_directory(installdir)) {
            throw BuildException("Installdir could not be created");
        }
    } else if (!is_directory(installdir)) {
        throw BuildException("Installdir is not a directory");
    }
    if (exists(builddir) && is_directory(builddir)) {
        if (tooling == Tooling::STATIC_FILES) {
            Fork extract([&installdir] () {
                if (chdir(installdir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << installdir << "\n";
                    return 1;
                }
                Exec exec{"cpio"};
                std::vector<std::string> args{};
                args.emplace_back("--extract");
                exec.exec(args, Exec::getenv());
                return 0;
            }, ForkInputOutput::INPUT);
            Fork packing{[&builddir] () {
                if (chdir(builddir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                Exec exec{"cpio"};
                std::vector<std::string> args{};
                args.emplace_back("--create");
                exec.exec(args, Exec::getenv());
                return 0;
            }, ForkInputOutput::INPUTOUTPUT};
            std::string list = FileListString(ListFiles(builddir));
            Fork submit{[&packing, &list] () {
                packing << list;
                packing.CloseInput();
                return 0;
            }};
            packing.CloseInput();
            extract << packing;
            extract.CloseInput();
            submit.Require();
            packing.Require();
            extract.Require();
        } else {
            Fork f{[this, builddir, installdir]() {
                if (chdir(builddir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                Exec make{"make"};
                std::vector<std::string> args{};
                {
                    std::string idir = installdir;
                    std::string destdir{"DESTDIR="};
                    destdir.append(idir);
                    args.push_back(destdir);
                }
                for (const auto &target: installTargets) {
                    args.emplace_back(target);
                }
                auto env = Exec::getenv();
                Buildenv buildenv{cxxflags};
                buildenv.FilterEnv(env);
                make.exec(args, env);
                return 0;
            }};
        }
    } else {
        throw BuildException("Build dir not found");
    }
    create_directory(workdir / "installed");
}

void Build::Package(const std::vector<std::string> &flags) {
    Install(flags);
    std::cout << "==> Packaging " << GetName() << "-" << GetVersion() << "\n";
    path installdir = port->GetRoot() / "work" / "install";
    if (!exists(installdir) || !is_directory(installdir)) {
        throw BuildException("Install dir not found");
    }
    std::string fileListStr = FileListString(ListFiles(installdir));
    std::cout << fileListStr << "\n";
    Fork fork{[installdir] () {
        std::string directory = installdir;
        if (chdir(directory.c_str()) != 0) {
            std::cerr << "chdir: build dir: " << directory << "\n";
            return 1;
        }
        Exec exec{"cpio"};
        std::vector<std::string> args{};
        args.emplace_back("--create");
        auto env = Exec::getenv();
        exec.exec(args, env);
        return 0;
    }, ForkInputOutput::INPUTOUTPUT};
    Fork submit{[&fork, &fileListStr] () {
        fork << fileListStr;
        fork.CloseInput();
        return 0;
    }};
    fork.CloseInput();
    std::string packageFilename{GetName()};
    packageFilename.append("-");
    packageFilename.append(GetVersion());
    packageFilename.append(".pkg");
    std::fstream outputStream{};
    outputStream.open(packageFilename, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
    if (!outputStream.is_open()) {
        throw BuildException("Failed to open package file for writing");
    }
    fork >> outputStream;
}
