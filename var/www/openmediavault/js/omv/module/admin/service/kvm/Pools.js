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
// require("js/omvextras/window/RootFolderBrowser.js")

Ext.define("OMV.module.admin.service.kvm.Pool", {
    extend: "OMV.workspace.window.Form",
    requires: [
        "OMV.workspace.window.plugin.ConfigObject"
    ],
    uses: [
        "OMV.data.Model",
        "OMV.data.Store",
        "OmvExtras.window.RootFolderBrowser"
    ],

    rpcService: "Kvm",
    rpcGetMethod: "getPool",
    rpcSetMethod: "setPool",
    plugins: [{
        ptype: "configobject"
    },{
        ptype: 'linkedfields',
        correlations: [{
            conditions: [{
                name: 'type',
                value: 'zfs'
            }],
            name: ['zpoolname'],
            properties: ['show', 'submitValue', '!allowBlank']
        },{
            conditions: [{
                name: 'type',
                value: 'zfs'
            }],
            name: ['path'],
            properties: ['!show', '!submitValue', 'allowBlank']
        },{
            conditions: [{
                name: 'type',
                value: ['iscsi','netfs']
            }],
            name: ['hostname'],
            properties: ['show', 'submitValue', '!allowBlank']
        },{
            conditions: [{
                name: 'type',
                value: ['dir','zfs']
            }],
            name: ['sourcepath'],
            properties: ['!show', '!submitValue', 'allowBlank']
        }]
    }],

    getFormItems: function() {
        var me = this;
        return [{
            xtype: "textfield",
            name: "name",
            fieldLabel: _("Name"),
            allowBlank: false
        },{
            xtype: "combo",
            name: "type",
            fieldLabel: _("Type"),
            queryMode: "local",
            store: [
                [ "dir", _("Directory") ],
                [ "fs", _("Filesystem") ],
                [ "netfs", _("Network Filesystem") ],
                [ "disk", _("Disk") ],
                [ "iscsi", _("iSCSI") ],
                [ "zfs", _("ZFS") ],
            ],
            allowBlank: false,
            editable: false,
            triggerAction: "all",
            value: "dir",
            plugins: [{
                ptype: "fieldinfo",
                text: _("For description of each type and how to configure:") + " <a href='https://libvirt.org/storage.html' target='_blank'>https://libvirt.org/storage.html</a>"
            }],
            listeners: {
                scope: me,
                change: function(field) {
                    var path = me.findField("path");
                    if (field.value == "dir") {
                        path.setFieldLabel(_("Path"));
                    } else {
                        path.setFieldLabel(_("Target Path"));
                    }
                }
            }
        },{
            xtype: "textfield",
            name: "hostname",
            fieldLabel: _("Host name"),
            allowBlank: false
        },{
            xtype: "textfield",
            name: "zpoolname",
            fieldLabel: _("zPool name"),
            allowBlank: false
        },{
            xtype: "textfield",
            name: "sourcepath",
            fieldLabel: _("Source Path"),
            allowBlank: false,
            triggers: {
                folder: {
                    cls: Ext.baseCSSPrefix + "form-folder-trigger",
                    handler: "onTriggerClick"
                }
            },
            onTriggerClick: function() {
                Ext.create("OmvExtras.window.RootFolderBrowser", {
                    listeners: {
                        scope: this,
                        select: function(wnd, node, path) {
                            // Set the selected path.
                            this.setValue(path);
                        }
                    }
                }).show();
            }
        },{
            xtype: "textfield",
            name: "path",
            fieldLabel: _("Path"),
            allowBlank: false,
            triggers: {
                folder: {
                    cls: Ext.baseCSSPrefix + "form-folder-trigger",
                    handler: "onTriggerClick"
                }
            },
            onTriggerClick: function() {
                Ext.create("OmvExtras.window.RootFolderBrowser", {
                    listeners: {
                        scope: this,
                        select: function(wnd, node, path) {
                            // Set the selected path.
                            this.setValue(path);
                        }
                    }
                }).show();
            }
        }];
    }
});

