#!/bin/bash
# Install Ubuntu packages on a CI runner without depending on the mirror each run.
#
#   .github/ci-apt.sh DEBS_DIR NEEDED_COMMAND PACKAGE...
#
# DEBS_DIR is kept by actions/cache: once it holds the packages' .deb files they
# are installed from it and no mirror is asked. Otherwise (no cache, or a runner
# image the cached files no longer fit) apt fetches them into DEBS_DIR: three
# tries, each bounded (a stalled mirror hangs apt rather than failing it), an
# update with a failed fetch counted as failed (it exits 0 with only a warning
# otherwise, and the install then can't find the packages), and from the second
# try archive.ubuntu.com instead of the runner's own mirror. NEEDED_COMMAND
# must exist afterwards. DEBUGNOTES 2026-11-20 / 2026-11-26.
set -u
dir=$1 need=$2
shift 2
ok() { command -v "$need" >/dev/null; }

if ls "$dir"/*.deb >/dev/null 2>&1; then
  echo "installing $(ls "$dir"/*.deb | wc -l) cached packages"
  sudo dpkg -i "$dir"/*.deb >/dev/null 2>&1 || true
  if ok && sudo dpkg --audit >/dev/null 2>&1; then
    echo "installed from the cache"
    exit 0
  fi
  echo "the cached packages didn't install (another runner image?): fetching"
  sudo dpkg --configure -a >/dev/null 2>&1 || true
  # the cached files stay: the fetch adds what's missing (a dependency the
  # cache installed isn't fetched again, and the cache saved after has it)
fi

mkdir -p "$dir/partial"
APT=(-q -o Acquire::Retries=3 -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30
     -o "Dir::Cache::archives=$dir")
for i in 1 2 3; do
  if [ "$i" -ge 2 ]; then
    echo "try $i: archive.ubuntu.com instead of the runner's mirror"
    sudo sed -i 's#//azure\.archive\.ubuntu\.com#//archive.ubuntu.com#g' \
      /etc/apt/sources.list /etc/apt/sources.list.d/*.sources /etc/apt/sources.list.d/*.list 2>/dev/null
  fi
  sudo timeout 240 apt-get update "${APT[@]}" --error-on=any &&
    sudo timeout 300 apt-get install -y --download-only "${APT[@]}" "$@" &&
    sudo timeout 300 apt-get install -y "${APT[@]}" "$@" && ok && break
  echo "apt failed or hung (try $i), again in 20 s"
  sudo dpkg --configure -a || true
  sleep 20
done
# the cache saves DEBS_DIR as the runner's user: the .deb files only, readable,
# one version of each package (the newest)
sudo rm -rf "$dir/partial" "$dir/lock"
declare -A best
for f in "$dir"/*.deb; do
  [ -e "$f" ] || continue
  p=$(dpkg-deb -f "$f" Package 2>/dev/null) || { sudo rm -f "$f"; continue; }  # not a package
  v=$(dpkg-deb -f "$f" Version)
  if [ -n "${best[$p]:-}" ]; then
    o=${best[$p]}
    if dpkg --compare-versions "$v" gt "$(dpkg-deb -f "$o" Version)"; then sudo rm -f "$o"; best[$p]=$f; else sudo rm -f "$f"; fi
  else
    best[$p]=$f
  fi
done
sudo chown -R "$(id -u):$(id -g)" "$dir"
ok || { echo "::error::$need is missing: the packages didn't install"; exit 1; }
