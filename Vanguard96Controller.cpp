#include "Vanguard96Controller.h"
#include <chrono>
#include <thread>
#include <iostream>

Vanguard96Controller::Vanguard96Controller()
{
    if (hid_init() != 0)
    {
        std::cerr << "[Vanguard96] hid_init() failed\n";
    }
}

Vanguard96Controller::~Vanguard96Controller()
{
    close();
    hid_exit();
}

bool Vanguard96Controller::open()
{
    if (dev)
        return true;

    return openInternal();
}

bool Vanguard96Controller::openInternal()
{
    struct hid_device_info* devs = hid_enumerate(VENDOR_ID, PRODUCT_ID);
    if (!devs)
    {
        std::cerr << "[Vanguard96] No HID devices found\n";
        return false;
    }

    const hid_device_info* target = nullptr;
    for (auto* cur = devs; cur != nullptr; cur = cur->next)
    {
        // RGB-interfacet har usage_page 0xFF42
        if (cur->usage_page == 0xFF42)
        {
            target = cur;
            break;
        }
        if (!target)
            target = cur;
    }

    if (!target)
    {
        hid_free_enumeration(devs);
        std::cerr << "[Vanguard96] No suitable HID interface\n";
        return false;
    }

    dev = hid_open_path(target->path);
    hid_free_enumeration(devs);

    if (!dev)
    {
        std::cerr << "[Vanguard96] hid_open_path() failed\n";
        return false;
    }

    hid_set_nonblocking(dev, 1);
    return true;
}

void Vanguard96Controller::close()
{
    if (dev)
    {
        hid_close(dev);
        dev = nullptr;
    }
}

bool Vanguard96Controller::sendFrames(const std::vector<std::vector<uint8_t>>& frames,
                                      double delaySeconds)
{
    if (!dev && !open())
        return false;

    for (const auto& fr : frames)
    {
        std::vector<uint8_t> report;
        report.reserve(fr.size() + 1);
        report.push_back(0x00);  // report ID
        report.insert(report.end(), fr.begin(), fr.end());

        int res = hid_write(dev, report.data(), report.size());
        if (res < 0)
        {
            std::cerr << "[Vanguard96] Write error: "
                      << hid_error(dev) << "\n";
        }

        if (delaySeconds > 0.0)
        {
            auto ms = static_cast<int>(delaySeconds * 1000.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
    }

    return true;
}

std::vector<std::vector<uint8_t>> Vanguard96Controller::makeFrames(uint8_t r,
                                                                   uint8_t g,
                                                                   uint8_t b,
                                                                   uint8_t brightness)
{
    // BASE_FRAMES = samma sekvens som i ditt Python-script (finale.py)
    static const std::vector<std::vector<uint8_t>> BASE_FRAMES = {
        {0,1,0,2,225,0},
        {0,1,0,1,3,0,2,0,0,0},
        {0,1,0,13,0,96,109},
        {0,1,0,9,0},
        {0,1,0,2,225,0},
        {0,1,0,8,0},
        {0,1,0,5,1,0},
        {0,1,0,13,0,98,109},
        {0,1,0,9,0},
        {0,1,0,8,0},
        {0,1,0,5,1,0},
        {0,1,0,13,1,101,109},

        // FÄRGPACKET (index 12)
        {0,1,0,6,1,115,0,0,0,126,32,1,0,0,0,1,
         255,255,0,0,103,41,58,59,60,61,62,63,
         64,65,66,67,68,69,70,76,53,30,31,32,
         33,34,35,36,37,38,39,45,46,42,43,20,
         26,8,21,23,28,24,12,18,19,47,48,40,
         57,4,22,7,9,10,11,13,14,15,51,52,50,
         106,100,29,27,6,25,5,17,16,54,55,56,
         110,105,108,107,44,111,122,136,82,80,
         81,79,83,84,85,86,95,96,97,87,92,93,
         94,89,90,91,88,98,99,130,131,132,133,
         134,135},

        {0,1,0,5,1,1},
        {0,1,0,13,2,98,109},
        {0,1,0,6,2,8,0,0,0,105,108,1,0,8,0,101,109},
        {0,1,0,5,1,2},
        {0,1,0,1,3,0,2,0,0,0},
        {0,1,0,13,3,96,109},
        {0,1,0,9,3},
        {0,1,0,8,3},
        {0,1,0,5,1,3},
        {0,1,0,13,3,96,109},
        {0,1,0,6,3,42,0,0,0,30,241,81,145,118,105,0,0,
         97,109,0,0,98,109,99,109,0,0,100,109,0,0,0,0,
         0,0,0,0,0,0,1,0,49,0,0,0,0,0,0,0,102,109},
        {0,1,0,5,1,3},
        {0,1,0,1,3,0,1,0,0,0},
        {0,1,0,1,3,0,1,0,0,0}
    };

    std::vector<std::vector<uint8_t>> frames = BASE_FRAMES;

    auto& f = frames[12];  // färgpacket

    // Samma index som i Python: brightness, B, G, R
    f[16] = brightness;
    f[17] = b;
    f[18] = g;
    f[19] = r;

    return frames;
}

bool Vanguard96Controller::setGlobalColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    if (!dev && !open())
        return false;

    auto frames = makeFrames(r, g, b, brightness);
    return sendFrames(frames, 0.02);
}
