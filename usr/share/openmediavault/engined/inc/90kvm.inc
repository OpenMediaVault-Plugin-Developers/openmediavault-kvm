<?php
/**
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @copyright Copyright (c) 2013-2024 OpenMediaVault Plugin Developers
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
 require_once("openmediavault/functions.inc");

\OMV\System\LogFileSpec::registerSpecification("kvm", [
    "filename" => "openmediavault-kvm.log",
    "filepath" => "/var/log/openmediavault-kvm.log",
    "regex" => "/^\[((.*?)\s+(.*?))\s+(.*?)\]:\s+(.*?):\s+(.*?)$/",
    "columns" => [
        "date" => 1,
        "component" => 4,
        "call" => 5,
        "message" => 6
    ]
]);

\OMV\System\LogFileSpec::registerSpecification("omv-backup-vm", [
    "filename" => "omv-backup-vm.log",
    "filepath" => "/var/log/omv-backup-vm.log",
    "regex" => "/^\[((.*?)\s+(.*?))\]\s+\[(.*?)\]\s+(.*?)$/",
    "columns" => [
        "date" => 1,
        "action" => 4,
        "message" => 5
    ]
]);
