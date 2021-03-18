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

Ext.define('OMV.module.admin.service.kvm.Isos', {
    extend: 'OMV.workspace.grid.Panel',
    requires: [
        'OMV.data.Store',
        'OMV.data.Model',
        'OMV.data.proxy.Rpc'
    ],

    autoReload: true,
    rememberSelected: true,
    disableLoadMaskOnLoad: true,
    hidePagingToolbar: false,
    hideAddButton: true,
    hideEditButton: true,
    hideDeleteButton: true,
    stateful: true,
    stateId: '94ac20f2-7783-11eb-934f-ff8b5251b0ef',
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
        xtype: 'binaryunitcolumn',
        text: _('Size'),
        sortable: true,
        dataIndex: 'capacity',
        stateId: 'capacity'
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
                        { name: 'capacity', type: 'string' },
                        { name: 'path', type: 'string' }
                    ]
                }),
                proxy: {
                    type: 'rpc',
                    rpcData: {
                        service: 'Kvm',
                        method: 'getVolumeList',
                        params: {
                            "optical": true
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
        }]);
        return items;
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
    }
});

OMV.WorkspaceManager.registerPanel({
    id: 'isos',
    path: '/service/kvm',
    text: _('ISOs'),
    position: 40,
    className: 'OMV.module.admin.service.kvm.Isos'
});
