// Network load diagnostics (device only): low-water marks of the heap and largest block, high-water marks of the
// active and TIME-WAIT TCP connections, sampled every loop pass; shown in /api/info "net", reset by ?reset_net=1
#pragma once
#include <stdint.h>

struct NetStats { uint32_t heapMin, blockMin; uint8_t tcpMax, twMax, tcpNow, twNow; uint32_t starveMaxMs, starveAtS; };
void netStatsSample();
// Longest gap between two runs of a 10 ms system-context timer: how long lwIP could not run (a loop() that does
// not yield); starveAtS = uptime of that gap. Started by netGuardHttpAccept().
void netStarveStart();
const NetStats& netStats();
void netStatsReset();
uint8_t netHttpConnections();   // open TCP connections to the web server (port 80), from the lwIP list
void netGuardHttpAccept();   // after server.begin(): reset new port-80 connections beyond admitTcp (api-admission.h)
uint16_t netTcpRefused();    // connections reset by the accept guard since boot
bool netHttpAlive(uint32_t remoteIp, uint16_t remotePort);   // that connection to port 80 is still open
