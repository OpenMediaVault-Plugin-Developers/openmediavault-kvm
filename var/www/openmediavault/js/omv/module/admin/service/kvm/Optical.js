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

Ext.define("OMV.module.admin.service.kvm.AddOptical", {
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
    rpcSetMethod: "addOptical",

    getFormItems: function() {
        var me = this;
        return [{
            xtype: "fieldset",
            title: _("Storage Volume"),
            defaults: {
                labelSeparator: ""
            },
            items : [{
                xtype: "combo",
                name: "voliso",
                fieldLabel: _("Optical Disk"),
                emptyText: _("Select a file ..."),
                editable: false,
                triggerAction: "all",
                displayField: "path",
                valueField: "path",
                allowNone: true,
                allowBlank: true,
                store: Ext.create("OMV.data.Store", {
                    autoLoad: true,
                    model: OMV.data.Model.createImplicit({
                        idProperty: "path",
                        fields: [
                            { name: "path", type: "string" }
                        ]
                    }),
                    proxy: {
                        type: "rpc",
                        rpcData: {
                            service: "Kvm",
                            method: "enumerateVolumes",
                            params: {
                                "optical": true,
                                "opticalNone": false
                            }
                        },
                        appendSortParams: false
                    },
                    sorters: [{
                        direction: "ASC",
                        property: "path"
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

Ext.define("OMV.module.admin.service.kvm.RemoveOptical", {
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
    rpcSetMethod: "removeDisk",

    getFormItems: function() {
        var me = this;
        return [{
            xtype: "fieldset",
            title: _("Storage Volume"),
            defaults: {
                labelSeparator: ""
            },
            items : [{
                xtype: "combo",
                name: "disk",
                fieldLabel: _("Optical Disk"),
                emptyText: _("Select a file ..."),
                editable: false,
                triggerAction: "all",
                displayField: "description",
                valueField: "device",
                allowNone: true,
                allowBlank: true,
                store: Ext.create("OMV.data.Store", {
                    autoLoad: true,
                    model: OMV.data.Model.createImplicit({
                        idProperty: "device",
                        fields: [
                            { name: "device", type: "string" },
                            { name: "description", type: "string" }
                        ]
                    }),
                    proxy: {
                        type: "rpc",
                        rpcData: {
                            service: "Kvm",
                            method: "enumerateVolumesByVm",
                            params: {
                                "optical": true,
                                "vmname": me.vmname
                            }
                        },
                        appendSortParams: false
                    },
                    sorters: [{
                        direction: "ASC",
                        property: "device"
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
