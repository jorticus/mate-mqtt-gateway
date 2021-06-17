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
        , is_connected(false)
    {
        initialize();
    }

    static MateCollector* make_collector(DeviceType dtype);

    void publishInfo();

    virtual void process()
    { }

protected:
    void initialize();

    void publishTopic(const char* topic_suffix, const char* payload, bool retained);

    void ping(bool initial_publish);

protected:
    MateControllerDevice& dev;
    MatePubContext& context;
    PubSubClient& client;

    char m_prefix[MAX_TOPIC_LEN];
    std::array<uint8_t, (size_t)DeviceType::MaxDevices> m_deviceCounts;
    bool is_connected;
};
