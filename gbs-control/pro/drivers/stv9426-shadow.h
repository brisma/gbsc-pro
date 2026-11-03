// ESP-side copy of the STV9426 text rows with per-cell dirty tracking (pure, host-testable)
#pragma once
#include <stdint.h>
#include "stv9426-map.h"
#include "stv9426-bus.h"

class StvShadow {
public:
    void reset();                                        // every row width 30, cells (attr 0xC0, code EOL), all dirty
    void setWidth(uint8_t row, uint8_t width);           // 1..30; cell[width] becomes the end-of-line cell
    uint8_t width(uint8_t row) const;
    void set(uint8_t row, uint8_t col, uint8_t code, uint8_t attr);   // ignored when col >= width
    uint8_t code(uint8_t row, uint8_t col) const;
    uint8_t attr(uint8_t row, uint8_t col) const;
    void fill(uint8_t row, uint8_t from, uint8_t to, uint8_t code, uint8_t attr);   // [from, to)
    void invalidate();                                   // all rows dirty (after init)
    bool dirty() const;
    void flush(OsdBus& bus);                             // one write per contiguous dirty run, attr then code;
                                                         // a failed write keeps its cells dirty for the next flush
private:
    uint8_t cells_[stv::kRows][stv::kRowCells][2];       // [0] = attribute (even), [1] = code (odd)
    uint8_t width_[stv::kRows];
    uint32_t dirty_[stv::kRows];                         // bit c = cell c
};
