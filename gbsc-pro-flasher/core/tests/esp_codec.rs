// ESP8266 loader framing (SLIP) and command packets (esptool serial protocol)
use gbsc_core::esp::cmd::{data_checksum, parse_response, request, status_ok, Op, Response};
use gbsc_core::esp::slip::{slip_encode, SlipDecoder};

fn decode_all(bytes: &[u8]) -> Vec<Vec<u8>> {
    let mut d = SlipDecoder::default();
    bytes.iter().filter_map(|&b| d.push(b)).collect()
}

#[test]
fn slip_roundtrip_with_escapes() {
    let payload = vec![0x01, 0xC0, 0x02, 0xDB, 0x03];
    let enc = slip_encode(&payload);
    assert_eq!(enc, vec![0xC0, 0x01, 0xDB, 0xDC, 0x02, 0xDB, 0xDD, 0x03, 0xC0]);
    assert_eq!(decode_all(&enc), vec![payload]);
}

#[test]
fn decoder_skips_noise_between_frames() {
    let mut bytes = b"ets Jan  8 2013,rst cause".to_vec();
    bytes.extend(slip_encode(&[1, 2]));
    bytes.extend(b"\r\nnoise");
    bytes.extend(slip_encode(&[3]));
    assert_eq!(decode_all(&bytes), vec![vec![1, 2], vec![3]]);
}

#[test]
fn sync_request_bytes() {
    let mut data = vec![0x07, 0x07, 0x12, 0x20];
    data.extend([0x55; 32]);
    let r = request(Op::Sync, &data, 0);
    assert_eq!(r.len(), 44);
    assert_eq!(&r[..8], &[0x00, 0x08, 36, 0, 0, 0, 0, 0]);
    assert_eq!(&r[8..], &data[..]);
}

#[test]
fn checksum_seed() {
    assert_eq!(data_checksum(&[]), 0xEF);
    assert_eq!(data_checksum(&[0x01]), 0xEE);
}

#[test]
fn response_status_error() {
    let frame = [0x01, 0x02, 0x02, 0x00, 0x78, 0x56, 0x34, 0x12, 0x01, 0x05];
    let r: Response = parse_response(&frame).unwrap();
    assert_eq!((r.op, r.value), (0x02, 0x1234_5678));
    assert_eq!(status_ok(&r), Err(5));
    let ok = parse_response(&[0x01, 0x08, 0x02, 0x00, 0, 0, 0, 0, 0x00, 0x00]).unwrap();
    assert_eq!(status_ok(&ok), Ok(()));
    assert!(parse_response(&[0x00, 0x08]).is_none());
}
