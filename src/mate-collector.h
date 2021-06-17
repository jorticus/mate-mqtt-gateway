#pragma once

#include <PubSubClient.h>
#include <uMate.h>

#include "debug.h"

#define MAX_TOPIC_LEN (40)

class MatePubContext {
public:
    MatePubContext(PubSubClient& client)
        : client(client)
    { }

    PubSubClient& client;
    const char* prefix;         // eg. 'mate'
    const char* device_name;    // eg. 'mate'
};

class MateCollector {
public:
    MateCollector(MateControllerDevice& dev, MatePubContext& context)
        : dev(dev)
        , context(context)
        , client(context.client)
        , m_deviceCounts{0}
    {
        // TODO: Make static in class
        const char* dtypes[] = {
            nullptr,
            "hub",
            "fx",
            "mx",
            "dc"
        };

        DeviceType dtype = dev.deviceType();
        if (dtype >= DeviceType::MaxDevices)
            return;

        m_deviceCounts[dtype]++;

        snprintf(m_prefix, sizeof(m_prefix), "%s/%s-%d", context.prefix, dtypes[dtype], m_deviceCounts[dtype]);
    }

    void publish() {
        // Publish to topic:
        // mate/status [connected/disconnected]
        // mate/mx-1/rev    [Revision:a.b.c]
        // mate/mx-1/raw    [raw data]
        // mate/mx-1/ts     [timestamp of last data read]
        // mate/mx-1/port   [0-9]

        char payload[MQTT_MAX_PACKET_SIZE];

        auto rev = dev.get_revision();
        snprintf(payload, sizeof(payload), "%d.%d.%d", rev.a, rev.b, rev.c);
        publish_mqtt("rev", payload);

        snprintf(payload, sizeof(payload), "%d", dev.port());
        publish_mqtt("port", payload);

        // Future: (HAComponent)
        // mate/mx-1/sensor/bat_voltage
        // mate/mx-1/sensor/bat_current
        // ...
    }


    void process() {


        //client.publish()
    }

protected:
    void publish_mqtt(const char* topic_suffix, const char* payload) {
        char topic[MAX_TOPIC_LEN];
        snprintf(topic, sizeof(topic), "%s/%s", m_prefix, topic_suffix);

        Debug.print("Publish: ");
        Debug.print(topic);
        Debug.println();

        client.publish(topic, payload);
    }

    void connect() {

    }

    void ping() {
        // Check if device is still responding
        // If not, update status topic (disconnected).
    }

protected:
    MateControllerDevice& dev;
    MatePubContext& context;
    PubSubClient& client;

    char m_prefix[MAX_TOPIC_LEN];
    std::array<uint8_t, (size_t)DeviceType::MaxDevices> m_deviceCounts;
};
