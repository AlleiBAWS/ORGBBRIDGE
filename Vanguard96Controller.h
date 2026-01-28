#ifndef VANGUARD96CONTROLLER_H
#define VANGUARD96CONTROLLER_H

#include <vector>
#include <cstdint>
#include <hidapi/hidapi.h>

class Vanguard96Controller
{
public:
    static constexpr uint16_t VENDOR_ID  = 0x1B1C;
    static constexpr uint16_t PRODUCT_ID = 0x2B0D;

    Vanguard96Controller();
    ~Vanguard96Controller();

    bool open();
    void close();
    bool isOpen() const { return dev != nullptr; }

    // Sätter global färg på hela bordet
    bool setGlobalColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness = 255);

private:
    hid_device* dev = nullptr;

    bool openInternal();
    bool sendFrames(const std::vector<std::vector<uint8_t>>& frames, double delaySeconds = 0.02);
    std::vector<std::vector<uint8_t>> makeFrames(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
};

#endif // VANGUARD96CONTROLLER_H
