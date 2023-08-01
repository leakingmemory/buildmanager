#include <iostream>
#include <vector>
#include <map>
#include "Ports.h"
#include "PortGroup.h"
#include "Port.h"
#include "Build.h"
#include "Unpack.h"
#include "Chroot.h"
#include "Exec.h"
#include "Db.h"
#include "Installed.h"

const char *portdir = "/var/ports";

enum class Command {
    NONE,
    LIST_GROUPS,
    LIST_PORTS,
    LIST_BUILDS,
    LIST_INSTALLED,
    CLEAN,
    FETCH,
    EXTRACT,
    CONFIGURE,
    BUILD,
    INSTALL,
    PACKAGE,
    UNPACK,
    REPLACE,
    REGISTER,
    FIND,
    VERIFY,
    UNINSTALL,
    UNREGISTER,
    RDEP,
    FILES,
    REBOOTSTRAP,
    BOOTSTRAPSHELL,
    CHROOT
};

static int ListGroups(Ports &ports) {
    auto groups = ports.GetGroups();
    for (const auto &group : groups) {
        std::cout << group->GetName() << "\n";
    }
    return 0;
}

static int ListPorts(Ports &ports, const std::string &groupName) {
    auto group = ports.GetGroup(groupName);
    auto portList = group->GetPorts();
    for (const auto &port : portList) {
        std::cout << port->GetName() << "\n";
    }
    return 0;
}