Ext.define('OMV.module.admin.service.kvm.Pools', {
    extend: 'OMV.workspace.grid.Panel',
    requires: [
        'OMV.data.Store',
        'OMV.data.Model',
        'OMV.data.proxy.Rpc',
        'OMV.window.Execute'
    ],
    uses: [
        "OMV.module.admin.service.kvm.Pool",
        "OMV.module.admin.service.kvm.AdvancedPool"
    ],

    autoReload: true,
    rememberSelected: true,
    disableLoadMaskOnLoad: true,
    hidePagingToolbar: false,
    hideEditButton: true,
    hideDeleteButton: true,
    stateful: true,
    stateId: 'a4bec1a6-6bef-11eb-87f3-731abcfb36e8',
    columns: [{
        xtype: 'textcolumn',
        text: _('Name'),
        sortable: true,
        dataIndex: 'name',
        stateId: 'name'
    },{
        xtype: 'textcolumn',
        text: _('Activity'),
        sortable: true,
        dataIndex: 'active',
        stateId: 'active'
    },{
        xtype: 'textcolumn',
        text: _('Volume Count'),
        sortable: true,
        dataIndex: 'volume_count',
        stateId: 'volume_count'
    },{
        xtype: 'textcolumn',
        text: _('State'),
        sortable: true,
        dataIndex: 'state',
        stateId: 'state'
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
        xtype: 'binaryunitcolumn',
        text: _('Available'),
        sortable: true,
        dataIndex: 'available',
        stateId: 'available'
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
                        { name: 'active', type: 'string' },
                        { name: 'volume_count', type: 'string' },
                        { name: 'state', type: 'string' },
                        { name: 'capacity', type: 'string' },
                        { name: 'allocation', type: 'string' },
                        { name: 'available', type: 'string' },
                        { name: 'path', type: 'string' }
                    ]
                }),
                proxy: {
                    type: 'rpc',
                    rpcData: {
                        service: 'Kvm',
                        method: 'getPoolList'
                    }
                }
            })
        });
        me.callParent(arguments);
    },

    getTopToolbarItems: function() {
        var me = this;
        var items = me.callParent(arguments);

        Ext.Array.insert(items, 1, [{
            xtype: 'button',
            text: _('Delete'),
            icon: 'images/delete.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            handler: Ext.Function.bind(me.onDeleteButton, me, [ me ]),
            scope: me,
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1,
                enabledFn: function(c, records) {
                    var state = records[0].get("state");
                    return (state !== 'Running');
                }
            }
        },{
            xtype: "button",
            text: _("State"),
            scope: this,
            icon: "images/pulse.png",
            menu: [{
                text: _("Start"),
                icon: "images/arrow-up.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "start" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var state = records[0].get("state");
                        return (state !== 'Running');
                    }
                }
            },{
                text: _("Stop"),
                icon: "images/arrow-down.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "stop" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var state = records[0].get("state");
                        return (state == 'Running');
                    }
                }

            }]
        },{
            xtype: "button",
            text: _("Download"),
            scope: this,
            icon: "images/download.png",
            menu: [{
                text: _('Download virtio iso'),
                icon: 'images/download.png',
                iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
                handler: Ext.Function.bind(me.onDownloadIsoButton, me, [ "virtio" ]),
                scope: me
            },{
                text: _('Download iso from URL'),
                icon: 'images/download.png',
                iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
                handler: Ext.Function.bind(me.onDownloadIsoButton, me, [ "url" ]),
                scope: me,
            }],
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1
            }
        }]);
        return items;
    },

    onAddButton: function() {
        var me = this;
        Ext.create("OMV.module.admin.service.kvm.Pool", {
            title: _("Add pool"),
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
                method: "deletePool",
                params: {
                    name: record.get("name")
                }
            }
        });
        me.doReload();
    },

    onCommandButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        OMV.Rpc.request({
            scope: me,
            rpcData: {
                service: "Kvm",
                method: "poolCommand",
                params: {
                    name: record.get("name"),
                    command: cmd
                }
            }
        });
        me.doReload();
    },

    onDownloadIsoButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        var path = record.get("path");
        var msg = "";
        var url = "";
        if (cmd == "virtio") {
            msg =_("Download virtio iso");
            url = "https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/stable-virtio/virtio-win.iso";
            filename = "virtio-win.iso";
        } else if (cmd == "url") {
            msg =_("Download iso from url");
            url = prompt("Enter iso url", "");
            filename = prompt("Enter filename", url.split('/').pop());
        }
        var wnd = Ext.create("OMV.window.Execute", {
            title: _("Download virtio iso ..."),
            rpcService: "Kvm",
            rpcMethod: "downloadIso",
            rpcParams: {
                "filename": filename,
                "path": path,
                "url": url
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
    }
});

OMV.WorkspaceManager.registerPanel({
    id: 'pools',
    path: '/service/kvm',
    text: _('Pools'),
    position: 20,
    className: 'OMV.module.admin.service.kvm.Pools'
});
