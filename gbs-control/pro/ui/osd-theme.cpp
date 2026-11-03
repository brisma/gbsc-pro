#include "../settings/settings-pgm.h"
#include "osd-theme.h"

// Colour indices (STV9426 R G B bits): K0 B1 G2 C3 R4 M5 Y6 W7; attribute = (bg << 4) | fg
enum : uint8_t { K, B, G, C, R, M, Y, W };
#define A(fg, bg) (uint8_t)(((bg) << 4) | (fg))

static const uint8_t kThemes[4][R_COUNT] SET_PROGMEM = {
    //  n        s        c        d        t        h        a        f        lb       ok       bad      bf       dc       msg
    {A(W, K), A(Y, K), A(K, Y), A(R, K), A(K, W), A(K, W), A(Y, K), A(Y, K), A(W, K), A(K, G), A(W, R), A(Y, K), A(R, Y), A(K, Y)},   // GBSC PRO: black and yellow, no blue, title and help on white bars, messages on a yellow bar (default)
    {A(W, B), A(Y, B), A(K, Y), A(R, B), A(K, C), A(C, B), A(Y, B), A(C, B), A(C, B), A(K, G), A(W, R), A(C, B), A(R, Y), A(Y, B)},   // Classic
    {A(K, W), A(B, W), A(K, C), A(R, W), A(W, B), A(B, W), A(M, W), A(B, W), A(B, W), A(W, G), A(W, R), A(B, W), A(R, C), A(M, W)},   // Light
    {A(K, G), A(Y, G), A(K, Y), A(R, G), A(G, K), A(G, K), A(Y, G), A(B, G), A(B, G), A(G, K), A(W, R), A(Y, G), A(R, Y), A(Y, G)},   // Retro: help on the title's dark bar
};
#undef A

uint8_t osdAttr(uint8_t theme, uint8_t role) {
    if (theme > 3) theme = 0;
    if (role >= R_COUNT) role = R_N;
    return pgm_read_byte(&kThemes[theme][role]);
}
