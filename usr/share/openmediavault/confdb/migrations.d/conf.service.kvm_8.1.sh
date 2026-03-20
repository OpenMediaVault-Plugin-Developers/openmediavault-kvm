#!/bin/bash

set -e

. /usr/share/openmediavault/scripts/helper-functions

echo "Updating database ..."

xpath="/config/services/kvm/jobs/job"

xmlstarlet sel -t -m "${xpath}" -v "uuid" -n ${OMV_CONFIG_FILE} |
  xmlstarlet unesc |
  while read uuid; do
    if ! omv_config_exists "${xpath}[uuid='${uuid}']/emailonerror"; then
      omv_config_add_key "${xpath}[uuid='${uuid}']" "emailonerror" "0"
    fi
  done

exit 0
