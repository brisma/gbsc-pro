// Who feeds the Updater (pure): a web upload or an OTA pull. Both share one state, so each refuses the other; only an
// upload makes other web requests busy (its client sends the image)
#pragma once
#include <stdint.h>

enum UpdateSource : uint8_t { US_NONE, US_UPLOAD, US_PULL };
