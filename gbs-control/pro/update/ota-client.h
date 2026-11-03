// One plain HTTP GET at a time for the OTA pull (device only). ESPAsyncTCP AsyncClient: DNS, connect and the body
// run in the network context, so the loop never stalls; the loop only drives the timeouts (OtaPull) and owns the
// client's lifetime
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "http-parse.h"

// Results beyond the HTTP_* codes of http-parse.h
enum : int { OTA_E_DNS = -10, OTA_E_CONNECT = -11, OTA_E_TIMEOUT = -12, OTA_E_SHORT = -13, OTA_E_URL = -15 };

// Starts a GET of an http:// URL; the body (Content-Length <= maxBody) goes to sink in the network context.
// false: a fetch already runs, the URL is not plain http, or no memory
bool otaFetchStart(const char* url, uint32_t maxBody, HttpSink sink, void* ctx);
void otaFetchLoop();                  // loop(): timeouts, completion, client teardown
bool otaFetchBusy();
int otaFetchResult();                 // after the fetch: HTTP_DONE, an HTTP_E_* or an OTA_E_* code
const char* otaFetchReason();         // the failure as a reason code for otaReasonWords ("" after HTTP_DONE)
