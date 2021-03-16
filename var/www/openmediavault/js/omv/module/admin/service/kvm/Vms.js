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
// require("js/omv/module/admin/service/kvm/Disk.js")
// require("js/omv/module/admin/service/kvm/Optical.js")
// require("js/omv/module/admin/service/kvm/Snapshot.js")
// require("js/omv/module/admin/service/kvm/Usb.js")
// require("js/omv/module/admin/service/kvm/VcpuMemory.js")
// require("js/omv/module/admin/service/kvm/Vm.js")

Ext.define("OMV.module.admin.service.kvm.VmXml", {
    extend: "OMV.workspace.window.Form",
    uses: [
        "OMV.data.Model",
        "OMV.data.Store",
    ],

    rpcService: "Kvm",
    rpcGetMethod: "getVmXml",
    rpcSetMethod: "setVmXml",

    getFormItems: function() {
        var me = this;
        return [{
            xtype: "textarea",
            cls: Ext.baseCSSPrefix + "form-textarea-monospaced",
            name: "vmxml",
            fieldLabel: _("XML"),
            height: 500,
            plugins: [{
                ptype: "fieldinfo",
                text: _("For more info") + "  <a href='https://libvirt.org/formatdomain.html' target='_blank'>https://libvirt.org/formatdomain.html</a>"
            }]
        },{
            xtype: "hidden",
            name: "vmname",
            value: me.vmname
        }];
    }
});

