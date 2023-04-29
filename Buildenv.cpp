//
// Created by sigsegv on 4/10/23.
//

#include "Buildenv.h"
#include <cctype>
#include <algorithm>

Buildenv::Buildenv(const std::string &cxxflags) : cxxflags(cxxflags) {}

void Buildenv::FilterEnv(std::map<std::string,std::string> &env) {
    std::map<std::string,std::string>::iterator end = env.end();
    std::map<std::string,std::string>::iterator musl_bootstrap = end;
    std::map<std::string,std::string>::iterator libcpp_bootstrap = end;
    std::map<std::string,std::string>::iterator cflags_iter = end;
    std::map<std::string,std::string>::iterator cxxflags_iter = end;
    std::map<std::string,std::string>::iterator ldflags_iter = end;
    std::map<std::string,std::string>::iterator ld_library_path_iter = end;
    auto iterator = env.begin();
    while (iterator != end) {
        auto key = iterator->first;
        std::transform(key.begin(), key.end(), key.begin(), [] (auto c) { return std::tolower(c); });
        if (key == "musl_bootstrap") {
            musl_bootstrap = iterator;
        } else if (key == "libcpp_bootstrap") {
            libcpp_bootstrap = iterator;
        } else if (key == "cflags") {
            cflags_iter = iterator;
        } else if (key == "cxxflags") {
            cxxflags_iter = iterator;
        } else if (key == "ldflags") {
            ldflags_iter = iterator;
        } else if (key == "ld_library_path") {
            ld_library_path_iter = iterator;
        }
        ++iterator;
    }
    std::string cflags{};
    std::string cxxflags{};
    std::string ldflags{};
    std::string ld_library_path{};
    if (cflags_iter != end) {
        cflags = cflags_iter->second;
    }
    if (cxxflags_iter != end) {
        cxxflags = cxxflags_iter->second;
    }
    if (ldflags_iter != end) {
        ldflags = ldflags_iter->second;
    }
    if (ld_library_path_iter != end) {
        ld_library_path = ld_library_path_iter->second;
    }
    bool add_cflags{false};
    bool add_cxxflags{false};
    bool add_ldflags{false};
    bool add_ld_library_path{false};
    if (musl_bootstrap != end) {
        std::string musl_cflags = " --sysroot=";
        musl_cflags.append(musl_bootstrap->second);
        musl_cflags.append(" ");
        auto musl_cxxflags = musl_cflags;
        musl_cxxflags.append("-isystem ");
        musl_cxxflags.append(musl_bootstrap->second);
        musl_cxxflags.append("/usr/include ");
        musl_cflags.append(cflags);
        musl_cxxflags.append(cxxflags);
        cflags = musl_cflags;
        cxxflags = musl_cxxflags;

        add_cflags = true;
        add_cxxflags = true;

        std::string musl_libdir = musl_bootstrap->second;
        musl_libdir.append("/lib");
        std::string musl = musl_libdir;
        musl.append("/libc.so");
        std::string musl_ldflags = " -L";
        musl_ldflags.append(musl_libdir);
        musl_ldflags.append(" -Wl,--sysroot=");
        musl_ldflags.append(musl_bootstrap->second);
        musl_ldflags.append(" -Wl,--dynamic-linker=");
        musl_ldflags.append(musl);
        musl_ldflags.append(" ");
        musl_ldflags.append(ldflags);
        ldflags = musl_ldflags;
        add_ldflags = true;

        std::string muslld = musl_libdir;
        if (!ld_library_path.empty()) {
            muslld.append(":");
            muslld.append(ld_library_path);
        }
        ld_library_path = muslld;
        add_ld_library_path = true;
    }
    if (libcpp_bootstrap != end) {
        std::string musl_cxxflags{" -Wl,-lc++ -isystem "};
        musl_cxxflags.append(musl_bootstrap->second);
        musl_cxxflags.append("/usr/include/c++/v1 ");
        musl_cxxflags.append(cxxflags);
        cxxflags = musl_cxxflags;

        add_cxxflags = true;
    }
    if (!(this->cxxflags.empty())) {
        cxxflags.append(" ");
        cxxflags.append(this->cxxflags);
        add_cxxflags = true;
    }
    if (add_cflags && cflags_iter != end) {
        cflags_iter->second = cflags;
        add_cflags = false;
    }
    if (add_cxxflags && cxxflags_iter != end) {
        cxxflags_iter->second = cxxflags;
        add_cxxflags = false;
    }
    if (add_ldflags && ldflags_iter != end) {
        ldflags_iter->second = ldflags;
        add_ldflags = false;
    }
    if (add_ld_library_path && ld_library_path_iter != end) {
        ld_library_path_iter->second = ld_library_path;
        add_ld_library_path = false;
    }
    /* iterators invalid mark */
    if (add_cflags) {
        env.insert_or_assign("CFLAGS", cflags);
    }
    if (add_cxxflags) {
        env.insert_or_assign("CXXFLAGS", cxxflags);
    }
    if (add_ldflags) {
        env.insert_or_assign("LDFLAGS", ldflags);
    }
    if (add_ld_library_path) {
        env.insert_or_assign("LD_LIBRARY_PATH", ld_library_path);
    }
}
