#!/bin/bash

set -e

. /usr/share/openmediavault/scripts/helper-functions

echo "Updating database ..."

xpath="/config/services/kvm/jobs/job"

xmlstarlet sel -t -m "${xpath}" -v "uuid" -n ${OMV_CONFIG_FILE} |
  xmlstarlet unesc |
  while read uuid; do
    if ! omv_config_exists "${xpath}[uuid='${uuid}']/keep"; then
      omv_config_add_key "${xpath}[uuid='${uuid}']" "keep" "0"
    fi
    if ! omv_config_exists "${xpath}[uuid='${uuid}']/samefmt"; then
      omv_config_add_key "${xpath}[uuid='${uuid}']" "samefmt" "0"
    fi
    if ! omv_config_exists "${xpath}[uuid='${uuid}']/poweroff"; then
      omv_config_add_key "${xpath}[uuid='${uuid}']" "poweroff" "0"
    fi
  done

exit 0
