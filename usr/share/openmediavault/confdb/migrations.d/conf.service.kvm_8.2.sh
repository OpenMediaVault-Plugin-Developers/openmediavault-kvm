#!/bin/sh

set -e

. /etc/default/openmediavault
. /usr/share/openmediavault/scripts/helper-functions

# Add keys that may be missing on upgrade.
if ! omv_config_exists "/config/services/kvm/lxc"; then
    omv_config_add_key "/config/services/kvm" "lxc" "1"
fi
if ! omv_config_exists "/config/services/kvm/lxcnet"; then
    omv_config_add_key "/config/services/kvm" "lxcnet" "1"
fi
if ! omv_config_exists "/config/services/kvm/monitor_enable"; then
    omv_config_add_key "/config/services/kvm" "monitor_enable" "1"
fi
if ! omv_config_exists "/config/services/kvm/monitor_dbpath"; then
    omv_config_add_key "/config/services/kvm" "monitor_dbpath" "/var/lib/openmediavault/kvm/monitor.db"
fi
if ! omv_config_exists "/config/services/kvm/monitor_collect_interval"; then
    omv_config_add_key "/config/services/kvm" "monitor_collect_interval" "10"
fi
if ! omv_config_exists "/config/services/kvm/monitor_detail_retention_hours"; then
    omv_config_add_key "/config/services/kvm" "monitor_detail_retention_hours" "24"
fi
if ! omv_config_exists "/config/services/kvm/monitor_summary_retention_days"; then
    omv_config_add_key "/config/services/kvm" "monitor_summary_retention_days" "7"
fi

exit 0
