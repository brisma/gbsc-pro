/* PersWiFiManager
   version 3.0.1
   https://r-downing.github.io/PersWiFiManager/

   modified for inclusion in gbs-control
   see /3rdparty/PersWiFiManager/ for original code and license
*/
#if defined(ESP8266)
#include "PersWiFiManager.h"

// #define WIFI_HTM_PROGMEM
// #ifdef WIFI_HTM_PROGMEM
// static const char wifi_htm[] PROGMEM = R"=====(<!DOCTYPE html><html> <head> <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no" /> <title>ESP WiFi</title> <script> function setSSID(ssid) { document.getElementById("s").value = ssid; document.getElementById("wl").style.display = "none"; } function scan() { const request = new XMLHttpRequest(); const wl = document.getElementById("wl"); document.getElementById("wl").style.display = "block"; wl.innerHTML = "Scanning..."; request.onreadystatechange = function () { if (this.readyState == 4 && this.status == 200) { wl.innerHTML = "Scanning..."; const lines = this.responseText.split("\n"); wl.innerHTML = "<ul>" + lines .map((line) => { const [, status, ssid] = line.split(","); return `<li onclick="setSSID('${ssid}')"><span>${ status === "1" ? " \uD83D\uDD12" : " \u26A0" }</span> ${ssid}</li>`; }) .join("") + "</ul>"; } }; request.open("GET", "wifi/list", true); request.send(); } </script> <style> body { background-color: #202020; text-align: center; font-family: verdana; color: #00c0fb; } a, #wl { text-decoration: none; color: #00c0fb; font-size: 14px; } a { cursor: pointer; } .btn, input { box-sizing: border-box; width: 100%; line-height: 34px; appearance: none; background-color: #292929; border-radius: 4px; border: 1px dashed rgba(0, 192, 251, 0.2); color: rgba(0, 192, 251, 0.6); cursor: pointer; font-family: inherit; font-size: 14px; font-weight: 300; margin: 0 0 8px 0; outline: 0; overflow: hidden; padding: 4px 8px; user-select: none; transition: all 0.2s linear; } .btn[active], .btn:hover { color: #00c0fb; border: 1px solid #00c0fb; box-shadow: inset 0 0 6px 4px rgba(0, 192, 251, 0.2), 0 0 6px 4px rgba(0, 192, 251, 0.2); } .btn.secondary { color: rgba(234, 182, 56, 0.7); border: 1px dashed rgba(234, 182, 56, 0.3); } .btn.secondary[active], .btn.secondary:hover { transform: scale(0.98); color: black; box-shadow: inset 0 0 6px 4px rgba(234, 182, 56, 0.2), 0 0 6px 8px rgba(234, 182, 56, 0.2); background-color: rgba(234, 182, 56, 0.7); } .container { text-align: left; display: inline-block; max-width: 320px; padding: 5px; } .mb-16 { margin-bottom: 16px; } ul { list-style-type: none; padding: 0; } li { border-radius: 8px; padding: 6px; cursor: pointer; padding-left: 8px; padding-right: 8px; border: 1px solid transparent; } li:hover { border: 1px solid #00c0fb; } </style> </head> <body> <div class="container"> <button onclick="scan()" class="btn" active>&#x21bb; Scan</button> <p id="wl"></p> <form method="post" action="/wifi/connect"> <input id="s" name="n" length="32" placeholder="SSID" /> <input id="p" name="p" length="64" type="password" placeholder="password" class="mb-16" /> <button class="btn secondary" type="submit" active>Connect</button> </form> <div> <a href="javascript:history.back()">Back</a> | <a href="/">Home</a> </div> </div> </body></html>)=====";
// #endif

extern const char *device_hostname_full;
extern const char *device_hostname_partial;

PersWiFiManager::PersWiFiManager(AsyncWebServer &s, DNSServer &d)
{
    _server = &s;
    _dnsServer = &d;
    _apPass = "";
    _lastConnectionCheck = 0;
    _connectionLostTime = 0;
} //PersWiFiManager

