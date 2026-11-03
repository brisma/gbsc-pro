// The real port's write timeout. On Windows serialport uses one COMMTIMEOUTS constant for reads and writes, and the
// last read can leave it at 1 ms: a 6 KB SLIP packet at 115200 baud then stopped halfway (Windows 11,
// "no answer to MemData"). Every write gets room for the whole buffer on the wire.
use gbsc_core::link::write_timeout;
use std::time::Duration;

#[test]
fn a_write_has_time_for_every_byte_on_the_wire() {
    // 10 bits per byte: 6144 bytes at 115200 baud take 533 ms
    assert!(write_timeout(6144, 115_200) >= Duration::from_millis(1066));
    // at 460800 a 16 KB block takes 355 ms
    assert!(write_timeout(16_384, 460_800) >= Duration::from_millis(710));
}

#[test]
fn a_short_write_still_gets_a_floor() {
    assert!(write_timeout(1, 115_200) >= Duration::from_millis(500));
    assert!(write_timeout(0, 0) >= Duration::from_millis(500));
}
