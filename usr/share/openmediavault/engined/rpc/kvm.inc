<?php
/*
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2021-2025 openmediavault plugin developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
// libvirt-php api reference - https://libvirt.org/php/api-reference.html
// libvirt-php examples used for reference - https://github.com/libvirt/libvirt-php/tree/master/examples
// VMDashboard code used for reference - https://github.com/VMDashboard

require_once('openmediavault/libvirt.php');
require_once("openmediavault/functions.inc");

class OMVRpcServiceKvm extends \OMV\Rpc\ServiceAbstract
{

    private $imageCache = "/var/cache/openmediavault/lxc_image_cache";

    public function getName()
    {
        return 'Kvm';
    }

    public function initialize()
    {
        $this->registerMethod('getNetworkList');
        $this->registerMethod('enumerateNetworks');
        $this->registerMethod('enumerateBridges');
        $this->registerMethod('getNetworkXml');
        $this->registerMethod('setNetwork');
        $this->registerMethod('setMacvtap');
        $this->registerMethod('networkCommand');

        $this->registerMethod('addVmNic');
        $this->registerMethod('removeVmNic');
        $this->registerMethod('enumerateVmNic');

        $this->registerMethod('getPoolList');
        $this->registerMethod('enumeratePools');
        $this->registerMethod('getPool');
        $this->registerMethod('setPool');
        $this->registerMethod('deletePool');
        $this->registerMethod('poolCommand');

        $this->registerMethod('getVmList');
        $this->registerMethod('getVmNameStateList');
        $this->registerMethod('getLxcNameStateList');
        $this->registerMethod('getVmNameList');
        $this->registerMethod('getVmXml');
        $this->registerMethod('setVmXml');
        $this->registerMethod('getVmDetails');
        $this->registerMethod('setVm');
        $this->registerMethod('getDisk');

        $this->registerMethod('cloneVm');
        $this->registerMethod('createBacking');
        $this->registerMethod('createLinkedClone');

        $this->registerMethod('addDisk');
        $this->registerMethod('addHostDisk');
        $this->registerMethod('addOptical');
        $this->registerMethod('addHostOptical');
        $this->registerMethod('resizeDisk');
        $this->registerMethod('changeDisc');
        $this->registerMethod('ejectDisc');
        $this->registerMethod('removeDisk');
        $this->registerMethod('enumerateHostDisk');
        $this->registerMethod('enumerateHostOptical');

        $this->registerMethod('getVolumeList');
        $this->registerMethod('enumerateVolumes');
        $this->registerMethod('enumerateVolumesByVm');
        $this->registerMethod('getVolume');
        $this->registerMethod('setVolume');
        $this->registerMethod('deleteVolume');
        $this->registerMethod('convertVolume');
        $this->registerMethod('volumeCommand');
        $this->registerMethod('downloadIso');
        $this->registerMethod('addFsPass');
        $this->registerMethod('removeFsPass');
        $this->registerMethod('enumerateFsPassByVm');

        $this->registerMethod('enumerateSnapshots');
        $this->registerMethod('addSnapshot');
        $this->registerMethod('deleteSnapshot');
        $this->registerMethod('revertSnapshot');

        $this->registerMethod('addUsb');
        $this->registerMethod('removeUsb');
        $this->registerMethod('enumerateUsbByHost');
        $this->registerMethod('enumerateUsbByVm');

        $this->registerMethod('addPci');
        $this->registerMethod('removePci');
        $this->registerMethod('enumeratePciByHost');
        $this->registerMethod('enumeratePciByVm');

        $this->registerMethod('addVnc');
        $this->registerMethod('removeVnc');

        $this->registerMethod('getNotes');
        $this->registerMethod('setNotes');

        $this->registerMethod('doCommand');
        $this->registerMethod('doWeb');
        $this->registerMethod('doChangeCpuMemory');
        $this->registerMethod('getVcpu');

        $this->registerMethod('getBackupList');
        $this->registerMethod('deleteBackup');
        $this->registerMethod('doBackup');

        $this->registerMethod('getRestoreList');
        $this->registerMethod('doRestore');

        $this->registerMethod('getJobList');
        $this->registerMethod('getJob');
        $this->registerMethod('setJob');
        $this->registerMethod('deleteJob');
        $this->registerMethod('doJob');

        $this->registerMethod('getVirtTop');

        $this->registerMethod('enumerateArchitectures');
        $this->registerMethod('enumerateCpus');
        $this->registerMethod('enumerateOses');
        $this->registerMethod('enumerateVg');

        $this->registerMethod('enumerateImages');
        $this->registerMethod('forceImageListRefresh');

        $this->registerMethod('getSettings');
        $this->registerMethod('setSettings');
    }

    private function connectQemu()
    {
        $lv = new Libvirt('/var/log/openmediavault-kvm.log');
        $lv->connect('qemu:///system');
        return ($lv);
    }

    private function connectLxc()
    {
        $lv = new Libvirt('/var/log/openmediavault-kvm.log');
        $lv->connect('lxc:///');
        return ($lv);
    }

    private function escapeName($name)
    {
        $ename = htmlspecialchars_decode($name);
        $ename = str_replace(['&#34;','&#39;'], "'", $ename);
        $ename = addslashes($ename);
        return ($ename);
    }

    private function getArch()
    {
        $cmd = new \OMV\System\Process('dpkg --print-architecture');
        $cmd->execute($output);
        return (strtolower($output[0]));
    }

    private function getPoolType($pool) {
        $poolFile = sprintf('/etc/libvirt/storage/%s.xml', $pool);
        $poolXml = new SimpleXMLElement(file_get_contents($poolFile));
        return (trim($poolXml['type']));
    }

    private function getVolumeSizes($path)
    {
        $sizes = 'n/a';
        if (file_exists($path)) {
            $output = [];
            $cmdArgs = [];
            $cmdArgs[] = '/usr/bin/qemu-img';
            $cmdArgs[] = 'info';
            $cmdArgs[] = sprintf("'%s'", $path);
            $cmdArgs[] = '| grep -E \'virtual size|disk size\'';
            $cmdArgs[] = '| awk \'{ print $3" "$4 }\'';
            $cmd = new \OMV\System\Process($cmdArgs);
            $cmd->execute($output);
            if (empty($output[0])) {
                $sizes = '(n/a)';
            } else {
                $sizes = sprintf('(%s :: %s)', $output[1], $output[0]);
            }
        }
        return $sizes;
    }

    public function getNetworkList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
        // create new libvirt object
        $lv = $this->connectQemu();
        $nets = $lv->get_networks(VIR_NETWORKS_ALL);
        $objects= [];
        foreach ($nets as $net) {
            $info = $lv->get_network_information($net);
            if (array_key_exists('forwarding', $info) && $info['forwarding'] != 'None') {
                if (array_key_exists('forward_dev', $info)) {
                    $forward = $info['forwarding'].' to '.$info['forward_dev'];
                } else {
                    $forward = $info['forwarding'];
                }
            }
            $dhcp = 'Disabled';
            if (array_key_exists('dhcp_start', $info) && array_key_exists('dhcp_end', $info)) {
                $dhcp = $info['dhcp_start'].' - '.$info['dhcp_end'];
            }
            $objects[] = [
                'netname' => $info['name'],
                'active' => $info['active'] ? 'Active' : 'Inactive',
                'ip' => $info['ip'],
                'iprange' => $info['ip_range'],
                'forward' => $forward,
                'dhcp' => $dhcp
            ];
        }
        // Filter the result.
        return $this->applyFilter($objects, $params['start'], $params['limit'],
            $params['sortfield'], $params['sortdir']);
    }

    public function enumerateNetworks($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // create new libvirt object
        $lv = $this->connectQemu();
        $nets = $lv->get_networks(VIR_NETWORKS_ALL);
        $objects= [];
        foreach ($nets as $net) {
            $info = $lv->get_network_information($net);
            $objects[] = [
                'netname' => $info['name']
            ];
        }
        // Filter the result.
        return ($objects);
    }

    public function enumerateBridges($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $output = [];
        $cmdLine = 'ls /sys/class/net | grep ^br';
        $cmd = new \OMV\System\Process($cmdLine);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $objects = [];
        foreach ($output as $net) {
            $objects[] = [
                'bridge' => $net
            ];
        }
        // Filter the result.
        return ($objects);
    }

    public function getNetworkXml($params, $context)
    {
    }

    public function setNetwork($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.setnetwork');
        // create new libvirt object
        $lv = $this->connectQemu();
        $xml = "<network>";
        $xml .= sprintf("<name>%s</name>", $params['name']);
        $xml .= "<forward mode='nat'/>";
        $xml .= sprintf("<mac address='%s'/>", $params['macaddress']);
        $xml .= sprintf("<ip address='%s' netmask='%s'>", $params['gatewayip'], $params['subnet']);
        if ($params['dhcp']) {
            $xml .= "<dhcp>";
            $xml .= sprintf("<range start='%s' end='%s'/>", $params['startaddress'], $params['endaddress']);
            $xml .= "</dhcp>";
        }
        $xml .= "</ip>";
        $xml .= "</network>";
        if (!$lv->network_define_xml($xml)) {
            $msg = "Error defining network: " . $lv->get_last_error();
            $msg = filter_var($msg, FILTER_SANITIZE_SPECIAL_CHARS);
            throw new \OMV\Exception( gettext("Unable to create network - ") . $msg);
        }
    }

    public function setMacvtap($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.setmacvtap');
        // create new libvirt object
        $lv = $this->connectQemu();
        $name = $params['name'];
        $nic = $params['nic'];
        $xml = sprintf('<network><name>%s</name><forward mode="bridge"><interface dev="%s"/></forward></network>', $name, $nic);
        if (!$lv->network_define_xml($xml)) {
            throw new \OMV\Exception( gettext("Unable to create Macvap - ") . $lv->get_last_error());
        }
    }

    public function networkCommand($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.docommand');
        // create new libvirt object
        $lv = $this->connectQemu();
        $disable = "";
        switch ($params['command']) {
            case 'delete':
                $disable = " --disable";
                if (!$lv->network_undefine($params['name'])) {
                    throw new \OMV\Exception( gettext("Unable to delete network - ") . $lv->get_last_error());
                }
                break;
            case 'start':
                if (!$lv->set_network_active($params['name'], true)) {
                    throw new \OMV\Exception( gettext("Unable to start network - ") . $lv->get_last_error());
                }
                break;
            case 'stop':
                $disable = " --disable";
                if (!$lv->set_network_active($params['name'], false)) {
                    throw new \OMV\Exception( gettext("Unable to stop network - ") . $lv->get_last_error());
                }
                break;
        }
        // enable or disable autostart
        $cmdArgs = [];
        $cmdArgs[] = 'net-autostart';
        $cmdArgs[] = '--network';
        $cmdArgs[] = $this->escapeName($params['name']);
        $cmdArgs[] = $disable;
        $err = gettext("Failed to ") . $params['command'] . gettext(" the network!");
        $this->virshCommand($cmdArgs, $err);
    }

    public function addVmNic($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // create new libvirt object
        $lv = ($params['virttype'] == 'lxc' ? $this->connectLxc() : $this->connectQemu());
        // get VM object
        $vmname = $params['vmname'];
        $vm = $lv->get_domain_by_name($vmname);
        // add network
        $mac = $params['macaddress'];
        if (!$mac) {
            $mac = $lv->generate_random_mac_addr();
        }
        $model = $params['model'];
        $net = $params['network'];
        if ($model == "bridge") {
            $result = $this->addVmBridgeNic($vmname, $mac, $params['bridge']);
        } else {
            $result = $lv->domain_nic_add($vm, $mac, $net, $model);
        }
        if (!$result) {
            throw new \OMV\Exception( gettext("Unable to add network adapter to VM - ") . $lv->get_last_error());
        }
    }

    public function removeVmNic($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $mac = trim($params['network']);
        $vmname = trim($params['vmname']);
        // virsh domiflist
        $output = [];
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = 'domiflist';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($vmname));
        $cmdArgs[] = sprintf('| awk \'$5 == "%s" { print $2 }\'', $mac);
        $cmdArgs[] = '| awk NF';
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $type = $output[0];
        if (empty($type)) {
            throw new \OMV\Exception( gettext("Unable to get network adapter type - ") . $cmd->getCommandLine());
        }
        // virsh detach-interface
        $output = [];
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = 'detach-interface';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($vmname));
        $cmdArgs[] = sprintf('--type %s', $type);
        $cmdArgs[] = sprintf('--mac %s', $mac);
        $cmdArgs[] = '--persistent';
        $cmdArgs[] = '--config';
        if ($params['state'] == 'running') {
            $cmdArgs[] = '--live';
        }
        $err = gettext("Failed to remove network.");
        $this->virshCommand($cmdArgs, $err);
    }

    public function enumerateVmNic($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $vmname = trim($params['vmname']);
        // enumerate interfaces with virsh
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = 'domiflist';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($vmname));
        $cmdArgs[] = '| sed 1,2d';
        $cmdArgs[] = '| awk NF';
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $objects = [];
        foreach ($output as $nic) {
            if (empty($nic)) {
                continue;
            }
            $parts = preg_split('/\s+/', $nic, -1, PREG_SPLIT_NO_EMPTY);
            $type = $parts[1];
            $src = $parts[2];
            $mac = $parts[4];
            $desc= sprintf('%s - %s (%s)', $type, $src, $mac);
            $objects[] = [
                'mac' => $mac,
                'description' => $desc
            ];
        }
        return ($objects);
    }

    public function getPoolList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
        // create new libvirt object
        $lv = $this->connectQemu();
        $pools = $lv->get_storagepools();
        $objects= [];
        foreach ($pools as $pool) {
            $lv->storagepool_refresh($pool);
            $info = $lv->get_storagepool_info($pool);
            $capacity = $info['capacity'];
            if ($capacity <= 0) {
                $capacity = 0;
                $percentage = 0;
            } else {
                $percentage = ($info['allocation'] / $info['capacity']) * 100;
            }
            $objects[] = [
                'name' => $pool,
                'active' => $info['active'] ? 'Active' : 'Inactive',
                'volcount' => $info['volume_count'],
                'state' => $lv->translate_storagepool_state($info['state']),
                'capacity' => $capacity,
                'allocation' => $info['allocation'],
                'available' => $info['available'],
                'percentage' => $percentage,
                'path' => $info['path']
            ];
        }
        // Filter the result.
        return $this->applyFilter($objects, $params['start'], $params['limit'],
            $params['sortfield'], $params['sortdir']);
    }

    public function enumeratePools($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // create new libvirt object
        $lv = $this->connectQemu();
        $pools = $lv->get_storagepools();
        $objects= [];
        foreach ($pools as $pool) {
            $lv->storagepool_refresh($pool);
            $info = $lv->get_storagepool_info($pool);
            $objects[] = [
                'name' => $pool,
                'path' => $info['path']
            ];
        }
        // Filter the result.
        return ($objects);
    }

    public function getPool($params, $context)
    {
    }

    public function setPool($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.setpool');
        $host = $params['hostname'];
        $name = $params['name'];
        $path = $params['path'];
        $source = $params['sourcepath'];
        $type = $params['type'];
        $vg = $params['vg'];
        $zpool = $params['zpoolname'];
        // create new libvirt object
        $lv = $this->connectQemu();
        $xml = sprintf("<pool type='%s'>", $type);
        $xml .= sprintf("<name>%s</name>", $name);
        switch ($type) {
            case 'dir':
                $path = (substr($path,0) == '/' ? $path : '/' . $path);
                $path = str_replace('//', '/', $path);
                mkdir($path, 0755, true);

                $fsType = \OMV\System\Filesystem\Filesystem::getTypeByPath($path);

                if($fsType == "btrfs") {
                    $cmd = new \OMV\System\Process(sprintf("chattr +C %s", $path));
                    $output = [];
                    $exitStatus = NULL;
                    $cmd->execute($output, $exitStatus);

                    if (($exitStatus !== 0) || (!empty($output))) {
                        throw new \OMV\ExecException($cmd->getCommandLine(), $output, $exitStatus);
                    }
                }

                break;
            case 'fs':
                mkdir($path, 0755, true);
                $xml .= "<source>";
                $xml .= sprintf("<device path='%s'/>", $source);
                $xml .= "</source>";
                break;
            case 'netfs':
                mkdir($path, 0755, true);
                $xml .= "<source>";
                $xml .= sprintf("<host name='%s'/>", $host);
                $xml .= sprintf("<dir path='%s'/>", $source);
                $xml .= "<format type='auto'/>";
                $xml .= "</source>";
                break;
            case 'logical':
                $xml .= "<source>";
                $xml .= sprintf("<name>%s</name>", $vg);
                $xml .= "</source>";
                break;
            case 'disk':
                $xml .= "<source>";
                $xml .= sprintf("<device path='%s'/>", $source);
                $xml .= "</source>";
                break;
            case 'iscsi':
                $xml .= "<source>";
                $xml .= sprintf("<host name='%s'/>", $host);
                $xml .= sprintf("<device path='%s'/>", $source);
                $xml .= "</source>";
                break;
            case 'zfs':
                $xml .= "<source>";
                $xml .= sprintf("<name>%s</name>", $zpool);
                $xml .= sprintf("<device path='%s'/>", $path);
                $xml .= "</source>";
                break;
        }
        if ($type != 'zfs') {
            $xml .= "<target>";
            $xml .= sprintf("<path>%s</path>", $path);
            $xml .= "</target>";
        }
        $xml .= "</pool>";
        if (!$lv->storagepool_define_xml($xml)) {
            $msg = "Error creating pool: " . $lv->get_last_error();
            $msg .= PHP_EOL . PHP_EOL . $xml;
            throw new \OMV\Exception( gettext("Unable to create pool - ") . $msg);
        }
    }

    public function deletePool($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.name');
        // create new libvirt object
        $lv = $this->connectQemu();
        $res = $lv->get_storagepool_res($params['name']);
        if (!$lv->storagepool_undefine($res)) {
            throw new \OMV\Exception( gettext("Unable to delete pool - ") . $lv->get_last_error());
        }
    }

    public function poolCommand($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.docommand');
        $command = $params['command'];
        $pool = $params['name'];
        $disable = '';
        // start or stop pool
        switch ($command) {
            case 'start':
                $cmdArgs = [];
                $cmdArgs[] = 'pool-start';
                $cmdArgs[] = $this->escapeName($pool);
                $err = gettext("Failed to start pool.");
                $this->virshCommand($cmdArgs, $err);
                break;
            case 'stop':
                $cmdArgs = [];
                $cmdArgs[] = 'pool-destroy';
                $cmdArgs[] = $this->escapeName($pool);
                $err = gettext("Failed to start pool.");
                $this->virshCommand($cmdArgs, $err);
                $disable = ' --disable';
                break;
        }
        // enable or disable autostart
        $cmdArgs = [];
        $cmdArgs[] = 'pool-autostart';
        $cmdArgs[] = $this->escapeName($pool);
        $cmdArgs[] = $disable;
        $err = gettext("Failed to set pool autostart.");
        $this->virshCommand($cmdArgs, $err);
    }



    public function getVmList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
        // get hostname and domain name
        $db = \OMV\Config\Database::getInstance();
        $dns = $db->get('conf.system.network.dns');
        $fqdn = $dns->get('hostname');
        $dn = $dns->get('domainname');
        if (strlen($dn) > 1) {
            $fqdn = sprintf('%s.%s', $fqdn, $dn);
        }
        $webadmin = $db->get("conf.webadmin");
        $ssl = boolval($webadmin->get("enablessl"));
        $forcessl = boolval($webadmin->get("forcesslonly"));
        $http = 'http';
        $vncField = 8;
        $remoteField = 7;
        if ($ssl) {
            $http = 'https';
            $vncField = $forcessl ? 13 : 12;
            $remoteField = $forcessl ? 12 : 11;
        }
        // create new libvirt object for VMs
        $lv = $this->connectQemu();
        $doms = $lv->get_domains();
        $objects= [];
        foreach ($doms as $name) {
            if (!$lv->domain_get_xml($name)) {
                continue;
            }
            $dom = $lv->get_domain_object($name);
            $domXml = new SimpleXMLElement($lv->domain_get_xml($name));
            $info = $lv->domain_get_info($dom);
            $diskcnt = $lv->get_disk_count($dom);
            if ($diskcnt > 0) {
                $dSize = $lv->get_disk_capacity($dom);
                $disks = sprintf('%d / %s', $diskcnt, $dSize);
            } else {
                $disks = 'n/a';
            }
            // vnc and novnc
            $vncexists = false;
            $xpath = $domXml->xpath('//graphics');
            $xpathSize = sizeof($xpath);
            for ($i = 0; $i < $xpathSize; $i++) {
                $graphics = $domXml->devices->graphics[$i];
                if ($graphics['type'] == 'vnc') {
                    $vncexists = true;
                    break;
                }
            }
            $vncport = $lv->domain_get_vnc_port($dom);
            if (! $vncport || $vncport < 1024 || $vncport == '') {
                $vncport = 'n/a';
            }
            $novncport = 'n/a';
            $novncurl = 'n/a';
            if (intval($vncport) >= 1024) {
                $output = [];
                $cmdLine = sprintf('pgrep -u openmediavault-kvmweb,root -a websockify | awk \'$%d == "localhost:%d" { print $%d }\'', $vncField, $vncport, $remoteField);
                $cmd = new \OMV\System\Process($cmdLine);
                $cmd->setQuiet(true);
                $cmd->execute($output, $exitStatus);
                $novncport = intval($output[0]);
                if ($novncport >= 1024) {
                    $novncurl = sprintf('<a href="%s://%s:%d/vnc.html?resize=remote&autoconnect=1" target="_blank">link</a>', $http, $fqdn, $novncport);
                }
            }
            if (!$lv->domain_get_xml($name)) {
                continue;
            }
            // spice and spice_html5
            $spiceport = '';
            $spiceexists = false;
            $xpath = $domXml->xpath('//graphics');
            $xpathSize = sizeof($xpath);
            for ($i = 0; $i < $xpathSize; $i++) {
                $graphics = $domXml->devices->graphics[$i];
                if ($graphics['type'] == 'spice') {
                    $spiceport = $graphics['port'];
                    $spiceexists = true;
                    break;
                }
            }
            if (!$spiceport || $spiceport < 1024 || $spiceport == '') {
                $spiceport = 'n/a';
            }
            $spicehtml5port = 'n/a';
            $spicehtml5url = 'n/a';
            if (intval($spiceport) >= 1024) {
                $output = [];
                $cmdLine = sprintf('pgrep -u openmediavault-kvmweb,root -a websockify | awk \'$%d == "localhost:%d" { print $%d }\'', $vncField, $spiceport, $remoteField);
                $cmd = new \OMV\System\Process($cmdLine);
                $cmd->setQuiet(true);
                $cmd->execute($output, $exitStatus);
                $spicehtml5port = intval($output[0]);
                if ($spicehtml5port >= 1024) {
                    $spicehtml5url = sprintf('<a href="%s://%s:%d/spice_auto.html?resize=remote" target="_blank">link</a>', $http, $fqdn, $spicehtml5port);
                }
            }
            $snaplist = $lv->domain_snapshots_list($dom);
            $snaps = 0;
            if ($snaplist) {
                $snaps = count($snaplist);
            }
            $notes = $lv->domain_get_description($dom);
            $objects[] = [
                'vmname' => $name,
                'virttype' => 'vm',
                'mem' => $info['memory'] * 1024,
                'cpu' => $info['nrVirtCpu'],
                'state' => $lv->domain_state_translate($info['state']),
                'disks' => $disks,
                'arch' => $lv->domain_get_arch($dom),
                'autostart' => $lv->domain_get_autostart($dom),
                'vncexists' => $vncexists,
                'spiceexists' => $spiceexists,
                'vncport' => $vncport,
                'spiceport' => $spiceport,
                'novncport' => $novncport,
                'novncurl' => $novncurl,
                'spicehtml5port' => $spicehtml5port,
                'spicehtml5url' => $spicehtml5url,
                'snaps' => $snaps,
                'notes' => str_replace(PHP_EOL,'<br/>',$notes)
            ];
        }
        // create new libvirt object for LXC containers
        $lv2 = $this->connectLxc();
        $doms = $lv2->get_domains();
        foreach ($doms as $name) {
            if (!$lv2->domain_get_xml($name)) {
                continue;
            }
            $dom = $lv2->get_domain_object($name);
            $domXml = new SimpleXMLElement($lv2->domain_get_xml($name));
            $info = $lv2->domain_get_info($dom);
            $disks = 'n/a';
            // vnc and novnc
            $vncexists = false;
            $xpath = $domXml->xpath('//graphics');
            $xpathSize = sizeof($xpath);
            for ($i = 0; $i < $xpathSize; $i++) {
                $graphics = $domXml->devices->graphics[$i];
                if ($graphics['type'] == 'vnc') {
                    $vncexists = true;
                    break;
                }
            }
            $vncport = $lv2->domain_get_vnc_port($dom);
            if (! $vncport || $vncport < 1024 || $vncport == '') {
                $vncport = 'n/a';
            }
            $novncport = 'n/a';
            $novncurl = 'n/a';
            if (intval($vncport) >= 1024) {
                $output = [];
                $cmdLine = sprintf('pgrep -u openmediavault-kvmweb,root -a websockify | awk \'$%d == "localhost:%d" { print $%d }\'', $vncField, $vncport, $remoteField);
                $cmd = new \OMV\System\Process($cmdLine);
                $cmd->setQuiet(true);
                $cmd->execute($output, $exitStatus);
                $novncport = intval($output[0]);
                if ($novncport >= 1024) {
                    $novncurl = sprintf('<a href="http://%s:%d/vnc.html?resize=remote&autoconnect=1" target="_blank">link</a>', $fqdn, $novncport);
                }
            }
            if (!$lv2->domain_get_xml($name)) {
                continue;
            }
            // spice and spice_html5
            $spiceport = '';
            $spiceexists = false;
            $xpath = $domXml->xpath('//graphics');
            $xpathSize = sizeof($xpath);
            for ($i = 0; $i < $xpathSize; $i++) {
                $graphics = $domXml->devices->graphics[$i];
                if ($graphics['type'] == 'spice') {
                    $spiceport = $graphics['port'];
                    $spiceexists = true;
                    break;
                }
            }
            if (!$spiceport || $spiceport < 1024 || $spiceport == '') {
                $spiceport = 'n/a';
            }
            $spicehtml5port = 'n/a';
            $spicehtml5url = 'n/a';
            if (intval($spiceport) >= 1024) {
                $output = [];
                $cmdLine = sprintf('pgrep -u openmediavault-kvmweb,root -a websockify | awk \'$%d == "localhost:%d" { print $%d }\'', $vncField, $spiceport, $remoteField);
                $cmd = new \OMV\System\Process($cmdLine);
                $cmd->setQuiet(true);
                $cmd->execute($output, $exitStatus);
                $spicehtml5port = intval($output[0]);
                if ($spicehtml5port >= 1024) {
                    $spicehtml5url = sprintf('<a href="http://%s:%d/spice_auto.html?resize=remote" target="_blank">link</a>', $fqdn, $spicehtml5port);
                }
            }
            $snaps = count($this->doLxcSnapshot($name, 'list', ''));
            $notes = $lv2->domain_get_description($dom);
            $objects[] = [
                'vmname' => $name,
                'virttype' => 'lxc',
                'mem' => $info['memory'] * 1024,
                'cpu' => $info['nrVirtCpu'],
                'state' => $lv2->domain_state_translate($info['state']),
                'disks' => $disks,
                'arch' => $lv2->domain_get_arch($dom),
                'autostart' => $lv2->domain_get_autostart($dom),
                'vncexists' => $vncexists,
                'spiceexists' => $spiceexists,
                'vncport' => $vncport,
                'spiceport' => $spiceport,
                'novncport' => $novncport,
                'novncurl' => $novncurl,
                'spicehtml5port' => $spicehtml5port,
                'spicehtml5url' => $spicehtml5url,
                'snaps' => $snaps,
                'notes' => str_replace(PHP_EOL,'<br/>',$notes)
            ];
        }
        // Filter the result.
        return $this->applyFilter($objects, $params['start'], $params['limit'],
            $params['sortfield'], $params['sortdir']);
    }

    public function getVmNameStateList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // create new libvirt object
        $lv = $this->connectQemu();
        $doms = $lv->get_domains();
        $objects= [];
        foreach ($doms as $name) {
            if (!$lv->domain_get_xml($name)) {
                continue;
            }
            $dom = $lv->get_domain_object($name);
            $info = $lv->domain_get_info($dom);
            $objects[] = [
                'vmname' => $name,
                'mem' => $info['memory'] * 1024,
                'cpu' => $info['nrVirtCpu'],
                'state' => $lv->domain_state_translate($info['state'])
            ];
        }
        // Filter the result.
        return $this->applyFilter($objects, $params['start'], $params['limit'],
            $params['sortfield'], $params['sortdir']);
    }

    public function getLxcNameStateList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // create new libvirt object
        $lv = $this->connectLxc();
        $doms = $lv->get_domains();
        $objects= [];
        foreach ($doms as $name) {
            if (!$lv->domain_get_xml($name)) {
                continue;
            }
            $dom = $lv->get_domain_object($name);
            $info = $lv->domain_get_info($dom);
            $objects[] = [
                'vmname' => $name,
                'mem' => $info['memory'] * 1024,
                'cpu' => $info['nrVirtCpu'],
                'state' => $lv->domain_state_translate($info['state'])
            ];
        }
        // Filter the result.
        return $this->applyFilter($objects, $params['start'], $params['limit'],
            $params['sortfield'], $params['sortdir']);
    }

    public function getVmNameList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // create new libvirt object
        $lv = $this->connectQemu();
        $doms = $lv->get_domains();
        $objects= [];
        foreach ($doms as $name) {
            if (!$lv->domain_get_xml($name)) {
                continue;
            }
            $objects[] = [ 'vmname' => $name ];
        }
        // Filter the result.
        return ($objects);
    }

    public function getVmXml($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // virsh dumpxml
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = 'dumpxml';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $vmxml = implode(PHP_EOL, array_filter($output));
        return ["vmxml" => $vmxml];
    }

    public function setVmXml($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.setvmxml');
        $vmname = $params['vmname'];
        // create temp xml file
        $xml = tempnam(sys_get_temp_dir(), $vmname . '_');
        file_put_contents($xml, $params['vmxml']);
        // virsh undefine
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = 'undefine';
        $cmdArgs[] = sprintf('--domain "%s"', $vmname);
        $err = gettext("Unable to undefine VM :: ") . $vmname;
        $this->virshCommand($cmdArgs, $err);
        // virsh define
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = 'define';
        $cmdArgs[] = sprintf('--file "%s"', $xml);
        $err = gettext("Unable to set VM XML :: ") . $vmname;
        $this->virshCommand($cmdArgs, $err);
    }

    public function getVmDetails($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        // vm info
        $output = [];
        $cmdArgs = [];
        $cmdArgs[] = 'dominfo';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $vminfo = implode(PHP_EOL, array_filter($output));
        // nic info
        $output = [];
        $cmdArgs = [];
        $cmdArgs[] = 'domiflist';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $nicinfo = implode(PHP_EOL, array_filter($output));
        // net info
        $netinfo = gettext("VM is not running!");
        if (preg_match("/State:\s+running/", $vminfo)) {
            $output = [];
            $cmdArgs = [];
            $cmdArgs[] = 'domifaddr';
            $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
            $cmdArgs[] = '--source agent';
            $cmd = new \OMV\System\Process('virsh', $cmdArgs);
            $cmd->setQuiet(true);
            $cmd->execute($output, $exitStatus);
            if ($exitStatus > 0) {
                $netinfo = gettext("VM does not have qemu guest agent installed!");
            } else {
                $netinfo = implode(PHP_EOL, array_filter($output));
            }
        }
        // disk list
        $output = [];
        $cmdArgs = [];
        $cmdArgs[] = 'domblklist';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
        $cmdArgs[] = '--details';
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $disklist = implode(PHP_EOL, array_filter($output));
        // disk info
        $output = [];
        $cmdArgs = [];
        $cmdArgs[] = 'domblkinfo';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
        $cmdArgs[] = '--all';
        $cmdArgs[] = '--human';
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $diskinfo = implode(PHP_EOL, array_filter($output));
        // disk stats
        $output = [];
        $cmdArgs = [];
        $cmdArgs[] = 'domblkstat';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
        $cmdArgs[] = '--human';
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $diskstats = implode(PHP_EOL, array_filter($output));
        // return results
        return [
            "vminfo" => $vminfo,
            "nicinfo" => $nicinfo,
            "netinfo" => $netinfo,
            "disklist" => $disklist,
            "diskinfo" => $diskinfo,
            "diskstats" => $diskstats
        ];
    }

    public function setVm($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $lxc = $params['lxc'];
        if ($lxc) {
            // Validate the parameters of the RPC service method.
            $this->validateMethodParams($params, 'rpc.kvm.setlxc');
            $path = $params['path'];
            $path = (substr($path, 0, 1) == '/' ? $path : '/' . $path);
            $image = $params['image'];
        } else {
            // Validate the parameters of the RPC service method.
            $this->validateMethodParams($params, 'rpc.kvm.setvm');
            $arch = $params['arch'];
            $cpu = explode(' ', $params['cpu']);
            $cpuArch = $cpu[0];
            if ($cpuArch == 'other') {
                $cpuModel = $params['otherCpu'];
            } else {
                $cpuModel = $cpu[1];
            }
            $hostArch = $this->getArch();
            $os = strtolower($params['os']);
            $uefi = $params['uefi'];
            $secure = $params['secure'];
            // verify if arch and cpu are compatible
            $archOk = false;
            switch ($cpuArch) {
                case 'arm':
                    $arm = ['armv6l','armv7l','aarch64','sh4','sh4eb'];
                    if (in_array($arch, $arm)) $archOk = true;
                    break;
                case 'ppc64':
                    $ppc = ['ppc','ppc64','ppc64le'];
                    if (in_array($arch, $ppc)) $archOk = true;
                    break;
                case 'x86':
                    $x86 = ['i686','x86_64'];
                    if (in_array($arch, $x86)) $archOk = true;
                    break;
                case 'host':
                case 'other':
                    $archOk = true;
                    break;
            }
            if($archOk !== true) {
                throw(gettext('Incompatible cpu and architecture!'));
            }
        }
        $vcpu = $params['vcpu'];
        $vmname = $params['vmname'];
        $xmlFile = sprintf('/tmp/%s.xml', $vmname);

        // memory
        $memory = $params['memory'];
        if ($params['memoryunit'] == 'GiB') {
            $memory *= 1024;
        }

        // OS-based arguments
        $vcpuArgs = '';
        $clockArgs = '';
        if (strpos($os, 'win') == 0) {
            $vcpuArgs = sprintf('%d,sockets=1,cores=%d,threads=1', $vcpu, $vcpu);
            $clockArgs = 'offset=localtime';
        } else {
            $vcpuArgs = $vcpu;
            $clockArgs = 'offset=utc';
        }

        if ($lxc) {
            $native_graphics = false;
        } else {
            // arch-based arguments
            switch ($arch) {
                case 'aarch64':
                    $fd = '/usr/share/AAVMF/AAVMF_CODE.fd';
                    if (($hostArch == 'arm64') || ($hostArch == 'armhf')) {
                        $native_graphics = true;
                    } else {
                        $native_graphics = false;
                    }
                    break;
                case 'arm':
                    $fd = '/usr/share/AAVMF/AAVMF_CODE.fd';
                    if (($hostArch == 'arm64') || ($hostArch == 'armhf')) {
                        $native_graphics = true;
                    } else {
                        $native_graphics = false;
                    }
                    break;
                case 'i386':
                    $native_graphics = true;
                    break;
                case 'x86_64':
                    if ($secure) {
                        $fd = '/usr/share/OVMF/OVMF_CODE_4M.ms.fd';
                    } else {
                        $fd = '/usr/share/OVMF/OVMF_CODE.fd';
                    }
                    $native_graphics = true;
                    break;
            }

            // disk arguments
            $volbus = $params['volbus'];
            $voldisk = $params['voldisk'];
            $volformat = $params['volformat'];
            $voliso = $params['voliso'];
            $volname = $params['volname'];
            $volsize = $params['volsize'];
            if (($volname == "") || (empty($volname))) {
                $volname = $vmname;
            }
            $volpool = $params['volpool'];

            if ($voldisk == 'Create new disk') {
                if ($volunit == "T") {
                    $volsize *= 1024;
                }
                $volpooltype = $this->getPoolType($volpool);
                $formatArg = '';
                if ($volpooltype != 'logical') {
                    $formatArg = sprintf(',format=%s', $volformat);
                }
                $diskArgs = sprintf('size=%d,bus=%s,pool="%s",cache=none,io=native%s', $volsize, $volbus, $this->escapeName($volpool), $formatArg);
            } else {
                $diskArgs = sprintf("'%s',bus=%s,cache=none,io=native", $voldisk, $volbus);
                $volformat = pathinfo($voldisk, PATHINFO_EXTENSION);
            }
        }

        // add network
        $mac = $params['macaddress'];
        $net = $params['network'];
        if (strlen($mac) != 17) {
            $mac = 'RANDOM';
        }
        $model = $params['model'];
        if ($model == "bridge") {
            $netArgs = sprintf('bridge=%s,mac=%s', $params['bridge'], $mac);
        } else {
            $netArgs = sprintf('network=%s,model=%s,mac=%s', $net, $model, $mac);
        }

        // build virt-install arguments
        $cmdArgs = [];
        if ($lxc) {
            $cmdArgs[] = '--connect lxc:///';
            $cmdArgs[] = '--container';
        } else {
            $cmdArgs[] = '--boot hd,cdrom';
            $cmdArgs[] = sprintf('--arch %s', $arch);
            $cmdArgs[] = sprintf('--cpu %s', $cpuModel);
            $cmdArgs[] = sprintf('--disk %s', $diskArgs);
            $cmdArgs[] = sprintf('--os-variant name=%s', $params['os']);
        }
        $cmdArgs[] = sprintf('--memory %s', $memory);
        $cmdArgs[] = sprintf('--metadata description="%s"', htmlspecialchars($params['notes'], ENT_XML1));
        $cmdArgs[] = sprintf('--name %s', $vmname);
        $cmdArgs[] = sprintf('--network %s', $netArgs);
        $cmdArgs[] = sprintf('--vcpus %s', $vcpuArgs);

        if ($lxc) {
            $cmdArgs[] = sprintf('--filesystem %s,/', $path);
        } else {
            // Since version 3.1.0 of virt-manager, a sound device is
            // being added unconditionally. Therefore, we just have to
            // use --sound in case we want no sound device at all.
            if (!$params['audio']) {
                $cmdArgs[] = '--sound none';
            }

            if ($params['uefi']) {
                if ($secure) {
                    $cmdArgs[] = '--boot uefi,loader_secure=yes';
                    $cmdArgs[] = '--features smm=on';
                } else {
                    $cmdArgs[] = sprintf('--boot loader=%s', $fd);
                }
            }

            if ($voliso != 'none') {
                $cmdArgs[] = sprintf("--cdrom '%s' --install no_install=yes", $params['voliso']);
                $printStep = ' 1';
            } else {
                $printStep = '';
            }

            if ($params['tpm']) {
                $cmdArgs[] = '--tpm backend.type=emulator,backend.version=2.0,model=tpm-tis';
            }

            if ($params['vnc']) {
                $cmdArgs[] = '--graphics vnc,listen=0.0.0.0';
            }

            if ($params['spice']) {
                $cmdArgs[] = '--graphics spice,listen=0.0.0.0';
            }
            $cmdArgs[] = '--channel unix,target_type=virtio';
        }

        $cmdArgs[] = sprintf('--print-xml%s', $printStep);
        $cmdArgs[] = sprintf(' > %s', $xmlFile);

        $err = gettext("Failed to create VM XML.");
        $this->virshCommand($cmdArgs, $err, 'virt-install');

        // download distro files
        if ($lxc && $image !== 'noimage') {
            mkdir($path, 0750, true);
            if (count(scandir($path)) == 2) {
                $this->downloadImage($image, $path);
                $this->resetLxcPassword($path);
            } else {
                throw new \OMV\Exception(gettext("Directory is not empty!"));
            }
        }

        // add guest agent name
        if (!$lxc) {
            $this->addNameToUnixChannel($xmlFile);
        }

        // import newly created xml file
        $cmdArgs = [];
        if ($lxc) {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = 'define';
        $cmdArgs[] = sprintf('--file %s', $xmlFile);

        $err = gettext("Failed to create VM.");
        $this->virshCommand($cmdArgs, $err);

        // remove temporary xml file
        unlink($xmlFile);
    }

    private function addNameToUnixChannel($path)
    {
        $xml = new SimpleXMLElement(file_get_contents($path));
        $channel = $xml->xpath("//channel[@type='unix']");
        $guest = 'org.qemu.guest_agent.0'; // The new name for the target
        if ($channel) {
            $target = $channel[0]->target;
            if (!$target->attributes()->name) {
                $target->addAttribute('name', $guest);
            }
        }
        $xml->asXML($path);
    }

    private function addVmBridgeNic($vmname, $mac, $net)
    {
        // create new libvirt object
        $lv = ($params['virttype'] == 'lxc' ? $this->connectLxc() : $this->connectQemu());

        // add a new bridge nic XML
        $domXml = new SimpleXMLElement($lv->domain_get_xml($vmname));

        $interfaceXml = $domXml->devices->addChild('interface');
        $interfaceXml->addAttribute('type', 'bridge');

        $sourceXml = $interfaceXml->addChild('source');
        $sourceXml->addAttribute('bridge', $net);

        $macXml = $interfaceXml->addChild('mac');
        $macXml->addAttribute('address', $mac);

        $modelXml = $interfaceXml->addChild('model');
        $modelXml->addAttribute('type', 'virtio');

        $newXml = $domXml->asXML();
        $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);

        return ($lv->domain_change_xml($vmname, $newXml));
    }

    public function getVmVolumes($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
        // create new libvirt object
        $lv = $this->connectQemu();
        $vols = $lv->get_disk_stats($params['vmname']);
        $objects = [];
        foreach ($vols as $vol) {
            $objects[] = [
                'file' => $vol['file'],
                'type' => $vol['type'],
                'device' => $vol['device'],
                'capacity' => $vol['capacity'],
                'allocation' => $vol['allocation'],
                'percentage' => ($vol['allocation'] / $vol['capacity']) * 100,
                'physical' => $vol['physical']
            ];
        }
        // Filter the result.
        return $this->applyFilter($objects, $params['start'], $params['limit'],
            $params['sortfield'], $params['sortdir']);
    }

    public function addDisk($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.adddisk');
        // create new libvirt object
        $lv = $this->connectQemu();
        // get parameters
        $vmname = $params['vmname'];
        $volbus = $params['volbus'];
        $voldisk = $params['voldisk'];
        $volformat = $params['volformat'];
        $volname = $params['volname'];
        $volpool = $params['volpool'];
        $volsize = $params['volsize'];
        $volunit = $params['volunit'];
        if ($volunit == "") {
            $volunit = "G";
        }
        // get VM object
        $vm = $lv->get_domain_by_name($vmname);

        $formatArg = '';
        // create new disk (if specified)
        if ($voldisk == "Create new disk") {
            $volpooltype = $this->getPoolType($volpool);
            $block = true;
            if ($volpooltype != 'logical') {
                $block = false;
                $formatArg = sprintf(',format=%s', $volformat);
            }
            if ($block) {
                $cmdArgs = [];
                $cmdArgs[] = sprintf('-L %d%s', $volsize, $volunit);
                $cmdArgs[] = sprintf('-n %s', $this->escapeName($volname));
                $cmdArgs[] = $vg;
                $err = gettext("Failed to create new disk :: ") . $params['command'];
                $this->virshCommand($cmdArgs, $err, 'lvcreate');
            } else {
                $pool = $lv->get_storagepool_info($volpool);
                $diskpath = sprintf("'%s/%s.%s'", $pool['path'], $volname, $volformat);
                $cmdArgs = [];
                $cmdArgs[] = 'create';
                $cmdArgs[] = sprintf('-f %s', $volformat);
                $cmdArgs[] = $diskpath;
                $cmdArgs[] = sprintf('%d%s', $volsize, $volunit);
                $err = gettext("Failed to create new disk :: ") . $params['command'];
                $this->virshCommand($cmdArgs, $err, 'qemu-img');
            }
        } else {
            $diskpath = sprintf("'%s'", $voldisk);
            $volformat = pathinfo($voldisk, PATHINFO_EXTENSION);
            $volname = pathinfo($voldisk, PATHINFO_FILENAME);
            if ($volformat != "qcow2") {
                $volformat = "raw";
            }
        }
        // add disk to VM with virt-xml
        $cmdArgs = [];
        $cmdArgs[] = $this->escapeName($vmname);
        $cmdArgs[] = '--add-device';
        $cmdArgs[] = sprintf('--disk %s,target.bus=%s%s,cache=none,io=native', $diskpath, $volbus, $formatArg);
        $err = gettext("Failed to add disk.");
        $this->virshCommand($cmdArgs, $err, 'virt-xml');
    }

    public function addHostDisk($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.addhostdisk');
        // add host disk
        $cmdArgs = [];
        $cmdArgs[] = $this->escapeName($params['vmname']);
        $cmdArgs[] = '--add-device';
        $cmdArgs[] = sprintf("--disk '%s',device=disk,bus=%s,cache=none,io=native", $params['voldisk'], $params['volbus']);
        $err = gettext("Failed to add host disk.");
        $this->virshCommand($cmdArgs, $err, 'virt-xml');
    }

    public function addOptical($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.addoptical');
        // create new libvirt object
        $lv = $this->connectQemu();
        // get VM object
        $vmname = $params['vmname'];
        $vm = $lv->get_domain_by_name($vmname);
        $domXml = new SimpleXMLElement($lv->domain_get_xml($vmname));
        $voliso = $params['voliso'];
        // get next device
        if (isset($domXml->os->loader) || strpos($domXml->os->type['machine'], 'q35') > -1) {
            $bus = 'sata';
        } else {
            $bus = 'ide';
        }
        // add optical storage
        $cmdArgs = [];
        $cmdArgs[] = $this->escapeName($vmname);
        $cmdArgs[] = '--add-device';
        $cmdArgs[] = sprintf("--disk '%s',device=cdrom,bus=%s", $voliso, $bus);
        $err = gettext("Failed to add optical.");
        $this->virshCommand($cmdArgs, $err, 'virt-xml');
    }

    public function addHostOptical($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.addhostoptical');
        // add host optical
        $cmdArgs = [];
        $cmdArgs[] = $this->escapeName($params['vmname']);
        $cmdArgs[] = '--add-device';
        $cmdArgs[] = sprintf("--disk '%s',device=cdrom", $params['device']);
        $err = gettext("Failed to add host optical.");
        $this->virshCommand($cmdArgs, $err, 'virt-xml');
    }

    public function changeDisc($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.changedisc');
        // virsh change-media --update
        $cmdArgs = [];
        $cmdArgs[] = 'change-media';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
        $cmdArgs[] = sprintf("--path '%s'", $params['disk']);
        $cmdArgs[] = sprintf("--source '%s'", $params['voliso']);
        $cmdArgs[] = '--update';
        $cmdArgs[] = '--config';
        if ($params['state'] == 'running') {
            $cmdArgs[] = '--live';
        }
        $err = gettext("Failed to remove disk.");
        $this->virshCommand($cmdArgs, $err);
    }

    public function ejectDisc($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.ejectdisc');
        // virsh change-media --eject
        $cmdArgs = [];
        $cmdArgs[] = 'change-media';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
        $cmdArgs[] = sprintf("--path '%s'", $params['disk']);
        $cmdArgs[] = '--eject';
        if ($params['force']) {
            $cmdArgs[] = '--force';
        }
        $cmdArgs[] = '--config';
        if ($params['state'] == 'running') {
            $cmdArgs[] = '--live';
        }
        $err = gettext("Failed to remove disk.");
        $this->virshCommand($cmdArgs, $err);
    }

    public function removeDisk($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.removedisk');
        // virsh detach-disk
        $cmdArgs = [];
        $cmdArgs[] = 'detach-disk';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
        $cmdArgs[] = sprintf("--target '%s'", $params['disk']);
        $cmdArgs[] = '--persistent';
        $cmdArgs[] = '--config';
        if ($params['state'] == 'running') {
            $cmdArgs[] = '--live';
        }
        $err = gettext("Failed to remove disk.");
        $this->virshCommand($cmdArgs, $err);
    }

    public function resizeDisk($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.resizedisk');
        $amount = $params['amount'];
        $sign = '+';
        if ($amount < 0) $sign = '-';
        $cmdArgs = [];
        $cmdArgs[] = 'resize';
        $cmdArgs[] = sprintf("'%s'", $params['path']);
        $cmdArgs[] = sprintf('%s%d%s', $sign, $params['amount'], $params['amountunit']);
        $err = gettext("Failed to resize disk!");
        $this->virshCommand($cmdArgs, $err, 'qemu-img');
    }

    public function enumerateHostDisk($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // enumerate host disks
        $cmd = new \OMV\System\Process('omv-disk-list.sh');
        $cmd->execute($output, $exitStatus);
        $objects = [];
        foreach ($output as $dev) {
            if (empty($dev)) {
                continue;
            }
            $objects[] = [
                'device' => $dev,
                'description' => sprintf('[host] %s', $dev)
            ];
        }
        // Filter the result.
        return ($objects);
    }

    public function enumerateHostOptical($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // enumerate host optical devices with lsscsi
        $cmdArgs = [];
        $cmdArgs[] = '| awk \'$2 ~ "cd" { print $7 }\'';
        $cmdArgs[] = '| awk NF';
        $cmd = new \OMV\System\Process('lsscsi', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $objects = [];
        foreach ($output as $dev) {
            if (empty($dev)) {
                continue;
            }
            $objects[] = [
                'device' => $dev,
                'description' => sprintf('[host] %s', $dev)
            ];
        }
        // Filter the result.
        return ($objects);
    }

    public function getDisk($params, $context)
    {
    }

    public function cloneVm($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.kvm.clonevm");
        $newname = $params['newname'];
        $path = sprintf('/etc/libvirt/qemu/%s.xml', $newname);
        if (file_exists($path)) {
            throw new \OMV\Exception($newname . gettext(" already exists!"));
        }
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($params, $newname) {
                $cmdArgs = [];
                $cmdArgs[] = '/usr/bin/virt-clone';
                $cmdArgs[] = sprintf('--original %s', $params['vmname']);
                $cmdArgs[] = sprintf('--name %s', $newname);
                $cmdArgs[] = '--auto-clone';
                $cmd = new \OMV\System\Process($cmdArgs);
                $cmd->setRedirect2to1();
                $cmdLine = $cmd->getCommandLine();
                if (0 !== $this->exec($cmdLine, $output, $bgOutputFilename)) {
                    throw new \OMV\ExecException($cmdLine, $output);
                }
                return $output;
            }
        );
    }

    public function createLinkedClone($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.createlinkedclone');
        // omv-linked-clone
        $output = [];
        $cmdArgs = [];
        $cmdArgs[] = sprintf('-c %s', $params['clone']);
        $cmdArgs[] = sprintf('-v %s', $params['source']);
        $cmd = new \OMV\System\Process('omv-linked-clone', $cmdArgs);
        $cmd->execute($output, $exitStatus);
    }

    public function createBacking($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.createbacking');
        // qemu-img create
        $back = $params['backing'];
        $base = $params['base'];
        $path = dirname($base);
        $output = [];
        $cmdArgs = [];
        $cmdArgs[] = 'create';
        $cmdArgs[] = '-f qcow2';
        $cmdArgs[] = '-F qcow2';
        $cmdArgs[] = sprintf('-b "%s"', $base);
        $cmdArgs[] = sprintf("'%s/%s'", $path, $back);
        $cmd = new \OMV\System\Process('qemu-img', $cmdArgs);
        $cmd->execute($output, $exitStatus);
    }

    public function getVolumeList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
        // get list of disks on running VMs
        $disks = [];
        $cmdArgs = [];
        $cmdArgs[] = 'list --name';
        $cmdArgs[] = '| xargs -I@ virsh domblklist --details @';
        $cmdArgs[] = '| awk \'$2 == "disk" { $1="";$2="";$3="";sub(/^[ \t]+/,"");print $0 }\'';
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($disks, $exitStatus);
        // create new libvirt object
        $lv = $this->connectQemu();
        $pools = $lv->get_storagepools();
        $optical = $params["optical"];
        $objects= [];
        foreach ($pools as $pool) {
            $lv->storagepool_refresh($pool);
            $info = $lv->get_storagepool_info($pool);
            if ($info['volume_count'] > 0) {
                $volumes = $lv->storagepool_get_volume_information($pool);
                $keys = array_keys($volumes);
                $xpathSize = sizeof($volumes);
                for ($i = 0; $i < $xpathSize; $i++) {
                    $volume = $volumes[$keys[$i]];
                    if (is_dir($volume['path'])) continue;
                    $ext = pathinfo($volume['path'], PATHINFO_EXTENSION);
                    if (((strtolower($ext) != 'iso') && (!$optical)) || ((strtolower($ext) == 'iso') && ($optical))) {
                        $capacity = $volume['capacity'];
                        if ($capacity <= 0) {
                            $capacity = 0;
                            $percentage = 0;
                        } else {
                            $percentage = ($volume['allocation'] / $volume['capacity']) * 100;
                        }
                        $running = in_array($volume['path'], $disks);
                        $objects[] = [
                            'name' => $keys[$i],
                            'pool' => $pool,
                            'ext' => $ext,
                            'capacity' => $volume['capacity'],
                            'allocation' => $volume['allocation'],
                            'percentage' => $percentage,
                            'path' => $volume['path'],
                            'propConvert' => ($ext == 'qcow2') ? true : false,
                            'running' => $running
                        ];
                    }
                }
            }
        }
        // Filter the result.
        return $this->applyFilter($objects, $params['start'], $params['limit'],
            $params['sortfield'], $params['sortdir']);
    }

    public function enumerateVolumes($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.enumeratevolumes');
        // create new libvirt object
        $lv = $this->connectQemu();
        $pools = $lv->get_storagepools();
        $optical = $params["optical"];
        $opticalNone = $params["opticalNone"];
        $objects= [];
        if (!$optical) {
            $objects[] = [
                'name' => 'Create new disk',
                'capacity' => 0,
                'path' => 'Create new disk'
            ];
        } else {
            if ($opticalNone) {
                $objects[] = [
                    'name' => 'none',
                    'capacity' => 0,
                    'path' => 'none'
                ];
            }
        }
        foreach ($pools as $pool) {
            $output = [];
            $cmdArgs = [];
            $cmdArgs[] = 'vol-list';
            $cmdArgs[] = $this->escapeName($pool);
            $cmdArgs[] = '--details';
            $cmdArgs[] = '| sed 1,2d';
            $cmdArgs[] = '| awk NF';
            $cmdArgs[] = '| awk -F ';
            $cmdArgs[] = '\'[[:space:]][[:space:]]+\'';
            $cmdArgs[] = '\'{ print $1","$2","$4 }\'';
            $cmd = new \OMV\System\Process('virsh', $cmdArgs);
            $cmd->execute($output, $exitStatus);
            foreach ($output as $vol) {
                if (empty($vol)) {
                    continue;
                }
                $parts = explode(',', $vol);
                $name = $parts[0];
                $path = $parts[1];
                $cap = $this->getVolumeSizes($path);
                $ext = pathinfo($path, PATHINFO_EXTENSION);
                $add = false;
                if (((strtolower($ext) != 'iso') && (!$optical)) || ((strtolower($ext) == 'iso') && ($optical))) {
                    $add = true;
                }
                if ($add) {
                    $objects[] = [
                        'name' => sprintf('[%s] %s %s', $pool, $name, $cap),
                        'capacity' => $cap,
                        'path' => $path
                    ];
                }
            }
        }
        // Filter the result.
        return ($objects);
    }

    public function enumerateVolumesByVm($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.enumeratevolumesbyvm');
        // enumerate volumes with virsh
        $optical = $params["optical"];
        $cmdArgs = [];
        $cmdArgs[] = 'domblklist';
        $cmdArgs[] = $this->escapeName($params['vmname']);
        $cmdArgs[] = '--details';
        $cmdArgs[] = '| sed 1,2d';
        $cmdArgs[] = '| awk NF';
        if ($optical) {
            $cmdArgs[] = '| awk \'$2 == "cdrom" { printf $2","$3",";$1="";$2="";$3="";sub(/^[ \t]+/,"");print $0 }\'';
        } else {
            $cmdArgs[] = '| awk \'$2 != "cdrom" { printf $2","$3",";$1="";$2="";$3="";sub(/^[ \t]+/,"");print $0 }\'';
        }
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $objects = [];
        foreach ($output as $vol) {
            if (empty($vol)) {
                continue;
            }
            $parts = explode(',', $vol);
            $dev = $parts[0];
            $tgt = $parts[1];
            $src = $parts[2];
            $cap = binary_format(filesize($src));
            $objects[] = [
                'device' => $dev,
                'description' => sprintf('[%s] %s :: %s (%s)', $dev, $tgt, $src, $cap),
                'path' => $src,
                'devname' => $tgt
            ];
        }
        // Filter the result.
        return ($objects);
    }

    public function getVolume($params, $context)
    {
    }

    public function setVolume($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        //$this->validateMethodParams($params, "rpc.kvm.setvolume");
        // create new libvirt object
        $lv = $this->connectQemu();
        $pool = $params['pool'];
        $name = $params['name'];
        $format = $params['format'];
        $image = sprintf('%s.%s', $name, $format);
        $size = sprintf('%d%s', $params['volsize'], $params['volunit']);
        if (!$lv->storagevolume_create($pool, $image, $size, $size, $format)) {
            $msg = sprintf("%s on %s of type %s and size %s", $name, $pool, $format, $size);
            throw new \OMV\Exception( gettext("Unable to create volume - ") . $msg);
        }
    }

    public function deleteVolume($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.kvm.path");
        // create new libvirt object
        $lv = $this->connectQemu();
        if (!$lv->storagevolume_delete($params['path'])) {
            throw new \OMV\Exception( gettext("Unable to delete volume - ") . $params['path']);
        }
    }

    public function convertVolume($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.kvm.convertvolume");
        $path = $params['path'];
        $ext = pathinfo($path, PATHINFO_EXTENSION);
        $format = $params['format'];
        if ($ext == '') {
            $newPath = sprintf('%s.%s', $path, $format);
        } else {
            $newPath = str_replace($ext, $format, $path);
        }
        if (file_exists($newPath)) {
            throw new \OMV\Exception($newPath . gettext(" already exists!"));
        }
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($params, $format, $path, $newPath) {
                $cmdArgs = [];
                $cmdArgs[] = '/usr/bin/qemu-img';
                $cmdArgs[] = 'convert';
                $cmdArgs[] = '-O ' . $format;
                $cmdArgs[] = sprintf("'%s'", $path);
                $cmdArgs[] = sprintf("'%s'", $newPath);
                $cmd = new \OMV\System\Process($cmdArgs);
                $cmd->setRedirect2to1();
                $cmdLine = $cmd->getCommandLine();
                if (0 !== $this->exec($cmdLine, $output, $bgOutputFilename)) {
                    throw new \OMV\ExecException($cmdLine, $output);
                }
                return $output;
            }
        );
    }

    public function volumeCommand($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.kvm.volumecommand");
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($params) {
                $cmdArgs = [];
                $cmdArgs[] = '/usr/bin/qemu-img';
                $cmdArgs[] = $params['command'];
                $cmdArgs[] = sprintf("'%s'", $params['path']);
                if ($params['command'] == 'info') {
                    $cmdArgs[] = '--backing-chain';
                }
                $cmd = new \OMV\System\Process($cmdArgs);
                $cmd->setRedirect2to1();
                $cmdLine = $cmd->getCommandLine();
                if (0 !== $this->exec($cmdLine, $output, $bgOutputFilename)) {
                    throw new \OMV\ExecException($cmdLine, $output);
                }
                return $output;
            }
        );
    }

    public function downloadIso($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.kvm.downloadiso");
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($params) {
                $cmdArgs = [];
                $cmdArgs[] = '/usr/bin/wget';
                $cmdArgs[] = sprintf('--output-document="%s/%s"', $params['path'], $params['filename']);
                $cmdArgs[] = $params['url'];
                $cmd = new \OMV\System\Process($cmdArgs);
                $cmd->setRedirect2to1();
                $cmdLine = $cmd->getCommandLine();
                if (0 !== $this->exec($cmdLine, $output, $bgOutputFilename)) {
                    throw new \OMV\ExecException($cmdLine, $output);
                }
                return $output;
            }
        );
    }

    private function deleteDisksByVm($vmname, $lxc)
    {
        // create new libvirt object
        $lv = ($lxc ? $this->connectLxc() : $this->connectQemu());
        $domXml = new SimpleXMLElement($lv->domain_get_xml($vmname));
        if ($lxc) {
            $xpath = $domXml->xpath('//filesystem');
            $xpathSize = sizeof($xpath);
            for ($i = 0; $i < $xpathSize; $i++) {
                $dst = $domXml->devices->filesystem[$i]->target['dir'];
                if ($dst == "/") {
                    $src = $domXml->devices->filesystem[$i]->source['dir'];
                    if ($src !== "/" && file_exists($src)) {
                        $cmdLine = sprintf('rm -rf %s', $src);
                        $cmd = new \OMV\System\Process($cmdLine);
                        $cmd->execute($output, $exitStatus);
                    }
                    return (true);
                }
            }
        } else {
            $xpath = $domXml->xpath('//disk');
            $xpathSize = sizeof($xpath);
            for ($i = 0; $i < $xpathSize; $i++) {
                $disk = $domXml->devices->disk[$i];
                if ($disk['device'] == "disk") {
                    unlink($disk->source['file']);
                    unset($domXml->devices->disk[$i]);
                }
            }
            $newXml = $domXml->asXML();
            $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
            return ($lv->domain_change_xml($vmname, $newXml));
        }
        return (false);
    }

    private function doLxcSnapshot($cntr, $action, $snapname)
    {
        $cmdArgs = [];
        $cmdArgs[] = sprintf("-c '%s'", $cntr);
        $cmdArgs[] = sprintf("-a '%s'", $action);
        if ($action == 'revert' || $action == 'delete') {
            $cmdArgs[] = sprintf("-s '%s'", $snapname);
        }
        $cmd = new \OMV\System\Process('omv-lxc-snapshot', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        return (array_filter($output));
    }

    public function enumerateSnapshots($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        // get snapshots
        $objects= [];
        if ($params['virttype'] == 'lxc') {
            $output = $this->doLxcSnapshot($params['vmname'], 'list', '');
            foreach ($output as $snap) {
                $objects[] = [
                    'snapname' => $snap,
                    'description' => $snap
                ];
            }
        } else {
            // create new libvirt object
            $lv = $this->connectQemu();
            $dom = $lv->get_domain_object($params['vmname']);
            $snaps = $lv->domain_snapshots_list($dom);
            foreach ($snaps as $key => $value) {
                $xml = $lv->domain_snapshot_get_xml($dom, $value);
                $tmpxml = simplexml_load_string($xml);
                $name = $tmpxml->name[0];
                $creationTime = $tmpxml->creationTime[0];
                $ctime = date("Y.m.d H:i:s", $value);
                $snapstate = $tmpxml->state[0];
                $objects[] = [
                    'snapname' => strval($name),
                    'description' => sprintf("%s :: %s (%s)", $ctime, $snapstate, $name)
                ];
            }
        }
        // Filter the result.
        return ($objects);
    }

    public function addSnapshot($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        // create snapshot
        if ($params['virttype'] == 'lxc') {
            $output = $this->doLxcSnapshot($params['vmname'], 'create', '');
        } else {
            // create new libvirt object
            $lv = $this->connectQemu();
            $dom = $lv->get_domain_object($params['vmname']);
            if (!$lv->domain_snapshot_create($dom)) {
                throw new \OMV\Exception( gettext("Unable to create snapshot - ") . $lv->get_last_error());
            }
        }
    }

    public function deleteSnapshot($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.snapshot');
        // delete snapshot
        if ($params['virttype'] == 'lxc') {
            $output = $this->doLxcSnapshot($params['vmname'], 'delete', $params['snapname']);
        } else {
            // create new libvirt object
            $lv = $this->connectQemu();
            if (!$lv->domain_snapshot_delete($params['vmname'], $params['snapname'])) {
                throw new \OMV\Exception( gettext("Unable to delete snapshot - ") . $lv->get_last_error());
            }
        }
    }

    public function revertSnapshot($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.snapshot');
        // revert snapshot
        if ($params['virttype'] == 'lxc') {
            $output = $this->doLxcSnapshot($params['vmname'], 'revert', $params['snapname']);
        } else {
            // create new libvirt object
            $lv = $this->connectQemu();
            if (!$lv->domain_snapshot_revert($params['vmname'], $params['snapname'])) {
                throw new \OMV\Exception( gettext("Unable to revert to snapshot - ") . $lv->get_last_error());
            }
        }
    }

    public function enumerateUsbByHost($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $output = [];
        $cmdLine = 'lsusb | sort -h';
        $cmd = new \OMV\System\Process($cmdLine);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $objects = [];
        $usbCount = count($output);
        for ($i = 0; $i < $usbCount; $i++) {
            $objects[] = [
                'description' => $output[$i],
                'device' => $output[$i]
            ];
        }
        return ($objects);
    }

    public function enumerateUsbByVm($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        // create new libvirt object
        $lv = $this->connectQemu();
        $domXml = new SimpleXMLElement($lv->domain_get_xml($params['vmname']));
        $xpath = $domXml->xpath('//hostdev');
        // get list of current usb devices from host
        $output = [];
        $cmdLine = 'lsusb | sort -h';
        $cmd = new \OMV\System\Process($cmdLine);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $usbCount = count($output);
        // get list of usb devices from VM
        $objects = [];
        $xpathSize = sizeof($xpath);
        for ($i = 0; $i < $xpathSize; $i++) {
            $hostdev = $xpath[$i];
            if (trim($hostdev['type']) != 'usb') {
                continue;
            }
            $dev = $hostdev->source;
            if (isset($dev->vendor)) {
                $device = sprintf('%s:%s', $dev->vendor['id'], $dev->product['id']);
                $check = str_replace('0x', '', $device);
	    } else {
                $device = sprintf('Bus %03d Device %03d', $dev->address['bus'], $dev->address['device']);
                $check = $device;
            }
            $description = $device;
            for ($j = 0; $j < $usbCount; $j++) {
                if (strpos($output[$j], $check)) {
                    $description = $output[$j];
                    break;
                }
            }
            $objects[] = [
                'device' => $device,
                'description' => $description
            ];
        }
        return ($objects);
    }

    public function addUsb($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.usb');
        // get parameters
        $vmname = $this->escapeName($params['vmname']);
        $state = $params['state'];
        // create xml
        $xml = $this->createUsbXml($params['device'], $params['byaddress']);
        // add with virsh
        $cmdArgs = $this->deviceArgs('attach-device', $vmname, $xml, $state);
        $err = gettext("Failed to add USB device.");
        $this->virshCommand($cmdArgs, $err);
        // remove temp xml file
        unlink($xml);
    }

    public function removeUsb($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.usb');
        // get parameters
        $vmname = $this->escapeName($params['vmname']);
        $state = $params['state'];
        // create xml
        $address = true;
        if (strpos($params['device'], "Bus") === false) {
            $address = false;
        }
        $xml = $this->createUsbXml($params['device'], $address);
        // remove with virsh
        $cmdArgs = $this->deviceArgs('detach-device', $vmname, $xml, $state);
        $err = gettext("Failed to remove USB device.");
        $this->virshCommand($cmdArgs, $err);
        // remove temp xml file
        unlink($xml);
    }

    private function createUsbXml($device, $byaddress)
    {
        // create usb xml
        if (strpos($device, "Bus") === false) {
            $dev = str_replace('0x', '', trim($device));
            $vendorProduct = explode(':', $dev);
            $bus = '';
            $device = '';
            $byaddress = false;
        } else {
            $dev = explode(' ', $device);
            $vendorProduct = explode(':', $dev[5]);
            $bus = intval($dev[1]);
            $device = intval($dev[3]);
        }
        // create xml structure
        $hostDevXml = new SimpleXMLElement('<hostdev/>');
        $hostDevXml->addAttribute('mode', 'subsystem');
        $hostDevXml->addAttribute('type', 'usb');
        $hostDevXml->addAttribute('managed', 'yes');
        $sourceXml = $hostDevXml->addChild('source');
        if ($byaddress && $bus !== '' && $device !== '') {
            $addressXml = $sourceXml->addChild('address');
            $addressXml->addAttribute('bus', $bus);
            $addressXml->addAttribute('device', $device);
        } else {
            $vendor = $vendorProduct[0];
            $product = $vendorProduct[1];
            $vendorXml = $sourceXml->addChild('vendor');
            $vendorXml->addAttribute('id', '0x'.$vendor);
            $productXml = $sourceXml->addChild('product');
            $productXml->addAttribute('id', '0x'.$product);
        }
        // create xml file
        $temp = tempnam(sys_get_temp_dir(), 'virsh_usb');
        $newXml = $hostDevXml->asXML();
        $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
        file_put_contents($temp, $newXml);
        // return filename
        return ($temp);
    }

    public function enumeratePciByHost($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $output = [];
        $cmdLine = 'lspci';
        $cmd = new \OMV\System\Process($cmdLine);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $objects = [];
        $pciCount = count($output);
        for ($i = 0; $i < $pciCount; $i++) {
            $objects[] = [
                'description' => $output[$i],
                'device' => $output[$i]
            ];
        }
        return ($objects);
    }

    public function enumeratePciByVm($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        // create new libvirt object
        $lv = $this->connectQemu();
        $domXml = new SimpleXMLElement($lv->domain_get_xml($params['vmname']));
        $xpath = $domXml->xpath('//hostdev');
        // get list of current pci devices from host
        $output = [];
        $cmdLine = 'lspci';
        $cmd = new \OMV\System\Process($cmdLine);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $pciCount = count($output);
        // get list of pci devices from VM
        $objects = [];
        $xpathSize = sizeof($xpath);
        for ($i = 0; $i < $xpathSize; $i++) {
            $hostdev = $xpath[$i];
            if (trim($hostdev['type']) != 'pci') {
                continue;
            }
            $dev = $hostdev->source;
            $device = sprintf('%s:%s.%s', $dev->address['bus'], $dev->address['slot'], $dev->address['function']);
            $device = str_replace('0x', '', $device);
            $description = $device;
            $fulldesc = preg_grep('/^' . $device . '\s+.*/', $output);
            if (count($fulldesc) == 1) {
                $description = trim(implode('', $fulldesc));
            }
            $objects[] = [
                'device' => $device,
                'description' => $description
            ];
        }
        return ($objects);
    }

    public function addPci($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.pci');
        // get parameters
        $vmname = $this->escapeName($params['vmname']);
        $state = $params['state'];
        $dev = $params['device'];
        $hostdev = explode(' ', $dev)[0];
        // detach device from host
        $cmdArgs = [];
        $cmdArgs[] = 'nodedev-detach';
        $cmdArgs[] = sprintf('pci_0000_%s', str_replace([':', '.'], '_', $hostdev));
        $err = gettext("Failed to detach PCI device from host.");
        $this->virshCommand($cmdArgs, $err, 'virsh', true);
        // create xml
        $xml = $this->createPciXml($dev);
        // add with virsh
        $cmdArgs = $this->deviceArgs('attach-device', $vmname, $xml, $state);
        $err = gettext("Failed to add PCI device.");
        $this->virshCommand($cmdArgs, $err);
        // remove temp xml file
        unlink($xml);
    }

    public function removePci($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.pci');
        // get parameters
        $vmname = $this->escapeName($params['vmname']);
        $state = $params['state'];
        // create xml
        $xml = $this->createPciXml($params['device']);
        // remove with virsh
        $cmdArgs = $this->deviceArgs('detach-device', $vmname, $xml, $state);
        $err = gettext("Failed to remove PCI device.");
        $this->virshCommand($cmdArgs, $err);
        // remove temp xml file
        unlink($xml);
    }

    private function createPciXml($device)
    {
        // create pci xml
        $dev = explode(' ', $device)[0];
        $devid = explode(':', str_replace('.', ':', $dev));
        // create xml structure
        $hostDevXml = new SimpleXMLElement('<hostdev/>');
        $hostDevXml->addAttribute('mode', 'subsystem');
        $hostDevXml->addAttribute('type', 'pci');
        $hostDevXml->addAttribute('managed', 'yes');
        $sourceXml = $hostDevXml->addChild('source');
        $addressXml = $sourceXml->addChild('address');
        $addressXml->addAttribute('domain', '0x0');
        $addressXml->addAttribute('bus', '0x'.$devid[0]);
        $addressXml->addAttribute('slot', '0x'.$devid[1]);
        $addressXml->addAttribute('function', '0x'.$devid[2]);
        // create xml file
        $temp = tempnam(sys_get_temp_dir(), 'virsh_pci');
        $newXml = $hostDevXml->asXML();
        $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
        file_put_contents($temp, $newXml);
        // return filename
        return ($temp);
    }

    public function addFsPass($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.addfspass');
        // get parameters
        $vmname = $params['vmname'];
        $state = $params['state'];
        $src = $params['src'];
        $tgt = $params['tgt'];
        // add with virt-xml
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
            $tgt = $params['tgt2'];
            if ($tgt == '/') {
                throw new \OMV\Exception( gettext("Cannot use / for mount point."));
            }
        }
        $cmdArgs[] = $this->escapeName($vmname);
        $cmdArgs[] = '--add-device';
        $cmdArgs[] = sprintf("--filesystem '%s',%s", $src, $tgt);
        $err = gettext("Failed to add filesystem passthrough.");
        $this->virshCommand($cmdArgs, $err, 'virt-xml');
    }

    public function removeFsPass($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.removefspass');
        // get parameters
        $vmname = $params['vmname'];
        $state = $params['state'];
        $srctgt = explode("|", $params['srctgt']);
        $src = trim($srctgt[0]);
        $tgt = trim($srctgt[1]);
        // remove with virt-xml
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
            if ($tgt == '/') {
                throw new \OMV\Exception( gettext("Cannot remove / from a container."));
            }
        }
        $cmdArgs[] = $this->escapeName($vmname);
        $cmdArgs[] = '--remove-device';
        $cmdArgs[] = sprintf("--filesystem '%s',%s", $src, $tgt);
        $err = gettext("Failed to remove filesystem passthrough.");
        $this->virshCommand($cmdArgs, $err, 'virt-xml');
    }

    public function enumerateFsPassByVm($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        // create new libvirt object
        $lv = ($params['virttype'] == 'lxc' ? $this->connectLxc() : $this->connectQemu());
        $domXml = new SimpleXMLElement($lv->domain_get_xml($params['vmname']));
        $xpath = $domXml->xpath('//filesystem');
        // get list of passthrough filesystem devices from VM
        $objects = [];
        $xpathSize = sizeof($xpath);
        for ($i = 0; $i < $xpathSize; $i++) {
            $dev = $domXml->devices->filesystem[$i];
            $src = $dev->source['dir'];
            $tgt = $dev->target['dir'];
            if ($tgt == '/') {
                continue;
            }
            $srctgt = sprintf('%s | %s', $src, $tgt);
            $description = $srctgt;
            $objects[] = [
                'srctgt' => $srctgt,
                'description' => $description
            ];
        }
        return ($objects);
    }

    public function doCommand($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.docommand');
        // create new libvirt object
        $lxc = false;
        if ($params['virttype'] == 'lxc') {
            $lxc = true;
        }
        $lv = ($lxc ? $this->connectLxc() : $this->connectQemu());
        $vmname = $params['name'];
        $dom = $lv->get_domain_object($vmname);
        $undefine = false;
        switch ($params['command']) {
            case 'poweron':
                $result = $lv->domain_start($dom);
                break;
            case 'reboot':
                $result = $lv->domain_reboot($dom);
                break;
            case 'reset':
                $cmdArgs = [];
                if ($lxc) {
                    $cmdArgs[] = '--connect lxc:///';
                }
                $cmdArgs[] = 'reset';
                $cmdArgs[] = $this->escapeName($vmname);
                $err = gettext("Failed to reset VM.");
                $this->virshCommand($cmdArgs, $err);
                break;
            case 'poweroff':
                $this->shutdownWeb($params['vncport'], $params['spiceport'], $params['hostport'], $params['hostport2']);
                $result = $lv->domain_shutdown($dom);
                break;
            case 'force':
                $this->shutdownWeb($params['vncport'], $params['spiceport'], $params['hostport'], $params['hostport2']);
                $result = $lv->domain_destroy($dom);
                break;
            case 'pause':
                $result = $lv->domain_suspend($dom);
                break;
            case 'resume':
                $result = $lv->domain_resume($dom);
                break;
            case 'undefine':
                $this->shutdownWeb($params['vncport'], $params['spiceport'], $params['hostport'], $params['hostport2']);
                $cmdArgs = [];
                if ($lxc) {
                    $cmdArgs[] = '--connect lxc:///';
                }
                $cmdArgs[] = 'undefine';
                if (!$lxc) {
                    $cmdArgs[] = '--nvram';
                }
                $cmdArgs[] = $this->escapeName($vmname);
                $err = gettext("Failed to delete VM.");
                $this->virshCommand($cmdArgs, $err);
                $result = true;
                break;
            case 'undefineplus':
                $this->shutdownWeb($params['vncport'], $params['spiceport'], $params['hostport'], $params['hostport2']);
                $undefine = false;
                if ($this->deleteDisksByVm($vmname, $lxc)) {
                    $undefine = true;
                } else {
                    $undefine = false;
                    throw new \OMV\Exception( gettext("Unable to delete disk(s) from VM - ") . $lv->get_last_error());
                }
                if ($undefine) {
                    $cmdArgs = [];
                    if ($lxc) {
                        $cmdArgs[] = '--connect lxc:///';
                    }
                    $cmdArgs[] = 'undefine';
                    if (!$lxc) {
                        $cmdArgs[] = '--nvram';
                    }
                    $cmdArgs[] = $this->escapeName($vmname);
                    $err = gettext("Failed to create VM.");
                    $this->virshCommand($cmdArgs, $err);
                }
                $result = true;
                break;
            case 'autostartdisable':
                $result = $lv->domain_set_autostart($dom,false);
                break;
            case 'autostartenable':
                $result = $lv->domain_set_autostart($dom,true);
                break;
        }
        if (!$result) {
            throw new \OMV\Exception( gettext("Unable to - ") . $params['command'] . $lv->get_last_error());
        }
    }

    public function doWeb($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $command = $params['command'];
        $hostport = intval($params['hostport']);
        $hostport2 = intval($params['hostport2']);
        // shutdown existing consoles if running
        $this->shutdownWeb($params['vncport'], $params['spiceport'], $hostport, $hostport2);
        // Get configuration data.
        $db = \OMV\Config\Database::getInstance();
        $webadmin = $db->get("conf.webadmin");
        $ssl = boolval($webadmin->get("enablessl"));
        $forcessl = boolval($webadmin->get("forcesslonly"));
        if ($ssl) {
            $certref = $webadmin->get("sslcertificateref");
            $crt = sprintf("/etc/ssl/certs/openmediavault-%s.crt", $certref);
            $key = sprintf("/etc/ssl/private/openmediavault-%s.key", $certref);
        }
        // create websockify process for noVNC
        if ($command == "start" && $hostport >= 1024) {
            $output = [];
            $cmdArgs = [];
            $cmdArgs[] = "sudo ";
            $cmdArgs[] = "--shell";
            $cmdArgs[] = "--non-interactive";
            if (!$ssl) {
                $cmdArgs[] = "--user=openmediavault-kvmweb";
            }
            $cmdArgs[] = "--";
            $cmdArgs[] = 'websockify';
            $cmdArgs[] = '--daemon';
            if ($ssl) {
                if ($forcessl) {
                    $cmdArgs[] = '--ssl-only';
                }
                $cmdArgs[] = sprintf('--cert "%s"', $crt);
                $cmdArgs[] = sprintf('--key "%s"', $key);
            }
            $cmdArgs[] = '--web /usr/share/novnc/';
            $cmdArgs[] = $hostport;
            $cmdArgs[] = sprintf('localhost:%d', $params['vncport']);
            $cmd = new \OMV\System\Process($cmdArgs);
            $cmd->setQuiet(true);
            $cmd->execute($output, $exitStatus);
            $cmdLine = $cmd->getCommandLine();
            if ((0 == $exitStatus) && (!empty($output))) {
                throw new \OMV\Exception(gettext("Failed to start websockify for noVNC.") . $cmdLine);
            }
        }
        // create websockify process for spice html5
        if ($command == "start" && $hostport2 >= 1024) {
            $output = [];
            $cmdArgs = [];
            $cmdArgs[] = "sudo ";
            $cmdArgs[] = "--shell";
            $cmdArgs[] = "--non-interactive";
            if (!$ssl) {
                $cmdArgs[] = "--user=openmediavault-kvmweb";
            }
            $cmdArgs[] = "--";
            $cmdArgs[] = 'websockify';
            $cmdArgs[] = '--daemon';
            if ($ssl) {
                if ($forcessl) {
                    $cmdArgs[] = '--ssl-only';
                }
                $cmdArgs[] = sprintf('--cert "%s"', $crt);
                $cmdArgs[] = sprintf('--key "%s"', $key);
            }
            $cmdArgs[] = '--web /usr/share/spice-html5/';
            $cmdArgs[] = $hostport2;
            $cmdArgs[] = sprintf('localhost:%d', $params['spiceport']);
            $cmd = new \OMV\System\Process($cmdArgs);
            $cmd->setQuiet(true);
            $cmd->execute($output, $exitStatus);
            $cmdLine = $cmd->getCommandLine();
            if ((0 == $exitStatus) && (!empty($output))) {
                throw new \OMV\Exception(gettext("Failed to start websockify for spice-html5.") . $cmdLine);
            }
        }
    }

    private function shutdownWeb($vncport, $spiceport, $vnchostport, $spicehostport)
    {
        // Get configuration data.
        $db = \OMV\Config\Database::getInstance();
        $webadmin = $db->get("conf.webadmin");
        $ssl = boolval($webadmin->get("enablessl"));
        $forcessl = boolval($webadmin->get("forcesslonly"));
        $remoteField = 7;
        $localField = 8;
        if ($ssl) {
            $localField = $forcessl ? 13 : 12;
            $remoteField = $forcessl ? 12 : 11;
        }
        // remove existing websockify processes if they exist
        if (intval($vncport) > 1024) {
            $output = [];
            $cmdLine = sprintf('pgrep -u openmediavault-kvmweb,root -a websockify | awk \'$%d == "localhost:%d" { print $1 }\' | xargs kill', $localField, $vncport);
            $cmd = new \OMV\System\Process($cmdLine);
            $cmd->setQuiet(true);
            $cmd->execute($output, $exitStatus);
        }
        if (intval($vnchostport) > 1024) {
            $output = [];
            $cmdLine = sprintf('pgrep -u openmediavault-kvmweb,root -a websockify | awk \'$%d == %d { print $1 }\' | xargs kill', $remoteField, $vnchostport);
            $cmd = new \OMV\System\Process($cmdLine);
            $cmd->setQuiet(true);
            $cmd->execute($output, $exitStatus);
        }
        if (intval($spiceport) > 1024) {
            $cmdLine = sprintf('pgrep -u openmediavault-kvmweb,root -a websockify | awk \'$%d == "localhost:%d" { print $1 }\' | xargs kill', $localField, $spiceport);
            $cmd = new \OMV\System\Process($cmdLine);
            $cmd->setQuiet(true);
            $cmd->execute($output, $exitStatus);
        }
        if (intval($spicehostport) > 1024) {
            $cmdLine = sprintf('pgrep -u openmediavault-kvmweb,root -a websockify | awk \'$%d == %d { print $1 }\' | xargs kill', $remoteField, $spicehostport);
            $cmd = new \OMV\System\Process($cmdLine);
            $cmd->setQuiet(true);
            $cmd->execute($output, $exitStatus);
        }
    }

    public function doChangeCpuMemory($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.dochangecpumemory');
        // create new libvirt object
        $conn = '';
        $lxc = false;
        if ($params['virttype'] == 'lxc') {
            $conn = '--connect lxc:/// ';
            $lxc = true;
            $lv = $this->connectLxc();
        } else {
            $lv = $this->connectQemu();
        }
        $memory = $params['memory'];
        $memoryunit = $params['memoryunit'];
        $vcpu = $params['vcpu'];
        $vmname = $params['vmname'];
        $skip = false;
        if ($vcpu == -1) {
            switch ($memoryunit) {
                case 'GiB':  $multi = 1024 * 1024; break;
                case 'MiB':  $multi = 1024; break;
            }
            $newMem = $memory * $multi;
            $cmdLine = sprintf('%ssetmaxmem %s %s --config', $conn, $this->escapeName($vmname), $newMem);
            $cmdLine2 = sprintf('%ssetmem %s %s --config', $conn, $this->escapeName($vmname), $newMem);
        } else if ($memory == -1) {
            $cmdLine = sprintf('%ssetvcpus %s %d --config --maximum', $conn, $this->escapeName($vmname), $vcpu);
            $cmdLine2 = sprintf('%ssetvcpus %s %d --config ', $conn, $this->escapeName($vmname), $vcpu);
            $domXml = new SimpleXMLElement($lv->domain_get_xml($vmname));
            if (isset($domXml->cpu->topology)) {
                $domXml->cpu->topology['dies'] = 1;
                $domXml->cpu->topology['sockets'] = 1;
                $domXml->cpu->topology['threads'] = 1;
                $domXml->cpu->topology['cores'] = $vcpu;
                $domXml->vcpu = $vcpu;
                $newXml = $domXml->asXML();
                $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
                if (!$lv->domain_change_xml($vmname, $newXml)) {
                    throw new \OMV\Exception( gettext("Unable to change topolgy cores for VM - ") . $lv->get_last_error());
                }
                $skip = true;
            }
        }
        if (!$skip) {
            $err = gettext("Failed to set maximum.");
            $this->virshCommand($cmdLine, $err);

            $err = gettext("Failed to set change.");
            $this->virshCommand($cmdLine2, $err);
        }
    }

    public function getVcpu($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        //$this->validateMethodParams($params, 'rpc.kvm.vmname');
        // create new libvirt object
        $lv = ($params['virttype'] == 'lxc' ? $this->connectLxc() : $this->connectQemu());
        $dom = $lv->get_domain_object($params['vmname']);
        $info = $lv->domain_get_info($dom);
        return ["vcpu" => $info['nrVirtCpu']];
    }

    public function getNotes($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // virsh desc
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = 'desc';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $notes = trim(implode('', array_filter($output)));
        return ["notes" => $notes];
    }

    public function setNotes($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.setnotes');
        // virsh desc
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = 'desc';
        $cmdArgs[] = sprintf('--domain %s', $this->escapeName($params['vmname']));
        $cmdArgs[] = sprintf("--new-desc '%s'", $this->escapeName($params['notes']));
        $err = gettext("Unable to change notes for VM.");
        $this->virshCommand($cmdArgs, $err);
    }

    public function getBackupList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
        // get backup list from list file
        $objects = [];
        $backuplist = '/etc/omv-backup-vm.list';
        if (file_exists($backuplist)) {
            $backuplistarray = file($backuplist);
	    if ($backuplistarray !== false) {
                $rows = array_map('str_getcsv', file($backuplist));
                $header = ['uuid','path','vmname','date','used'];
                foreach($rows as $row) {
                    $objects[] = array_combine($header, $row);
                }
            }
        }
        return ($objects);
    }

    public function deleteBackup($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        //$this->validateMethodParams($params, 'rpc.kvm.dobackup');
        $date = $params['date'];
        $path = $params['path'];
        $uuid = $params['uuid'];
        $vmname = $params['vmname'];
        $dir = sprintf('%s/%s/%s', $path, $vmname, $date);
        if ((is_dir($dir)) && (strlen($dir) > 2)) {
            $cmdArgs = [];
            $cmdArgs[] = '-rf';
            $cmdArgs[] = sprintf("'%s'", $dir);
            $cmd = new \OMV\System\Process('rm', $cmdArgs);
            $cmd->setQuiet(true);
            $cmd->execute($output, $exitStatus);
        }
        $cmdArgs = [];
        $cmdArgs[] = '-i';
        $cmdArgs[] = sprintf("'/^%s.*%s.*%s.*$/d'", $uuid, $vmname, $date);
        $cmdArgs[] = '/etc/omv-backup-vm.list';
        $cmd = new \OMV\System\Process('sed', $cmdArgs);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
    }

    public function doBackup($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.dobackup');
        if (substr($params['path'],0,1) != '/') {
            $params['path'] = '/' . $params['path'];
        }
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($params) {
                $cmdArgs = [];
                $cmdArgs[] = '/usr/sbin/omv-backup-vm';
                $cmdArgs[] = sprintf('-v %s', $params['vmname']);
                $cmdArgs[] = sprintf("-d '%s'", $params['path']);
                if ($params['compression'] === true) {
                    $cmdArgs[] = '-c';
                }
                $cmd = new \OMV\System\Process($cmdArgs);
                $cmd->setRedirect2to1();
                $cmdLine = $cmd->getCommandLine();
                if (0 !== $this->exec($cmdLine, $output, $bgOutputFilename)) {
                    throw new \OMV\ExecException($cmdLine, $output);
                }
                return $output;
            }
        );
    }

    public function getRestoreList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // get backup list from list file
        $objects = [];
        $backuplist = '/etc/omv-backup-vm.list';
        if (file_exists($backuplist)) {
            $backuplistarray = file($backuplist);
            if ($backuplistarray !== false) {
                $rows = array_map('str_getcsv', file($backuplist));
                foreach($rows as $row) {
                    $desc = sprintf('%s | %s | %s', $row[2], $row[3], $row[1]);
                    $objects[] = [ 'description' => $desc ];
                }
            }
         }
        return ($objects);
    }

    public function doRestore($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.dorestore');
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($params) {
                $backup = explode('|', $params['backup']);
                $date = trim($backup[1]);
                $path = trim($backup[2]);
                $vmname = trim($backup[0]);
                $cmdArgs = [];
                $cmdArgs[] = '/usr/sbin/omv-restore-vm';
                $cmdArgs[] = sprintf("-d '%s'", $path);
                $cmdArgs[] = sprintf('-r %s', $params['newname']);
                $cmdArgs[] = sprintf('-t %s', $date);
                $cmdArgs[] = sprintf('-v %s', $vmname);
                $cmd = new \OMV\System\Process($cmdArgs);
                $cmd->setRedirect2to1();
                $cmdLine = $cmd->getCommandLine();
                if (0 !== $this->exec($cmdLine, $output, $bgOutputFilename)) {
                    throw new \OMV\ExecException($cmdLine, $output);
                }
                return $output;
            }
        );
    }

    public function getJobList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.common.getlist");
        // Get configuration data.
        $db = \OMV\Config\Database::getInstance();
        $objects = $db->getAssoc("conf.service.kvm.job");
        // Filter the result.
        return $this->applyFilter($objects, $params['start'], $params['limit'],
            $params['sortfield'], $params['sortdir']);
    }

    function getJob($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.common.objectuuid");
        // Get the configuration object.
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get("conf.service.kvm.job", $params['uuid']);
        return $object->getAssoc();
    }

    function setJob($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.kvm.setjob");
        // Prepare the configuration object.
        $object = new \OMV\Config\ConfigObject("conf.service.kvm.job");
        $db = \OMV\Config\Database::getInstance();
        if (substr($params['path'],0,1) != '/') {
            $params['path'] = '/' . $params['path'];
        }
        $object->setAssoc($params);
        // Set the configuration object.
        $db->set($object);
        // Return the configuration object.
        return $object->getAssoc();
    }

    public function deleteJob($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.common.objectuuid");
        // Delete the configuration object.
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get("conf.service.kvm.job", $params['uuid']);
        $db->delete($object);
        // Return the deleted configuration object.
        return $object->getAssoc();
    }

    public function doJob($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.common.objectuuid");
        // Get the configuration object.
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get("conf.service.kvm.job", $params['uuid']);
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($object) {
                $cmdArgs = [];
                $cmdArgs[] = '/usr/sbin/omv-backup-vm';
                $cmdArgs[] = sprintf('-v %s', $object->get('vmname'));
                $cmdArgs[] = sprintf("-d '%s'", $object->get('path'));
                if ($object->get('keep') > 0) {
                    $cmdArgs[] = sprintf('-k %s', $object->get('keep'));
                }
                if ($object->get('compression') === true) {
                    $cmdArgs[] = '-c';
                }
                if ($object->get('poweroff') === true) {
                    $cmdArgs[] = '-p';
                }
                if ($object->get('samefmt') === true) {
                    $cmdArgs[] = '-s';
                }
                $cmd = new \OMV\System\Process($cmdArgs);
                $cmd->setRedirect2to1();
                $cmdLine = $cmd->getCommandLine();
                if (0 !== $this->exec($cmdLine, $output, $bgOutputFilename)) {
                    throw new \OMV\ExecException($cmdLine, $output);
                }
                return $output;
            }
        );
    }

    public function addVnc($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vnc');
        $spice = $params['spice'];
        $vmname = $params['vmname'];
        // add spicevmc channel if spice
        if ($spice == true) {
            $cmdArgs = [];
            if ($params['virttype'] == 'lxc') {
                $cmdArgs[] = '--connect lxc:///';
            }
            $cmdArgs[] = $this->escapeName($vmname);
            $cmdArgs[] = '--add-device';
            $cmdArgs[] = '--channel spicevmc';
            $err = gettext("Failed to add spicevmc channel.");
            $this->virshCommand($cmdArgs, $err, 'virt-xml');
        }
        // add with virt-xml
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = $this->escapeName($vmname);
        $cmdArgs[] = '--add-device';
        if ($spice == true) {
            $cmdArgs[] = '--graphics spice,listen=0.0.0.0';
            $err = gettext("Failed to add spice device.");
        } else {
            $cmdArgs[] = '--graphics vnc,listen=0.0.0.0';
            $err = gettext("Failed to add VNC device.");
        }
        $this->virshCommand($cmdArgs, $err, 'virt-xml');
    }

    public function removeVnc($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vnc');
        $throwMsg = '';
        $spice = $params['spice'];
        $vmname = $params['vmname'];
        // remove with virt-xml
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = $this->escapeName($vmname);
        $cmdArgs[] = '--remove-device';
        if ($spice == true) {
            $cmdArgs[] = '--graphics spice';
            $err = gettext("Failed to remove spice device.");
        } else {
            $cmdArgs[] = '--graphics vnc';
            $err = gettext("Failed to remove VNC device.");
        }
        $remove = $this->virshCommand($cmdArgs, $err, 'virt-xml', true);
        if ($remove != '') {
            $throwMsg = $remove;
        }
        // remove spicevmc channel if spice
        if ($spice == true) {
            $cmdArgs = [];
            if ($params['virttype'] == 'lxc') {
                $cmdArgs[] = '--connect lxc:///';
            }
            $cmdArgs[] = $this->escapeName($vmname);
            $cmdArgs[] = '--remove-device';
            $cmdArgs[] = '--channel spicevmc';
            $err = gettext("Failed to remove spicevmc channel.");
            $remove = $this->virshCommand($cmdArgs, $err, 'virt-xml', true);
            if ($remove != '') {
                $throwMsg .= PHP_EOL . $remove;
            }
            $cmdArgs = [];
            if ($params['virttype'] == 'lxc') {
                $cmdArgs[] = '--connect lxc:///';
            }
            $cmdArgs[] = $this->escapeName($vmname);
            $cmdArgs[] = '--remove-device';
            $cmdArgs[] = '--redirdev usb,type=spicevmc';
            $err = gettext("Failed to remove spicevmc redirdev.");
            $this->virshCommand($cmdArgs, $err, 'virt-xml', true);
        }
        if ($throwMsg != '') {
            throw new \OMV\Exception($throwMsg);
        }
    }

    public function getSettings($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // lxc
        $systemCtl = new \OMV\System\SystemCtl("lxc");
        $lxc = $systemCtl->isActive();
        // lxc-net
        $systemCtl = new \OMV\System\SystemCtl("lxc-net");
        $lxcnet = $systemCtl->isActive();
        // return status
        $svc = [ 'lxc' => $lxc, 'lxcnet' => $lxcnet ];
        return ($svc);
    }

    public function setSettings($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        //$this->validateMethodParams($params, 'rpc.omvextras.set');
        // lxc
        $systemCtl = new \OMV\System\SystemCtl("lxc");
        $lxc = $systemCtl->isActive();
        if ($lxc !== $params['lxc']) {
            if ($params['lxc']) {
                $systemCtl->enable(true);
            } else {
                $systemCtl->disable(true);
            }
        }
        // lxc-net
        $systemCtl = new \OMV\System\SystemCtl("lxc-net");
        $lxcnet = $systemCtl->isActive();
        if ($lxcnet !== $params['lxcnet']) {
            if ($params['lxcnet']) {
                $systemCtl->enable(true);
            } else {
                $systemCtl->disable(true);
            }
        }
    }

    public function getVirtTop($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $output = [];
        $cmdArgs = [];
        $cmdArgs[] = "--stream";
        $cmdArgs[] = "-n 1";
        $cmd = new \OMV\System\Process('virt-top', $cmdArgs);
        $cmd->execute($output);
        return (implode(PHP_EOL, $output));
    }

    public function enumerateArchitectures($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $output = [];
        $arch = $params['arch'];
        $cmd = new \OMV\System\Process('virsh', 'capabilities');
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $virshXml = implode(PHP_EOL, $output);
        $xml = new SimpleXMLElement($virshXml);
        $xpath = $xml->xpath('/capabilities/guest');
        $xpathSize = sizeof($xpath);
        $objects = [];
        for ($i = 0; $i < $xpathSize; $i++) {
            $arch = $xml->guest[$i]->arch->attributes()->name->__toString();
            $objects[] = ['arch' => $arch];
        }
        return ($objects);
    }

    public function enumerateCpus($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $output = [];
        $arch = $params['arch'];
        //$cmd = new \OMV\System\Process('virsh', 'cpu-models', $arch);
        $maps = array_diff(scandir('/usr/share/libvirt/cpu_map/'), ["..", "."]);
        $objects = [];
        $objects[] = ['cpu' => 'host host-passthrough'];
        $badlist = ['features', 'index', 'vendors'];
        foreach ($maps as $map) {
            $cpu = str_replace('.xml', '', $map);
            $cpu = str_replace('_', ' ', $cpu);
            $skip = false;
            foreach ($badlist as $bad) {
                if (strpos($cpu, $bad) !== false) {
                    $skip = true;
                    break;
                } 
            }
            if ($skip) {
                continue;
            }
            $objects[] = ['cpu' => $cpu];
        }
        $objects[] = ['cpu' => 'other not listed'];
        return ($objects);
    }

    public function enumerateOses($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $output = [];
        $cmd = new \OMV\System\Process('osinfo-query', '--sort=name', '--fields=short-id,name os');
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $objects = [];
        $count = count($output);
        if ($count > 1) {
            for ($i = 1; $i < $count; $i++) {
                $os = explode('|', $output[$i]);
                if (strlen($os[1]) < 3) {
                    continue;
                }
                $objects[] = [
                    'id' => trim($os[0]),
                    'name' => trim($os[1])
                ];
            }
        }
        return ($objects);
    }

    public function enumerateVg($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $output = [];
        $cmdArgs = [];
        $cmdArgs[] = '--noheadings';
        $cmdArgs[] = '--all';
        $cmdArgs[] = '--options vg_name';
        $cmd = new \OMV\System\Process('vgs', $cmdArgs);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $objects = [];
        foreach (array_filter($output) as $vg) {
            $objects[] = ['vgname' => trim($vg)];
        }
        return ($objects);
    }

    private function virshCommand($cmdArgs, $msg, $virsh = 'virsh', $quiet = false) {
        $errMsg = '';
        $output = [];
        $cmd = new \OMV\System\Process($virsh, $cmdArgs);
        $cmd->setQuiet(true);
        $cmd->setRedirect2to1();
        $today = '[' . date("Y-m-d H:i:s") . '] ';
        file_put_contents("/var/log/omv-virsh-command.log",
          $today . $cmd->getCommandLine() . PHP_EOL,
          FILE_APPEND
        );
        $cmd->execute($output, $exitStatus);
        if (0 != $exitStatus) {
            $outMsg = implode(PHP_EOL, array_filter($output));
            $cmdMsg = implode(' ', array_filter($cmdArgs));
            $errMsg = sprintf('%s%s%s%s%s %s',
                $msg, PHP_EOL, $outMsg, PHP_EOL, $virsh, $cmdMsg
            );
            file_put_contents("/var/log/omv-virsh-command.log",
              $today . $errMsg . PHP_EOL,
              FILE_APPEND
            );
            if (!$quiet) {
                throw new \OMV\Exception($errMsg);
            }
        }
        return ($errMsg);
    }

    private function deviceArgs($cmd, $vmname, $filename, $state) {
        $cmdArgs = [];
        $cmdArgs[] = $cmd;
        $cmdArgs[] = sprintf('--domain %s', $vmname);
        $cmdArgs[] = sprintf("--file '%s'", $filename);
        $cmdArgs[] = '--persistent';
        $cmdArgs[] = '--config';
        if ($state == 'running') {
            $cmdArgs[] = '--live';
        }
        return ($cmdArgs);
    }


    // LXC functions

    public function enumerateImages($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $arch = $this->getArch();
        $this->fillImages();
        // get list of images
        $images = file($this->imageCache);
        $objects= [];
        $objects[] = [
            'desc' => gettext("No image - use existing image path"),
            'image' => 'noimage'
        ];
        foreach (array_filter($images) as $image) {
            $line = explode(';', $image);
            if ($line[3] == 'cloud') continue;
            if (($arch == $line[2] ) ||
              ($arch == 'amd64' && $line[2] == 'i386') ||
              ($arch == 'arm64' && $line[2] == 'armhf')) {
                array_pop($line);
                $desc = implode(';', $line);
                $objects[] = [
                    'desc' => $desc,
                    'image' => $desc
                ];
            }
        }
        return ($objects);
    }

    public function forceImageListRefresh($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // remove cache file if found and download new list
        unlink($this->imageCache);
        $this->fillImages();
    }

    private function downloadImage($image, $path)
    {
        // build image url
        $repo = 'https://images.linuxcontainers.org/images';
        $fldr = str_replace(';', '/', $image);
        $file = 'rootfs.tar.xz';
        $url = sprintf('%s/%s/%s', $repo, $fldr, $file);

        // build command args
        $cmdArgs = [];
        $cmdArgs[] = 'wget';
        $cmdArgs[] = '-qO-';
        $cmdArgs[] = $url;
        $cmdArgs[] = '|';
        $cmdArgs[] = 'tar';
        $cmdArgs[] = 'xJ';
        $cmdArgs[] = '-C';
        $cmdArgs[] = $path;
        $cmd = new \OMV\System\Process($cmdArgs);

        // log command
        $today = '[' . date("Y-m-d H:i:s") . '] ';
        file_put_contents("/var/log/omv-virsh-command.log",
            $today . $cmd->getCommandLine() . PHP_EOL,
            FILE_APPEND
        );

        // execute command
        $output = [];
        $cmd->execute($output, $exitStatus);

        // if failure
        if (0 != $exitStatus) {
            $outMsg = implode(PHP_EOL, array_filter($output));
            $cmdMsg = implode(' ', array_filter($cmdArgs));
            $errMsg = sprintf('%s%s%s%s%s %s',
                $msg, PHP_EOL, $outMsg, PHP_EOL, $virsh, $cmdMsg
            );
            file_put_contents("/var/log/omv-virsh-command.log",
                $today . $errMsg . PHP_EOL,
                FILE_APPEND
            );
            throw new \OMV\Exception($errMsg);
        }
    }

    private function fillImages()
    {
        // get current time
        $time = time();
        // if the cache doesn't exist or is older than 1 day, download it
        if ( !file_exists($this->imageCache) or
           ( $time - filemtime($this->imageCache) >= 60*60*24) ) {
            $url = 'https://images.linuxcontainers.org/meta/1.0/index-system';
            for ($i = 0; $i < 5; $i++) {
                $list = file($url);
                if ($list && count($list) > 0) {
                    break;
                }
                sleep(3);
            }
            // Get repos info and put in cache file
            if ($list && count($list) > 0) {
                file_put_contents($this->imageCache, $list);
            }
        }
    }

    private function resetLxcPassword($path)
    {
        // create encrypted password
        $cmd = new \OMV\System\Process('mkpasswd', 'openmediavault');
        $output = [];
        $cmd->execute($output, $exitStatus);
        $pass = $output[0];
        // reset password
        $cmdArgs = [];
        $cmdArgs[] = 'awk';
        $cmdArgs[] = '-i inplace';
        $cmdArgs[] = '-F":"';
        $cmdArgs[] = sprintf("'BEGIN{OFS = \":\"} /root/{\$2=\"%s\"}{ print }'", $pass);
        $cmdArgs[] = sprintf('%s/etc/shadow', $path);
        $cmd = new \OMV\System\Process($cmdArgs);
        $output = [];
        $cmd->execute($output, $exitStatus);
    }
}
