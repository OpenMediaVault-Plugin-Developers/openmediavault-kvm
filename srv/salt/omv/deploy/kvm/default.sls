# @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
# @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
# @copyright Copyright (c) 2022-2024 OpenMediaVault Plugin Developers
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

{% set config = salt['omv_conf.get']('conf.service.kvm.job') %}

configure_kvm_scheduled_jobs:
  file.managed:
    - name: "/etc/cron.d/omv-backup-vm"
    - source:
      - salt://{{ tpldir }}/files/jobs.j2
    - template: jinja
    - context:
        jobs: {{ config | json }}
    - user: root
    - group: root
    - mode: 644

configure_omv_backup_vm_logrotate:
  file.managed:
    - name: "/etc/logrotate.d/omv-backup-vm"
    - contents: |
        /var/log/omv-backup-vm.log {
          monthly
          missingok
          rotate 12
          compress
          notifempty
        }
    - user: root
    - group: root
    - mode: 644

configure_omv_virsh_command_logrotate:
  file.managed:
    - name: "/etc/logrotate.d/omv-virsh-command"
    - contents: |
        /var/log/omv-virsh-command.log {
          monthly
          missingok
          rotate 12
          compress
          notifempty
        }
    - user: root
    - group: root
    - mode: 644
