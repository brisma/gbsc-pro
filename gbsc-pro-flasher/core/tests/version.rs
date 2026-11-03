// Version order frozen from 3.0.0 (device, tools/ota/version.py, web app model/version.ts)
use gbsc_core::version::compare_versions;
use std::cmp::Ordering::*;

#[test]
fn version_order() {
    let chain = ["3.0.0-local", "3.0.0-dev0", "3.0.0-beta1", "3.0.0", "3.0.1-dev1"];
    for w in chain.windows(2) {
        assert_eq!(compare_versions(w[0], w[1]), Less, "{} < {}", w[0], w[1]);
        assert_eq!(compare_versions(w[1], w[0]), Greater, "{} > {}", w[1], w[0]);
    }
    assert_eq!(compare_versions("3.10.0", "3.9.9"), Greater);
    assert_eq!(compare_versions("3.0.0-beta2", "3.0.0-beta10"), Less);
    assert_eq!(compare_versions("3.0.0", "3.0.0"), Equal);
}
