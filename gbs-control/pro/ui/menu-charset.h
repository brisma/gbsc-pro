// Characters the STV9426 can draw: its ROM (datasheet Table 1) plus '?' (a UDC, OsdRenderer::put); every device
// string of the menu uses only these
#pragma once

inline bool menuCharOk(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '\'' ||
           c == '.' || c == '/' || c == ':' || c == '+' || c == '=' || c == '-' || c == '?';
}
