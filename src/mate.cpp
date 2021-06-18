
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
MateControllerDevice* mx_master = nullptr;
size_t num_devices = 0;

// Set up a fixed size pool for allocating MateControllerDevice instances.
// This avoids malloc()
fixed_pool_allocator<MateControllerDevice, NUM_MATE_PORTS> device_pool;
fixed_pool_allocator<MateCollectorContainer, NUM_MATE_PORTS> collector_pool;

static uint32_t tPrevSync = 0;
static const uint32_t syncIntervalMs = 60000; // Period to synchronize devices

extern const char* dtype_strings[];
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
    
    // Create a device object for interacting with the device type
    MateControllerDevice* device = new(device_pool) MateControllerDevice(mate_bus, dtype);
    if (device != nullptr) {

        // Create an appropriate wrapper class for the device type
        MateCollector* collector = nullptr;
        switch (dtype) {
            case DeviceType::Mx: collector = new (collector_pool) MxCollector(*device, mate_context); break;
            case DeviceType::Fx: collector = new (collector_pool) FxCollector(*device, mate_context); break;
            case DeviceType::Dc: collector = new (collector_pool) DcCollector(*device, mate_context); break;
            default: break;
        }
        //MateCollector* collector = new(collector_pool) MateCollector(*device, mate_context);

        if (collector != nullptr) {
            #ifdef DEBUG_COMMS
            Debug.println();
            #endif

            // Check that we can communicate and add it to our list
            if (device->begin(port)) {
                print_revision(*device);
                devices[num_devices] = device;
                collectors[num_devices] = collector;
                num_devices++;

                // First MX present is the master
                if ((dtype == DeviceType::Mx) && (mx_master == nullptr)) {
                    mx_master = device;
                }
            }
        } 
    }
    Debug.println();
}

void clearPublishedDevices()
{
#if 0
    // TODO: To do this properly, we really need to subscribe to all topics
    // so we have a list of retained devices, then unpublish each individually.
    // If we just blindly publish a 0-byte payload, any topics that don't exist
    // will be created (un-retained). Unsure if this is a bug in my broker.
    for (int dtype = DeviceType::Fx; dtype < DeviceType::MaxDevices; dtype++) {
        for (int i = 1; i <= 3; i++) {
            char topic[40];
            snprintf(topic, sizeof(topic), "%s/%s-%d/status", 
                mate_context.prefix,
                dtype_strings[dtype],
                i
            );

            Debug.print("Unpublish ");
            Debug.println(topic);
            mate_context.client.publish(topic, nullptr, 0, true);
        }
    }
#endif
}

// Scan the MateNET bus for new devices.
void scan()
{
    DeviceType dtype;

    num_devices = 0;
    mx_master = nullptr;
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

    // Publish initial (retained) info to MQTT, such as device revision & port
    for (int i = 0; i < num_devices; i++) {
        auto collector = collectors[i];
        assert(collector != nullptr);
        collector->publishInfo();
    }
}

void synchronize()
{
    struct tm timeinfo;
    uint16_t bat_temp = 0;

    if (mx_master != nullptr) {

        Debug.println("Synchronize...");
        
        if (!getLocalTime(&timeinfo)) {
            Debug.println("Sync: Error retrieving current time");
            return; // Cannot synchronize.
        }

        Serial.println(&timeinfo, "Time: %Y-%m-%d %H:%M:%S");

        for (int i = 0; i < num_devices; i++) {
            auto device = devices[i];
            if (device != mx_master) {
                DeviceType dtype = device->deviceType();

                // MX & DC devices want to know the current time for scheduling purposes
                if (dtype == DeviceType::Mx || dtype == DeviceType::Dc) {
                    device->update_time(&timeinfo);
                }

                // FX & DC devices want to know the battery temp reported by the MX master
                if (dtype == DeviceType::Fx || dtype == DeviceType::Dc) {
                    if (bat_temp == 0) {
                        bat_temp = mx_master->query(0x4000);
                        Debug.print( "Bat Temp: ");
                        Debug.println(bat_temp);
                    }
                    device->update_battery_temperature(bat_temp);
                }
            }
        }
    }
}

namespace MateAggregator {

void setup()
{
    clearPublishedDevices();
    scan();
}

void loop()
{
    // TODO: Periodically query status from each attached device

    if (num_devices > 0) {
        uint32_t now = static_cast<uint32_t>(millis());

        // Collect status / log information for each attached device
        for (int i = 0; i < num_devices; i++) {
            auto collector = collectors[i];
            assert(collector != nullptr);
            collector->process(now);
        }

        // Synchronize devices
        if ((now - tPrevSync) >= syncIntervalMs) {
            tPrevSync = now;
            synchronize();
        }
    }

}

};