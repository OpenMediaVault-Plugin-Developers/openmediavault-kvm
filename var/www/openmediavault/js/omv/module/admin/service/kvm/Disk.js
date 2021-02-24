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

Ext.define("OMV.module.admin.service.kvm.AddDisk", {
    extend: "OMV.workspace.window.Form",
    requires: [
        "OMV.workspace.window.plugin.ConfigObject"
    ],
    uses: [
        "OMV.data.Model",
        "OMV.data.Store",
        "OMV.window.FolderBrowser"
    ],

    rpcService: "Kvm",
    rpcGetMethod: "getDisk",
    rpcSetMethod: "addDisk",

    plugins: [{
        ptype: 'linkedfields',
        correlations: [{
            conditions: [{
                name: 'voldisk',
                value: 'Create new disk'
            }],
            name: ['volpool','volname','volsizefield','volsize','volunit','volformat'],
            properties: ['show', 'submitValue', '!allowBlank']
        }]
    }],

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
                name: "voldisk",
                fieldLabel: _("Volume"),
                emptyText: _("Select a volume ..."),
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
                                "optical": false,
                                "opticalNone": false
                            }
                        },
                        appendSortParams: false
                    }
               })
            },{
                xtype: "combo",
                name: "volpool",
                fieldLabel: _("Pool"),
                emptyText: _("Select a pool ..."),
                editable: false,
                triggerAction: "all",
                displayField: "name",
                valueField: "name",
                allowNone: true,
                allowBlank: true,
                store: Ext.create("OMV.data.Store", {
                    autoLoad: true,
                    model: OMV.data.Model.createImplicit({
                        idProperty: "name",
                        fields: [
                            { name: "name", type: "string" }
                        ]
                    }),
                    proxy: {
                        type: "rpc",
                        rpcData: {
                            service: "Kvm",
                            method: "enumeratePools"
                        },
                        appendSortParams: false
                    },
                    sorters: [{
                        direction: "ASC",
                        property: "name"
                    }]
                })
            },{
                xtype: "textfield",
                name: "volname",
                fieldLabel: _("Name"),
                allowBlank: false
            },{
                xtype: "compositefield",
                name: "volsizefield",
                fieldLabel: _("Size"),
                items: [{
                    xtype: "numberfield",
                    name: "volsize",
                    minValue: 1,
                    maxValue: 65536,
                    allowDecimals: false,
                    allowNegative: false,
                    allowBlank: false,
                    value: 1,
                    flex: 1
                },{
                    xtype: "combo",
                    name: "volunit",
                    queryMode: "local",
                    store: [
                        [ "T", _("T") ],
                        [ "G", _("G") ],
                        [ "M", _("M") ]
                    ],
                    allowBlank: false,
                    editable: false,
                    triggerAction: "all",
                    value: "G"
                }]
            },{
                xtype: "combo",
                name: "volformat",
                fieldLabel: _("Format"),
                queryMode: "local",
                store: [
                    [ "qcow2", _("qcow2") ],
                    [ "raw", _("raw") ]
                ],
                allowBlank: false,
                editable: false,
                triggerAction: "all",
                value: "qcow2"
            },{
                xtype: "combo",
                name: "volbus",
                fieldLabel: _("Bus"),
                queryMode: "local",
                store: [
                    [ "virtio", _("virtio") ],
                    [ "scsi", _("scsi") ],
                    [ "sata", _("sata") ],
                    [ "ide", _("ide") ]
                ],
                allowBlank: false,
                editable: false,
                triggerAction: "all",
                value: "virtio"
            },{
                xtype: "hidden",
                name: "vmname",
                value: me.vmname
            }]
        }];
    }
});

Ext.define("OMV.module.admin.service.kvm.ResizeDisk", {
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
    rpcSetMethod: "resizeDisk",

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
                fieldLabel: _("Disk"),
                emptyText: _("Select a file ..."),
                editable: false,
                triggerAction: "all",
                displayField: "description",
                valueField: "path",
                allowNone: true,
                allowBlank: true,
                store: Ext.create("OMV.data.Store", {
                    autoLoad: true,
                    model: OMV.data.Model.createImplicit({
                        idProperty: "path",
                        fields: [
                            { name: "path", type: "string" },
                            { name: "description", type: "string" }
                        ]
                    }),
                    proxy: {
                        type: "rpc",
                        rpcData: {
                            service: "Kvm",
                            method: "enumerateVolumesByVm",
                            params: {
                                "optical": false,
                                "vmname": me.vmname
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
                xtype: "compositefield",
                name: "volsizefield",
                fieldLabel: _("Size to add"),
                items: [{
                    xtype: "numberfield",
                    name: "volsize",
                    minValue: 1,
                    maxValue: 65536,
                    allowDecimals: false,
                    allowNegative: false,
                    allowBlank: false,
                    value: 1,
                    flex: 1
                },{
                    xtype: "combo",
                    name: "volunit",
                    queryMode: "local",
                    store: [
                        [ "T", _("T") ],
                        [ "G", _("G") ],
                        [ "M", _("M") ]
                    ],
                    allowBlank: false,
                    editable: false,
                    triggerAction: "all",
                    value: "G"
                }]
            },{
                xtype: "hidden",
                name: "vmname",
                value: me.vmname
            }]
        }];
    }
});

Ext.define("OMV.module.admin.service.kvm.RemoveDisk", {
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
                fieldLabel: _("Disk"),
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
                                "optical": false,
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
