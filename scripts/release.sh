#!/usr/bin/env bash
# One-command release: bump the version everywhere, commit, sign the tag, push,
# regenerate the AUR .SRCINFO, and queue the GitHub source release as a DRY RUN.
#
#   scripts/release.sh [-n] [-N] [-R] [--check] <version>
#     -n, --dry-run      compute and print everything, change nothing
#     -N, --no-push      local commit + signed tag only (push by hand afterwards)
#     -R, --no-release   do not start the "Release" GitHub workflow
#         --check        clean build + ctest before committing (the repo rule)
#
# The version is canonical in CMakeLists.txt project(VERSION); aboutdata.json
# must match it (the configure FATAL_ERRORs otherwise) and packaging/aur/PKGBUILD
# carries the same pkgver. src/main.cpp gets it from the KWAL_VERSION compile
# definition, so there is no string literal left to bump.
#
# The tag is SIGNED (gpg.format=ssh + user.signingkey) — that signature is the
# only authenticity claim this project makes, docs/kwal_signers is what makes it
# checkable, and release.yml refuses an unsigned tag. Required repo config,
# verified by the guards below:
#   git config gpg.format ssh
#   git config user.signingkey ~/.ssh/id_ed25519.pub
#
# Publishing stays a human gate: this script starts release.yml with
# publish=false (guards + signature check + the exact notes rendered in the run
# summary); read that summary, then re-run the workflow with publish=true.

set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

usage() { sed -n '2,24p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }
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

# Tag signing must be configured *and* the key must be the one the tree trusts,
# otherwise the release would be cut unsigned or unverifiable — fail before
# touching any file instead of discovering it in the workflow.
[[ "$(git config --get gpg.format || true)" == ssh ]] \
    || die "tag signing not configured: git config gpg.format ssh"
signingkey="$(git config --get user.signingkey || true)"
[[ -n "$signingkey" ]] || die "no signing key: git config user.signingkey ~/.ssh/id_ed25519.pub"
keyfile="${signingkey/#\~/$HOME}"
[[ -f "$keyfile" ]] || die "signing key '$signingkey' not found"
keymaterial="$(awk '{print $1, $2}' "$keyfile")"
grep -qF "$keymaterial" docs/kwal_signers \
    || die "the key in $keyfile is not listed in docs/kwal_signers: its tags could never be verified"

# --- compute the new file contents ----------------------------------------
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

sed -E "s|^ VERSION $current\$| VERSION $version|" CMakeLists.txt > "$tmp/CMakeLists.txt"
sed -E "s|(\"Version\": \")$current(\")|\1$version\2|" aboutdata.json > "$tmp/aboutdata.json"
sed -E "s|^pkgver=$current\$|pkgver=$version|" packaging/aur/PKGBUILD > "$tmp/PKGBUILD"
awk -v v="$version" -v d="$(date +%F)" '
    !stamped && /^## \[Unreleased\]$/ { print "## [Unreleased]"; print ""; print "## [" v "] - " d; stamped = 1; next }
    { print }
' CHANGELOG.md > "$tmp/CHANGELOG.md"
grep -qE "^## \[$version\] - " "$tmp/CHANGELOG.md" || die "CHANGELOG.md has no '## [Unreleased]' header to rename"

for f in CMakeLists.txt aboutdata.json packaging/aur/PKGBUILD CHANGELOG.md; do
    diff -u -- "$f" "$tmp/${f##*/}" | sed "1,2d" || true
done

if ((dry)); then
    printf '\ndry run: nothing written. Tag would be the signed %s, pushed with the .SRCINFO, then a DRY-RUN Release workflow from main.\n' "$tag"
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

# The AUR metadata for this exact pkgver belongs in the release commit; it
# depends only on these files, so it is regenerated here rather than after the
# push (nothing about the tag is needed for it any more).
aurfiles=(packaging/aur/PKGBUILD)
if command -v makepkg >/dev/null; then
    (cd packaging/aur && makepkg --printsrcinfo > .SRCINFO)
    aurfiles+=(packaging/aur/.SRCINFO)
else
    printf 'release: makepkg not found: packaging/aur/.SRCINFO left as is\n' >&2
fi

git add CMakeLists.txt aboutdata.json "${aurfiles[@]}" CHANGELOG.md
git commit -m "chore(release): $version" -m "Bumps project(VERSION), aboutdata.json and the AUR pkgver/.SRCINFO; CHANGELOG [Unreleased] becomes [$version]."

git tag -s -a "$tag" -m "Kwal $version"
# Verify our own tag with the key the tree publishes: catches a signing
# misconfiguration before anything is public.
git -c gpg.ssh.allowedSignersFile=docs/kwal_signers verify-tag "$tag" \
    || die "$tag is not verifiable against docs/kwal_signers: fix the signing setup and delete the tag"
printf 'committed and signed-tagged %s locally\n' "$tag"

if ((push == 0)); then
    printf 'not pushed (asked with -N). Later: git push origin HEAD --follow-tags\n'
    exit 0
fi
git push origin HEAD --follow-tags

# --- the GitHub release itself --------------------------------------------
if ((release)); then
    if command -v gh >/dev/null; then
        gh workflow run release.yml --ref main -f "tag=$tag" -f publish=false
        printf 'DRY-RUN Release workflow started for %s (watch: gh run watch)\n' "$tag"
        printf 'read the rendered notes in the run summary, then re-run that workflow with publish=true\n'
    else
        printf 'no gh CLI: run the "Release" workflow by hand from main with tag=%s, publish=false\n' "$tag"
    fi
else
    printf 'Release workflow not triggered (--no-release): run it by hand from main with tag=%s\n' "$tag"
fi
