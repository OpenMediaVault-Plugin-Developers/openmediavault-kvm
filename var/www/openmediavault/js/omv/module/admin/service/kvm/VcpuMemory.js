/**
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2021 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

Ext.define("OMV.module.admin.service.kvm.Vcpu", {
    extend: "OMV.workspace.window.Form",
    requires: [
        "OMV.workspace.window.plugin.ConfigObject"
    ],
    uses: [
        "OMV.data.Model",
        "OMV.data.Store"
    ],

    rpcService: "Kvm",
    rpcGetMethod: "getDisk",
    rpcSetMethod: "doChangeCpuMemory",

    getFormItems: function() {
        var me = this;
        return [{
            xtype: "numberfield",
            name: "vcpu",
            fieldLabel: _("CPUs"),
            minValue: 1,
            maxValue: 32,
            allowDecimals: false,
            allowNegative: false,
            allowBlank: false,
            value: me.vcpu
        },{
            xtype: "hidden",
            name: "memory",
            value: -1
        },{
            xtype: "hidden",
            name: "vmname",
            value: me.vmname
        }];
    }
});

Ext.define("OMV.module.admin.service.kvm.Memory", {
    extend: "OMV.workspace.window.Form",
    requires: [
        "OMV.workspace.window.plugin.ConfigObject"
    ],
    uses: [
        "OMV.data.Model",
        "OMV.data.Store"
    ],

    rpcService: "Kvm",
    rpcGetMethod: "getDisk",
    rpcSetMethod: "doChangeCpuMemory",

    getFormItems: function() {
        var me = this;
        return [{
            xtype: "compositefield",
            fieldLabel: _("Memory"),
            items: [{
                xtype: "numberfield",
                name: "memory",
                minValue: 1,
                maxValue: 65536,
                allowDecimals: false,
                allowNegative: false,
                allowBlank: false,
                value: me.memory,
                flex: 1
            },{
                xtype: "combo",
                name: "memoryunit",
                queryMode: "local",
                store: [
                    [ "GiB", _("GiB") ],
                    [ "MiB", _("MiB") ]
                ],
                allowBlank: false,
                editable: false,
                triggerAction: "all",
                value: "MiB"
            }]
        },{
            xtype: "hidden",
            name: "vcpu",
            value: -1
        },{
            xtype: "hidden",
            name: "vmname",
            value: me.vmname
        }];
    }
});