bool PersWiFiManager::attemptConnection(const String &ssid, const String &pass)
{
    //attempt to connect to wifi
    // Configure WiFi settings to avoid ESP8266 Core 3.1.x bug
    // Enable persistence only when explicitly saving new credentials
    if (ssid.length() > 0) {
        WiFi.persistent(true);  // Save new credentials to flash
    } else {
        WiFi.persistent(false); // Use saved credentials without re-writing
    }
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(true);
    
    // Ensure WiFi hardware is fully awake and ready
    WiFi.forceSleepWake();
    delay(100);
    
    // Properly disconnect before mode change
    WiFi.disconnect(false); // false = don't erase credentials
    delay(200); // Increased delay for stability
    WiFi.mode(WIFI_STA);
    delay(200); // Increased delay for stability
    WiFi.hostname(device_hostname_partial); // _full // before WiFi.begin();
    delay(100); // Give time for hostname to be set
    
    if (ssid.length()) {
        if (pass.length())
            WiFi.begin(ssid.c_str(), pass.c_str());
        else
            WiFi.begin(ssid.c_str());
        // Disable persistence after saving
        WiFi.persistent(false);
    } else {
        WiFi.begin();
    }

    //if in nonblock mode, skip this loop
    _connectStartTime = millis(); // + 1;
    while (!_connectNonBlock && _connectStartTime) {
        handleWiFi();
        delay(10);
    }

    return (WiFi.status() == WL_CONNECTED);

} //attemptConnection

void PersWiFiManager::handleWiFi()
{
    if (!_connectStartTime) {
        // Routine checks when not in initial connection phase
        if (WiFi.getMode() == WIFI_STA) {
            if (WiFi.status() == WL_CONNECTED) {
                _connectionLostTime = 0; // Reset counter
            } else {
                // Not connected
                if (_connectionLostTime == 0) {
                    _connectionLostTime = millis();
                }
                
                // If disconnected for more than 30 seconds, try to restart WiFi
                if ((millis() - _connectionLostTime) > 30000) {
                     _connectionLostTime = 0;
                     WiFi.reconnect();
                }
            }
        }
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        _connectStartTime = 0;
        if (_connectHandler)
            _connectHandler();
        return;
    }

    //if failed or not connected and time is up
    if ((WiFi.status() == WL_CONNECT_FAILED) || ((WiFi.status() != WL_CONNECTED) && ((millis() - _connectStartTime) > (1000 * WIFI_CONNECT_TIMEOUT)))) {
        startApMode();
        _connectStartTime = 0; //reset connect start time
    }

} //handleWiFi

void PersWiFiManager::startApMode()
{
    //start AP mode
    IPAddress apIP(192, 168, 4, 1);
    WiFi.disconnect(true); // true = erase STA credentials from this session
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    _apPass.length() ? WiFi.softAP(getApSsid().c_str(), _apPass.c_str(), 11) : WiFi.softAP(getApSsid().c_str());

    _dnsServer->stop();
    delay(50);
    // set which return code will be used for all other domains (e.g. sending
    // ServerFailure instead of NonExistentDomain will reduce number of queries
    // sent by clients)
    // default is DNSReplyCode::NonExistentDomain
    //_dnsServer->setErrorReplyCode(DNSReplyCode::ServerFailure);
    // modify TTL associated  with the domain name (in seconds) // default is 60 seconds
    _dnsServer->setTTL(300); // (in seconds) as per example
    //_dnsServer->start((byte)53, device_hostname_full, apIP);
    _dnsServer->start(53, "*", apIP);

    if (_apHandler)
        _apHandler();
} //startApMode

void PersWiFiManager::setConnectNonBlock(bool b)
{
    _connectNonBlock = b;
} //setConnectNonBlock

void PersWiFiManager::setupWiFiHandlers()
{
    // note: removed DNS server setup here

    // Wi-Fi HTTP routes live in pro/api/api-web.cpp (/api/wifi/*)

 //_server->on /wifi/list

    // #ifdef WIFI_HTM_PROGMEM
    //   _server->on("/wifi.htm", HTTP_GET, [](AsyncWebServerRequest* request) {
    //     request->send(200, "text/html", FPSTR(wifi_htm));
    //     WiFi.scanNetworks(true); // early scan to have results ready on /wifi/list
    //   });
    // #endif

} //setupWiFiHandlers

bool PersWiFiManager::begin(const String &ssid, const String &pass)
{
    setupWiFiHandlers();
    return attemptConnection(ssid, pass); //switched order of these two for return
} //begin

String PersWiFiManager::getApSsid()
{
    return _apSsid.length() ? _apSsid : "gbscontrol";
} //getApSsid

void PersWiFiManager::setApCredentials(const String &apSsid, const String &apPass)
{
    if (apSsid.length())
        _apSsid = apSsid;
    if (apPass.length() >= 8)
        _apPass = apPass;
} //setApCredentials

void PersWiFiManager::onConnect(WiFiChangeHandlerFunction fn)
{
    _connectHandler = fn;
}

void PersWiFiManager::onAp(WiFiChangeHandlerFunction fn)
{
    _apHandler = fn;
}
#endif
