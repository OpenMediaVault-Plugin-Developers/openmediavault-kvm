<?php
/*
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2021-2022 OpenMediaVault Plugin Developers
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
        $this->registerMethod('getVmXml');
        $this->registerMethod('setVmXml');
        $this->registerMethod('setVm');
        $this->registerMethod('setVmVI');
        $this->registerMethod('getDisk');
        $this->registerMethod('cloneVm');

        $this->registerMethod('addDisk');
        $this->registerMethod('addOptical');
        $this->registerMethod('resizeDisk');
        $this->registerMethod('removeDisk');

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

        $this->registerMethod('enumerateSnapshots');
        $this->registerMethod('addSnapshot');
        $this->registerMethod('deleteSnapshot');
        $this->registerMethod('revertSnapshot');

        $this->registerMethod('addUsb');
        $this->registerMethod('removeUsb');
        $this->registerMethod('enumerateUsbByHost');
        $this->registerMethod('enumerateUsbByVm');

        $this->registerMethod('addVnc');
        $this->registerMethod('removeVnc');

        $this->registerMethod('getNotes');
        $this->registerMethod('setNotes');

        $this->registerMethod('doCommand');
        $this->registerMethod('doWeb');
        $this->registerMethod('doChangeCpuMemory');
        $this->registerMethod('getVcpu');

        $this->registerMethod('enumerateArchitectures');
        $this->registerMethod('enumerateCpus');
        $this->registerMethod('enumerateOses');
    }

    private function connectQemu()
    {
        $lv = new Libvirt('/var/log/openmediavault-kvm.log');
        $lv->connect('qemu:///system');
        return ($lv);
    }

    private function getArch()
    {
        $cmd = new \OMV\System\Process('dpkg --print-architecture');
        $cmd->execute($output);
        return (strtolower($output[0]));
    }

    private function getVolumeSizes($path)
    {
	$sizes = 'n/a';
        if (file_exists($path)) {
	    $output = [];
            $cmdArgs = [];
            $cmdArgs[] = '/usr/bin/qemu-img';
            $cmdArgs[] = 'info';
            $cmdArgs[] = $path;
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
        $cmdArgs = [];
        $cmdArgs[] = 'net-autostart';
        $cmdArgs[] = '--network';
        $cmdArgs[] = $params['name'];
        $cmdArgs[] = $disable;
        $err = gettext("Failed to ") . $params['command'];
        $this->virshCommand($cmdArgs, $err);
    }

    public function addVmNic($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // create new libvirt object
        $lv = $this->connectQemu();
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
        if (!result) {
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
        $cmdArgs[] = 'domiflist';
        $cmdArgs[] = sprintf('--domain %s', $vmname);
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
        $cmdArgs[] = 'detach-interface';
        $cmdArgs[] = sprintf('--domain %s', $vmname);
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
        $cmdArgs[] = 'domiflist';
        $cmdArgs[] = sprintf('--domain %s', $vmname);
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
        $zpool = $params['zpoolname'];
        // create new libvirt object
        $lv = $this->connectQemu();
        $xml = sprintf("<pool type='%s'>", $type);
        $xml .= sprintf("<name>%s</name>", $name);
        switch ($type) {
            case 'dir':
                $path = (substr($path,0) == '/' ? $path : '/' . $path);
                mkdir($path, 0755, true);
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
        // create new libvirt object
        $lv = $this->connectQemu();
        $command = $params['command'];
        $pool = $params['name'];
        $res = $lv->get_storagepool_res($pool);
        $disable = '';
        switch ($command) {
            case 'start':
                $result = $lv->storagepool_create($res);
                break;
            case 'stop':
                $result = $lv->storagepool_destroy($res);
                $disable = ' --disable';
                break;
        }
        if (!$result) {
            $msg = sprint("%s %s %s %s", gettext("Unable to"), $command, gettext("pool"), $lv->get_last_error());
            throw new \OMV\Exception($msg);
        }
        $cmdLine = sprintf('virsh pool-autostart %s%s', $pool, $disable);
        $cmd = new \OMV\System\Process($cmdLine);
        $cmd->execute($output, $exitStatus);
        if (0 !== $exitStatus) {
            throw new \OMV\Exception(gettext("Failed to change autostart") . " :: " . $cmdLine);
        }
        $cmdArgs = [];
        $cmdArgs[] = 'pool-autostart';
        $cmdArgs[] = $pool;
        $cmdArgs[] = $disable;
        $err = gettext("Failed to create VM.");
        $this->virshCommand($cmdArgs, $err);
    }



    public function getVmList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
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
            $diskcnt = $lv->get_disk_count($dom);
            if ($diskcnt > 0) {
                $dSize = $lv->get_disk_capacity($dom);
                $disks = sprintf('%d / %s', $diskcnt, $dSize);
            } else {
                $disks = 'n/a';
            }
            // vnc and novnc
            $vncport = $lv->domain_get_vnc_port($dom);
            if (! $vncport || $vncport < 1024 || $vncport == '') {
                $vncport = 'n/a';
            }
            $novncport = 'n/a';
            $novncurl = 'n/a';
            if (intval($vncport) >= 1024) {
                $output = [];
                $cmdLine = sprintf('pgrep -u openmediavault-kvmweb -a websockify | awk \'$7 == "localhost:%d" { print $6 }\'', $vncport);
                $cmd = new \OMV\System\Process($cmdLine);
                $cmd->setQuiet(true);
                $cmd->execute($output, $exitStatus);
                $novncport = intval($output[0]);
                if ($novncport >= 1024) {
                    $novncurl = sprintf('<a href="http://%s:%d/vnc.html?resize=remote&autoconnect=1" target="_blank">link</a>', gethostname(), $novncport);
                }
            }
            if (!$lv->domain_get_xml($name)) {
                continue;
            }
            $domXml = new SimpleXMLElement($lv->domain_get_xml($name));
            // spice and spice_html5
            $spiceport = '';
            $xpath = $domXml->xpath('//graphics');
            $xpathSize = sizeof($xpath);
            for ($i = 0; $i < $xpathSize; $i++) {
                $graphics = $domXml->devices->graphics[$i];
                if ($graphics['type'] == 'spice') {
                    $spiceport = $graphics['port'];
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
                $cmdLine = sprintf('pgrep -u openmediavault-kvmweb -a websockify | awk \'$7 == "localhost:%d" { print $6 }\'', $spiceport);
                $cmd = new \OMV\System\Process($cmdLine);
                $cmd->setQuiet(true);
                $cmd->execute($output, $exitStatus);
                $spicehtml5port = intval($output[0]);
                if ($spicehtml5port >= 1024) {
                    $spicehtml5url = sprintf('<a href="http://%s:%d/spice_auto.html?resize=remote" target="_blank">link</a>', gethostname(), $spicehtml5port);
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
                'mem' => $info['memory'] * 1024,
                'cpu' => $info['nrVirtCpu'],
                'state' => $lv->domain_state_translate($info['state']),
                'disks' => $disks,
                'arch' => $lv->domain_get_arch($dom),
                'autostart' => $lv->domain_get_autostart($dom),
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

    public function getVmXml($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        //$this->validateMethodParams($params, 'rpc.kvm.vmname');
        // create new libvirt object
        $lv = $this->connectQemu();
        $vmxml = "";
        if ($params['vmname']) {
            $vmxml = $lv->domain_get_xml($params['vmname']);
        }
        return ["vmxml" => $vmxml];
    }

    public function setVmXml($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.setvmxml');
        // create new libvirt object
        $lv = $this->connectQemu();
        if ($params['vmname']) {
            $result = $lv->domain_change_xml($params['vmname'], $params['vmxml']);
        } else {
            $result = $lv->domain_define($params['vmxml']);
        }
        if (!results) {
            throw new \OMV\Exception( gettext("Unable to set VM XML - ") . $lv->get_last_error());
        }
    }

    public function setVm($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.setvm');
        // create new libvirt object
        $lv = $this->connectQemu();
        $arch = $params['arch'];
        $chipset = $params['chipset'];
        if ((empty($chipset)) || ($chipset != 'q35')) {
            $chipset = 'pc';
            $sound = 'ich9';
        } else {
            $chipset = 'q35';
            $sound = 'ac97';
        }
        $hostArch = $this->getArch();
        $os = strtolower($params['os']);
        $uefi = $params['uefi'];
        $vcpu = $params['vcpu'];
        $vmname = $params['vmname'];
        switch ($arch) {
            case 'aarch64':
                $apic = false;
                $cpuModel = 'cortex-a57';
                $fd = '/usr/share/AAVMF/AAVMF_CODE.fd';
                $ld = 'pflash';
                $machine = 'virt';
                if (($hostArch == 'arm64') || ($hostArch == 'armhf')) {
                    $domain_type = "kvm";
                    $native_graphics = true;
                } else {
                    $domain_type = "qemu";
                    $native_graphics = false;
                }
                break;
            case 'arm':
                $apic = false;
                $cpuModel = 'cortex-a15';
                $fd = '/usr/share/AAVMF/AAVMF_CODE.fd';
                $machine = 'virt';
                if (($hostArch == 'arm64') || ($hostArch == 'armhf')) {
                    $domain_type = "kvm";
                    $native_graphics = true;
                } else {
                    $domain_type = "qemu";
                    $native_graphics = false;
                }
                break;
            case 'i386':
                $apic = true;
                $domain_type = "kvm";
                $native_graphics = true;
                $machine = $chipset;
                if (($hostArch == 'amd64') || ($hostArch == 'i386')) {
                    $domain_type = "kvm";
                } else {
                    $domain_type = "qemu";
                }
                break;
            case 'x86_64':
                $apic = true;
                $fd = '/usr/share/OVMF/OVMF_CODE.fd';
                $ld = 'rom';
                $native_graphics = true;
                $machine = $chipset;
                if (($hostArch == 'amd64') || ($hostArch == 'i386')) {
                    $domain_type = "kvm";
                } else {
                    $domain_type = "qemu";
                }
                break;
        }
        $xml = sprintf("<domain type='%s'>", $domain_type);
        $xml .= sprintf("<name>%s</name>", $vmname);
        $xml .= sprintf("<description>%s</description>", htmlspecialchars($params['notes'], ENT_XML1));
        $xml .= sprintf("<memory unit='%s'>%d</memory>", $params['memoryunit'], $params['memory']);
        $xml .= sprintf("<vcpu>%d</vcpu>", $vcpu);
        $xml .= "<os>";
        $xml .= sprintf("<type arch='%s' machine='%s'>hvm</type>", $arch, $machine);
        if ($uefi) {
            $xml .= sprintf("<loader readonly='yes' type='%s'>%s</loader>", $ld, $fd);
        }
        $xml .= "<boot dev='hd'/>";
        $xml .= "<boot dev='cdrom'/>";
        $xml .= "<boot dev='network'/>";
        $xml .= "</os>";
        $xml .= "<features>";
        $xml .= "<acpi/>";
        if ($apic) {
            $xml .= "<apic/>";
        } else {
            //$xml .= "<gic version='2'/>";
        }
        if ($os == "windows") {
            $xml .= "<hyperv>";
            $xml .= "<relaxed state='on'/>";
            $xml .= "<vapic state='on'/>";
            $xml .= "<spinlocks state='on' retries='8191'/>";
            $xml .= "</hyperv>";
        }
        $xml .= "</features>";
        if ($os == "windows") {
            if ($hostArch == 'amd64' || $hostArch == 'i386') {
                $xml .= "<cpu mode='host-passthrough' check='none'>";
                $xml .= sprintf("<topology sockets='1' cores='%d' threads='1'/>", $vcpu);
                $xml .= "<feature policy='disable' name='smep'/>";
                $xml .= "</cpu>";
            }
            $xml .= "<clock offset='localtime'/>";
        } elseif ($arch == 'aarch64' || $arch == 'arm') {
            $xml .= "<cpu mode='custom' match='exact'>";
            $xml .= sprintf("<model>%s</model>", $cpuModel);
            $xml .= "</cpu>";
            $xml .= "<clock offset='utc'/>";
        }
        $xml .= "<devices>";
        if ($native_graphics) {
            if ($params['vnc']) {
                $xml .= "<graphics type='vnc' port='-1' autoport='yes'/>";
            }
            if ($params['spice']) {
                $xml .= "<graphics type='spice' port='-1' tlsPort='-1' autoport='yes' listen='0.0.0.0'>";
                $xml .= "<gl enable='no'/>";
                $xml .= "</graphics>";
            }
            $xml .= "<video>";
            $xml .= "<model type='qxl'/>";
            $xml .= "</video>";
            $xml .= "<channel type='spicevmc'>";
            $xml .= "<target type='virtio'/>";
            $xml .= "</channel>";
            $xml .= "<input type='tablet' bus='virtio'/>";
        } else {
             $xml .= "<console type='pty'/>";
             $xml .= "<channel type='unix'>";
             $xml .= "<source mode='bind'/>";
             $xml .= "<target type='virtio' name='org.qemu.guest.agent.0'/>";
             $xml .= "</channel>";
             $xml .= "<rng model='virtio'>";
             $xml .= "<backend model='random'>/dev/urandom</backend>";
             $xml .= "</rng>";
        }
        if ($params['audio']) {
            $xml .= sprintf("<sound model='%s'/>", $sound);
        }
        $xml .= "<memballoon model='virtio'>";
        $xml .= "<stats period='10'/>";
        $xml .= "</memballoon>";
        $xml .= "</devices>";
        $xml .= "</domain>";
        $newvm = $lv->domain_define($xml);
        if (!$newvm) {
            $msg = "Error defining domain: " . $lv->get_last_error();
            $msg .= PHP_EOL . PHP_EOL . $xml;
            throw new \OMV\Exception( gettext("Unable to create domain - ") . $msg);
        }
        $volbus = $params['volbus'];
        $voldisk = $params['voldisk'];
        $volformat = $params['volformat'];
        $voliso = $params['voliso'];
        $volname = $params['volname'];
        if (($volname == "") || (empty($volname))) {
            $volname = $vmname;
        }
        $volpool = $params['volpool'];
        $volsize = $params['volsize'];
        $volunit = $params['volunit'];
        if ($volunit == "") {
            $volunit = "G";
        }

        // create new disk (if specified)
        if ($voldisk == 'Create new disk') {
            $newdisk = $lv->storagevolume_create($volpool, $volname.'.'.$volformat, $volsize.$volunit, $volsize.$volunit, $volformat);  
            if ($newdisk) {
                $diskpath = libvirt_storagevolume_get_path($newdisk);
            } else {
                throw new \OMV\Exception( gettext("Unable to create new disk - ") . $lv->get_last_error());
            }
        } else {
            $volformat = pathinfo($voldisk, PATHINFO_EXTENSION);
            $diskpath = $voldisk;
        }

        // add disk to VM
        if (($uefi) || ($chipset == 'q35')) {
            $cdpre = "sd";
            $ctrl = "sata";
        } else {
            $cdpre = "hd";
            $ctrl = "ide";
        }
        switch ($volbus) {
            case "virtio":
                $dev = "vda";
                $cd = $cdpre . "a";
                break;
            case "sata":
            case "scsi":
                $dev = "sda";
                $cd = $cdpre . "b";
                break;
            case "ide":
                $dev = "hda";
                $cd = $cdpre . "b";
                break;
        }
        if (!$lv->domain_disk_add($newvm, $diskpath, $dev, $volbus, $volformat)) {
            throw new \OMV\Exception( gettext("Unable to add disk to VM - ") . $lv->get_last_error());
        }

        // add optical storage
        if ($voliso != "none") {
            // add a new cdrom XML
            $domXml = new SimpleXMLElement($lv->domain_get_xml($vmname));

            $diskXml = $domXml->devices->addChild('disk');
            $diskXml->addAttribute('type', 'file');
            $diskXml->addAttribute('device', 'cdrom');

            $driverXml = $diskXml->addChild('driver');
            $driverXml->addAttribute('name', 'qemu');
            $driverXml->addAttribute('type', 'raw');

            $sourceXml = $diskXml->addChild('source');
            $sourceXml->addAttribute('file', $voliso);

            $targetXml = $diskXml->addChild('target');
            $targetXml->addAttribute('dev', $cd);
            $targetXml->addAttribute('bus', $ctrl);

            $newXml = $domXml->asXML();
            $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);

            if (!$lv->domain_change_xml($vmname, $newXml)) {
                throw new \OMV\Exception( gettext("Unable to add ISO to VM - ") . $lv->get_last_error());
            }
        }

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
            $result = $lv->domain_nic_add($newvm, $mac, $net, $model);
        }
        if (!result) {
            throw new \OMV\Exception( gettext("Unable to add network adapter to VM - ") . $lv->get_last_error());
        }
    }

    public function setVmVI($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
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
        $vcpu = $params['vcpu'];
        $vmname = $params['vmname'];
        $xmlFile = sprintf('/tmp/%s.xml', $vmname);

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

        // memory
        $memory = $params['memory'];
        if ($params['memoryunit'] == 'GiB') {
            $memory *= 1024;
        }

        // OS-based arguments
        if (strpos($os, "Windows") !== false) {
            $vcpuArgs = sprintf('sockets=1,cores=%d,threads=1', $params['vcpu']);
            $clockArgs = 'offset=localtime';
        } else {
            $vcpuArgs = $params['vcpu'];
            $clockArgs = 'offset=utc';
        }

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
                $fd = '/usr/share/OVMF/OVMF_CODE.fd';
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
            $diskArgs = sprintf('size=%d,bus=%s,pool=%s,cache=none,io=native,format=%s', $volsize, $volbus, $volpool, $volformat);
        } else {
            $diskArgs = sprintf('%s,bus=%s,cache=none,io=native', $voldisk, $volbus);
            $volformat = pathinfo($voldisk, PATHINFO_EXTENSION);
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
        $cmdArgs[] = '--boot hd,cdrom';
        $cmdArgs[] = sprintf('--arch %s', $arch);
        $cmdArgs[] = sprintf('--cpu %s', $cpuModel);
        $cmdArgs[] = sprintf('--disk %s', $diskArgs);
        $cmdArgs[] = sprintf('--memory %s', $memory);
        $cmdArgs[] = sprintf('--metadata description="%s"', htmlspecialchars($params['notes'], ENT_XML1));
        $cmdArgs[] = sprintf('--name %s', $vmname);
        $cmdArgs[] = sprintf('--network %s', $netArgs);
        $cmdArgs[] = sprintf('--os-variant name=%s', $params['os']);
        $cmdArgs[] = sprintf('--vcpus %s', $vcpuArgs);

        if ($params['audio']) {
            $cmdArgs[] = '--sound default';
        }

        if ($params['uefi']) {
            $cmdArgs[] = sprintf('--boot loader=%s', $fd);
        }

        if ($voliso != 'none') {
            $cmdArgs[] = sprintf('--cdrom %s', $params['voliso']);
            $printStep = ' 2';
        } else {
            $printStep = '';
        }

        if ($params['vnc']) {
            $cmdArgs[] = '--graphics vnc,listen=0.0.0.0';
        }
        if ($params['spice']) {
            $cmdArgs[] = '--graphics spice,listen=0.0.0.0';
        }

        $cmdArgs[] = sprintf('--print-xml%s', $printStep);
        $cmdArgs[] = sprintf(' > %s', $xmlFile);

        $err = gettext("Failed to create VM XML.");
        $this->virshCommand($cmdArgs, $err, 'virt-install');

        // import newly created xml file
        $cmdArgs = [];
        $cmdArgs[] = 'define';
        $cmdArgs[] = sprintf('--file %s', $xmlFile);

        $err = gettext("Failed to create VM.");
        $this->virshCommand($cmdArgs, $err);
    }

    private function addVmBridgeNic($vmname, $mac, $net)
    {
        // create new libvirt object
        $lv = $this->connectQemu();

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
        // get VM object
        $vm = $lv->get_domain_by_name($params['vmname']);

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
        // create new disk (if specified)
        if ($voldisk == "Create new disk") {
            $newdisk = $lv->storagevolume_create($volpool, $volname.'.'.$volformat, $volsize.$volunit, $volsize.$volunit, $volformat);  
            if (!$newdisk) {
                throw new \OMV\Exception( gettext("Unable to create new disk - ") . $lv->get_last_error());
            }
            $diskpath = libvirt_storagevolume_get_path($newdisk);
        } else {
            $diskpath = $voldisk;
            $volformat = pathinfo($voldisk, PATHINFO_EXTENSION);
            $volname = pathinfo($voldisk, PATHINFO_FILENAME);
            if ($volformat != "qcow2") {
                $volformat = "raw";
            }
        }
        $targetdev = "";
        $devs = [];
        switch ($volbus) {
            case "virtio":
                $dev = "vd";
                break;
            case "sata":
            case "scsi":
                $dev = "sd";
                break;
            case "ide":
                $dev = "hd";
                break;
        }
        for ($i = 'a'; $i < 'z'; $i++) {
            $devs[] = $dev . $i;
        }
        $tmp = libvirt_domain_get_disk_devices($vm);
        $result = array_intersect($devs, $tmp);
        if (count($result) > 0) {
            $highestresult = max($result);
            $targetdev = ++$highestresult;
        } else {
            $targetdev = $dev . "a";
        }
        // add disk to VM
        if (!$lv->domain_disk_add($vm, $diskpath, $targetdev, $volbus, $volformat)) {
            throw new \OMV\Exception( gettext("Unable to add disk to VM - ") . $lv->get_last_error());
        }
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
            $targetbus = 'sata';
            $dev = "sd";
        } else {
            $targetbus = 'ide';
            $dev = "hd";
        }
        $targetdev = "";
        $devs = [];
        for ($i = 'a'; $i < 'z'; $i++) {
            $devs[] = $dev . $i;
        }
        $tmp = libvirt_domain_get_disk_devices($vm);
        $result = array_intersect($devs, $tmp);
        if (count($result) > 0) {
            $highestresult = max($result);
            $targetdev = ++$highestresult;
        } else {
            $targetdev = $dev . "a";
        }
        // add optical storage
        $cmdArgs = [];
        $cmdArgs[] = 'attach-disk';
        $cmdArgs[] = sprintf('--domain %s', $vmname);
        $cmdArgs[] = sprintf('--source %s', $voliso);
        $cmdArgs[] = sprintf('--target %s', $targetdev);
        $cmdArgs[] = sprintf('--targetbus %s', $targetbus);
        $cmdArgs[] = '--type cdrom';
        $cmdArgs[] = '--mode readonly';
        $cmdArgs[] = '--persistent';
        $cmdArgs[] = '--config';
        if ($params['state'] == 'running') {
            $cmdArgs[] = '--live';
        }
        $err = gettext("Failed to add optical disk.");
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
        $cmdArgs[] = sprintf('--domain %s', $params['vmname']);
        $cmdArgs[] = sprintf('--target %s', $params['disk']);
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
        // create new libvirt object
        $lv = $this->connectQemu();
        // resize disk
        switch ($params['volsizeunit']) {
            case 'GiB':  $multi = 1024 * 1024; break;
            case 'MiB':  $multi = 1024; break;
            case 'TiB':  $multi = 1024 * 1024 * 1024; break;
        }
        if (!$lv->resize_storagevolume_by_path($params['path'], $params['volsize'] * $multi)) {
            throw new \OMV\Exception( gettext("Unable to resize disk - ") . $lv->get_last_error());
        }
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

    public function getVolumeList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
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
                        $objects[] = [
                            'name' => $keys[$i],
                            'pool' => $pool,
                            'ext' => $ext,
                            'capacity' => $volume['capacity'],
                            'allocation' => $volume['allocation'],
                            'percentage' => $percentage,
                            'path' => $volume['path'],
                            'propConvert' => ($ext == 'qcow2') ? true : false,
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
            $cmdArgs[] = $pool;
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
        $cmdArgs[] = $params['vmname'];
        $cmdArgs[] = '--details';
        $cmdArgs[] = '| sed 1,2d';
        $cmdArgs[] = '| awk NF';
        if ($optical) {
            $cmdArgs[] = '| awk \'$2 == "cdrom" { print $2","$3","$4 }\'';
        } else {
            $cmdArgs[] = '| awk \'$2 != "cdrom" { print $2","$3","$4 }\'';
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
                'path' => $src
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
        $image = $name . '.' . $format;
        $size = $params['volsize'] . $params['volunit'];
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
        $newPath = str_replace($ext, $format, $path);
        if (file_exists($newPath)) {
            throw new \OMV\Exception($newPath . gettext(" already exists!"));
        }
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($params, $format, $path, $newPath) {
                $cmdArgs = [];
                $cmdArgs[] = '/usr/bin/qemu-img';
                $cmdArgs[] = 'convert';
                $cmdArgs[] = '-O ' . $format;
                $cmdArgs[] = $path;
                $cmdArgs[] = $newPath;
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
                $cmdArgs[] = $params['path'];
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
                $cmdArgs[] = sprintf('--output-document=%s/%s', $params['path'], $params['filename']);
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

    private function deleteDisksByVm($vmname)
    {
        // create new libvirt object
        $lv = $this->connectQemu();
        $domXml = new SimpleXMLElement($lv->domain_get_xml($vmname));
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

    public function enumerateSnapshots($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        // create new libvirt object
        $lv = $this->connectQemu();
        $dom = $lv->get_domain_object($params['vmname']);
        $snaps = $lv->domain_snapshots_list($dom);
        $objects= [];
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
        // Filter the result.
        return ($objects);
    }

    public function addSnapshot($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        // create new libvirt object
        $lv = $this->connectQemu();
        $dom = $lv->get_domain_object($params['vmname']);
        if (!$lv->domain_snapshot_create($dom)) {
            throw new \OMV\Exception( gettext("Unable to create snapshot - ") . $lv->get_last_error());
        }
    }

    public function deleteSnapshot($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.snapshot');
        // create new libvirt object
        $lv = $this->connectQemu();
        if (!$lv->domain_snapshot_delete($params['vmname'], $params['snapname'])) {
            throw new \OMV\Exception( gettext("Unable to delete snapshot - ") . $lv->get_last_error());
        }
    }

    public function revertSnapshot($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        //$this->validateMethodParams($params, 'rpc.kvm.snapshot');
        // create new libvirt object
        $lv = $this->connectQemu();
        if (!$lv->domain_snapshot_revert($params['vmname'], $params['snapname'])) {
            throw new \OMV\Exception( gettext("Unable to revert to snapshot - ") . $lv->get_last_error());
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
            $dev = $domXml->devices->hostdev[$i]->source;
            $vendorProduct = sprintf('%s:%s', $dev->vendor['id'], $dev->product['id']);
            $vpCheck = str_replace('0x', '', $vendorProduct);
            $description = $vendorProduct;
            for ($j = 0; $j < $usbCount; $j++) {
                if (strpos($output[$j], $vpCheck)) {
                    $description = $output[$j];
                    break;
                }
            }
            $objects[] = [
                'device' => $vendorProduct,
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
        // create usb xml
        $usbXml = $this->createUsbXml($params['device']);
        // add usb with virsh
        $cmdArgs = [];
        $cmdArgs[] = 'attach-device';
        $cmdArgs[] = sprintf('--domain %s', $params['vmname']);
        $cmdArgs[] = sprintf('--file %s', $usbXml);
        $cmdArgs[] = '--persistent';
        $cmdArgs[] = '--config';
        if ($params['state'] == 'running') {
            $cmdArgs[] = '--live';
        }
        $err = gettext("Failed to add USB.");
        $this->virshCommand($cmdArgs, $err);
    }

    public function removeUsb($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.usb');
        // create usb xml
        $usbXml = $this->createUsbXml($params['device']);
        // remove usb with virsh
        $cmdArgs = [];
        $cmdArgs[] = 'detach-device';
        $cmdArgs[] = sprintf('--domain %s', $params['vmname']);
        $cmdArgs[] = sprintf('--file %s', $usbXml);
        $cmdArgs[] = '--persistent';
        $cmdArgs[] = '--config';
        if ($params['state'] == 'running') {
            $cmdArgs[] = '--live';
        }
        $err = gettext("Failed to remove USB.");
        $this->virshCommand($cmdArgs, $err);
    }

    private function createUsbXml($device)
    {
        // create usb xml
        $dev = explode(' ', $device);
        $vendorProduct = explode(':', $dev[5]);
        $vendor = $vendorProduct[0];
        $product = $vendorProduct[1];
        // create xml structure
        $usbXml = new SimpleXMLElement('<xml/>');
        $hostDevXml = $usbXml->devices->addChild('hostdev');
        $hostDevXml->addAttribute('mode', 'subsystem');
        $hostDevXml->addAttribute('type', 'usb');
        $hostDevXml->addAttribute('managed', 'yes');
        $sourceXml = $hostDevXml->addChild('source');
        $vendorXml = $sourceXml->addChild('vendor');
        $vendorXml->addAttribute('id', '0x'.$vendor);
        $productXml = $sourceXml->addChild('product');
        $productXml->addAttribute('id', '0x'.$product);
        // create xml file
        $temp = tempnam(sys_get_temp_dir(), 'virsh_usb');
        $newXml = $usbXml->asXML($temp);
        // return filename
        return ($temp);
    }

    public function addFsPass($params, $context)
    {
    }

    private function createFsXml($src, $tgt)
    {
        // create filesystem passthrough xml
        $xml = new SimpleXMLElement('<xml/>');
        $fsXml = $xml->devices->addChild('filesystem');
        $fsXml->addAttribute('type', 'mount');
        $fsXml->addAttribute('accessmode', 'passthrough');
        $srcXml = $hostDevXml->addChild('source');
        $srcXml->addAttribute('dir', $src);
        $tgtXml = $fsXml->addChild('target');
        $productXml->addAttribute('dir', $tgt);
        // create xml file
        $temp = tempnam(sys_get_temp_dir(), 'virsh_fs');
        $newXml = $xml->asXML($temp);
        // return filename
        return ($temp);
    }

    public function doCommand($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.docommand');
        // create new libvirt object
        $lv = $this->connectQemu();
        $vmname = $params['name'];
        $dom = $lv->get_domain_object($vmname);
        switch ($params['command']) {
            case 'poweron':
                $result = $lv->domain_start($dom);
                break;
            case 'reboot':
                $result = $lv->domain_reboot($dom);
                break;
            case 'reset':
                $cmdArgs = [];
                $cmdArgs[] = 'reset';
                $cmdArgs[] = $vmname;
                $err = gettext("Failed to reset VM.");
                $this->virshCommand($cmdArgs, $err);
                break;
            case 'poweroff':
                $result = $lv->domain_shutdown($dom);
                break;
            case 'force':
                $result = $lv->domain_destroy($dom);
                break;
            case 'pause':
                $result = $lv->domain_suspend($dom);
                break;
            case 'resume':
                $result = $lv->domain_resume($dom);
                break;
            case 'undefine':
                $cmdArgs = [];
                $cmdArgs[] = 'undefine';
                $cmdArgs[] = '--nvram';
                $cmdArgs[] = $vmname;
                $err = gettext("Failed to delete VM.");
                $this->virshCommand($cmdArgs, $err);
                break;
            case 'undefineplus':
                $undefine = false;
                if ($this->deleteDisksByVm($vmname)) {
                    $undefine = true;
                } else {
                    $undefine = false;
                    throw new \OMV\Exception( gettext("Unable to delete disk(s) from VM - ") . $lv->get_last_error());
                }
                if ($undefine) {
                    $cmdArgs = [];
                    $cmdArgs[] = 'undefine';
                    $cmdArgs[] = '--nvram';
                    $cmdArgs[] = $vmname;
                    $err = gettext("Failed to create VM.");
                    $this->virshCommand($cmdArgs, $err);
                }
                break;
            case 'autostartdisable':
                $result = $lv->domain_set_autostart($dom,false);
                break;
            case 'autostartenable':
                $result = $lv->domain_set_autostart($dom,true);
                break;
        }
        if (!$result) {
            if (!$undefine) {
                throw new \OMV\Exception( gettext("Unable to - ") . $params['command'] . $lv->get_last_error());
            }
        }
    }

    public function doWeb($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.doweb');
        $command = $params['command'];
        $hostport = intval($params['hostport']);
        $hostport2 = intval($params['hostport2']);
        // remove existing websockify processes if they exist
        $output = [];
        $cmdLine = sprintf('pgrep -u openmediavault-kvmweb -a websockify | awk \'$6 == %d { print $1 }\' | xargs kill', $hostport);
        $cmd = new \OMV\System\Process($cmdLine);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $cmdLine = sprintf('pgrep -u openmediavault-kvmweb -a websockify | awk \'$6 == %d { print $1 }\' | xargs kill', $hostport2);
        $cmd = new \OMV\System\Process($cmdLine);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        // create websockify process for noVNC
        if ($command == "start" && $hostport >= 1024) {
            $output = [];
            $cmdArgs = [];
            $cmdArgs[] = "sudo ";
            $cmdArgs[] = "--shell";
            $cmdArgs[] = "--non-interactive";
            $cmdArgs[] = "--user=openmediavault-kvmweb";
            $cmdArgs[] = "--";
            $cmdArgs[] = 'websockify';
            $cmdArgs[] = '-D';
            $cmdArgs[] = '--web=/usr/share/novnc/';
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
            $cmdArgs[] = "--user=openmediavault-kvmweb";
            $cmdArgs[] = "--";
            $cmdArgs[] = 'websockify';
            $cmdArgs[] = '-D';
            $cmdArgs[] = '--web=/usr/share/spice-html5/';
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

    public function doChangeCpuMemory($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.dochangecpumemory');
        // create new libvirt object
        $lv = $this->connectQemu();
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
            $cmdLine = sprintf('virsh setmaxmem %s %s --config', $vmname, $newMem);
            $cmdLine2 = sprintf('virsh setmem %s %s --config', $vmname, $newMem);
        } else if ($memory == -1) {
            $cmdLine = sprintf('virsh setvcpus %s %d --config --maximum', $vmname, $vcpu);
            $cmdLine2 = sprintf('virsh setvcpus %s %d --config ', $vmname, $vcpu);
            $domXml = new SimpleXMLElement($lv->domain_get_xml($vmname));
            if (isset($domXml->cpu->topology)) {
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
        $lv = $this->connectQemu();
        $dom = $lv->get_domain_object($params['vmname']);
        $info = $lv->domain_get_info($dom);
        return ["vcpu" => $info['nrVirtCpu']];
    }

    public function getNotes($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        //$this->validateMethodParams($params, 'rpc.kvm.vmname');
        // create new libvirt object
        $lv = $this->connectQemu();
        $notes = "";
        $dom = $lv->get_domain_object($params['vmname']);
        $notes = $lv->domain_get_description($dom);
        return ["notes" => $notes];
    }

    public function setNotes($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.setnotes');
        // create new libvirt object
        $lv = $this->connectQemu();
        $vmname = $params['vmname'];
        $domXml = new SimpleXMLElement($lv->domain_get_xml($vmname));
        $domXml->description = htmlspecialchars($params['notes'], ENT_XML1);
        $newXml = $domXml->asXML();
        $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
        if (!$lv->domain_change_xml($vmname, $newXml)) {
            throw new \OMV\Exception( gettext("Unable to change notes for VM - ") . $lv->get_last_error());
        }
    }

    public function addVnc($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        // create new libvirt object
        $lv = $this->connectQemu();
        $vmname = $params['vmname'];
        $domXml = new SimpleXMLElement($lv->domain_get_xml($vmname));
        // check for existing elements
        $xpath = $domXml->xpath('//graphics');
        $xpathSize = sizeof($xpath);
        for ($i = 0; $i < $xpathSize; $i++) {
            $graphics = $domXml->devices->graphics[$i];
            $type = $graphics['type'];
            if ($type == 'vnc') {
                $vnc = true;
            }
            if ($type == 'spice') {
                $spice = true;
            }
        }
        // add elements if not found
        $change = false;
        if (!$vnc) {
            // add vnc devices
            $hostDevXml = $domXml->devices->addChild('graphics');
            $hostDevXml->addAttribute('type', 'vnc');
            $hostDevXml->addAttribute('port', '-1');
            $hostDevXml->addAttribute('autoport', 'yes');
            $change = true;
        }
        if (!$spice) {
            // add spice devices
            $hostDevXml = $domXml->devices->addChild('graphics');
            $hostDevXml->addAttribute('type', 'spice');
            $hostDevXml->addAttribute('port', '-1');
            $hostDevXml->addAttribute('tlsport', '-1');
            $hostDevXml->addAttribute('autoport', 'yes');
            $hostDevXml->addAttribute('listen', '0.0.0.0');
            $glXml = $hostDevXml->addChild('gl');
            $glXml->addAttribute('enable', 'no');
            $change = true;
        }
        if ($change) {
            $newXml = $domXml->asXML();
            $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
            if (!$lv->domain_change_xml($vmname, $newXml)) {
                throw new \OMV\Exception( gettext("Failed to add vnc and/or spice setup - ") . $lv->get_last_error());
            }
        }
    }

    public function removeVnc($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        // create new libvirt object
        $lv = $this->connectQemu();
        $vmname = $params['vmname'];
        $domXml = new SimpleXMLElement($lv->domain_get_xml($vmname));
        // check for existing elements
        $change = false;
        $xpath = $domXml->xpath('//graphics');
        $xpathSize = sizeof($xpath);
        for ($i = 0; $i < $xpathSize; $i++) {
            $graphics = $domXml->devices->graphics[$i];
            $type = $graphics['type'];
            if ($type == 'vnc') {
                unset($domXml->devices->graphics[$i]);
                $change = true;
            }
        }
        for ($i = 0; $i < $xpathSize; $i++) {
            $graphics = $domXml->devices->graphics[$i];
            $type = $graphics['type'];
            if ($type == 'spice') {
                unset($domXml->devices->graphics[$i]);
                $change = true;
            }
        }
        if ($change) {
            $newXml = $domXml->asXML();
            $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
            if (!$lv->domain_change_xml($vmname, $newXml)) {
                throw new \OMV\Exception( gettext("Failed to remove vnc and/or spice setup - ") . $lv->get_last_error());
            }
        }
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
        $virshXml = implode($output, PHP_EOL);
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
        $cmd = new \OMV\System\Process('osinfo-query', '--fields=short-id,name os');
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

    private function virshCommand($cmdArgs, $msg, $virsh = 'virsh') {
        $output = [];
        $cmd = new \OMV\System\Process($virsh, $cmdArgs);
        $cmd->setQuiet(true);
        $cmd->setRedirect2to1();
        $cmd->execute($output, $exitStatus);
        if (0 != $exitStatus) {
            $outMsg = implode(PHP_EOL, array_filter($output));
            $cmdMsg = implode(' ', array_filter($cmdArgs));
            $errMsg = sprintf('%s%s%s%s%s %s',
                $msg, PHP_EOL, $outMsg, PHP_EOL, $virsh, $cmdMsg
            );
            throw new \OMV\Exception($errMsg);
        }
    }
}
