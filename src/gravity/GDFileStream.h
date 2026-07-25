#pragma once
#include <SD.h>
#include <cstdint>
#include <algorithm>

// SD-card replacement for desktop FileStream
class FileStream {
    File _f;
public:
    FileStream() {}
    explicit FileStream(const char* path) { _f = SD.open(path, FILE_READ); }
    ~FileStream() { if (_f) _f.close(); }

    bool isOpen() { return (bool)_f; }
    void setPos(long pos) { _f.seek(pos); }

    template<class T>
    void readVariable(T* p, bool swapEndianness = false, size_t sz = 0) {
        if (!sz) sz = sizeof(T);
        _f.read(reinterpret_cast<uint8_t*>(p), sz);
        if (swapEndianness) {
            uint8_t* b = reinterpret_cast<uint8_t*>(p);
            std::reverse(b, b + sz);
        }
    }
};
