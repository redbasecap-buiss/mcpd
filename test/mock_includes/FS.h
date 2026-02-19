/**
 * Mock FS.h for native testing
 */
#ifndef MOCK_FS_H
#define MOCK_FS_H

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "Arduino.h"

namespace fs {

class File {
public:
    File() : _valid(false), _isDir(false), _pos(0), _dirIdx(0) {}
    File(const std::string& name, const std::string& content, bool isDir = false)
        : _name(name), _content(content), _valid(true), _isDir(isDir), _pos(0), _dirIdx(0) {}

    operator bool() const { return _valid; }
    const char* name() const { return _name.c_str(); }
    size_t size() const { return _content.size(); }
    bool isDirectory() const { return _isDir; }
    int available() { return (_pos < _content.size()) ? (_content.size() - _pos) : 0; }
    int read() {
        if (_pos < _content.size()) return (uint8_t)_content[_pos++];
        return -1;
    }
    size_t print(const char* s) {
        size_t len = strlen(s);
        _content += std::string(s, len);
        return len;
    }
    void close() {}

    // For directory iteration
    std::vector<File> _children;
    File openNextFile() {
        if (_dirIdx < _children.size()) return _children[_dirIdx++];
        return File();
    }

private:
    std::string _name;
    std::string _content;
    bool _valid;
    bool _isDir;
    size_t _pos;
    size_t _dirIdx;
};

class FS {
public:
    File open(const char* path, const char* mode = "r") {
        std::string p(path);
        if (mode[0] == 'w' || mode[0] == 'a') {
            if (mode[0] == 'w') _files[p] = "";
            if (_files.find(p) == _files.end()) _files[p] = "";
            // Return writable file
            return File(p, _files[p]);
        }
        // Check if it's the root directory
        if (p == "/") {
            File dir(p, "", true);
            for (auto& kv : _files) {
                dir._children.push_back(File(kv.first, kv.second));
            }
            return dir;
        }
        auto it = _files.find(p);
        if (it == _files.end()) return File();
        return File(it->first, it->second);
    }

    File open(const String& path, const char* mode = "r") {
        return open(path.c_str(), mode);
    }

    bool exists(const char* path) {
        return _files.find(std::string(path)) != _files.end();
    }

    bool exists(const String& path) { return exists(path.c_str()); }

    bool remove(const char* path) {
        auto it = _files.find(std::string(path));
        if (it != _files.end()) { _files.erase(it); return true; }
        return false;
    }

    bool remove(const String& path) { return remove(path.c_str()); }

    // For write — capture written content properly
    void _storeWrite(const char* path, const char* content, bool append) {
        std::string p(path);
        if (append && _files.find(p) != _files.end()) {
            _files[p] += content;
        } else {
            _files[p] = content;
        }
    }

    std::map<std::string, std::string> _files;
};

} // namespace fs

// SPIFFS mock
static fs::FS SPIFFS;

#endif // MOCK_FS_H
