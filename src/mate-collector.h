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

    void publishInfo();

    virtual void process(uint32_t now)
    { }

protected:
    void initialize();

    void publishTopic(const char* topic_suffix, const char* payload, bool retained);
    void publishTopic(const char* topic_suffix, const uint8_t* payload, size_t payload_size, bool retained);
    void ping(bool initial_publish);

protected:
    MateControllerDevice& dev;
    MatePubContext& context;
    PubSubClient& client;

    char m_prefix[MAX_TOPIC_LEN];
    std::array<uint8_t, (size_t)DeviceType::MaxDevices> m_deviceCounts;
    bool is_connected;
};

class FxCollector : public MateCollector
{
public:
    FxCollector(MateControllerDevice& dev, MatePubContext& context)
        : MateCollector(dev, context)
        , tPrevStatus(0)
    { }

    void process(uint32_t now) override;

protected:
    void publishStatus(uint32_t now, uint8_t* status, size_t size);

    static const uint statusIntervalMs = 5000; //ms
    uint tPrevStatus;
};

class MxCollector : public MateCollector
{
public:
    MxCollector(MateControllerDevice& dev, MatePubContext& context)
        : MateCollector(dev, context)
        , tPrevStatus(0)
    { }

    void process(uint32_t now) override;

protected:
    void publishStatus(uint32_t now, uint8_t* status, size_t size);

    void publishLog(uint32_t now, uint8_t* log, size_t size);

    static const uint statusIntervalMs = 5000; //ms
    //static const uint logIntervalMs = 0; 
    uint tPrevStatus;
    //uint tPrevLog;
};

class DcCollector : public MateCollector
{
public:
    DcCollector(MateControllerDevice& dev, MatePubContext& context)
        : MateCollector(dev, context)
        , tPrevStatus(0)
    { }

    void process(uint32_t now) override;

protected:
    void publishStatus(uint32_t now, uint8_t* status, size_t size);

    static const uint statusIntervalMs = 5000; //ms
    uint tPrevStatus;
};

// By inheriting from each class, we can figure out how much memory
// would be required to allocate each one.
class MateCollectorContainer : public MxCollector//, public MateCollector
{
public:

private:
    MateCollectorContainer() = delete;
};

static_assert(sizeof(MateCollector) <= sizeof(MateCollectorContainer), "MxCollector size exceeds available container");
static_assert(sizeof(MxCollector) <= sizeof(MateCollectorContainer), "MxCollector size exceeds available container");
//static_assert(sizeof(MateCollectorContainer) <= (sizeof(MxCollector) + sizeof(FxCollector)), "MateCollectorContainer size is larger than expected");