#include <cstdint>

#ifndef SHARED_H
#define SHARED_H

#pragma pack(push, 1)
struct EtData
{
    float leftEyeOpenness;
    float rightEyeOpenness;

    float leftEyePupilDilation;
    float rightEyePupilDilation;

    float leftEyeMiddleCanthusUvX;
    float leftEyeMiddleCanthusUvY;
    float rightEyeMiddleCanthusUvX;
    float rightEyeMiddleCanthusUvY;
};

struct Message
{
    int id;
    uint32_t value;
    char text[32];
    EtData etData;
};
#pragma pack(pop)

#endif