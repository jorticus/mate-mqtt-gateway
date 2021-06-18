#include <mate-collector.h>

void MxCollector::process(uint32_t now)
{ 
    // Rollover-safe timestamp check
    if ((now - tPrevStatus) >= statusIntervalMs) {
        Debug.println("Collect Status");

        uint8_t status[STATUS_RESP_SIZE];
        if (dev.read_status(status, sizeof(status))) {
            // Debug.println("Status:");
            // for (int i = 0; i < sizeof(status); i++) {
            //     Debug.print(status[i], 16);
            // }
            // Debug.println();

            publishStatus(now, status, sizeof(status));
        }

        tPrevStatus = now;
        Debug.print("ts:");
        Debug.println(now);
    }

    // TODO: Retrieve logpage
}

void MxCollector::publishStatus(uint32_t now, uint8_t* status, size_t size)
{
    // mate/mx-1/stat/raw
    // mate/mx-1/stat/ts
    publishTopic("stat/raw", status, size, false);

    // TODO: Real timestamp
    char ts_str[20];
    snprintf(ts_str, sizeof(ts_str), "%u", now);
    publishTopic("stat/ts", ts_str, false);
}

void MxCollector::publishLog(uint32_t now, uint8_t* log, size_t size)
{
    // mate/mx-1/log/raw
    // mate/mx-1/log/ts
}
