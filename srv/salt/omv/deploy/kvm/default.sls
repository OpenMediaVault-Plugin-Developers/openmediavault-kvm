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

{% set config = salt['omv_conf.get']('conf.service.kvm.job') %}
{% set has_ipv6_default = 'default' in salt['cmd.run']('ip -6 route') %}

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

{% if not salt['environ.get']('DPKG_MAINTSCRIPT_PACKAGE', '') %}
kvm_install_packages:
  pkg.installed:
    - pkgs:
      - openmediavault-cterm: '>= 7.8.5'
{% endif %}

configure_ip_forward:
  file.managed:
    - name: /etc/sysctl.d/ip_forward.conf
    - contents: |
        net.ipv4.ip_forward=1
        {% if has_ipv6_default %}
        net.ipv6.conf.all.forwarding=1
        {% endif %}
    - user: root
    - group: root
    - mode: 644

apply_ip_forward:
  cmd.run:
    - name: sysctl -p /etc/sysctl.d/ip_forward.conf
    - onchanges:
      - file: configure_ip_forward

configure_macvlan_module:
  file.managed:
    - name: /etc/modules-load.d/macvlan.conf
    - contents: |
        macvlan
    - user: root
    - group: root
    - mode: 644

load_macvlan_module:
  cmd.run:
    - name: modprobe macvlan
    - unless: lsmod | grep -q macvlan

configure_qemu_vnc_listen:
  file.replace:
    - name: /etc/libvirt/qemu.conf
    - pattern: '#vnc_listen = .*'
    - repl: 'vnc_listen = "0.0.0.0"'
    - onlyif: test -f /etc/libvirt/qemu.conf

configure_qemu_nvram:
  cmd.run:
    - name: |
        line=$(grep -n 'nvram = \[' /etc/libvirt/qemu.conf | cut -d":" -f1)
        line2=$((line+6))
        sed -i "${line},${line2} s/#//" /etc/libvirt/qemu.conf
    - onlyif: grep -q '^#nvram = \[' /etc/libvirt/qemu.conf

disable_ksm_service:
  service.dead:
    - name: ksm
    - enable: False
    - onlyif: test ! -f /sys/kernel/mm/ksm/run

libvirtd_service:
  service.running:
    - name: libvirtd
    - enable: True
    - watch:
      - file: configure_qemu_vnc_listen
      - cmd: configure_qemu_nvram

kvm_web_user:
  user.present:
    - name: openmediavault-kvmweb
    - system: True
    - createhome: False
    - shell: /usr/sbin/nologin
    - usergroup: True
