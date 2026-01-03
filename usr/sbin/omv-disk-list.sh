#!/bin/bash
#
# shellcheck disable=SC2155,SC2207
#
# Copyright (c) 2022-2026 openmediavault plugin developers
#
# This file is licensed under the terms of the GNU General Public
# License version 2. This program is licensed "as is" without any
# warranty of any kind, whether express or implied.
#

find_symlink() {
  local symlink=$(find "$1" -type l -lname "*$2" -not -name '*wwn-*' | head -n1)
  if [ -n "${symlink}" ]; then
    echo "${symlink}"
  fi
}

disks=$(lsblk --json | jq -r '.blockdevices[] | select(.type == "disk") | .name' | awk NF)

output=()

for disk in ${disks}; do
  output+=("$(find_symlink "/dev/disk/by-id/" "${disk}")")
  output+=("$(find_symlink "/dev/disk/by-path/" "${disk}")")
  if [ -b "/dev/${disk}" ]; then
    output+=("/dev/${disk}")
  fi
done

sorted=($(printf "%s\n" "${output[@]}" | sort))

for entry in "${sorted[@]}"; do
  echo "${entry}"
done

exit 0
