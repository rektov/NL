#pragma once
#include <cstddef>

namespace nl::embed {

struct Blob {
    const unsigned char* data;
    size_t size;
};

Blob Avatar();
Blob Cs2();
Blob Csgo();
Blob Valorant();
Blob Apex();
Blob CheatsJson();

}
