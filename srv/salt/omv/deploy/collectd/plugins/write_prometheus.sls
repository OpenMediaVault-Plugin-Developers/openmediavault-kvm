configure_collectd_conf_writeprometheus_plugin:
  file.managed:
    - name: "/etc/collectd/collectd.conf.d/writeprometheus.conf"
    - contents: |
        LoadPlugin write_prometheus
        <Plugin write_prometheus>
          Port "9103"
        </Plugin>
