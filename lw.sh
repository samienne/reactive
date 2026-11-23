#!/bin/sh
# loomworks repo-local launcher. Committed alongside lw.pin. Fetches the pinned,
# verified lw host binary into .lw/cache/ and execs it; the host provisions the
# pinned bundle itself. Regenerate with `lw bootstrap install`. See
# `lw help bootstrap`. LOOMWORKS_LAUNCHER tells lw which launcher ran it, so the
# commands it prints read ./lw.sh.
set -eu

# Dev / test-at-head override: run a named binary, bypassing the pin entirely.
if [ -n "${LOOMWORKS_LW:-}" ]; then
  LOOMWORKS_LAUNCHER=lw.sh exec "$LOOMWORKS_LW" "$@"
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
  *) os=$(printf %s "$os" | tr '[:upper:]' '[:lower:]') ;;
esac
case "$arch" in
  x86_64|amd64) arch=x86_64 ;;
  arm64|aarch64) arch=arm64 ;;
esac
case "$os-$arch" in
  linux-x86_64) asset=lw-linux-x86_64 ;;
  linux-arm64) asset=lw-linux-arm64 ;;
  macos-arm64) asset=lw-macos-arm64 ;;
  macos-x86_64) asset=lw-macos-x86_64 ;;
  windows-x86_64) asset=lw-windows-x86_64.exe ;;
  *) echo "lw: no lw build for $os-$arch; supported: linux-arm64, linux-x86_64, macos-arm64, macos-x86_64, windows-x86_64" >&2; exit 1 ;;
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
# The repository the pinned version is published in: lw below 0.2 lives in
# samienne/loomworks.nvim, 0.2 and later in samienne/loomworks (no fallback).
case "$version" in
  0.0.*|0.1.*) repo=samienne/loomworks.nvim ;;
  *) repo=samienne/loomworks ;;
esac

# Where the binary is cached: .lw/cache for lw 0.2 and later; a pin below 0.2
# keeps .nvim/cache, where that release expects its own binary.
case "$version" in
  0.0.*|0.1.*) cache="$here/.nvim/cache" ;;
  *) cache="$here/.lw/cache" ;;
esac
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
  # .lw/ ignores itself (created once, never rewritten or written through a link)
  if [ "$cache" = "$here/.lw/cache" ] && [ ! -e "$here/.lw/.gitignore" ] && [ ! -L "$here/.lw/.gitignore" ]; then
    (set -C; printf '%s\n' '# lw workspace state, machine-local: ignore everything here' '*' > "$here/.lw/.gitignore") 2>/dev/null || :
  fi
  if [ -n "${LOOMWORKS_RELEASE_URL:-}" ]; then
    url="$LOOMWORKS_RELEASE_URL/$asset"
  else
    url="https://github.com/$repo/releases/download/v$version/$asset"
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
    gh attestation verify "$bin" --repo "$repo" \
      || { echo "lw: gh attestation verify failed" >&2; exit 1; }
  else
    echo "lw: --verify: gh not found; skipping attestation (sha256 already verified)" >&2
  fi
fi

# --- exec the pinned host; it provisions the pinned bundle itself ----------
LOOMWORKS_LAUNCHER=lw.sh LOOMWORKS_PINNED="$version" LW_ROOT="$PWD" exec "$bin" "$@"
