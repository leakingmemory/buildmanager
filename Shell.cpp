//
// Created by sigsegv on 4/3/23.
//

#include "Shell.h"

Shell::Shell() : execsh("sh") {
}

void Shell::exec(const std::vector<std::string> &cmd, const std::map<std::string,std::string> &env) {
    execsh.exec(cmd, env);
}