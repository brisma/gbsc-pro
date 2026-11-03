# OTA update server

GBS-Control checks for updates and downloads them from a plain web server. Any host that follows the rules below
works: an object store behind a CDN, a VPS with nginx, a home server. The official builds use
`http://gbsc.brisma.it/v1/`.

## Base URL

The base is compiled into the firmware: `GBSC_OTA_BASE` in `gbs-control/pro/update/ota-update.h`. A fork that
publishes its own builds changes that default. The base must be:
- `http://host[:port]/[path/]v1/`, ending in `v1/`, at most 64 characters;
- a host name of `0-9 A-Z a-z . -` with an IPv4 address (an A record): the device has no IPv6;
- path segments of `0-9 A-Z a-z . _ -`;
- plain HTTP: the device has no TLS.

## Files

Under the base:

| Path | What it is |
|------|------------|
| `stable/manifest.txt` | the newest Stable build: **always present** |
| `beta/manifest.txt` | the newest Beta build: may be missing |
| `fw/gbsc-pro-<version>.bin.gz` | one firmware image per version, made by `tools/release.py` |

A Stable build reads `stable/`; a Beta build reads `beta/` and `stable/`. Either reads the other channel when the
user opens System > Update > Other channels. A check succeeds when at least one manifest it reads is valid: a Stable
build with no `stable/manifest.txt` shows "Check failed" and retries every 10 minutes (up to every 2 hours) instead of
once a day.

`python3 tools/release.py --channel <stable|beta> --version <X.Y.Z or X.Y.Z-betaN> --out <dir>` builds the
firmware and writes `<dir>/v1/` as the server must hold it. Upload the image first and the manifest last (see
Publishing rules), and skip the `.sha256` file next to each image: it is only for the publishing workflow. For
Stable, `release.py` sets `notes` to the official release page: a fork passes its own with `--notes <URL>`.

The image is the gzip `release.py` makes: the device refuses a file without the gzip header ("not firmware"), a
firmware above 1044464 bytes once decompressed, or an image larger than the free flash.

## Manifest

`manifest.txt`, format v1: ASCII, at most 1024 bytes, one `key=value` per line in any order.

```
version=3.0.0
size=612345
sha256=<64 hex digits of the .bin.gz file>
file=fw/gbsc-pro-3.0.0.bin.gz
notes=https://github.com/brisma/gbsc-pro/releases/tag/v3.0.0
```

The whole manifest is invalid ("bad manifest" on the unit) when:
- a line has a CR (CRLF line endings) or no `=`;
- a key has characters other than `a-z 0-9 _` (`Version=` is invalid);
- a key is repeated;
- a required key is missing or a value breaks its rule below.

No whitespace is trimmed: `version = 3.0.0` is the key `version `, which is invalid. Blank lines, a missing final LF
and unknown keys are fine.

| Key | Rule |
|-----|------|
| `version` | 1 to 15 characters of `0-9 A-Z a-z . -`: `X.Y.Z` or `X.Y.Z-betaN` |
| `size` | bytes of the image file: decimal, not zero, at most 11 digits; the server must send this Content-Length |
| `sha256` | SHA-256 of the image file, 64 hex digits |
| `file` | path of the image relative to the base, under 64 characters of `0-9 A-Z a-z . _ / -`: no leading `/`, no `..`, no query string |
| `notes` | optional: a release notes URL, under 96 characters |

Version order: `X.Y.Z` numerically, then for the same `X.Y.Z` `-devN` < `-betaN` < no suffix
(`tools/ota/version.py`). The recommended check offers a version only when it is newer than the installed one;
Other channels offers any version different from the installed one, older ones included.

## What the device sends and accepts

The request, for both the manifests and the images:

```
GET /v1/stable/manifest.txt HTTP/1.0
Host: gbsc.brisma.it
User-Agent: GBSC-Pro/3.0.0-beta1
Connection: close
```

`Host` carries `:port` when the base names a port other than 80. The server must answer:
- **on plain HTTP**, with no redirect: the device treats any redirect (to HTTPS or elsewhere) as an error. HTTPS on
  the same paths is fine for browsers and apps.
- **`200` with `Content-Length`**: at most 1024 bytes for a manifest, at most `size` for an image, followed by
  exactly that many bytes and nothing more.
- **no transfer or content encoding**: no chunked body, no gzip on the fly. The device sends no `Accept-Encoding`
  and does not read `Transfer-Encoding` or `Content-Encoding`: an encoded body only fails later, as a bad manifest or a
  bad checksum. The image is already gzip and must be served byte for byte.
- **short header lines**: the status line and every header under 255 bytes (long `Set-Cookie` or
  `Content-Security-Policy` headers break updates).
- **in time**: resolving and connecting within 5 seconds, then never more than 10 seconds without data.

The device checks `size` and `sha256` after the download and installs nothing on a mismatch. What a unit shows when
the server breaks a rule:

| Server | Unit |
|--------|------|
| a status other than 200 (redirects and 404 included), a header line too long | server error |
| no Content-Length, a body too large or longer than Content-Length | wrong size |
| an invalid manifest | bad manifest |
| an image whose bytes do not match `sha256` | bad checksum |
| the connection closed before Content-Length bytes | cut off |
| no answer in time | timeout |

## Publishing rules

- Upload the image first, the manifest last: a manifest never points at a missing file.
- Never reuse an image name for other bytes. Images can then be cached for a long time (a year); keep manifests
  short-lived (a minute or a few), so a new release reaches units quickly.
- Keep every image that a live manifest points at; older ones can go (`tools/ota/prune.py` keeps the last 3 per
  channel).
- A rollback is a manifest pointing back at an older image still on the server. Units never downgrade by
  themselves: users on the newer build choose the older one in System > Update > Other channels.
- Optional: CORS (`Access-Control-Allow-Origin: *` for GET and HEAD) on `v1/`, for web pages that read the
  manifests. Paths outside `v1/` are not used by the device.

## The official server

`.github/workflows/ota-publish.yml` publishes the tags `vX.Y.Z-betaN` and `vX.Y.Z` to an S3 bucket behind
CloudFront. It is off until the repository variable `OTA_ENABLED` is `true`, and needs `OTA_ROLE_ARN` (a role GitHub
Actions assumes through OIDC), `OTA_BUCKET` and `OTA_DISTRIBUTION_ID`. The workflow uses the bucket for more than
uploads: it reads the live manifests, keeps each image's `sha256` and commit as object metadata (re-runs and
rollbacks read them back), lists and deletes old images, and downloads the published image for the GitHub release.
Another host needs those steps rewritten, and the release page URLs changed to the fork's. `tools/ota/` holds the
helpers it uses: `version.py` (version order), `guard.py` (a channel never goes backwards), `prune.py` (old images).
