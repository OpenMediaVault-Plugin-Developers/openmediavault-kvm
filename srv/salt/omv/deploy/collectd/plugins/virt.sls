configure_collectd_conf_virt_plugin:
  file.managed:
    - name: "/etc/collectd/collectd.conf.d/virt.conf"
    - contents: |
        LoadPlugin virt
        <Plugin virt>
          Connection "qemu:///system"
          HostnameFormat name
        </Plugin>