Ext.define('OMV.module.admin.service.kvm.Vms', {
    extend: 'OMV.workspace.grid.Panel',
    requires: [
        'OMV.data.Store',
        'OMV.data.Model',
        'OMV.data.proxy.Rpc'
    ],

    autoReload: false,
    rememberSelected: true,
    disableLoadMaskOnLoad: true,
    hideAddButton: true,
    hideEditButton: true,
    hideDeleteButton: true,
    hidePagingToolbar: false,
    stateful: true,
    stateId: '95b0cc44-6998-11eb-9bad-97c000edb30b',
    columns: [{
        xtype: "booleaniconcolumn",
        text: _("AutoStart"),
        sortable: true,
        dataIndex: "autostart",
        stateId: "autostart",
        align: "center",
        width: 80,
        resizable: false,
        trueIcon: "switch_on.png",
        falseIcon: "switch_off.png"
    },{
        xtype: 'textcolumn',
        text: _('VM Name'),
        sortable: true,
        dataIndex: 'vmname',
        stateId: 'vmname'
    },{
        xtype: 'textcolumn',
        text: _('State'),
        sortable: true,
        dataIndex: 'state',
        stateId: 'state'
    },{
        xtype: 'textcolumn',
        text: _('vCPU'),
        sortable: true,
        dataIndex: 'cpu',
        stateId: 'cpu'
    },{
        xtype: 'binaryunitcolumn',
        text: _('Memory'),
        sortable: true,
        dataIndex: 'mem',
        stateId: 'mem'
    },{
        xtype: 'textcolumn',
        text: _('Disk(s)'),
        sortable: true,
        dataIndex: 'disks',
        stateId: 'disks'
    },{
        xtype: 'textcolumn',
        text: _('Snapshots'),
        sortable: true,
        dataIndex: 'snaps',
        stateId: 'snaps'
    },{
        xtype: 'textcolumn',
        text: _('Spice port'),
        sortable: true,
        dataIndex: 'spiceport',
        stateId: 'spiceport'
    },{
        xtype: 'textcolumn',
        text: _('VNC port'),
        sortable: true,
        dataIndex: 'vncport',
        stateId: 'vncport'
    },{
        xtype: 'textcolumn',
        text: _('noVNC Docker URL'),
        sortable: true,
        dataIndex: 'dockerurl',
        stateId: 'dockerurl',
        renderer: function(val) {
            var url = val; 
            if (url != 'n/a') {
                url = ('<a href="' + url + '" target="_blank">' + url + '</a>');
            }
            return (url);
        },
        flex: 1
    },{
        xtype: 'textcolumn',
        text: _('Docker port'),
        sortable: true,
        dataIndex: 'dockerport',
        stateId: 'dockerport'
    },{
        xtype: 'textcolumn',
        text: _('Arch'),
        sortable: true,
        dataIndex: 'arch',
        stateId: 'arch'
    }],

    initComponent: function () {
        var me = this;
        Ext.apply(me, {
            store: Ext.create('OMV.data.Store', {
                autoLoad: true,
                model: OMV.data.Model.createImplicit({
                    idProperty: 'uuid',
                    fields: [
                        { name: 'vmname', type: 'string' },
                        { name: 'mem', type: 'string' },
                        { name: 'cpu', type: 'string' },
                        { name: 'state', type: 'string' },
                        { name: 'arch', type: 'string' },
                        { name: 'disks', type: 'string' },
                        { name: 'vncport', type: 'string' },
                        { name: 'spiceport', type: 'string' },
                        { name: 'dockerport', type: 'string' },
                        { name: 'dockerurl', type: 'string' },
                        { name: 'autostart', type: 'boolean' },
                        { name: 'snaps', type: 'integer' }
                    ]
                }),
                proxy: {
                    type: 'rpc',
                    rpcData: {
                        service: 'Kvm',
                        method: 'getVmList'
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
            xtype: "button",
            text: _("Add"),
            scope: this,
            icon: "images/add.png",
            handler: Ext.Function.bind(me.onAddButton, me, [ me ])
        },{
            xtype: "button",
            text: _("Devices"),
            scope: this,
            icon: "images/details.png",
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1
            },
            menu: [{
                text: _("Add Disk"),
                icon: "images/add.png",
                handler: Ext.Function.bind(me.onDiskButton, me, [ "add" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1
                }
            },{
                text: _("Resize Disk"),
                icon: "images/expand.png",
                handler: Ext.Function.bind(me.onDiskButton, me, [ "resize" ]),
                hidden: true
            },{
                text: _("Remove Disk"),
                icon: "images/minus.png",
                handler: Ext.Function.bind(me.onDiskButton, me, [ "remove" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var snaps = records[0].get("snaps");
                        return (snaps == 0);
                    }
                }
            },{
                xtype: 'menuseparator'
            },{
                text: _("Add Optical"),
                icon: "images/add.png",
                handler: Ext.Function.bind(me.onOpticalButton, me, [ "add" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1
                }
            },{
                text: _("Remove Optical"),
                icon: "images/eject.png",
                handler: Ext.Function.bind(me.onOpticalButton, me, [ "remove" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var snaps = records[0].get("snaps");
                        return (snaps == 0);
                    }
                }
            },{
                xtype: 'menuseparator'
            },{
                text: _("Add Network"),
                icon: "images/network.png",
                handler: Ext.Function.bind(me.onNicButton, me, [ "add" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1
                }
            },{
                text: _("Remove Network"),
                icon: "images/minus.png",
                handler: Ext.Function.bind(me.onNicButton, me, [ "remove" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1
                }
            },{
                xtype: 'menuseparator'
            },{
                text: _("Add USB"),
                icon: "images/share.png",
                handler: Ext.Function.bind(me.onUsbButton, me, [ "add" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1
                }
            },{
                text: _("Remove USB"),
                icon: "images/eject.png",
                handler: Ext.Function.bind(me.onUsbButton, me, [ "remove" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var snaps = records[0].get("snaps");
                        return (snaps == 0);
                    }
                }
            }]
        },{
            xtype: "button",
            text: _("Edit"),
            scope: this,
            icon: "images/edit.png",
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1,
                enabledFn: function(c, records) {
                    var state = records[0].get("state");
                    return (state == 'shutoff');
                }
            },
            menu: [{
                text: _("vCPU"),
                icon: "images/details.png",
                handler: Ext.Function.bind(me.onEditButton, me, [ "vcpu" ])
            },{
                text: _("Memory"),
                icon: "images/details.png",
                handler: Ext.Function.bind(me.onEditButton, me, [ "memory" ])
            }]
        },{
            xtype: "button",
            text: _("State"),
            scope: this,
            icon: "images/pulse.png",
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1
            },
            menu: [{
                text: _("Start"),
                icon: "images/play.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "poweron" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var state = records[0].get("state");
                        return (state !== 'running');
                    }
                }
            },{
                text: _("Reboot"),
                icon: "images/reboot.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "reboot" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var state = records[0].get("state");
                        return (state == 'running');
                    }
                }
            },{
                text: _("Stop"),
                icon: "images/shutdown.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "poweroff" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var state = records[0].get("state");
                        return (state == 'running');
                    }
                }
            },{
                text: _("Force"),
                icon: "images/arrow-down.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "force" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1
                }
            },{
                xtype: 'menuseparator'
            },{
                text: _("Pause"),
                icon: "images/pause.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "pause" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var state = records[0].get("state");
                        return (state == 'running');
                    }
                }
            },{
                text: _("Resume"),
                icon: "images/play.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "resume" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var state = records[0].get("state");
                        return (state == 'paused');
                    }
                }
            },{
                xtype: 'menuseparator'
            },{
                text: _("Delete"),
                icon: "images/minus.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "undefine" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var state = records[0].get("state");
                        var snaps = records[0].get("snaps");
                        return (state == 'shutoff' && snaps == 0);
                    }
                }
            },{
                text: _("Delete + storage"),
                icon: "images/delete.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "undefineplus" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var state = records[0].get("state");
                        var snaps = records[0].get("snaps");
                        return (state == 'shutoff' && snaps == 0);
                    }
                }
            }]
        },{
            xtype: "button",
            text: _("AutoStart"),
            scope: this,
            icon: "images/cron.png",
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1
            },
            menu: [{
                text: _("Enable autostart"),
                icon: "images/play.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "autostart_enable" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var autostart = records[0].get("autostart");
                        return (!autostart);
                    }
                }
            },{
                text: _("Disable autostart"),
                icon: "images/pause.png",
                handler: Ext.Function.bind(me.onCommandButton, me, [ "autostart_disable" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var autostart = records[0].get("autostart");
                        return (autostart);
                    }
                }
            }]
        },{
            xtype: "button",
            text: _("Snapshots"),
            scope: this,
            icon: "images/filesystem.png",
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1
            },
            menu: [{
                text: _("Create"),
                icon: "images/add.png",
                handler: Ext.Function.bind(me.onSnapButton, me, [ "snap_create" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                }
            },{
                text: _("Revert"),
                icon: "images/undo.png",
                handler: Ext.Function.bind(me.onSnapButton, me, [ "snap_revert" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var snaps = records[0].get("snaps");
                        return (snaps > 0);
                    }
                }
            },{
                text: _("Delete"),
                icon: "images/delete.png",
                handler: Ext.Function.bind(me.onSnapButton, me, [ "snap_delete" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var snaps = records[0].get("snaps");
                        return (snaps > 0);
                    }
                }
            }]
        },{
            xtype: "button",
            text: _("Console"),
            scope: this,
            icon: "images/details.png",
            disabled : true,
            selectionConfig : {
                minSelections : 1,
                maxSelections : 1
            },
            menu: [{
                text: _("Create noVNC console"),
                icon: "images/book.png",
                handler: Ext.Function.bind(me.onVncButton, me, [ "start" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var url = records[0].get("dockerurl");
                        var port = records[0].get("vncport");
                        var regex = /[a-zA-Z]/g; 
                        if (regex.test(port)) {
                            port = -1;
                        }
                        return (url.indexOf("vnc.html") < 0 && port >= 1024);
                    }
                }
            },{
                text: _("Stop noVNC console"),
                icon: "images/delete.png",
                handler: Ext.Function.bind(me.onVncButton, me, [ "stop" ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1,
                    enabledFn: function(c, records) {
                        var url = records[0].get("dockerurl");
                        return (url.indexOf("vnc.html") > 0);
                    }
                }
            }]
        },{
            xtype: "button",
            text: _("Advanced"),
            scope: this,
            icon: "images/details.png",
            menu: [{
                text: _("Add XML"),
                icon: "images/add.png",
                handler: Ext.Function.bind(me.onVmXmlButton, me, [ 'add' ])
            },{
                text: _("Edit XML"),
                icon: "images/edit.png",
                handler: Ext.Function.bind(me.onVmXmlButton, me, [ 'edit' ]),
                disabled : true,
                selectionConfig : {
                    minSelections : 1,
                    maxSelections : 1
                }
            }]
        }]);
        return items;
    },

    onAddButton: function() {
        var me = this;
        Ext.create("OMV.module.admin.service.kvm.Vm", {
            title: _("Add VM"),
            uuid: OMV.UUID_UNDEFINED,
            listeners: {
                scope: me,
                submit: function() {
                    this.doReload();
                }
            }
        }).show();
    },

    onVmXmlButton: function(cmd) {
        var me = this;
        var msg = "";
        var vmname = ""; 
        if (cmd == "edit") {
            msg = "Edit VM XML";
            var record = me.getSelected();
            vmname = record.get("vmname");
        } else {
            msg = "Add VM with XML";
        }
        Ext.create("OMV.module.admin.service.kvm.VmXml", {
            title: msg,
            vmname: vmname,
            rpcGetParams: {
                vmname: vmname
            },
            listeners: {
                scope: me,
                submit: function() {
                    this.doReload();
                }
            }
        }).show();
    },

    onVncButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        var vncport = parseInt(record.get("vncport"));
        var vmname = record.get("vmname");
        var hostport = 0;
        if (cmd == "start") {
            hostport = parseInt(prompt("Enter host port", "8081"));
            if (vncport < 1024) {
                alert("VM does not appear to have a VNC port or is powered off. " + vncport);
                return;
            } else if(hostport < 1024) {
                alert("Port for host is too low.");
                return;
            }
        }
        OMV.Rpc.request({
            scope: me,
            rpcData: {
                service: "Kvm",
                method: "doVnc",
                params: {
                    hostport: hostport,
                    vmname: vmname,
                    vncport: vncport
                }
            }
        });
        me.doReload();
    },

    onEditButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        var vmname = record.get("vmname");
        if (cmd == "vcpu") {
            var curvcpu = record.get("cpu");
            Ext.create("OMV.module.admin.service.kvm.Vcpu", {
                title: _("Change vCPU quantity ..."),
                vmname: vmname,
                vcpu: curvcpu,
                rpcGetParams: {
                    vmname: vmname,
                    vcpu: curvcpu
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        } else if (cmd == "memory") {
            var curmem = record.get("mem") / 1024 / 1024;
            Ext.create("OMV.module.admin.service.kvm.Memory", {
                title: _("Change memory ..."),
                vmname: vmname,
                memory: curmem,
                rpcGetParams: {
                    vmname: vmname,
                    memory: curmem
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        }
        me.doReload();
    },

    onDiskButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        var vmname = record.get("vmname");
        if (cmd == "add") {
            Ext.create("OMV.module.admin.service.kvm.AddDisk", {
                title: _("Add disk ..."),
                vmname: vmname,
                rpcGetParams: {
                    vmname: vmname
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        } else if (cmd == "resize") {
            Ext.create("OMV.module.admin.service.kvm.ResizeDisk", {
                title: _("Resize disk ..."),
                vmname: vmname,
                rpcGetParams: {
                    vmname: vmname
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        } else if (cmd == "remove") {
            Ext.create("OMV.module.admin.service.kvm.RemoveDisk", {
                title: _("Delete disk ..."),
                vmname: vmname,
                rpcGetParams: {
                    vmname: vmname
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        }
    },

    onNicButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        var vmname = record.get("vmname");
        if (cmd == "add") {
            Ext.create("OMV.module.admin.service.kvm.AddVmNic", {
                title: _("Add network adapter ..."),
                vmname: vmname,
                rpcGetParams: {
                    vmname: vmname
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        } else if (cmd == "remove") {
            Ext.create("OMV.module.admin.service.kvm.RemoveVmNic", {
                title: _("Remove network adapter ..."),
                vmname: vmname,
                rpcGetParams: {
                    vmname: vmname
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        }
    },

    onOpticalButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        var vmname = record.get("vmname");
        if (cmd == "add") {
            Ext.create("OMV.module.admin.service.kvm.AddOptical", {
                title: _("Add optical disk ..."),
                vmname: vmname,
                rpcGetParams: {
                    vmname: vmname
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        } else if (cmd == "remove") {
            Ext.create("OMV.module.admin.service.kvm.RemoveOptical", {
                title: _("Remove optical disk ..."),
                vmname: vmname,
                rpcGetParams: {
                    vmname: vmname
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        }
    },

    onSnapButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        var vmname = record.get("vmname");
        if (cmd == "snap_create") {
            OMV.Rpc.request({
                scope: me,
                rpcData: {
                    service: "Kvm",
                    method: "addSnapshot",
                    params: {
                        vmname: vmname
                    }
                }
            });
            me.doReload();
        } else if (cmd == "snap_revert") {
            Ext.create("OMV.module.admin.service.kvm.SnapRevert", {
                title: _("Revert to snapshot ..."),
                vmname: vmname,
                rpcGetParams: {
                    vmname: vmname
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        } else if (cmd == "snap_delete") {
            Ext.create("OMV.module.admin.service.kvm.SnapDelete", {
                title: _("Delete snapshot ..."),
                vmname: vmname,
                rpcGetParams: {
                    vmname: vmname
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        }
    },

    onUsbButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        var vmname = record.get("vmname");
        if (cmd == "add") {
            Ext.create("OMV.module.admin.service.kvm.AddUsb", {
                title: _("Add USB device ..."),
                vmname: vmname,
                rpcGetParams: {
                    vmname: vmname
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        } else if (cmd == "remove") {
            Ext.create("OMV.module.admin.service.kvm.RemoveUsb", {
                title: _("Remove USB device ..."),
                vmname: vmname,
                rpcGetParams: {
                    vmname: vmname
                },
                listeners: {
                    scope: me,
                    submit: function() {
                        this.doReload();
                    }
                }
            }).show();
        }
    },

    onCommandButton: function(cmd) {
        var me = this;
        var record = me.getSelected();
        var vmname = record.get("vmname");
        if (cmd == "stop" || cmd == "force") {
            var url = record.get("dockerurl");
            if (url.indexOf("vnc.html") > 0) {
                me.onVncButton("stop");
            }
        } else if (cmd == "undefineplus") {
            hostdelete = prompt("confirm VM name to delete", "");
            if (hostdelete != vmname) {
                alert(_("Wrong VM name entered! Skipping delete ..."));
                return;
            }
        }
        OMV.Rpc.request({
            scope: me,
            rpcData: {
                service: "Kvm",
                method: "doCommand",
                params: {
                    name: vmname,
                    command: cmd
                }
            }
        });
        me.doReload();
    }
});

OMV.WorkspaceManager.registerPanel({
    id: 'vms',
    path: '/service/kvm',
    text: _('VMs'),
    position: 10,
    className: 'OMV.module.admin.service.kvm.Vms'
});
