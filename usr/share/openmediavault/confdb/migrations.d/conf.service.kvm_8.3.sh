#!/bin/bash

set -e

. /usr/share/openmediavault/scripts/helper-functions

echo "Updating database ..."

xpath="/config/services/kvm/jobs/job"

xmlstarlet sel -t -m "${xpath}" -v "uuid" -n ${OMV_CONFIG_FILE} |
  xmlstarlet unesc |
  while read uuid; do
    if ! omv_config_exists "${xpath}[uuid='${uuid}']/incremental"; then
      omv_config_add_key "${xpath}[uuid='${uuid}']" "incremental" "0"
    fi
    if ! omv_config_exists "${xpath}[uuid='${uuid}']/fullinterval"; then
      omv_config_add_key "${xpath}[uuid='${uuid}']" "fullinterval" "0"
    fi
  done

exit 0
