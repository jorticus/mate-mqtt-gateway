
#include <uMate.h>
#include <Serial9b.h>
#include <SoftwareSerial.h>

#include "mqtt.h"
#include "debug.h"
#include "allocator.h"
#include "mate-collector.h"

//#define DEBUG_COMMS

// MATEnet bus is connected to a software serial port (9-bit)
extern SoftwareSerial Serial9b; // main.cpp
#ifdef DEBUG_COMMS
MateControllerProtocol mate_bus(Serial9b, &Debug);
#else
MateControllerProtocol mate_bus(Serial9b);
#endif

extern MatePubContext mate_context;

// Functional devices found on the bus (excludes the hub)
std::array<MateControllerDevice*, NUM_MATE_PORTS> devices;
std::array<MateCollector*, NUM_MATE_PORTS> collectors;
size_t num_devices = 0;

// Set up a fixed size pool for allocating MateControllerDevice instances.
// This avoids malloc()
fixed_pool_allocator<MateControllerDevice, NUM_MATE_PORTS> device_pool;
fixed_pool_allocator<MateCollector, NUM_MATE_PORTS> collector_pool;

const char* dtypes[] = {
    "None",
    "Hub",
    "FX",
    "MX",
    "DC"
};

void print_dtype(DeviceType dtype) {
    if (dtype < DeviceType::MaxDevices) {
        Serial.print(dtypes[dtype]);
        Serial.print(" ");
    } else {
        Serial.print(dtype);
    }
}

void print_revision(MateControllerDevice& device)
{
    auto rev = device.get_revision();
    Debug.print("(Rev:");
    Debug.print(rev.a); Debug.print("."); 
    Debug.print(rev.b); Debug.print(".");
    Debug.print(rev.c); Debug.print(")");
}

void create_device(int port, DeviceType dtype)
{
    Debug.print(port);
    Debug.print(": ");
    print_dtype(dtype);
    
    MateControllerDevice* device = new(device_pool) MateControllerDevice(mate_bus, dtype);
    if (device != nullptr) {
        MateCollector* collector = new(collector_pool) MateCollector(*device, mate_context);
        if (collector != nullptr) {
            #ifdef DEBUG_COMMS
            Debug.println();
            #endif

            if (device->begin(port)) {
                print_revision(*device);
                devices[num_devices] = device;
                collectors[num_devices] = collector;
                num_devices++;
            }
        } 
    }
    Debug.println();
}

// Scan the MateNET bus for new devices.
void scan()
{
    DeviceType dtype;

    num_devices = 0;
    device_pool.deallocate_all(); // TODO: Call destructors

    // Force re-scan of bus.
    // Any future calls to scan() will use cached devices.
    Debug.println("Scanning for MATE devices...");
    mate_bus.scan_ports();

    // Port 0 must be either a hub or a device.
    // If nothing responds to this, we don't have a valid network
    dtype = mate_bus.scan(0);
    if (dtype == DeviceType::None) {
        Debug.println("ERROR: No devices found!");
        return;
    }

    // If a hub is present, we can scan for additional devices
    if (dtype == DeviceType::Hub) {
        Debug.print("0: ");
        print_dtype(dtype);
        Debug.println();

        for (int i = 1; i < NUM_MATE_PORTS; i++) {
            dtype = mate_bus.scan(i);
            if (dtype != DeviceType::None) {
                create_device(i, dtype);
            }
        }
    }
    // If port 0 is not a hub, then there can only be one device on the network.
    else {
        create_device(0, dtype);
    }

    if (num_devices == 0) {
        Debug.println("ERROR: No devices created!");
        return;
    }

    for (int i = 0; i < num_devices; i++) {
        auto collector = collectors[i];
        assert(collector != nullptr);
        collector->publishInfo();
    }
}

namespace MateAggregator {

void setup()
{
    scan();
}

void loop()
{
    // TODO: Periodically query status from each attached device

    if (num_devices > 0) {
        for (int i = 0; i < num_devices; i++) {
            auto collector = collectors[i];
            assert(collector != nullptr);
            collector->process();
        }
    }

}

};