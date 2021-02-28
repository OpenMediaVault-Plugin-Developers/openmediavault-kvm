/**
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2013-2021 OpenMediaVault Plugin Developers
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
// require("js/omv/module/admin/diagnostic/log/plugin/Plugin.js")
// require("js/omv/util/Format.js")

Ext.define("OMV.module.admin.diagnostic.log.plugin.Kvm", {
    extend: "OMV.module.admin.diagnostic.log.plugin.Plugin",
    alias: "omv.plugin.diagnostic.log.kvm",

    id: "kvm",
    text: _("KVM"),
    stateful: true,
    stateId: "756b6f62-79d7-11eb-aff0-a393aec26966",
    columns: [{
        text: _("Date & Time"),
        sortable: true,
        dataIndex: "date",
        stateId: "date",
        flex: 1
    },{
        text: _("Component"),
        sortable: true,
        dataIndex: "component",
        stateId: "component",
        flex: 1
    },{
        text: _("Call"),
        sortable: true,
        dataIndex: "call",
        stateId: "call",
        flex: 1
    },{
        text: _("Message"),
        sortable: true,
        dataIndex: "message",
        stateId: "message",
        flex: 1
    }],
    rpcParams : {
        id: "kvm"
    },
    rpcFields: [
        { name: "date", type: "string" },
        { name: "component", type: "string" },
        { name: "call", type: "string" },
        { name: "message", type: "string" },
    ]
});
