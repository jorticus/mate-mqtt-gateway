#include <mate-collector.h>

void FxCollector::process(uint32_t now)
{ 
    static uint8_t status[STATUS_RESP_SIZE] = {0};
    struct tm currTime;

    // Rollover-safe timestamp check
    if ((now - tPrevStatus) >= statusIntervalMs) {
        tPrevStatus = now;
        Debug.println("Collect FX Status");

        if (!getLocalTime(&currTime)) {
            Debug.println("Error retrieving current time");
            return; // Cannot publish.
        }

#ifdef FAKE_MATE_DEVICES
        publishStatus(&currTime, status, sizeof(status));
#else
        if (dev.read_status(status, sizeof(status))) {
            publishStatus(&currTime, status, sizeof(status));
        }
#endif
    }
}

typedef struct {
    time_t  timestamp;
    uint8_t status[STATUS_RESP_SIZE];
} FxStatusMqttPayload;

void FxCollector::publishStatus(struct tm* currTime, uint8_t* status, size_t size)
{
    // mate/fx-1/stat/raw
    // mate/fx-1/stat/ts

    StatusMqttPayload payload;

    if (size > sizeof(payload.status))
        size = sizeof(payload.status);

    payload.timestamp = mktime(currTime);
    memcpy(payload.status, status, sizeof(payload.status));
    

    publishTopic("fx-status", 
        reinterpret_cast<uint8_t*>(&payload), 
        sizeof(payload), 
        false);
 
    // TODO: Deprecate
    publishTopic("stat/raw", status, size, false);

    char ts_str[20];
    snprintf(ts_str, sizeof(ts_str), "%lu", payload.timestamp);
    // TODO: Deprecate
    publishTopic("stat/ts", ts_str, false);
}
