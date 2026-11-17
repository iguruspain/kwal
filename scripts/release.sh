#!/usr/bin/env bash
# One-command release: bump the version everywhere, commit, tag, push, fill the
# AUR source hash + .SRCINFO, and trigger the GitHub source release.
#
#   scripts/release.sh [-n] [-N] [-R] [--check] <version>
#     -n, --dry-run      compute and print everything, change nothing
#     -N, --no-push      local commit + tag only (push by hand afterwards)
#     -R, --no-release   do not trigger the "Release" GitHub workflow
#         --check        clean build + ctest before committing (the repo rule)
#
# The version is canonical in CMakeLists.txt project(VERSION); aboutdata.json
# must match it (the configure FATAL_ERRORs otherwise) and packaging/aur/PKGBUILD
# carries the same pkgver. src/main.cpp gets it from the KWAL_VERSION compile
# definition, so there is no string literal left to bump.
#
# After a real run, the release itself (notes + GitHub release) is produced by
# .github/workflows/release.yml, which this script starts with publish=true.

set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

usage() { sed -n '2,17p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }
die() { printf 'release: %s\n' "$*" >&2; exit 1; }

dry=0 push=1 release=1 fullcheck=0 version=''
while (($#)); do
    case "$1" in
        -n|--dry-run) dry=1 ;;
        -N|--no-push) push=0 ;;
        -R|--no-release) release=0 ;;
        --check) fullcheck=1 ;;
        -h|--help) usage; exit 0 ;;
        -*) die "unknown option '$1' (see --help)" ;;
        *) version="$1" ;;
    esac
    shift
done

