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

Ext.define("OMV.module.admin.service.kvm.AddVmNic", {
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
    rpcSetMethod: "addVmNic",

    getFormItems: function() {
        var me = this;
        return [{
            xtype: "fieldset",
            title: _("Network"),
            defaults: {
                labelSeparator: ""
            },
            items : [{
              xtype: "combo",
                name: "model",
                fieldLabel: _("Model"),
                queryMode: "local",
                store: [
                    [ "virtio", _("virtio") ],
                    [ "e1000", _("e1000") ],
                    [ "rtl8139", _("rtl8139") ],
                    [ "ne2k_pci", _("ne2k_pci") ],
                    [ "pcnet", _("pcnet") ]
                ],
                allowBlank: false,
                editable: false,
                triggerAction: "all",
                value: "virtio"
            },{
                xtype: "combo",
                name: "network",
                fieldLabel: _("Network"),
                emptyText: _("Select a network ..."),
                editable: false,
                triggerAction: "all",
                displayField: "netname",
                valueField: "netname",
                allowNone: true,
                allowBlank: true,
                store: Ext.create("OMV.data.Store", {
                    autoLoad: true,
                    model: OMV.data.Model.createImplicit({
                        idProperty: "netname",
                        fields: [
                            { name: "netname", type: "string" }
                        ]
                    }),
                    proxy: {
                        type: "rpc",
                        rpcData: {
                            service: "Kvm",
                            method: "enumerateNetworks"
                        },
                        appendSortParams: false
                    },
                    sorters: [{
                        direction: "ASC",
                        property: "netname"
                    }]
                })
            },{
                xtype: "textfield",
                name: "macaddress",
                fieldLabel: _("MAC address"),
                allowBlank: true,
                plugins: [{
                    ptype: "fieldinfo",
                    text: _("Leave blank for random address.")
                }]
            },{
                xtype: "hidden",
                name: "vmname",
                value: me.vmname
            }]
        }];
    }
});

Ext.define("OMV.module.admin.service.kvm.RemoveVmNic", {
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
    rpcSetMethod: "removeVmNic",

    getFormItems: function() {
        var me = this;
        return [{
            xtype: "fieldset",
            title: _("Network"),
            defaults: {
                labelSeparator: ""
            },
            items : [{
                xtype: "combo",
                name: "network",
                fieldLabel: _("Network"),
                emptyText: _("Select a nic ..."),
                editable: false,
                triggerAction: "all",
                displayField: "description",
                valueField: "mac",
                allowNone: true,
                allowBlank: true,
                store: Ext.create("OMV.data.Store", {
                    autoLoad: true,
                    model: OMV.data.Model.createImplicit({
                        idProperty: "mac",
                        fields: [
                            { name: "mac", type: "string" },
                            { name: "description", type: "string" }
                        ]
                    }),
                    proxy: {
                        type: "rpc",
                        rpcData: {
                            service: "Kvm",
                            method: "enumerateVmNic",
                            params: {
                                "vmname": me.vmname
                            }

                        },
                        appendSortParams: false
                    },
                    sorters: [{
                        direction: "ASC",
                        property: "nic"
                    }]
                })
            },{
                xtype: "hidden",
                name: "vmname",
                value: me.vmname
            }]
        }];
    }
});
