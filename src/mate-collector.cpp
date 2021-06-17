#include "mate-collector.h"
#include "debug.h"

class MxCollector : public MateCollector
{
public:
    MxCollector(MateControllerDevice& dev, MatePubContext& context)
        : MateCollector(dev, context)
    { }

    void process() override
    {

    }
};

const char* dtype_strings[] = {
    nullptr,
    "hub",
    "fx",
    "mx",
    "dc"
};

void MateCollector::initialize()
{
    DeviceType dtype = dev.deviceType();
    assert(dtype < DeviceType::MaxDevices);

    m_deviceCounts[dtype]++;

    snprintf(m_prefix, sizeof(m_prefix), "%s/%s-%d", context.prefix, dtype_strings[dtype], m_deviceCounts[dtype]);
}

MateCollector* MateCollector::make_collector(DeviceType dtype)
{
    switch (dtype) {
        case DeviceType::Mx: return nullptr;
        case DeviceType::Fx: return nullptr;
        case DeviceType::Dc: return nullptr;
        default: return nullptr;
    }
}

void MateCollector::publishInfo()
{
    // Publish to topic:
    // mate/status [connected/disconnected]
    // mate/mx-1/rev    [Revision:a.b.c]
    // mate/mx-1/raw    [raw data]
    // mate/mx-1/ts     [timestamp of last data read]
    // mate/mx-1/port   [0-9]

    char payload[MQTT_MAX_PACKET_SIZE];

    snprintf(payload, sizeof(payload), "%d", dev.port());
    publishTopic("port", payload, true); // Retained

    auto rev = dev.get_revision();
    snprintf(payload, sizeof(payload), "%d.%d.%d", rev.a, rev.b, rev.c);
    publishTopic("rev", payload, true); // Retained

    ping(true);

    // Future: (HAComponent)
    // mate/mx-1/sensor/bat_voltage
    // mate/mx-1/sensor/bat_current
    // ...
}

void MateCollector::publishTopic(const char* topic_suffix, const char* payload, bool retained)
{
    char topic[MAX_TOPIC_LEN];
    snprintf(topic, sizeof(topic), "%s/%s", m_prefix, topic_suffix);

    Debug.print("Publish: ");
    Debug.print(topic);
    Debug.println();

    client.publish(topic, payload, retained);
}

void MateCollector::ping(bool initial_publish)
{
    // Check if device is still responding, and update status topic if needed.
    //bool is_connected       = dev.isConnected();
    bool is_still_connected = dev.begin(dev.port());
    if ((is_still_connected != this->is_connected) || initial_publish) {
        this->is_connected = is_still_connected;
        if (!is_still_connected) {
            Debug.println("Device disconnected");
            publishTopic("status", "offline", true); // Retained
        } else {
            Debug.println("Device connected");
            publishTopic("status", "online", true); // Retained
        }
    }
}