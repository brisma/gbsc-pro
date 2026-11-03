// Host/Origin allow-list: blocks cross-site and DNS-rebinding pages (pure, host-testable)
#pragma once
#include <stddef.h>

// Lowercases, strips "scheme://", path, port and trailing dots; false for empty, IPv6 or over-long input
bool guardNormalizeHost(const char* in, char* out, size_t n);
// Any IPv4 literal, <hostname>, <hostname>.local or an entry of the comma-separated extraHosts list
bool guardHostAllowed(const char* host, const char* hostname, const char* extraHosts);
// Same rules applied to the Origin host; the literal "null" origin is rejected
bool guardOriginAllowed(const char* origin, const char* hostname, const char* extraHosts);
// system.extra_hosts value check (every writer): comma-separated names of 1-63 [A-Za-z0-9.-] (spaces only around
// commas), no empty entry, no
// IPv4 literal (always allowed anyway), not "localhost"; the whole list fits the 63-char field; "" allowed
bool guardExtraHostsValid(const char* list);
// True when host passes the guard with oldList but not with newList (that browser tab would lose the page)
bool guardHostLostByChange(const char* host, const char* hostname, const char* oldList, const char* newList);
