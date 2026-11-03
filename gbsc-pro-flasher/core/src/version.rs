//! Firmware version order, frozen from 3.0.0: the same as the device (update-manifest.cpp versionCompare),
//! tools/ota/version.py and the web app (model/version.ts). X.Y.Z, then "-devN" < "-betaN" < no suffix with N
//! numeric; any other suffix sorts below "-dev" (a local build).
use std::cmp::Ordering;

struct Parsed<'a> {
    key: [u64; 5], // X, Y, Z, rank (0 other suffix, 1 dev, 2 beta, 3 none), N
    rest: &'a str,
}

fn leading_number(s: &str) -> (Option<u64>, usize) {
    let n = s.bytes().take_while(u8::is_ascii_digit).count();
    if n == 0 { (None, 0) } else { (s[..n].parse().ok(), n) }
}

fn parse(v: &str) -> Parsed<'_> {
    let mut key = [0u64; 5];
    let mut i = 0;
    for k in 0..3 {
        let (num, len) = leading_number(&v[i..]);
        if let Some(x) = num {
            key[k] = x;
        }
        i += len;
        if v[i..].starts_with('.') {
            i += 1;
        }
    }
    let rest = &v[i..];
    let suffix = ["-beta", "-dev"].iter().enumerate().find_map(|(r, p)| {
        let tail = rest.strip_prefix(p)?;
        (!tail.is_empty() && tail.bytes().all(|b| b.is_ascii_digit())).then(|| (r, tail.parse::<u64>().unwrap_or(0)))
    });
    key[3] = match (rest.starts_with('-'), suffix) {
        (false, _) => 3,
        (true, Some((0, _))) => 2,
        (true, Some(_)) => 1,
        (true, None) => 0,
    };
    key[4] = suffix.map_or(0, |(_, n)| n);
    Parsed { key, rest }
}

/// Order of two firmware versions (Less = `a` is older).
pub fn compare_versions(a: &str, b: &str) -> Ordering {
    let (x, y) = (parse(a), parse(b));
    let by_key = x.key.cmp(&y.key);
    if by_key != Ordering::Equal {
        return by_key;
    }
    if x.key[3] == 0 || x.key[3] == 3 {
        return x.rest.cmp(y.rest); // the rest by text, as strcmp
    }
    Ordering::Equal
}
