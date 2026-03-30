#!/bin/sh
#
# @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
# @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
# @copyright Copyright (c) 2022-2026 openmediavault plugin developers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

set -e

. /etc/default/openmediavault
. /usr/share/openmediavault/scripts/helper-functions

if ! omv_config_exists "/config/services/kvm"; then
    omv_config_add_node "/config/services" "kvm"
    omv_config_add_key "/config/services/kvm" "lxc" "1"
    omv_config_add_key "/config/services/kvm" "lxcnet" "1"
    omv_config_add_key "/config/services/kvm" "monitor_enable" "1"
    omv_config_add_key "/config/services/kvm" "monitor_dbpath" "/var/lib/openmediavault/kvm/monitor.db"
    omv_config_add_key "/config/services/kvm" "monitor_collect_interval" "10"
    omv_config_add_key "/config/services/kvm" "monitor_detail_retention_hours" "24"
    omv_config_add_key "/config/services/kvm" "monitor_summary_retention_days" "7"
    omv_config_add_node "/config/services/kvm" "jobs"
fi

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
