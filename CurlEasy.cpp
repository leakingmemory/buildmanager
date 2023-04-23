//
// Created by sigsegv on 3/17/23.
//

#include "CurlEasy.h"
extern "C" {
    #include <curl/curl.h>
}
#include <algorithm>
#include <iostream>

static size_t ce_writefunc(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto ce = (CurlEasyCallbacks *) userdata;
    return ce->writefunc(ptr, size, nmemb);
}

static size_t ce_readfunc(char *buffer, size_t size, size_t nitems, void *userdata) {
    auto ce = (CurlEasyCallbacks *) userdata;
    return ce->readfunc(buffer, size, nitems);
}

static int ce_seekfunc(void *clientp, curl_off_t offset, int origin) {
    auto ce = (CurlEasyCallbacks *) clientp;
    return ce->seekfunc(offset, origin);
}

static int ce_sockoptfunc(void *clientp,
                     curl_socket_t curlfd,
                     curlsocktype purpose) {
    auto ce = (CurlEasyCallbacks *) clientp;
    Curlsocktype socktype;
    switch (purpose) {
        case CURLSOCKTYPE_ACCEPT:
            socktype = Curlsocktype::ACCEPT;
            break;
        case CURLSOCKTYPE_IPCXN:
            socktype = Curlsocktype::IPCXN;
            break;
        default:
            std::cerr << "Curl socket type is unsupported\n";
            return CURL_SOCKOPT_ERROR;
    }
    return ce->sockoptfunc(curlfd, socktype);
}

static int ce_xferinfofunc(void *clientp,
                      curl_off_t dltotal,
                      curl_off_t dlnow,
                      curl_off_t ultotal,
                      curl_off_t ulnow) {
    auto ce = (CurlEasyCallbacks *) clientp;
    return ce->xferinfofunc(dltotal, dlnow, ultotal, ulnow);
}

size_t CurlEasyCallbacks::writefunc(char *ptr, size_t size, size_t nmemb) {
    auto result = ce.write(ptr, size * nmemb);
    if (result == CurlEasy::writeError) {
        return CURL_WRITEFUNC_ERROR;
    } else {
        return result;
    }
}

size_t CurlEasyCallbacks::readfunc(char *buffer, size_t size, size_t nitems) {
    return CURL_READFUNC_ABORT;
}

int CurlEasyCallbacks::seekfunc(off_t offset, int origin) {
    return CURL_SEEKFUNC_FAIL;
}

int CurlEasyCallbacks::sockoptfunc(int curlfd, Curlsocktype purpose) {
    return CURL_SOCKOPT_OK;
}

int CurlEasyCallbacks::xferinfofunc(off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow) {
    return 0;
}

void CurlEasy::SetupHandleDatapointers() {
    CurlEasyCallbacks &callbacks = *this;
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &callbacks);
    curl_easy_setopt(handle, CURLOPT_READDATA, &callbacks);
    curl_easy_setopt(handle, CURLOPT_SEEKDATA, &callbacks);
    curl_easy_setopt(handle, CURLOPT_SOCKOPTDATA, &callbacks);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, &callbacks);

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, ce_writefunc);
    curl_easy_setopt(handle, CURLOPT_READFUNCTION, ce_readfunc);
    curl_easy_setopt(handle, CURLOPT_SEEKFUNCTION, ce_seekfunc);
    curl_easy_setopt(handle, CURLOPT_SOCKOPTFUNCTION, ce_sockoptfunc);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, ce_xferinfofunc);
}
CurlEasy::CurlEasy() : CurlEasyCallbacks(*this), handle(curl_easy_init()),
        write([] (const void *, size_t) { return writeError; })
{
    SetupHandleDatapointers();
}

CurlEasy::~CurlEasy() {
    if (handle != nullptr) {
        curl_easy_cleanup(handle);
    }
}

CurlEasy::CurlEasy(CurlEasy &&mv) noexcept : CurlEasyCallbacks(*this), handle(mv.handle) {
    mv.handle = nullptr;
    if (handle != nullptr) {
        SetupHandleDatapointers();
    }
}

CurlEasy::CurlEasy(const CurlEasy &cp) : CurlEasyCallbacks(*this), handle(cp.handle != nullptr ? curl_easy_duphandle(cp.handle) : nullptr) {
    if (handle != nullptr) {
        SetupHandleDatapointers();
    }
}

CurlEasy &CurlEasy::operator=(CurlEasy &&mv) noexcept {
    CurlEasy tmp{std::move(mv)};
    Swap(tmp);
    if (handle != nullptr) {
        SetupHandleDatapointers();
    }
    return *this;
}

CurlEasy &CurlEasy::operator=(const CurlEasy &cp) {
    CurlEasy tmp{cp};
    Swap(tmp);
    if (handle != nullptr) {
        SetupHandleDatapointers();
    }
    return *this;
}

void CurlEasy::SetUrl(const std::string &url) {
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
}

void CurlEasy::SetWriteCallback(const std::function<size_t(const void *, size_t)> &write) {
    this->write = write;
}

bool CurlEasy::Perform() {
    auto res = curl_easy_perform(handle);
    return res == CURLE_OK;
}

long CurlEasy::GetResponseCode() {
    long response_code{0};
    auto res = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
    if (res == CURLE_OK) {
        return response_code;
    }
    return 0;
}

std::optional<std::string> CurlEasy::PotentialRedirectUrl() {
    auto response_code = GetResponseCode();
    if ((response_code / 100) == 3) {
        const char *location;
        auto res = curl_easy_getinfo(handle, CURLINFO_REDIRECT_URL, &location);
        if (res == CURLE_OK && location != nullptr) {
            std::string str{location};
            return str;
        }
    }
    return {};
}
