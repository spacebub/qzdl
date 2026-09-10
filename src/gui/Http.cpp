/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <fstream>
#include <utility>

#include "gui/Http.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
#endif

namespace {

const char *AGENT = "qzdl/" QZDL_VERSION;

struct Sink {
    std::ofstream file;
    std::string held;

    bool write(const char *data, const size_t size) {
        if (file.is_open()) {
            file.write(data, static_cast<std::streamsize>(size));

            return static_cast<bool>(file);
        }

        held.append(data, size);

        return true;
    }
};

#ifdef _WIN32

std::wstring widen(const std::string &value) {
    const int wide = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(wide > 0 ? wide - 1 : 0), L'\0');

    if (wide > 1) {
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), wide);
    }

    return out;
}

struct Handle {
    HINTERNET value{nullptr};

    ~Handle() {
        if (value != nullptr) {
            WinHttpCloseHandle(value);
        }
    }

    explicit operator bool() const { return value != nullptr; }
};

std::string lastError() {
    return "The connection failed (" + std::to_string(GetLastError()) + ")";
}

#else

size_t take(const char *data, const size_t size, const size_t count, void *into) {
    return static_cast<Sink *>(into)->write(data, size * count) ? size * count : 0;
}

#endif

}

#ifdef _WIN32

void Http::start() {
}

void Http::stop() {
}

#else

void Http::start() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void Http::stop() {
    curl_global_cleanup();
}

#endif

Http::Fetch::Fetch(std::string url, const bool json, std::filesystem::path into)
    : _url(std::move(url)), _json(json), _into(std::move(into)) {
    _thread = std::thread([this] { work(); });
}

Http::Fetch::~Fetch() {
    _cancelled.store(true);

    if (_thread.joinable()) {
        _thread.join();
    }
}

std::string Http::Fetch::error() const {
    const std::scoped_lock hold(_guard);

    return _error;
}

std::string Http::Fetch::body() const {
    const std::scoped_lock hold(_guard);

    return _body;
}

#ifdef _WIN32

void Http::Fetch::work() {
    Sink sink;

    if (!_into.empty()) {
        sink.file.open(_into, std::ios::binary | std::ios::trunc);

        if (!sink.file) {
            const std::scoped_lock hold(_guard);

            _error = "Could not write to " + _into.string();
            _done.store(true);

            return;
        }
    }

    const std::wstring url = widen(_url);
    URL_COMPONENTS parts = {};
    wchar_t host[256] = {};
    wchar_t path[4096] = {};

    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = std::size(host);
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = std::size(path);

    std::string trouble;

    const auto give = [this, &trouble] {
        const std::scoped_lock hold(_guard);

        _error = trouble;
        _done.store(true);
    };

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) {
        trouble = "That address makes no sense";
        give();

        return;
    }

    const Handle session{WinHttpOpen(widen(AGENT).c_str(),
                                     WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};

    if (!session) {
        trouble = lastError();
        give();

        return;
    }

    WinHttpSetTimeouts(session.value, STALL_SECONDS * 1000, STALL_SECONDS * 1000,
                       STALL_SECONDS * 1000, STALL_SECONDS * 1000);

    const Handle connection{WinHttpConnect(session.value, host, parts.nPort, 0)};

    if (!connection) {
        trouble = lastError();
        give();

        return;
    }

    const Handle request{WinHttpOpenRequest(connection.value, L"GET", path, nullptr,
                                            WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            parts.nScheme == INTERNET_SCHEME_HTTPS
                                                ? WINHTTP_FLAG_SECURE : 0)};

    if (!request) {
        trouble = lastError();
        give();

        return;
    }

    const wchar_t *headers = _json ? L"Accept: application/vnd.github+json\r\n"
                                   : WINHTTP_NO_ADDITIONAL_HEADERS;

    if (!WinHttpSendRequest(request.value, headers, headers == nullptr ? 0 : -1L,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        || !WinHttpReceiveResponse(request.value, nullptr)) {
        trouble = lastError();
        give();

        return;
    }

    DWORD code = 0;
    DWORD size = sizeof(code);

    WinHttpQueryHeaders(request.value,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX);

    _status.store(static_cast<int>(code));

    long long whole = 0;
    wchar_t length[32] = {};

    size = sizeof(length);

    if (WinHttpQueryHeaders(request.value, WINHTTP_QUERY_CONTENT_LENGTH,
                            WINHTTP_HEADER_NAME_BY_INDEX, length, &size,
                            WINHTTP_NO_HEADER_INDEX)) {
        whole = _wtoll(length);
    }

    std::string chunk(64 * 1024, '\0');
    long long done = 0;

    while (!_cancelled.load()) {
        DWORD read = 0;

        if (!WinHttpReadData(request.value, chunk.data(), static_cast<DWORD>(chunk.size()),
                             &read)) {
            trouble = lastError();
            give();

            return;
        }

        if (read == 0) {
            break;
        }

        if (!sink.write(chunk.data(), read)) {
            trouble = "Could not keep what came down";
            give();

            return;
        }

        done += read;

        if (whole > 0) {
            _progress.store(static_cast<double>(done) / static_cast<double>(whole));
        }
    }

    if (_cancelled.load()) {
        trouble = "Stopped";
    } else if (code >= 400) {
        trouble = "The other end answered " + std::to_string(code);
    }

    sink.file.close();

    const std::scoped_lock hold(_guard);

    _error = trouble;
    _body = std::move(sink.held);
    _done.store(true);
}

#else

void Http::Fetch::work() {
    Sink sink;

    if (!_into.empty()) {
        sink.file.open(_into, std::ios::binary | std::ios::trunc);

        if (!sink.file) {
            const std::scoped_lock hold(_guard);

            _error = "Could not write to " + _into.string();
            _done.store(true);

            return;
        }
    }

    CURL *handle = curl_easy_init();

    if (handle == nullptr) {
        const std::scoped_lock hold(_guard);

        _error = "There is no way out to the network from here";
        _done.store(true);

        return;
    }

    curl_slist *headers = nullptr;

    if (_json) {
        headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    }

    curl_easy_setopt(handle, CURLOPT_URL, _url.c_str());
    curl_easy_setopt(handle, CURLOPT_USERAGENT, AGENT);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, STALL_SECONDS);
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, STALL_SECONDS);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, take);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, this);

    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION,
                     +[](void *owner, const curl_off_t whole, const curl_off_t done,
                         curl_off_t, curl_off_t) {
                         auto *fetch = static_cast<Fetch *>(owner);

                         if (whole > 0) {
                             fetch->_progress.store(static_cast<double>(done)
                                                    / static_cast<double>(whole));
                         }

                         return fetch->_cancelled.load() ? 1 : 0;
                     });

    char said[CURL_ERROR_SIZE] = {};

    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, said);

    const CURLcode result = curl_easy_perform(handle);
    long code = 0;

    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &code);
    _status.store(static_cast<int>(code));

    std::string trouble;

    if (result == CURLE_ABORTED_BY_CALLBACK) {
        trouble = "Stopped";
    } else if (result != CURLE_OK) {
        trouble = said[0] != '\0' ? said : curl_easy_strerror(result);
    } else if (code >= 400) {
        trouble = "The other end answered " + std::to_string(code);
    }

    if (headers != nullptr) {
        curl_slist_free_all(headers);
    }

    curl_easy_cleanup(handle);
    sink.file.close();

    const std::scoped_lock hold(_guard);

    _error = trouble;
    _body = std::move(sink.held);
    _done.store(true);
}

#endif
