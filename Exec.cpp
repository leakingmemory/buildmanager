//
// Created by sigsegv on 3/24/23.
//

#include "Exec.h"
extern "C" {
#include <unistd.h>
}

class ExecException : public std::exception {
private:
    const char *error;
public:
    ExecException(const char *error) : error(error) {}
    const char * what() const noexcept override {
        return error;
    }
};

constexpr const char *binpaths[] = {
        "/bin",
        "/usr/bin",
        "/sbin",
        "/usr/sbin",
};
constexpr const typeof(sizeof(binpaths)) numBinpaths = sizeof(binpaths) / sizeof(binpaths[0]);

Exec::Exec(const std::string &name) : bin() {
    bool binpathFound{false};
    if (name.starts_with("./") || name.starts_with("/")) {
        bin = name;
        binpathFound = true;
    } else {
        for (std::remove_const<typeof(numBinpaths)>::type i = 0; i < numBinpaths; i++) {
            std::filesystem::path binpath = binpaths[i];
            binpath = binpath / name;
            if (exists(binpath) && is_regular_file(binpath)) {
                bin = binpath;
                binpathFound = true;
                break;
            }
        }
        if (!binpathFound) {
            throw ExecException("Binary not found");
        }
    }
}

static std::map<std::string,std::string> envmap{};
static bool envmapSet = false;

void Exec::setenv(const std::map<std::string, std::string> &env) {
    envmap = env;
    envmapSet = true;
}

std::map<std::string, std::string> Exec::getenv() {
    if (envmapSet) {
        return envmap;
    }
    std::map<std::string,std::string> map{};
    int c = 0;
    while (environ[c] != nullptr) {
        std::string str{environ[c]};
        auto eq = str.find('=');
        if (eq < str.size()) {
            std::string name = str.substr(0, eq);
            std::string value = str.substr(eq + 1);
            map.insert_or_assign(name, value);
        } else {
            map.insert_or_assign(str, "");
        }
        ++c;
    }
    return map;
}

void Exec::exec(const std::vector<std::string> &args, const std::map<std::string,std::string> &env) {
    std::vector<const char *> argv{};
    std::vector<std::string> envd{};
    std::vector<const char *> envp{};

    const std::string binstr{bin};
    argv.push_back(binstr.c_str());
    for (const std::string &arg: args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    for (const auto &e : env) {
        std::string str{e.first};
        str.append("=");
        str.append(e.second);
        envd.emplace_back(str);
    }
    for (const auto &e : envd) {
        envp.push_back(e.c_str());
    }
    envp.push_back(nullptr);

    auto argvptr = (char * const *) argv.data();
    auto envptr = (char * const *) envp.data();
    execve(binstr.c_str(), argvptr, envptr);
    throw ExecException("execve failed");
}