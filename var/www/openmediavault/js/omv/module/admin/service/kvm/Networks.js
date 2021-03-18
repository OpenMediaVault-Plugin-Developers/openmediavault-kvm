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

Ext.define("OMV.module.admin.service.kvm.Macvtap", {
    extend: "OMV.workspace.window.Form",
    requires: [
        "OMV.workspace.window.plugin.ConfigObject"
    ],
    uses: [
        "OMV.data.Model",
        "OMV.data.Store"
    ],

    rpcService: "Kvm",
    rpcGetMethod: "getNetworkXml",
    rpcSetMethod: "setMacvtap",
    plugins: [{
        ptype: "configobject"
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
            name: "nic",
            fieldLabel: _("Network adapter"),
            emptyText: _("Select a network adapter ..."),
            editable: false,
            triggerAction: "all",
            displayField: "devicename",
            valueField: "devicename",
            allowNone: true,
            allowBlank: true,
            store: Ext.create("OMV.data.Store", {
                autoLoad: true,
                model: OMV.data.Model.createImplicit({
                    idProperty: "devicename",
                    fields: [
                        { name: "devicename", type: "string" }
                    ]
                }),
                proxy: {
                    type: "rpc",
                    rpcData: {
                        service: "Network",
                        method: "enumerateDevices"
                    },
                    appendSortParams: false
                },
                sorters: [{
                    direction: "ASC",
                    property: "devicename"
                }]
            })
        }];
    }
});

