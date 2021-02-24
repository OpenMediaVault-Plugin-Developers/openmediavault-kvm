<?php
/*
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2021 OpenMediaVault Plugin Developers
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

        $this->registerMethod('getVmList');
        $this->registerMethod('getVmXml');
        $this->registerMethod('setVmXml');
        $this->registerMethod('setVm');
        $this->registerMethod('getDisk');

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
        $this->registerMethod('downloadVirtioIso');

        $this->registerMethod('enumerateSnapshots');
        $this->registerMethod('addSnapshot');
        $this->registerMethod('deleteSnapshot');
        $this->registerMethod('revertSnapshot');

        $this->registerMethod('doCommand');
        $this->registerMethod('doVnc');
        $this->registerMethod('doChangeCpuMemory');
    }

    private function connectQemu()
    {
        $lv = new Libvirt();
        $lv->connect("qemu:///system");
        return ($lv);
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
        $cmdLine = sprintf('virsh net-autostart --network %s%s', $params['name'], $disable);
        $cmd = new \OMV\System\Process($cmdLine);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
    }

    public function addVmNic($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // create new libvirt object
        $lv = $this->connectQemu();
        // get VM object
        $vm = $lv->get_domain_by_name($params['vmname']);
        // add network
        $mac = $params['macaddress'];
        if (!$mac) {
            $mac = $lv->generate_random_mac_addr();
        }
        $model = $params['model'];
        $net = $params['network'];
        if (!$lv->domain_nic_add($vm, $mac, $net, $model)) {
            throw new \OMV\Exception( gettext("Unable to add network adapter to VM - ") . $lv->get_last_error());
        }
    }

    public function removeVmNic($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // create new libvirt object
        $lv = $this->connectQemu();
        $domXml = new SimpleXMLElement($lv->domain_get_xml($params['vmname']));
        $path = $domXml->xpath('//interface');
        $mac = $params['network'];
        for ($i = 0; $i < sizeof($path); $i++) {
            if ($domXml->devices->interface[$i]->mac['address'] == $mac) {
                unset($domXml->devices->interface[$i]);
                $newXml = $domXml->asXML();
                $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
                if (!$lv->domain_change_xml($params['vmname'], $newXml)) {
                    throw new \OMV\Exception( gettext("Unable to remove network interface from VM - ") . $lv->get_last_error());
                }
                break;
            }
        }
    }

    public function enumerateVmNic($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // create new libvirt object
        $lv = $this->connectQemu();
        $xml = $lv->domain_get_xml($params['vmname']);
        $domXml = new SimpleXMLElement($lv->domain_get_xml($params['vmname']));
        $path = $domXml->xpath('//interface');
        for ($i = 0; $i < sizeof($path); $i++) {
            $nic = $domXml->devices->interface[$i];
            $type = $nic['type'];
            $mac = $nic->mac['address'];
            switch ($type) {
                case "network":
                    $net = $nic->source['network'];
                    $desc= sprintf('%s - %s (%s)', $type, $net, $mac);
                    break;
                case "direct":
                    $dev = $nic->source['dev'];
                    $mode = $nic->source['mode'];
                    $desc= sprintf('%s - %s %s (%s)', $type, $dev, $mode, $mac);
                    break;
            }
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
            $info = $lv->get_storagepool_info($pool);
            $objects[] = [
                'name' => $pool,
                'active' => $info['active'] ? 'Active' : 'Inactive',
                'volume_count' => $info['volume_count'],
                'state' => $lv->translate_storagepool_state($info['state']),
                'capacity' => $info['capacity'],
                'allocation' => $info['allocation'],
                'allocationT' => binary_format($info['allocation']),
                'available' => $info['available'],
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
        $poolname = $params['name'];
        $poolpath = $params['path'];
        // create new libvirt object
        $lv = $this->connectQemu();
        $xml = "<pool type='dir'>";
        $xml .= sprintf("<name>%s</name>", $poolname);
        $xml .= "<target>";
        $xml .= sprintf("<path>%s</path>", $poolpath);
        $xml .= "<permissions>";
        $xml .= "</permissions>";
        $xml .= "</target>";
        $xml .= "</pool>";
        if (!$lv->storagepool_define_xml($xml)) {
            throw new \OMV\Exception( gettext("Unable to create pool - ") . $lv->get_last_error());
        }
    }

    public function deletePool($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.path');
        // create new libvirt object
        $lv = $this->connectQemu();
        $res = $lv->get_storagepool_res($params['name']);
        if (!$lv->storagepool_delete($res)) {
            throw new \OMV\Exception( gettext("Unable to delete pool - ") . $lv->get_last_error());
        }
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
            $dom = $lv->get_domain_object($name);
            $info = $lv->domain_get_info($dom);
            $diskcnt = $lv->get_disk_count($dom);
            if ($diskcnt > 0) {
                $dSize = $lv->get_disk_capacity($dom);
                $disks = sprintf('%d / %s', $diskcnt, $dSize);
            } else {
                $disks = 'n/a';
            }
            $vncport = $lv->domain_get_vnc_port($dom);
            if ($vncport == -1) {
                $vncport = "n/a";
            }
            $dockerport = "n/a";
            $dockerurl = "n/a";
            if (file_exists("/usr/bin/docker")) {
                $output = [];
                $containerName = sprintf('novnc_%s', $name);
                $cmdLine = sprintf('docker container port %s', $containerName);
                $cmd = new \OMV\System\Process($cmdLine);
                $cmd->setQuiet(true);
                $cmd->execute($output, $exitStatus);
                $dockerport = array_pop(explode(':', $output[0]));
                if ($dockerport >= 1024) {
                    $dockerurl = sprintf('http://%s:%d/vnc.html', gethostname(), $dockerport);
                } else {
                    $dockerport = 'n/a';
                }
            }
            $snaplist = $lv->domain_snapshots_list($dom);
            $snaps = 0;
            if ($snaplist) {
                $snaps = count($snaplist);
            }
            $objects[] = [
                'vmname' => $name,
                'mem' => $info['memory'] * 1024,
                'cpu' => $info['nrVirtCpu'],
                'state' => $lv->domain_state_translate($info['state']),
                'disks' => $disks,
                'arch' => $lv->domain_get_arch($dom),
                'autostart' => $lv->domain_get_autostart($dom),
                'vncport' => $vncport,
                'dockerport' => $dockerport,
                'dockerurl' => $dockerurl,
                'snaps' => $snaps
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

        $domain_type = "kvm";
        $xml = sprintf("<domain type='%s'>", $domain_type);
        $xml .= sprintf("<name>%s</name>", $params['vmname']);
        $xml .= "<description>created by the openmediavault-kvm plugin</description>";
        $xml .= sprintf("<memory unit='%s'>%d</memory>", $params['memoryunit'], $params['memory']);
        $xml .= sprintf("<vcpu>%d</vcpu>", $params['vcpu']);
        $xml .= "<os>";
        if ($params['uefi']) {
            $xml .= "<type arch='x86_64' machine='q35'>hvm</type>";
            $xml .= "<loader readonly='yes' type='pflash'>/usr/share/OVMF/OVMF_CODE.fd</loader>";
        } else {
            $xml .= "<type machine='pc'>hvm</type>";
        }
        $xml .= "<boot dev='hd'/>";
        $xml .= "<boot dev='cdrom'/>";
        $xml .= "<boot dev='network'/>";
        $xml .= "</os>";
        $xml .= "<features>";
        $xml .= "<acpi/>";
        $xml .= "<apic/>";
        if ($params['os'] == "windows") {
            $xml .= "<hyperv>";
            $xml .= "<relaxed state='on'/>";
            $xml .= "<vapic state='on'/>";
            $xml .= "<spinlocks state='on' retries='8191'/>";
            $xml .= "</hyperv>";
        }
        $xml .= "</features>";
        if ($params['os'] == "windows") {
            $xml .= "<cpu mode='host-model' check='partial'>";
            $xml .= "<model fallback='allow'/>";
            $xml .= "</cpu>";
        }
        $xml .= "<clock offset='localtime'/>";
        $xml .= "<devices>";
        $xml .= "<graphics type='vnc' port='-1' autoport='yes'/>";
        $xml .= "<video>";
        $xml .= "<model type='qxl'/>";
        $xml .= "</video>";
        $xml .= "<memballoon model='virtio'>";
        $xml .= "<stats period='10'/>";
        $xml .= "</memballoon>";
        $xml .= "</devices>";
        $xml .= "</domain>";
        $newvm = $lv->domain_define($xml);
        if (!$newvm) {
            $msg = "Error defining domain: " . $lv->get_last_error();
            throw new \OMV\Exception( gettext("Unable to create domain - ") . $msg);
        }

        $volbus = $params['volbus'];
        $volformat = $params['volformat'];
        $voliso = $params['voliso'];
        $volname = $params['volname'];
        $volpool = $params['volpool'];
        $volsize = $params['volsize'];
        $volunit = $params['volunit'];
        if ($volunit == "") {
            $volunit = "G";
        }

        // create new disk (if specified)
        if ($params['voldisk'] == 'Create new disk') {
            $newdisk = $lv->storagevolume_create($volpool, $volname.'.'.$volformat, $volsize.$volunit, $volsize.$volunit, $volformat);  
            if ($newdisk) {
                $diskpath = libvirt_storagevolume_get_path($newdisk);
            } else {
                throw new \OMV\Exception( gettext("Unable to create new disk - ") . $lv->get_last_error());
            }
        }

        // add disk to VM
        if ($params['uefi']) {
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
                $cd = $cdpre . "a";
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
            $domXml = new SimpleXMLElement($lv->domain_get_xml($params['vmname']));

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

            if (!$lv->domain_change_xml($params['vmname'], $newXml)) {
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

        if (!$lv->domain_nic_add($newvm, $mac, $net, $model)) {
            throw new \OMV\Exception( gettext("Unable to add network adapter to VM - ") . $lv->get_last_error());
        }
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
                'allocationT' => binary_format($vol['allocation']),
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
            $volformat = array_pop(explode('.', $voldisk));
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
        $vm = $lv->get_domain_by_name($params['vmname']);
        $voliso = $params['voliso'];
        // get next device
        $targetdev = "";
        $devs = [];
        $dev = "hd";
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
        $domXml = new SimpleXMLElement($lv->domain_get_xml($params['vmname']));

        $diskXml = $domXml->devices->addChild('disk');
        $diskXml->addAttribute('type', 'file');
        $diskXml->addAttribute('device', 'cdrom');

        $driverXml = $diskXml->addChild('driver');
        $driverXml->addAttribute('name', 'qemu');
        $driverXml->addAttribute('type', 'raw');

        $sourceXml = $diskXml->addChild('source');
        $sourceXml->addAttribute('file', $voliso);

        $targetXml = $diskXml->addChild('target');
        $targetXml->addAttribute('dev', $targetdev);
        $targetXml->addAttribute('bus', 'ide');

        $newXml = $domXml->asXML();
        $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
        if (!$lv->domain_change_xml($params['vmname'], $newXml)) {
            throw new \OMV\Exception( gettext("Unable to add ISO to VM - ") . $lv->get_last_error());
        }
    }

    public function removeDisk($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.removedisk');
        // create new libvirt object
        $lv = $this->connectQemu();
        $domXml = new SimpleXMLElement($lv->domain_get_xml($params['vmname']));
        $dev = $params['disk'];
        $path = $domXml->xpath('//disk');
        for ($i = 0; $i < sizeof($path); $i++) {
            if ($domXml->devices->disk[$i]->target['dev'] == $dev) {
                unset($domXml->devices->disk[$i]);
                $newXml = $domXml->asXML();
                $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
                if (!$lv->domain_change_xml($params['vmname'], $newXml)) {
                    throw new \OMV\Exception( gettext("Unable to remove disk from VM - ") . $lv->get_last_error());
                }
                break;
            }
        }
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

    public function getVolumeList($params, $context)
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
            $info = $lv->get_storagepool_info($pool);
            if ($info['volume_count'] > 0) {
                $volumes = $lv->storagepool_get_volume_information($pool);
                $keys = array_keys($volumes);
                for ($i = 0; $i < sizeof($volumes); $i++) {
                    $volume = $volumes[$keys[$i]];
                    $path = base64_encode($volume['path']);
                    $objects[] = [
                        'name' => $keys[$i],
                        'pool' => $pool,
                        'capacity' => $volume['capacity'],
                        'allocation' => $volume['allocation'],
                        'allocationT' => binary_format($volume['allocation']),
                        'path' => $volume['path']
                    ];
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
                'name' => 'new',
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
            $info = $lv->get_storagepool_info($pool);
            if ($info['volume_count'] > 0) {
                $volumes = $lv->storagepool_get_volume_information($pool);
                $keys = array_keys($volumes);
                for ($i = 0; $i < sizeof($volumes); $i++) {
                    $volume = $volumes[$keys[$i]];
                    $path = base64_encode($volume['path']);
                    $ext = pathinfo($volume['path'], PATHINFO_EXTENSION);
                    $add = false;
                    if (((strtolower($ext) != 'iso') && (!$optical)) || ((strtolower($ext) == 'iso') && ($optical))) {
                        $add = true;
                    } 
                    if ($add) {
                        $objects[] = [
                            'name' => $keys[$i],
                            'capacity' => $volume['capacity'],
                            'path' => $volume['path']
                        ];
                    }
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
        // create new libvirt object
        $lv = $this->connectQemu();
        $pools = $lv->get_storagepools();
        $optical = $params["optical"];
        $objects= [];
        if ($optical) {
            $domXml = new SimpleXMLElement($lv->domain_get_xml($params['vmname']));
            $xpath = $domXml->xpath('//disk');
            for ($i = 0; $i < sizeof($xpath); $i++) {
                $disk = $domXml->devices->disk[$i];
                $devtype = $disk['device'];
                $device = $disk->source['file'];
                $dev = $disk->target['dev'];
                if ($devtype == "cdrom") {
                    $objects[] = [
                        'device' => $dev,
                        'description' => sprintf('%s :: %s', $device, $dev)
                    ];
                }
            }
        } else {
            $disks = $lv->get_disk_stats($params['vmname']);
            for ($i = 0; $i < sizeof($disks); $i++) {
                $disk = $disks[$i];
                $capacity = $lv->format_size($disk['capacity'], 2);
                $dev = (array_key_exists('file', $disk)) ? $disk['file'] : $disk['partition'];
                $device = $disk['device'];
                $ext = array_pop(explode('.', $dev));
                $add = false;
                $objects[] = [
                    'device' => $device,
                    'description' => sprintf('%s :: %s', $dev, $device),
                    'path' => $dev
                ];
            }
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
            throw new \OMV\Exception( gettext("Unable to create volume - ") . $params['path']);
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

    public function downloadVirtioIso($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.kvm.path");
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($params) {
                $isoname = 'virtio-win.iso';
                $cmdArgs = [];
                $cmdArgs[] = '/usr/bin/wget';
                $cmdArgs[] = sprintf('--output-document=%s/%s', $params['path'], $isoname);
                $cmdArgs[] = 'https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/stable-virtio/' . $isoname;
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
                'snapname' => $name,
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

    public function doCommand($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.docommand');
        // create new libvirt object
        $lv = $this->connectQemu();
        if (substr($params['command'], 0, 5) == "pool_") {
            $pool = $params['name'];
            $res = $lv->get_storagepool_res($pool);
        } else {
            $dom = $lv->get_domain_object($params['name']);
        }
        switch ($params['command']) {
            case 'poweron':
                $result = $lv->domain_start($dom);
                break;
            case 'reboot':
                $result = $lv->domain_reboot($dom);
                break;
            case 'poweroff':
                $result = $lv->domain_shutdown($dom);
                break;
            case 'force':
                $result = $lv->domain_destroy($dom);
                break;
            case 'pause':
                $result = $lv->domain_destroy($dom);
                break;
            case 'resume':
                $result = $lv->domain_destroy($dom);
                break;
            case 'undefine':
                $result = $lv->domain_undefine($dom);
                break;
            case 'pool_start':
                $result = $lv->storagepool_create($res);
                break;
            case 'pool_stop':
                $result = $lv->storagepool_destroy($res);
                break;
            case 'autostart_disable':
                $result = $lv->domain_set_autostart($dom,false);
                break;
            case 'autostart_enable':
                $result = $lv->domain_set_autostart($dom,true);
                break;
        }
        if (!$result) {
            throw new \OMV\Exception( gettext("Unable to - ") . $params['command'] . $lv->get_last_error());
        }
    }

    public function doVnc($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.dovnc');
        $containerName = sprintf('novnc_%s', $params['vmname']);
        $hostport = $params['hostport'];
        // remove existing container if exists
        if (file_exists("/usr/bin/docker")) {
            $output = [];
            $cmdLine = sprintf('docker container rm -f %s', $containerName);
            $cmd = new \OMV\System\Process($cmdLine);
            $cmd->setQuiet(true);
            $cmd->execute($output, $exitStatus);
            if ($hostport >= 1024) {
                // create container
                $output = [];
                $cmdLine = sprintf('docker run -d -e REMOTE_HOST=$(hostname) -e REMOTE_PORT=%d -p %d:8081 --name %s dougw/novnc', $params['vncport'], $hostport, $containerName);
                $cmd = new \OMV\System\Process($cmdLine);
                $cmd->setQuiet(true);
                $cmd->execute($output, $exitStatus);
                if ((0 == $exitStatus) && (!empty($output))) {
                    throw new \OMV\Exception(gettext("Failed to start container."));
                }
            }
        } else {
            throw new \OMV\Exception(gettext("Docker is not installed. Please install from omv-extras Docker tab."));
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
        $domXml = new SimpleXMLElement($lv->domain_get_xml($params['vmname']));
        if ($vcpu == -1) {
            switch ($memoryunit) {
                case 'GiB':  $multi = 1024 * 1024; break;
                case 'MiB':  $multi = 1024; break;
            }
            $newMem = $memory * $multi;
            $domXml->memory = $newMem;
            $domXml->currentMemory = $newMem;
            //$result = $lv->domain_change_memory_allocation($params['vmname'], $newMem, $newMem);
        } else if ($memory == -1) {
            $domXml->vcpu = $vcpu;
            //$result = $lv->domain_change_numVCpus($params['vmname'], $vcpu);
        }
        $newXml = $domXml->asXML();
        $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
        if (!$lv->domain_change_xml($params['vmname'], $newXml)) {
            throw new \OMV\Exception( gettext("Unable to change memory or vCPUs on VM - ") . $lv->get_last_error());
        }
    }
}
