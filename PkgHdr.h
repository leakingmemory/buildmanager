//
// Created by sigsegv on 7/27/23.
//

#ifndef BM_PKGHDR_H
#define BM_PKGHDR_H

#include <cstdint>

constexpr uint32_t PkgHdrMagic = 0x4a504b47;

#define PKG_MAGIC (PkgHdrMagic)

struct PkgHdr {
    uint32_t magic;
    uint32_t jsonLength;
    uint32_t listLength;
};

#endif //BM_PKGHDR_H
