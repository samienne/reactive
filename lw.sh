#!/bin/sh
# loomworks repo-local launcher. Committed alongside lw.pin. Fetches the pinned,
# verified lw host binary into .nvim/cache/ and execs it; the host provisions the
# pinned bundle itself. Regenerate with `lw update`. See `lw help bootstrap`.
set -eu

# Dev / test-at-head override: run a named binary, bypassing the pin entirely.
if [ -n "${LOOMWORKS_LW:-}" ]; then
  exec "$LOOMWORKS_LW" "$@"
fi

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
pin="$here/lw.pin"
[ -f "$pin" ] || { echo "lw: no lw.pin next to this launcher" >&2; exit 1; }

# --- select the host-binary asset for this OS/arch -------------------------
os=$(uname -s 2>/dev/null || echo unknown)
arch=$(uname -m 2>/dev/null || echo unknown)
case "$os" in
  Linux) os=linux ;;
  Darwin) os=macos ;;
  MINGW*|MSYS*|CYGWIN*|Windows_NT) os=windows ;;
  *) echo "lw: unsupported OS '$os'" >&2; exit 1 ;;
esac
case "$arch" in
  x86_64|amd64) arch=x86_64 ;;
  arm64|aarch64) arch=arm64 ;;
esac
case "$os-$arch" in
  linux-x86_64) asset=lw-linux-x86_64 ;;
  macos-arm64) asset=lw-macos-arm64 ;;
  windows-x86_64) asset=lw-windows-x86_64.exe ;;
  *) echo "lw: no pinned lw binary for $os/$arch" >&2; exit 1 ;;
esac

# --- read version + the asset's pinned sha256 from lw.pin ------------------
version=$(sed -n 's/^[[:space:]]*version[[:space:]]*=[[:space:]]*//p' "$pin" | head -n1)
want=$(sed -n "s/^[[:space:]]*sha256_$asset[[:space:]]*=[[:space:]]*//p" "$pin" | head -n1)
[ -n "$version" ] || { echo "lw: lw.pin has no version" >&2; exit 1; }
[ -n "$want" ] || { echo "lw: lw.pin has no sha256 for $asset" >&2; exit 1; }
# Reject a malicious pinned version before it reaches a download URL: a repo
# cannot redirect the fetch (a traversal like /../ would leave the origin).
case "$version" in
  *..*|*[!0-9A-Za-z._+-]*)
    echo "lw: invalid pinned version '$version'" >&2; exit 1 ;;
esac
want=$(printf '%s' "$want" | tr 'A-Z' 'a-z')

cache="$here/.nvim/cache"
bin="$cache/lw-$version-$asset"

# --- peel launcher-only flags (--insecure / --verify); forward the rest ---
insecure=0; do_verify=0
[ "${LOOMWORKS_INSECURE:-}" = "1" ] && insecure=1
new=""
for a in "$@"; do
  case "$a" in
    --insecure) insecure=1; continue ;;
    --verify) do_verify=1; continue ;;
  esac
  new="$new $(printf "%s" "$a" | sed "s/'/'\\\\''/g; 1s/^/'/; \$s/\$/'/")"
done
eval "set -- $new"

sha_of() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
  elif command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | cut -d' ' -f1
  else echo "lw: need sha256sum or shasum to verify the download" >&2; exit 1; fi
}

# Fetch $1 -> $2, quietly (a progress meter is noise in CI logs; errors still
# show). A bare path / file:// (offline mirror) is copied, matching how the host
# reads a local LOOMWORKS_RELEASE_URL; only real URLs use curl/wget. Status 127:
# no downloader at all (retrying cannot help).
fetch_to() {
  case "$1" in
    file://*) cp "$(printf '%s' "$1" | sed 's,^file://,,')" "$2" ;;
    *://*)
      if command -v curl >/dev/null 2>&1; then
        k=""; [ "$insecure" = "1" ] && k="-k"
        curl -fsSL $k -o "$2" "$1"
      elif command -v wget >/dev/null 2>&1; then
        k=""; [ "$insecure" = "1" ] && k="--no-check-certificate"
        wget -q $k -O "$2" "$1"
      else
        echo "lw: need curl or wget to download the pinned binary" >&2; return 127
      fi ;;
    *) cp "$1" "$2" ;;
  esac
}

# Bounded retry for a network fetch: at most 3 attempts, 1 s then 2 s apart,
# each from an empty file. A local copy is not retried. Reports its own failure.
fetch_retry() {
  remote=0
  case "$1" in file://*) ;; *://*) remote=1 ;; esac
  if [ "$remote" = 0 ]; then
    fetch_to "$1" "$2" && return 0
    echo "lw: copy failed: $1" >&2; return 1
  fi
  n=1
  while :; do
    rm -f "$2"
    rc=0; fetch_to "$1" "$2" || rc=$?
    [ "$rc" = 0 ] && return 0
    [ "$rc" = 127 ] && return 1
    if [ "$n" -ge 3 ]; then
      echo "lw: download failed after $n attempts: $1" >&2; return 1
    fi
    echo "lw: download attempt $n failed; retrying..." >&2
    sleep "$n"; n=$((n + 1))
  done
}

# --- ensure the pinned binary is cached + verified (hash is mandatory) ----
if [ ! -f "$bin" ] || [ "$(sha_of "$bin" | tr 'A-Z' 'a-z')" != "$want" ]; then
  rm -f "$bin"
  mkdir -p "$cache"
  if [ -n "${LOOMWORKS_RELEASE_URL:-}" ]; then
    url="$LOOMWORKS_RELEASE_URL/$asset"
  else
    url="https://github.com/samienne/loomworks.nvim/releases/download/v$version/$asset"
  fi
  echo "lw: fetching pinned lw $version ($asset) for $pin..." >&2
  tmp="$bin.dl.$$"
  fetch_retry "$url" "$tmp" || { rm -f "$tmp"; exit 1; }
  got=$(sha_of "$tmp" | tr 'A-Z' 'a-z')
  if [ "$got" != "$want" ]; then
    echo "lw: sha256 mismatch for $asset (pin $want, got $got) -- aborting" >&2
    rm -f "$tmp"; exit 1
  fi
  mv "$tmp" "$bin"
  [ "$os" = windows ] || chmod +x "$bin"
  printf 'version=%s\nasset=%s\nsha256=%s\n' "$version" "$asset" "$want" > "$cache/lw.marker"
fi

# --- optional stronger provenance check (never required) ------------------
if [ "$do_verify" = "1" ]; then
  if command -v gh >/dev/null 2>&1; then
    gh attestation verify "$bin" --repo samienne/loomworks.nvim \
      || { echo "lw: gh attestation verify failed" >&2; exit 1; }
  else
    echo "lw: --verify: gh not found; skipping attestation (sha256 already verified)" >&2
  fi
fi

# --- exec the pinned host; it provisions the pinned bundle itself ----------
LOOMWORKS_PINNED="$version" LW_ROOT="$PWD" exec "$bin" "$@"
