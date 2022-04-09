#include <mate-collector.h>

void DcCollector::process(uint32_t now)
{ 
    // A DC status packet consists of 6 individual status packets
    static uint8_t status[STATUS_RESP_SIZE * 6] = {0};
    struct tm currTime;

    // Rollover-safe timestamp check
    if ((now - tPrevStatus) >= statusIntervalMs) {
        Debug.println("Collect DC Status");

        if (!getLocalTime(&currTime)) {
            Debug.println("Error retrieving current time");
            return; // Cannot publish.
        }

#ifndef FAKE_MATE_DEVICES
        uint8_t* curr_status = status;
        for (int i = 0x0A; i <= 0x0F; i++) {
            if (!dev.read_status(curr_status, STATUS_RESP_SIZE, i)) {
                Debug.println("ERROR: Cannot read complete DC status");
                return;
            }

            curr_status += STATUS_RESP_SIZE;
        }
#endif

        publishStatus(&currTime, status, sizeof(status));

        tPrevStatus = now;
    }
}

void DcCollector::publishStatus(struct tm* currTime, uint8_t* status, size_t size)
{
    // mate/dc-1/stat/raw
    // mate/dc-1/stat/ts
    DcStatusMqttPayload payload;

    if (size > sizeof(payload.status))
        size = sizeof(payload.status);

    payload.timestamp = mktime(currTime);
    memcpy(payload.status, status, sizeof(payload.status));
    
    publishTopic("dc-status", 
        reinterpret_cast<uint8_t*>(&payload), 
        sizeof(payload), 
        false);

    // TODO: Deprecate
    publishTopic("stat/raw", status, size, false);

    // TODO: Deprecate
    char ts_str[20];
    snprintf(ts_str, sizeof(ts_str), "%lu", payload.timestamp);
    publishTopic("stat/ts", ts_str, false);
}
