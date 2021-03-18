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
// require("js/omv/WorkspaceManager.js")
// require("js/omv/workspace/grid/Panel.js")
// require("js/omv/workspace/window/Form.js")
// require("js/omv/workspace/window/plugin/ConfigObject.js")
// require("js/omv/Rpc.js")
// require("js/omv/data/Store.js")
// require("js/omv/data/Model.js")
// require("js/omv/data/proxy/Rpc.js")
// require("js/omv/window/Execute.js")

Ext.define("OMV.module.admin.service.kvm.Volume", {
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
    rpcGetMethod: "getVolume",
    rpcSetMethod: "setVolume",
    plugins: [{
        ptype: "configobject"
    }],

    getFormItems: function() {
        var me = this;
        return [{
            xtype: "combo",
            name: "pool",
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
            name: "name",
            fieldLabel: _("Name"),
            allowBlank: false
        },{
            xtype: "compositefield",
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
            name: "format",
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
        }];
    }
});

Ext.define('OMV.module.admin.service.kvm.Volumes', {
    extend: 'OMV.workspace.grid.Panel',
    requires: [
        'OMV.data.Store',
        'OMV.data.Model',
        'OMV.data.proxy.Rpc',
        'OMV.window.Execute'
    ],
    uses: [
        "OMV.module.admin.service.kvm.Volume"
    ],

    autoReload: true,
    rememberSelected: true,
    disableLoadMaskOnLoad: true,
    hidePagingToolbar: false,
    hideAddButton: true,
    hideEditButton: true,
    hideDeleteButton: true,
    stateful: true,
    stateId: 'b02a5fd6-6bff-11eb-8c5b-f3581dedb11b',
    columns: [{
        xtype: 'textcolumn',
        text: _('Name'),
        sortable: true,
        dataIndex: 'name',
        stateId: 'name',
        flex: 1
    },{
        xtype: 'textcolumn',
        text: _('Pool'),
        sortable: true,
        dataIndex: 'pool',
        stateId: 'pool'
    },{
        xtype: 'textcolumn',
        text: _('Extension'),
        sortable: true,
        dataIndex: 'ext',
        stateId: 'ext'
    },{
        xtype: 'binaryunitcolumn',
        text: _('Capacity'),
        sortable: true,
        dataIndex: 'capacity',
        stateId: 'capacity'
    },{
        text: _("Allocation"),
        sortable: true,
        dataIndex: "allocationT",
        stateId: "allocationT",
        align: "center",
        renderer: function(value, metaData, record) {
            var allocation = parseInt(record.get("allocation"));
            var capacity = parseInt(record.get("capacity"));
            var percentage = allocation / capacity;
            if (percentage < 0)
                return _("n/a");
            var renderer = OMV.util.Format.progressBarRenderer(percentage, value, 0.85);
            return renderer.apply(this, arguments);
        }
    },{
        xtype: 'textcolumn',
        text: _('Path'),
        sortable: true,
        dataIndex: 'path',
        stateId: 'path',
        flex: 1
    }],

    initComponent: function () {
        var me = this;
        Ext.apply(me, {
            store: Ext.create('OMV.data.Store', {
                autoLoad: true,
                model: OMV.data.Model.createImplicit({
                    idProperty: 'name',
                    fields: [
                        { name: 'name', type: 'string' },
                        { name: 'pool', type: 'string' },
                        { name: 'ext', type: 'string' },
                        { name: 'capacity', type: 'string' },
                        { name: 'allocation', type: 'string' },
                        { name: 'allocationT', type: 'string' },
                        { name: 'path', type: 'string' }
                    ]
                }),
                proxy: {
                    type: 'rpc',
                    rpcData: {
                        service: 'Kvm',
                        method: 'getVolumeList',
                        params: {
                            "optical": false
                        }
                    }
                }
            })
        });
        me.callParent(arguments);
    },

   getTopToolbarItems: function() {
        var me = this;
        var items = me.callParent(arguments);

        Ext.Array.insert(items, 0, [{
            xtype: 'button',
            text: _('Add'),
            icon: 'images/add.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            handler: Ext.Function.bind(me.onAddButton, me, [ me ]),
            scope: me
        },{
            xtype: 'button',
            text: _('Delete'),
            icon: 'images/delete.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            handler: Ext.Function.bind(me.onDeleteButton, me, [ me ]),
            scope: me,
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1
            }
        },{
            xtype: 'button',
            text: _('Check'),
            icon: 'images/aid.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            handler: Ext.Function.bind(me.onCommandButton, me, [ "check" ]),
            scope: me,
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1,
                enabledFn: function(c, records) {
                    var ext = records[0].get("ext").toLowerCase();
                    return (ext == 'qcow2');
                }
            }
        },{
            xtype: 'button',
            text: _('Info'),
            icon: 'images/info.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            handler: Ext.Function.bind(me.onCommandButton, me, [ "info" ]),
            scope: me,
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1
            }
       },{
            xtype: "button",
            text: _("Convert"),
            scope: this,
            icon: "images/edit.png",
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1
            },
            menu: [{
                text: _("Convert to qcow2"),
                icon: "images/edit.png",
                handler: Ext.Function.bind(me.onConvertButton, me, [ "qcow2" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var ext = records[0].get("ext").toLowerCase();
                        return (ext != 'qcow2');
                    }
                }
            },{
                text: _("Convert to raw"),
                icon: "images/edit.png",
                handler: Ext.Function.bind(me.onConvertButton, me, [ "raw" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var ext = records[0].get("ext").toLowerCase();
                        return (ext != 'img');
                    }
                }
            }]
        }]);
        return items;
    },

    onAddButton: function() {
        var me = this;
        Ext.create("OMV.module.admin.service.kvm.Volume", {
            title: _("Add volume"),
            uuid: OMV.UUID_UNDEFINED,
            listeners: {
                scope: me,
                submit: function() {
                    this.doReload();
                }
            }
        }).show();
    },

    onDeleteButton: function() {
        var me = this;
        var record = me.getSelected();
        OMV.Rpc.request({
            scope: me,
            rpcData: {
                service: "Kvm",
                method: "deleteVolume",
                params: {
                    path: record.get("path")
                }
            }
        });
        me.doReload();
    },

    onConvertButton: function(format) {
        var me = this;
        var record = me.getSelected();
        var path = record.get("path");
        var wnd = Ext.create("OMV.window.Execute", {
            title: _("Convert to ") + format + " ...",
            rpcService: "Kvm",
            rpcMethod: "convertVolume",
            rpcParams: {
                "path": path,
                "format": format
            },
            rpcIgnoreErrors: true,
            hideStartButton: true,
            hideStopButton: true,
            listeners: {
                scope: me,
                finish: function(wnd, response) {
                    wnd.appendValue(_("Done."));
                    wnd.setButtonDisabled("close", false);
                },
                exception: function(wnd, error) {
                    OMV.MessageBox.error(null, error);
                    wnd.setButtonDisabled("close", false);
                },
                close: function() {
                    me.doReload();
                }
            }
        });
        wnd.setButtonDisabled("close", true);
        wnd.show();
        wnd.start();
    },

    onCommandButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        var path = record.get("path");
        var msg = "";
        if (cmd == "check") {
            msg = _("Checking volume ...");
        } else if (cmd == "info") {
            msg = _("Showing volume info ...");
        }
        var wnd = Ext.create("OMV.window.Execute", {
            title: msg,
            rpcService: "Kvm",
            rpcMethod: "volumeCommand",
            rpcParams: {
                "path": path,
                "command": cmd
            },
            rpcIgnoreErrors: true,
            hideStartButton: true,
            hideStopButton: true,
            listeners: {
                scope: me,
                finish: function(wnd, response) {
                    wnd.appendValue(_("Done."));
                    wnd.setButtonDisabled("close", false);
                },
                exception: function(wnd, error) {
                    OMV.MessageBox.error(null, error);
                    wnd.setButtonDisabled("close", false);
                }
            }
        });
        wnd.setButtonDisabled("close", true);
        wnd.show();
        wnd.start();
    }
});

OMV.WorkspaceManager.registerPanel({
    id: 'volumes',
    path: '/service/kvm',
    text: _('Volumes'),
    position: 30,
    className: 'OMV.module.admin.service.kvm.Volumes'
});