Ext.define("OMV.module.admin.service.kvm.Network", {
    extend: "OMV.workspace.window.Form",
    requires: [
        "OMV.workspace.window.plugin.ConfigObject"
    ],
    uses: [
        "OMV.data.Model",
        "OMV.data.Store"
    ],

    rpcService: "Kvm",
    rpcGetMethod: "getNetworkXml",
    rpcSetMethod: "setNetwork",
    plugins: [{
        ptype: "configobject"
    },{
        ptype: 'linkedfields',
        correlations: [{
            conditions: [{
                name: 'dhcp',
                value: true
            }],
            name: ['startaddress','endaddress'],
            properties: ['show', 'submitValue', '!allowBlank']
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
            xtype: "textfield",
            name: "macaddress",
            fieldLabel: _("MAC address"),
            allowBlank: false,
            value: "52:54:00:XX:XX:XX".replace(/X/g, function() {
                return "0123456789ABCDEF".charAt(Math.floor(Math.random() * 16))
            })
        },{
            xtype: "textfield",
            name: "gatewayip",
            fieldLabel: _("Gateway IP Address"),
            vtype: "IPv4",
            allowBlank: false
        },{
            xtype: "combo",
            name: "subnet",
            fieldLabel: _("Subnet mask"),
            queryMode: "local",
            store: [
                [ "255.255.255.0", "/24 --> 255.255.255.0" ],
                [ "255.255.255.128", "/25 --> 255.255.255.128" ],
                [ "255.255.255.192", "/26 --> 255.255.255.192" ],
                [ "255.255.255.224", "/27 --> 255.255.255.224" ],
                [ "255.255.255.240", "/28 --> 255.255.255.240" ],
                [ "255.255.255.248", "/29 --> 255.255.255.248" ],
                [ "255.255.255.252", "/30 --> 255.255.255.252" ]
            ],
            allowBlank: false,
            editable: false,
            triggerAction: "all",
            value: "255.255.255.0"
        },{
            xtype: "checkbox",
            name: "dhcp",
            fieldLabel: _("DHCP"),
            checked: false
        },{
            xtype: "textfield",
            name: "startaddress",
            fieldLabel: _("DHCP starting address"),
            vtype: "IPv4",
            allowBlank: false
        },{
            xtype: "textfield",
            name: "endaddress",
            fieldLabel: _("DHCP ending address"),
            vtype: "IPv4",
            allowBlank: false
        }];
    }
});

Ext.define('OMV.module.admin.service.kvm.Networks', {
    extend: 'OMV.workspace.grid.Panel',
    requires: [
        'OMV.Rpc',
        'OMV.data.Store',
        'OMV.data.Model',
        'OMV.data.proxy.Rpc'
    ],
   uses: [
        "OMV.module.admin.service.kvm.Network",
        "OMV.module.admin.service.kvm.Macvtap"
    ],

    autoReload: true,
    rememberSelected: true,
    disableLoadMaskOnLoad: true,
    hidePagingToolbar: false,
    hideAddButton: true,
    hideEditButton: true,
    hideDeleteButton: true,
    stateful: true,
    stateId: '96b2e392-6bec-11eb-acfe-3f28acbd13c8',
    columns: [{
        xtype: 'textcolumn',
        text: _('Network Name'),
        sortable: true,
        dataIndex: 'netname',
        stateId: 'netname'
    },{
        xtype: 'textcolumn',
        text: _('Network State'),
        sortable: true,
        dataIndex: 'active',
        stateId: 'active'
    },{
        xtype: 'textcolumn',
        text: _('Gateway IP'),
        sortable: true,
        dataIndex: 'ip',
        stateId: 'ip'
    },{
        xtype: 'textcolumn',
        text: _('IP Range'),
        sortable: true,
        dataIndex: 'iprange',
        stateId: 'iprange'
    },{
        xtype: 'textcolumn',
        text: _('Forwarding'),
        sortable: true,
        dataIndex: 'forward',
        stateId: 'forward'
    },{
        xtype: 'textcolumn',
        text: _('DHCP Range'),
        sortable: true,
        dataIndex: 'dhcp',
        stateId: 'dhcp'
    }],

    initComponent: function () {
        var me = this;
        Ext.apply(me, {
            store: Ext.create('OMV.data.Store', {
                autoLoad: true,
                model: OMV.data.Model.createImplicit({
                    idProperty: 'netname',
                    fields: [
                        { name: 'netname', type: 'string' },
                        { name: 'active', type: 'string' },
                        { name: 'ip', type: 'string' },
                        { name: 'iprange', type: 'string' },
                        { name: 'forward', type: 'string' },
                        { name: 'dhcp', type: 'string' }
                    ]
                }),
                proxy: {
                    type: 'rpc',
                    rpcData: {
                        service: 'Kvm',
                        method: 'getNetworkList'
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
            text: _('Add Network'),
            icon: 'images/add.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            handler: Ext.Function.bind(me.onAddButton, me, [ me ]),
            scope: me
        },{
            xtype: 'button',
            text: _('Add Macvtap Bridge'),
            icon: 'images/add.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            handler: Ext.Function.bind(me.onMacvtapButton, me, [ me ]),
            scope: me
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
                        var state = records[0].get("active");
                        return (state !== 'Active');
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
                        var state = records[0].get("active");
                        return (state == 'Active');
                    }
                }
            }]
        },{
            xtype: 'button',
            text: _('Delete'),
            icon: 'images/delete.png',
            iconCls: Ext.baseCSSPrefix + 'btn-icon-16x16',
            handler: Ext.Function.bind(me.onCommandButton, me, [ 'delete' ]),
            scope: me,
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1,
                enabledFn: function(c, records) {
                    var state = records[0].get("active");
                    return (state !== 'Active');
                }
            }
        }]);
        return items;
    },

    onAddButton: function() {
        var me = this;
        Ext.create("OMV.module.admin.service.kvm.Network", {
            title: _("Add network"),
            uuid: OMV.UUID_UNDEFINED,
            listeners: {
                scope: me,
                submit: function() {
                    this.doReload();
                }
            }
        }).show();
    },

    onMacvtapButton: function() {
        var me = this;
        Ext.create("OMV.module.admin.service.kvm.Macvtap", {
            title: _("Add macvtap bridge"),
            uuid: OMV.UUID_UNDEFINED,
            listeners: {
                scope: me,
                submit: function() {
                    this.doReload();
                }
            }
        }).show();
    },

    onCommandButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        OMV.Rpc.request({
            scope: me,
            rpcData: {
                service: "Kvm",
                method: "networkCommand",
                params: {
                    name: record.get("netname"),
                    command: cmd
                }
            }
        });
        me.doReload();
    }
});

OMV.WorkspaceManager.registerPanel({
    id: 'networks',
    path: '/service/kvm',
    text: _('Networks'),
    position: 50,
    className: 'OMV.module.admin.service.kvm.Networks'
});
