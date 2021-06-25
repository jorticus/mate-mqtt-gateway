#include <mate-collector.h>

void MxCollector::process(uint32_t now)
{ 
    static uint8_t status[STATUS_RESP_SIZE] = {0};
    static uint8_t logpage[LOG_RESP_SIZE] = {0};
    struct tm currTime;

    // Rollover-safe timestamp check
    if ((now - tPrevStatus) >= statusIntervalMs) {
        tPrevStatus = now;

        Debug.println("Collect MX Status");

        if (!getLocalTime(&currTime)) {
            Debug.println("Error retrieving current time");
            return; // Cannot publish.
        }

#ifdef FAKE_MATE_DEVICES   
        publishStatus(&currTime, status, sizeof(status));
#else
        if (dev.read_status(status, sizeof(status))) {
            // Debug.println("Status:");
            // for (int i = 0; i < sizeof(status); i++) {
            //     Debug.print(status[i], 16);
            // }
            // Debug.println();

            publishStatus(&currTime, status, sizeof(status));
        }
#endif
    }

    if ((now - tPrevLog) >= logIntervalMs) {
        tPrevLog = now;

        struct tm currTime;
        if (getLocalTime(&currTime)) {
            Serial.println(&currTime, "Time: %Y-%m-%d %H:%M:%S");

            // Next logpage timestamp not yet set, use current time
            if (nextLogpageTime.tm_year == 0) {
                setNextLogpage(&currTime);
                nextLogpageTime.tm_mday -= 1; // Force collection on first run
            }

            if ((currTime.tm_year >= nextLogpageTime.tm_year) &&
                (currTime.tm_mon >= nextLogpageTime.tm_mon) &&
                (currTime.tm_mday >= nextLogpageTime.tm_mday) &&
                (currTime.tm_hour >= nextLogpageTime.tm_hour) &&
                (currTime.tm_min >= nextLogpageTime.tm_min))
            {
                Debug.println("Collect MX Logpage");

#ifdef FAKE_MATE_DEVICES
                publishLog(&currTime, logpage, sizeof(logpage));
#else
                if (dev.read_log(logpage, sizeof(logpage))) {
                    publishLog(&currTime, logpage, sizeof(logpage));
                }
#endif

                setNextLogpage(&currTime);
                nextLogpageTime.tm_mday += 1;
            }
        }
    }
        
    // TODO: Retrieve logpage
}

void MxCollector::setNextLogpage(struct tm* currTime)
{
    nextLogpageTime = *currTime;

    // Tomorrow
    nextLogpageTime.tm_mday += 1;

    // 5 minutes past midnight
    nextLogpageTime.tm_hour = 0;
    nextLogpageTime.tm_min = 5;
    nextLogpageTime.tm_sec = 0;

    Serial.println(&nextLogpageTime, "Next Logpage: %Y-%m-%d %H:%M:%S");
}

void MxCollector::publishStatus(struct tm* currTime, uint8_t* status, size_t size)
{
    // mate/mx-1/stat/raw
    // mate/mx-1/stat/ts
    publishTopic("stat/raw", status, size, false);

    time_t t = mktime(currTime);
    char ts_str[20];
    snprintf(ts_str, sizeof(ts_str), "%lu", t);
    publishTopic("stat/ts", ts_str, false);
}

void MxCollector::publishLog(struct tm* currTime, uint8_t* log, size_t size)
{
    // mate/mx-1/log/raw
    // mate/mx-1/log/ts
}