static int ListBuilds(Ports &ports, const std::string &portName) {
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

static int ListInstalled(const std::string &rootDir) {
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

static int Fetch(Ports &ports, const std::string &buildName) {
    Build build = GetBuild(ports, buildName, {});
    if (build.IsValid()) {
        build.Fetch();
        return 0;
    } else {
        return 2;
    }
}

static int Extract(Ports &ports, const std::string &buildName) {
    Build build = GetBuild(ports, buildName, {});
    if (build.IsValid()) {
        build.Extract();
        return 0;
    } else {
        return 2;
    }
}

static int Clean(Ports &ports, const std::string &buildName) {
    Build build = GetBuild(ports, buildName, {});
    if (build.IsValid()) {
        build.Clean();
        return 0;
    } else {
        return 2;
    }
}

static int Configure(Ports &ports, const std::string &buildName) {
    Build build = GetBuild(ports, buildName, {});
    if (build.IsValid()) {
        build.Configure();
        return 0;
    } else {
        return 2;
    }
}

static int Make(Ports &ports, const std::string &buildName) {
    Build build = GetBuild(ports, buildName, {});
    if (build.IsValid()) {
        build.Make();
        return 0;
    } else {
        return 2;
    }
}

static int Install(Ports &ports, const std::string &buildName) {
    Build build = GetBuild(ports, buildName, {});
    if (build.IsValid()) {
        build.Install();
        return 0;
    } else {
        return 2;
    }
}

static int Package(Ports &ports, const std::string &buildName) {
    Build build = GetBuild(ports, buildName, {});
    if (build.IsValid()) {
        build.Package();
        return 0;
    } else {
        return 2;
    }
}

static int BootstrapShell(Ports &ports, const std::string &buildName) {
    Build build = GetBuild(ports, buildName, {});
    if (build.IsValid()) {
        build.BootstrapShell();
        return 0;
    } else {
        return 2;
    }
}

static int Rebootstrap(Ports &ports, const std::string &buildName) {
    Build build = GetBuild(ports, buildName, {});
    if (build.IsValid()) {
        build.ReBootstrap();
        return 0;
    } else {
        return 2;
    }
}

static int Chroot(const std::string &dir) {
    class Chroot chroot{dir, [] () {
        Exec exec{"/bin/bash"};
        exec.exec({}, Exec::getenv());
    }};
    return 0;
}

static int Unpack(const std::string &filename, const std::string &targetDir) {
    class Unpack unpack{filename, targetDir};
    return 0;
}

static int Replace(const std::string &port, const std::string &filename, const std::string &targetDir) {
    Db db{targetDir};
    try {
        auto installedPort = db.Find(port);
        class Unpack unpack{filename, targetDir};
        unpack.Replace(installedPort, targetDir);
    } catch (const PkgAmbiguous &amb) {
        std::cout << "Ambiguous: " << amb.what() << "\n";
        return 1;
    } catch (const PkgNotFound &notFound) {
        std::cout << "Not found: " << notFound.what() << "\n";
        return 1;
    }
    return 0;
}

static int Register(const std::string &filename, const std::string &targetDir) {
    class Unpack unpack{filename};
    unpack.Register(targetDir);
    return 0;
}

static int Find(const std::string &port, const std::string &rootDir) {
    Db db{rootDir};
    try {
        auto installedPort = db.Find(port);
        std::cout << installedPort.GetGroup() << "/" << installedPort.GetName() << "/" << installedPort.GetVersion()
                  << "\n";
    } catch (const PkgAmbiguous &amb) {
        std::cout << "Ambiguous: " << amb.what() << "\n";
        return 1;
    } catch (const PkgNotFound &notFound) {
        std::cout << "Not found: " << notFound.what() << "\n";
        return 1;
    }
    return 0;
}

static int Verify(const std::string &port, const std::string &rootDir) {
    Db db{rootDir};
    try {
        auto installedPort = db.Find(port);
        installedPort.Verify(rootDir, [] (auto path, auto result) {
            switch (result) {
                case FileMatch::OK:
                    //std::cout << "Matching: " << path << "\n";
                    break;
                case FileMatch::NOT_MATCHING:
                    std::cout << "Modified: " << path << "\n";
                    break;
                case FileMatch::NOT_FOUND:
                    std::cout << "Not found: " << path << "\n";
                    break;
            }
        });
    } catch (const PkgAmbiguous &amb) {
        std::cout << "Ambiguous: " << amb.what() << "\n";
        return 1;
    } catch (const PkgNotFound &notFound) {
        std::cout << "Not found: " << notFound.what() << "\n";
        return 1;
    }
    return 0;
}

static int Uninstall(const std::string &port, const std::string &rootDir) {
    Db db{rootDir};
    try {
        auto installedPort = db.Find(port);
        installedPort.Uninstall(rootDir);
    } catch (const PkgAmbiguous &amb) {
        std::cout << "Ambiguous: " << amb.what() << "\n";
        return 1;
    } catch (const PkgNotFound &notFound) {
        std::cout << "Not found: " << notFound.what() << "\n";
        return 1;
    }
    return 0;
}

static int Unregister(const std::string &port, const std::string &rootDir) {
    Db db{rootDir};
    try {
        auto installedPort = db.Find(port);
        installedPort.Unregister();
    } catch (const PkgAmbiguous &amb) {
        std::cout << "Ambiguous: " << amb.what() << "\n";
        return 1;
    } catch (const PkgNotFound &notFound) {
        std::cout << "Not found: " << notFound.what() << "\n";
        return 1;
    }
    return 0;
}

static int Rdep(const std::string &port, const std::string &rootDir) {
    Db db{rootDir};
    try {
        auto installedPort = db.Find(port);
        auto rdep = installedPort.GetRdep();
        for (const auto &dep : rdep) {
            std::cout << dep << "\n";
        }
    } catch (const PkgAmbiguous &amb) {
        std::cout << "Ambiguous: " << amb.what() << "\n";
        return 1;
    } catch (const PkgNotFound &notFound) {
        std::cout << "Not found: " << notFound.what() << "\n";
        return 1;
    }
    return 0;
}

static int Files(const std::string &port, const std::string &rootDir) {
    Db db{rootDir};
    try {
        auto installedPort = db.Find(port);
        auto files = installedPort.GetFiles();
        for (const auto &file : files) {
            std::cout << file.filename << "\n";
        }
    } catch (const PkgAmbiguous &amb) {
        std::cout << "Ambiguous: " << amb.what() << "\n";
        return 1;
    } catch (const PkgNotFound &notFound) {
        std::cout << "Not found: " << notFound.what() << "\n";
        return 1;
    }
    return 0;
}

int usage(const std::string &cmd) {
    std::cerr << "Usage:\n " << cmd << " list-groups\n " << cmd << " list-ports <group-name>\n"
            << " " << cmd << " list-builds <group/port>\n " << cmd << " list-installed <root-dir>\n"
            << " " << cmd << " clean <group/port/build>\n " << cmd << " fetch <group/port/build>\n"
            << " " << cmd << " extract <group/port/build>\n " << cmd << " configure <group/port/build>\n"
            << " " << cmd << " build <group/port/build>\n " << cmd << " install <group/port/build>\n"
            << " " << cmd << " package <group/port/build>\n "
            << " " << cmd << " unpack <file> <target-dir>\n" << cmd << " replace <group/port/build> <file> <target-dir>\n"
            << " " << cmd << " register <file> <target-dir>\n " << cmd << " find <pkg> <root-dir>\n"
            << " " << cmd << " verify <pkg> <root-dir>\n " << cmd << " uninstall <pkg> <root-dir>\n"
            << " " << cmd << " unregister <pkg> <root-dir>\n " << cmd << " rdep <pkg> <root-dir>\n"
            << " " << cmd << " files <pkg> <root-dir>\n"
            << " " << cmd << " rebootstrap <group/port/build>\n " << cmd << " bootstrapshell <group/port/build>\n"
            << " " << cmd << " chroot <dir>\n";
    return 1;
}

static int RunCmd(const std::string &cmdExec, Ports &ports, Command cmd, std::vector<std::string> &args) {
    switch (cmd) {
        case Command::LIST_GROUPS:
            if (args.begin() != args.end()) {
                return usage(cmdExec);
            }
            return ListGroups(ports);
        case Command::LIST_PORTS: {
            std::string groupName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                groupName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return ListPorts(ports, groupName);
        }
        case Command::LIST_BUILDS: {
            std::string portName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                portName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return ListBuilds(ports, portName);
        }
        case Command::LIST_INSTALLED: {
            std::string rootDir{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                rootDir = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return ListInstalled(rootDir);
        }
        case Command::CLEAN: {
            std::string buildName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                buildName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Clean(ports, buildName);
        }
        case Command::FETCH: {
            std::string buildName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                buildName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Fetch(ports, buildName);
        }
        case Command::EXTRACT: {
            std::string buildName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                buildName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Extract(ports, buildName);
        }
        case Command::CONFIGURE: {
            std::string buildName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                buildName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Configure(ports, buildName);
        }
        case Command::BUILD: {
            std::string buildName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                buildName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Make(ports, buildName);
        }
        case Command::INSTALL: {
            std::string buildName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                buildName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Install(ports, buildName);
        }
        case Command::PACKAGE: {
            std::string buildName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                buildName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Package(ports, buildName);
        }
        case Command::UNPACK: {
            std::string filename{};
            std::string targetDir{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                filename = *iterator;
                ++iterator;
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                targetDir = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Unpack(filename, targetDir);
        }
        case Command::REPLACE: {
            std::string subject{};
            std::string filename{};
            std::string targetDir{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                subject = *iterator;
                ++iterator;
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                filename = *iterator;
                ++iterator;
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                targetDir = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Replace(subject, filename, targetDir);
        }
        case Command::REGISTER: {
            std::string filename{};
            std::string targetDir{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                filename = *iterator;
                ++iterator;
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                targetDir = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Register(filename, targetDir);
        }
        case Command::FIND: {
            std::string port{};
            std::string rootDir{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                port = *iterator;
                ++iterator;
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                rootDir = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Find(port, rootDir);
        }
        case Command::VERIFY: {
            std::string port{};
            std::string rootDir{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                port = *iterator;
                ++iterator;
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                rootDir = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Verify(port, rootDir);
        }
        case Command::UNINSTALL: {
            std::string port{};
            std::string rootDir{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                port = *iterator;
                ++iterator;
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                rootDir = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Uninstall(port, rootDir);
        }
        case Command::UNREGISTER: {
            std::string port{};
            std::string rootDir{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                port = *iterator;
                ++iterator;
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                rootDir = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Unregister(port, rootDir);
        }
        case Command::RDEP: {
            std::string port{};
            std::string rootDir{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                port = *iterator;
                ++iterator;
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                rootDir = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Rdep(port, rootDir);
        }
        case Command::FILES: {
            std::string port{};
            std::string rootDir{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                port = *iterator;
                ++iterator;
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                rootDir = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Files(port, rootDir);
        }
        case Command::REBOOTSTRAP: {
            std::string buildName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                buildName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Rebootstrap(ports, buildName);
        }
        case Command::BOOTSTRAPSHELL: {
            std::string buildName{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                buildName = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return BootstrapShell(ports, buildName);
        }
        case Command::CHROOT: {
            std::string dir{};
            {
                auto iterator = args.begin();
                if (iterator == args.end()) {
                    return usage(cmdExec);
                }
                dir = *iterator;
                ++iterator;
                if (iterator != args.end()) {
                    return usage(cmdExec);
                }
            }
            return Chroot(dir);
        }
        case Command::NONE:
            break;
    }
    return 0;
}

static std::map<std::string,Command> GetInitialCmdMap() {
    std::map<std::string,Command> cmdMap{};
    cmdMap.insert_or_assign("list-groups", Command::LIST_GROUPS);
    cmdMap.insert_or_assign("list-ports", Command::LIST_PORTS);
    cmdMap.insert_or_assign("list-builds", Command::LIST_BUILDS);
    cmdMap.insert_or_assign("list-installed", Command::LIST_INSTALLED);
    cmdMap.insert_or_assign("clean", Command::CLEAN);
    cmdMap.insert_or_assign("fetch", Command::FETCH);
    cmdMap.insert_or_assign("extract", Command::EXTRACT);
    cmdMap.insert_or_assign("configure", Command::CONFIGURE);
    cmdMap.insert_or_assign("build", Command::BUILD);
    cmdMap.insert_or_assign("install", Command::INSTALL);
    cmdMap.insert_or_assign("package", Command::PACKAGE);
    cmdMap.insert_or_assign("register", Command::REGISTER);
    cmdMap.insert_or_assign("find", Command::FIND);
    cmdMap.insert_or_assign("verify", Command::VERIFY);
    cmdMap.insert_or_assign("uninstall", Command::UNINSTALL);
    cmdMap.insert_or_assign("unregister", Command::UNREGISTER);
    cmdMap.insert_or_assign("rdep", Command::RDEP);
    cmdMap.insert_or_assign("files", Command::FILES);
    cmdMap.insert_or_assign("unpack", Command::UNPACK);
    cmdMap.insert_or_assign("replace", Command::REPLACE);
    cmdMap.insert_or_assign("rebootstrap", Command::REBOOTSTRAP);
    cmdMap.insert_or_assign("bootstrapshell", Command::BOOTSTRAPSHELL);
    cmdMap.insert_or_assign("chroot", Command::CHROOT);
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

int cppmain(const std::string &cmd, const std::vector<std::string> &i_args) {
    auto ports = Ports::Create(portdir);
    std::vector<std::string> args{i_args};
    auto command = GetCmd(args);
    if (command == Command::NONE) {
        return usage(cmd);
    }
    try {
        return RunCmd(cmd, *ports, command, args);
    } catch (const std::exception &e) {
        const auto *what = e.what();
        std::cerr << "error: " << (what != nullptr ? what : "std::exception(what=nullptr)") << "\n";
        return 1;
    }
}

extern "C" {

int main(int argc, const char *argv[]) {
    std::string cmd{};
    std::vector<std::string> args{};
    if (argc < 1) {
        std::cerr << "Unexpected invocation context: argc<1\n";
        return 1;
    }
    cmd.append(argv[0]);
    args.reserve(argc - 1);
    for (typeof(argc) i = 1; i < argc; i++) {
        std::string &arg = args.emplace_back();
        arg.append(argv[i]);
    }
    return cppmain(cmd, args);
}

}