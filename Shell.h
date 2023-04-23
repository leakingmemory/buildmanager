//
// Created by sigsegv on 4/3/23.
//

#ifndef BM_SHELL_H
#define BM_SHELL_H

#include "Exec.h"

class Shell {
private:
    Exec execsh;
public:
    Shell();
    void exec(const std::vector<std::string> &, const std::map<std::string,std::string> &);
};


#endif //BM_SHELL_H
