//
// Created by sigsegv on 8/1/23.
//

#include "App.h"
#include "Ports.h"
#include <iostream>

const char *portdir = "/var/ports";

App::App(int argc, const char *const *argv) {
    if (argc < 1) {
        std::cerr << "Unexpected invocation context: argc<1\n";
        throw std::exception();
    }
    cmd.append(argv[0]);
    args.reserve(argc - 1);
    for (typeof(argc) i = 1; i < argc; i++) {
        std::string &arg = args.emplace_back();
        arg.append(argv[i]);
    }
}

int App::main() {
    auto ports = Ports::Create(portdir);
    std::vector<std::string> args{this->args};
    ConsumeCmd(args);
    if (!IsValidCmd()) {
        return usage();
    }
    try {
        return RunCmd(*ports, args);
    } catch (const std::exception &e) {
        const auto *what = e.what();
        std::cerr << "error: " << (what != nullptr ? what : "std::exception(what=nullptr)") << "\n";
        return 1;
    }
}
