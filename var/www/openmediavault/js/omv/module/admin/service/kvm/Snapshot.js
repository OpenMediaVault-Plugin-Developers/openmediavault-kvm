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

Ext.define("OMV.module.admin.service.kvm.SnapRevert", {
    extend: "OMV.workspace.window.Form",
    uses: [
        "OMV.data.Model",
        "OMV.data.Store"
    ],

    rpcService: "Kvm",
    rpcSetMethod: "revertSnapshot",

    getFormItems: function() {
        var me = this;
        return [{
            xtype: "combo",
            name: "snapname",
            fieldLabel: _("Snapshot"),
            emptyText: _("Select a snapshot ..."),
            editable: false,
            triggerAction: "all",
            displayField: "description",
            valueField: "snapname",
            allowNone: true,
            allowBlank: true,
            store: Ext.create("OMV.data.Store", {
                autoLoad: true,
                model: OMV.data.Model.createImplicit({
                    idProperty: "snapname",
                    fields: [
                        { name: "snapname", type: "string" },
                        { name: "description", type: "string" }
                    ]
                }),
                proxy: {
                    type: "rpc",
                    rpcData: {
                        service: "Kvm",
                        method: "enumerateSnapshots",
                        params: {
                            "vmname": me.vmname
                        }
                    },
                    appendSortParams: false
               }
           })
        },{
            xtype: "hidden",
            name: "vmname",
            value: me.vmname
        }];
    }
});

Ext.define("OMV.module.admin.service.kvm.SnapDelete", {
    extend: "OMV.workspace.window.Form",
    uses: [
        "OMV.data.Model",
        "OMV.data.Store"
    ],

    rpcService: "Kvm",
    rpcSetMethod: "deleteSnapshot",

    getFormItems: function() {
        var me = this;
        return [{
            xtype: "combo",
            name: "snapname",
            fieldLabel: _("Snapshot"),
            emptyText: _("Select a snapshot ..."),
            editable: false,
            triggerAction: "all",
            displayField: "description",
            valueField: "snapname",
            allowNone: true,
            allowBlank: true,
            store: Ext.create("OMV.data.Store", {
                autoLoad: true,
                model: OMV.data.Model.createImplicit({
                    idProperty: "snapname",
                    fields: [
                        { name: "snapname", type: "string" },
                        { name: "description", type: "string" }
                    ]
                }),
                proxy: {
                    type: "rpc",
                    rpcData: {
                        service: "Kvm",
                        method: "enumerateSnapshots",
                        params: {
                            "vmname": me.vmname
                        }
                    },
                    appendSortParams: false
               }
           })
        },{
            xtype: "hidden",
            name: "vmname",
            value: me.vmname
        }];
    }
});