[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { usage >&2; die "version must be X.Y.Z"; }
tag="v$version"

# --- guards ---------------------------------------------------------------
git rev-parse --git-dir >/dev/null || die "not a git repository"
[[ -z "$(git status --porcelain)" ]] || die "uncommitted changes: commit or stash them first"
branch="$(git rev-parse --abbrev-ref HEAD)"
[[ "$branch" == main ]] || die "releases run from main, current branch is $branch"
git rev-parse --verify --quiet "refs/tags/$tag" >/dev/null && die "tag $tag already exists locally"
git ls-remote --exit-code --tags origin "refs/tags/$tag" >/dev/null 2>&1 && die "tag $tag is already published on origin"
git rev-parse --abbrev-ref --symbolic-full-name '@{upstream}' >/dev/null 2>&1 || die "main has no upstream branch"
behind="$(git rev-list --count HEAD..@{upstream})"
((behind == 0)) || die "main is $behind commit(s) behind origin/main: pull first"

current="$(sed -nE 's/^ VERSION ([0-9.]+)$/\1/p' CMakeLists.txt)"
aboutdata="$(sed -nE 's/.*"Version": "([^"]+)".*/\1/p' aboutdata.json)"
[[ -n "$current" && "$current" == "$aboutdata" ]] \
    || die "CMakeLists VERSION ($current) != aboutdata.json Version ($aboutdata)"
grep -qE "^pkgver=$current$" packaging/aur/PKGBUILD \
    || die "packaging/aur/PKGBUILD pkgver is not $current"
grep -qE "^## \[$version\]" CHANGELOG.md && die "CHANGELOG.md already documents [$version]"

# --- compute the new file contents ----------------------------------------
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

sed -E "s|^ VERSION $current\$| VERSION $version|" CMakeLists.txt > "$tmp/CMakeLists.txt"
sed -E "s|(\"Version\": \")$current(\")|\1$version\2|" aboutdata.json > "$tmp/aboutdata.json"
sed -E "s|^pkgver=$current\$|pkgver=$version|" packaging/aur/PKGBUILD > "$tmp/PKGBUILD"
awk -v v="$version" -v d="$(date +%F)" '
    !stamped && /^## \[Unreleased\]$/ { print "## [Unreleased]"; print ""; print "## [" v "] - " d; print ""; stamped = 1; next }
    { print }
' CHANGELOG.md > "$tmp/CHANGELOG.md"
grep -qE "^## \[$version\] - " "$tmp/CHANGELOG.md" || die "CHANGELOG.md has no '## [Unreleased]' header to rename"

for f in CMakeLists.txt aboutdata.json packaging/aur/PKGBUILD CHANGELOG.md; do
    diff -u -- "$f" "$tmp/${f##*/}" | sed "1,2d" || true
done

if ((dry)); then
    printf '\ndry run: nothing written. Tag would be %s, then origin push, AUR hash fill and the Release workflow.\n' "$tag"
    exit 0
fi

# --- apply ----------------------------------------------------------------
cp -- "$tmp/CMakeLists.txt" CMakeLists.txt
cp -- "$tmp/aboutdata.json" aboutdata.json
cp -- "$tmp/PKGBUILD" packaging/aur/PKGBUILD
cp -- "$tmp/CHANGELOG.md" CHANGELOG.md

# CMake's own guard: aboutdata.json must agree with project(VERSION).
cmake --preset default > "$tmp/configure.log" 2>&1 \
    || die "configure failed with the new version (see $tmp/configure.log); revert with: git checkout -- CMakeLists.txt aboutdata.json packaging/aur/PKGBUILD CHANGELOG.md"
grep -iE '^CMake Warning|deprecat' "$tmp/configure.log" && die "configure emitted warnings for the new version"

if ((fullcheck)); then
    ./build.sh clean
    cmake --preset default
    cmake --build --preset default
    ctest --test-dir build
fi

git add CMakeLists.txt aboutdata.json packaging/aur/PKGBUILD CHANGELOG.md
git commit -m "chore(release): $version" -m "Bumps project(VERSION), aboutdata.json and the AUR pkgver; CHANGELOG [Unreleased] becomes [$version]."
git tag -a "$tag" -m "Kwal $version"
printf 'committed and tagged %s locally\n' "$tag"

if ((push == 0)); then
    printf 'not pushed (asked with -N). Later: git push origin HEAD --follow-tags\n'
    exit 0
fi
git push origin HEAD --follow-tags

# --- AUR: the source hash only exists once the tag is on the remote -------
pkgbuild=packaging/aur/PKGBUILD
url="$(sed -nE 's|^url="([^"]+)".*|\1|p' "$pkgbuild")/archive/refs/tags/$tag.tar.gz"
hash="$(curl -fsSL "$url" | sha256sum | cut -d' ' -f1)"
[[ -n "$hash" ]] || die "no sha256 for $url (GitHub has not produced the tarball yet; rerun the AUR part later)"

awk -v h="$hash" '
    BEGIN { q = sprintf("%c", 39) }
    inblk { print "    " q h q; inblk = 0; next }
    /^sha256sums=\(/ { print; inblk = 1; next }
    { print }
' "$pkgbuild" > "$tmp/pkgbuild.filled"
cmp -s "$tmp/pkgbuild.filled" "$pkgbuild" || cp -- "$tmp/pkgbuild.filled" "$pkgbuild"
grep -qF "'$hash'" "$pkgbuild" || die "could not write the sha256 into $pkgbuild"

if command -v makepkg >/dev/null; then
    (cd packaging/aur && makepkg --printsrcinfo > .SRCINFO)
else
    printf 'release: makepkg not found: packaging/aur/.SRCINFO not generated\n' >&2
fi

git add "$pkgbuild" packaging/aur/.SRCINFO
git diff --cached --quiet || { git commit -m "chore(aur): source hash and .SRCINFO for $tag"; git push origin HEAD; }

# --- the GitHub release itself --------------------------------------------
if ((release)); then
    if command -v gh >/dev/null; then
        gh workflow run release.yml --ref "$tag" -f "tag=$tag" -f publish=true
        printf 'Release workflow started for %s (watch: gh run watch)\n' "$tag"
    else
        printf 'no gh CLI: run the "Release" workflow by hand with tag=%s, publish=true\n' "$tag"
    fi
else
    printf 'Release workflow not triggered (--no-release): run it by hand with tag=%s\n' "$tag"
fi
