#include "stv9426-map.h"
#include "stv9426-shadow.h"
using namespace stv;

void StvShadow::reset() {
    for (uint8_t r = 0; r < kRows; r++) {
        width_[r] = kRowWidth;
        for (uint8_t c = 0; c < kRowCells; c++) { cells_[r][c][0] = 0xC0; cells_[r][c][1] = kEol; }
    }
    invalidate();
}
void StvShadow::invalidate() { for (uint8_t r = 0; r < kRows; r++) dirty_[r] = (1UL << kRowCells) - 1; }
uint8_t StvShadow::width(uint8_t row) const { return row < kRows ? width_[row] : 0; }
void StvShadow::setWidth(uint8_t row, uint8_t w) {
    if (row >= kRows || w < 1 || w > kRowWidth || w == width_[row]) return;
    uint8_t old = width_[row];
    width_[row] = w;
    if (cells_[row][old][1] == kEol) { cells_[row][old][0] = 0xC0; cells_[row][old][1] = 0xC0; dirty_[row] |= 1UL << old; }
    cells_[row][w][0] = 0xC0; cells_[row][w][1] = kEol;
    dirty_[row] |= 1UL << w;
}
void StvShadow::set(uint8_t row, uint8_t col, uint8_t code, uint8_t attr) {
    if (row >= kRows || col >= width_[row]) return;
    if (cells_[row][col][0] == attr && cells_[row][col][1] == code) return;
    cells_[row][col][0] = attr; cells_[row][col][1] = code;
    dirty_[row] |= 1UL << col;
}
uint8_t StvShadow::code(uint8_t row, uint8_t col) const { return cells_[row][col][1]; }
uint8_t StvShadow::attr(uint8_t row, uint8_t col) const { return cells_[row][col][0]; }
void StvShadow::fill(uint8_t row, uint8_t from, uint8_t to, uint8_t code, uint8_t attr) {
    for (uint8_t c = from; c < to; c++) set(row, c, code, attr);
}
bool StvShadow::dirty() const { for (uint8_t r = 0; r < kRows; r++) if (dirty_[r]) return true; return false; }
void StvShadow::flush(OsdBus& bus) {
    for (uint8_t r = 0; r < kRows; r++) {
        uint8_t c = 0;
        while (dirty_[r] && c < kRowCells) {
            if (!(dirty_[r] & (1UL << c))) { c++; continue; }
            uint8_t start = c;
            while (c < kRowCells && (dirty_[r] & (1UL << c))) c++;
            if (!bus.write((uint16_t)(rowAddr(r) + start * 2), &cells_[r][start][0], (uint8_t)((c - start) * 2)))
                return;                                  // I2C error: retry on the next flush
            for (uint8_t i = start; i < c; i++) dirty_[r] &= ~(1UL << i);
        }
    }
}
