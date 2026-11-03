// Single-producer / single-consumer ring: network handlers push, loop() pops.
// On the ESP8266 network callbacks never preempt loop() code between yields, so plain indices are enough.
#pragma once
#include <stdint.h>

template <typename T, uint8_t N>
class RingQueue {
public:
    bool push(const T& v) {
        uint8_t next = (uint8_t)((head_ + 1) % N);
        if (next == tail_) return false;
        buf_[head_] = v;
        head_ = next;
        return true;
    }
    bool pop(T& out) {
        if (tail_ == head_) return false;
        out = buf_[tail_];
        tail_ = (uint8_t)((tail_ + 1) % N);
        return true;
    }
    uint8_t freeSlots() const { return (uint8_t)((tail_ + N - head_ - 1) % N); }
    bool empty() const { return head_ == tail_; }
    uint8_t size() const { return (uint8_t)((head_ + N - tail_) % N); }
    const T& at(uint8_t i) const { return buf_[(tail_ + i) % N]; }   // i-th queued item, 0 = oldest
private:
    T buf_[N];
    uint8_t head_ = 0;
    uint8_t tail_ = 0;
};
