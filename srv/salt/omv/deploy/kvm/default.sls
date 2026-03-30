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

{% set settings = salt['omv_conf.get']('conf.service.kvm') %}
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

kvm_monitor_db_dir:
  file.directory:
    - name: /var/lib/openmediavault/kvm
    - makedirs: True
    - user: root
    - group: root
    - mode: "0755"

configure_kvm_monitor_service:
  file.managed:
    - name: /etc/systemd/system/omv-kvm-monitor.service
    - contents: |
        [Unit]
        Description=OMV KVM Monitoring Daemon
        After=network.target libvirtd.service
        Wants=libvirtd.service

        [Service]
        Type=simple
        ExecStart=/usr/sbin/omv-kvm-monitor
        Restart=on-failure
        RestartSec=10
        StandardOutput=journal
        StandardError=journal

        [Install]
        WantedBy=multi-user.target
    - user: root
    - group: root
    - mode: "0644"

omv_kvm_monitor_systemd_reload:
  module.run:
    - name: service.systemctl_reload
    - onchanges:
      - file: configure_kvm_monitor_service

configure_kvm_monitor_config:
  file.managed:
    - name: /etc/omv-kvm-monitor.conf
    - contents: |
        {{ {
          'db_path': settings.monitor_dbpath,
          'collect_interval': settings.monitor_collect_interval,
          'detail_retention_hours': settings.monitor_detail_retention_hours,
          'summary_retention_days': settings.monitor_summary_retention_days
        } | tojson }}
    - user: root
    - group: root
    - mode: "0644"

{% if settings.lxc %}
lxc_service:
  service.running:
    - name: lxc
    - enable: True
{% else %}
lxc_service:
  service.dead:
    - name: lxc
    - enable: False
{% endif %}

{% if settings.lxcnet %}
lxcnet_service:
  service.running:
    - name: lxc-net
    - enable: True
{% else %}
lxcnet_service:
  service.dead:
    - name: lxc-net
    - enable: False
{% endif %}

{% if settings.monitor_enable %}
omv_kvm_monitor_service:
  service.running:
    - name: omv-kvm-monitor
    - enable: True
    - watch:
      - file: configure_kvm_monitor_service
      - file: configure_kvm_monitor_config
{% else %}
omv_kvm_monitor_service:
  service.dead:
    - name: omv-kvm-monitor
    - enable: False
{% endif %}

configure_kvm_monitor_nginx:
  file.managed:
    - name: /etc/nginx/openmediavault-webgui.d/kvm-monitor.conf
    - contents: |
        location /kvm-monitor/ {
            proxy_pass         http://127.0.0.1:8889/;
            proxy_set_header   Host $host;
            proxy_set_header   X-Real-IP $remote_addr;
            proxy_read_timeout 30s;
        }
    - user: root
    - group: root
    - mode: "0644"

reload_nginx_for_kvm_monitor:
  cmd.run:
    - name: systemctl reload nginx
    - onlyif: systemctl is-active nginx
