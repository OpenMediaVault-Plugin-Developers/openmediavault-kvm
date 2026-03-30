<?php
/*
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2021-2026 openmediavault plugin developers
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
        $this->registerMethod('fixPoolPermissions');

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
        $this->registerMethod('volumeShrink');
        $this->registerMethod('downloadIso');
        $this->registerMethod('addFsPass');
        $this->registerMethod('removeFsPass');
        $this->registerMethod('enumerateFsPassByVm');

        $this->registerMethod('enumerateSnapshots');
        $this->registerMethod('addSnapshot');
        $this->registerMethod('deleteSnapshot');
        $this->registerMethod('deleteAllSnapshots');
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

        $this->registerMethod('doMove');

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

        $this->registerMethod('getMonitorStats');
        $this->registerMethod('clearMonitorDb');
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

    private function formatBytes(int $bytes): string
    {
        if ($bytes >= 1024 ** 4) return round($bytes / 1024 ** 4, 2) . ' TiB';
        if ($bytes >= 1024 ** 3) return round($bytes / 1024 ** 3, 2) . ' GiB';
        if ($bytes >= 1024 ** 2) return round($bytes / 1024 ** 2, 2) . ' MiB';
        if ($bytes >= 1024)      return round($bytes / 1024, 2) . ' KiB';
        return $bytes . ' B';
    }

    private function getExecUrl($context, $db, $fqdn)
    {
        try {
            $settings = $db->get("conf.service.cterm");
            $basepath = $settings->get("basepath");
            $port = $settings->get("port");
            $webadmin = $db->get("conf.webadmin");
            $protocol = boolval($webadmin->get("enablessl")) ? "https" : "http";
            return sprintf('%s://%s:%d%s/terminal', $protocol, $fqdn, $port, $basepath);
        } catch (Exception $e) {
            // cterm not installed
            return "";
        }
    }

    public function getNetworkList($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
        $cmd = new \OMV\System\Process('virsh', ['net-list', '--all', '--name']);
        $cmd->execute($output);
        $nets = array_values(array_filter(array_map('trim', $output)));
        $objects = [];
        foreach ($nets as $net) {
            $info = $this->virshGetNetworkInfo($net);
            $forward = '';
            if (!empty($info['forwarding']) && $info['forwarding'] !== 'None') {
                $forward = !empty($info['forward_dev'])
                    ? $info['forwarding'] . ' to ' . $info['forward_dev']
                    : $info['forwarding'];
            }
            $dhcp = 'Disabled';
            if (!empty($info['dhcp_start']) && !empty($info['dhcp_end'])) {
                $dhcp = $info['dhcp_start'] . ' - ' . $info['dhcp_end'];
            }
            $objects[] = [
                'netname' => $info['name'],
                'active'  => $info['active'] ? 'Active' : 'Inactive',
                'ip'      => $info['ip'],
                'iprange' => $info['ip_range'],
                'forward' => $forward,
                'dhcp'    => $dhcp,
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
        $cmd = new \OMV\System\Process('virsh', ['net-list', '--all', '--name']);
        $cmd->execute($output);
        $objects = [];
        foreach (array_values(array_filter(array_map('trim', $output))) as $net) {
            $objects[] = ['netname' => $net];
        }
        // Filter the result.
        return ($objects);
    }

    public function enumerateBridges($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $objects = [];
        foreach (glob('/sys/class/net/br*', GLOB_ONLYDIR) as $path) {
            $objects[] = ['bridge' => basename($path)];
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
        $this->virshNetDefineXml($xml);
    }

    public function setMacvtap($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.setmacvtap');
        $name = $params['name'];
        $nic = $params['nic'];
        $xml = sprintf('<network><name>%s</name><forward mode="bridge"><interface dev="%s"/></forward></network>', $name, $nic);
        $this->virshNetDefineXml($xml);
    }

    public function networkCommand($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.docommand');
        $disable = "";
        switch ($params['command']) {
            case 'delete':
                $this->virshCommand(['net-undefine', $this->escapeName($params['name'])],
                    gettext("Unable to delete network"));
                return;
            case 'start':
                $this->virshCommand(['net-start', $this->escapeName($params['name'])],
                    gettext("Unable to start network"));
                break;
            case 'stop':
                $disable = " --disable";
                $this->virshCommand(['net-destroy', $this->escapeName($params['name'])],
                    gettext("Unable to stop network"));
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
        $lxc = ($params['virttype'] == 'lxc');
        $vmname = $params['vmname'];
        $mac = $params['macaddress'];
        if (!$mac) {
            $mac = $this->generateMacAddr();
        }
        $model = $params['model'];
        $net = $params['network'];
        if ($model == "bridge") {
            $this->addVmBridgeNic($vmname, $mac, $params['bridge'], $lxc);
        } else {
            $cmdArgs = [];
            if ($lxc) { $cmdArgs[] = '--connect lxc:///'; }
            $cmdArgs[] = 'attach-interface';
            $cmdArgs[] = sprintf('--domain %s', $this->escapeName($vmname));
            $cmdArgs[] = '--type network';
            $cmdArgs[] = sprintf('--source %s', $net);
            $cmdArgs[] = sprintf('--model %s', $model);
            $cmdArgs[] = sprintf('--mac %s', $mac);
            $cmdArgs[] = '--config';
            $this->virshCommand($cmdArgs, gettext("Unable to add network adapter to VM"));
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
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $type = '';
        foreach (array_slice($output, 2) as $line) {
            $parts = preg_split('/\s+/', trim($line), -1, PREG_SPLIT_NO_EMPTY);
            if (isset($parts[4]) && $parts[4] === $mac) {
                $type = $parts[1];
                break;
            }
        }
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
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $output = array_slice($output, 2);
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
        $pools = $this->virshListPools();
        $objects = [];
        foreach ($pools as $pool) {
            $this->virshCommand(['pool-refresh', $this->escapeName($pool)], '', 'virsh', true);
            $info = $this->virshGetPoolInfo($pool);
            $capacity = $info['capacity'];
            if ($capacity <= 0) {
                $capacity = 0;
                $percentage = 0;
            } else {
                $percentage = ($info['allocation'] / $info['capacity']) * 100;
            }
            $objects[] = [
                'name'       => $pool,
                'active'     => $info['active'] ? 'Active' : 'Inactive',
                'volcount'   => $info['volume_count'],
                'state'      => ucfirst($info['state']),
                'capacity'   => $capacity,
                'allocation' => $info['allocation'],
                'available'  => $info['available'],
                'percentage' => $percentage,
                'path'       => $info['path'],
                'type'       => $info['type'],
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
        $pools = $this->virshListPools();
        $objects = [];
        foreach ($pools as $pool) {
            $this->virshCommand(['pool-refresh', $this->escapeName($pool)], '', 'virsh', true);
            $objects[] = [
                'name' => $pool,
                'path' => $this->virshGetPoolPath($pool),
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
        $this->virshPoolDefineXml($xml);
    }

    public function deletePool($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.name');
        $this->virshCommand(['pool-undefine', $this->escapeName($params['name'])],
            gettext("Unable to delete pool"));
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
                $err = gettext("Failed to stop pool.");
                $this->virshCommand($cmdArgs, $err);
                $disable = ' --disable';
                break;
        }
        // enable or disable autostart
        $cmdArgs = [];
        $cmdArgs[] = 'pool-autostart';
        $cmdArgs[] = $this->escapeName($pool);
        if ($disable !== '') $cmdArgs[] = $disable;
        $err = gettext("Failed to set pool autostart.");
        $this->virshCommand($cmdArgs, $err);
    }

    public function fixPoolPermissions($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.fixpoolperms');
        $path = $this->virshGetPoolPath($params['name']);
        if (empty($path)) {
            throw new \OMV\Exception(gettext("Unable to determine pool path."));
        }
        if ($params['fixparents']) {
            // chmod o+x on each ancestor directory so libvirt-qemu can traverse the path
            $parent = dirname(rtrim($path, '/'));
            while ($parent !== '/' && $parent !== '') {
                $cmd = new \OMV\System\Process('chmod', ['o+x', $parent]);
                $cmd->setQuiet(false);
                $cmd->execute($output, $exitStatus);
                if (0 != $exitStatus) {
                    throw new \OMV\Exception(sprintf(
                        gettext("Failed to fix permissions on parent directory: %s"), $parent));
                }
                $parent = dirname($parent);
            }
        } else {
            $cmdArgs = ['libvirt-qemu:kvm', $path];
            if ($params['recursive']) {
                array_unshift($cmdArgs, '-R');
            }
            $cmd = new \OMV\System\Process('chown', $cmdArgs);
            $cmd->setQuiet(false);
            $cmd->execute($output, $exitStatus);
            if (0 != $exitStatus) {
                throw new \OMV\Exception(gettext("Failed to fix pool permissions."));
            }
        }
    }

    private function normalizePort($port)
    {
        if ($port === null || $port === '' || $port === false) {
            return null;
        }
        $p = intval($port);
        if ($p < 1024 || $p > 65535) {
            return null;
        }
        return $p;
    }

    private function fmtPort($port)
    {
        return $port === null ? 'n/a' : $port;
    }

    private function graphicsInfo($domXml, $type)
    {
        // First match only
        $node = $domXml->xpath("//devices/graphics[@type='{$type}'][1]");
        $node = $node[0] ?? null;
        if ($node === null) {
            return [false, null];
        }
        $port = null;
        if (isset($node['port'])) {
            $port = $this->normalizePort($node['port']);
        }
        return [true, $port];
    }

    private function webUrlFor($backendPort, $http, $fqdn, $path)
    {
        if ($backendPort === null) {
            return ['port' => 'n/a', 'url' => 'n/a'];
        }
        $wsInfo = $this->getWebsockifyInfo($backendPort);
        if (!$wsInfo || empty($wsInfo['hostport'])) {
            return ['port' => 'n/a', 'url' => 'n/a'];
        }
        $hostport = $wsInfo['hostport'];
        if ($hostport < 1024 || $hostport > 65535) {
            return ['port' => 'n/a', 'url' => 'n/a'];
        }
        $url = sprintf(
            '<a href="%s://%s:%d/%s" target="_blank">link</a>',
            $http, $fqdn, $hostport, ltrim($path, '/')
        );
        return ['port' => $hostport, 'url' => $url];
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
        // cterm settings (optional)
        $cterm = false;
        $autouser = $context['username'];
        $hmac = '';
        $ts = '';
        $execurl = '';
        try {
            $cterm = $db->get("conf.service.cterm");
            $autouser = $cterm->get('autouser') ?: $context['username'];
            $secret = trim(@file_get_contents('/etc/omv_cterm.secret'));
            $ts = sprintf('%s', microtime(true));
            $userts = sprintf('%s:%s', $autouser, $ts);
            $hmac = hash_hmac('sha256', $userts, $secret);
            $execurl = $this->getExecUrl($context, $db, $fqdn);
        } catch (\Exception $e) {
            $cterm = false;
        }
        // ssl
        $webadmin = $db->get("conf.webadmin");
        $ssl = boolval($webadmin->get("enablessl"));
        $http = $ssl ? 'https' : 'http';
        // VMs
        $objects = [];
        foreach ($this->virshGetDomains(false) as $name) {
            $xml = $this->virshDumpXml($name);
            if (!$xml) {
                continue;
            }
            $domXml = new SimpleXMLElement($xml);
            $info = $this->virshGetDomInfo($name);
            // disk count and capacity from dumpxml + filesize
            $diskNodes = $domXml->xpath('//devices/disk[@device="disk"]');
            $diskcnt = count($diskNodes);
            if ($diskcnt > 0) {
                $totalBytes = 0;
                foreach ($diskNodes as $d) {
                    $f = (string)($d->source['file'] ?? $d->source['dev'] ?? '');
                    if ($f && file_exists($f)) {
                        $totalBytes += filesize($f);
                    }
                }
                $cap = $totalBytes > 0 ? binary_format($totalBytes) : 'n/a';
                $disks = sprintf('%d / %s', $diskcnt, $cap);
            } else {
                $disks = 'n/a';
            }
            // vnc
            [$vncexists, $vncportInt] = $this->graphicsInfo($domXml, 'vnc');
            $vncport = $this->fmtPort($vncportInt);
            $novnc = $this->webUrlFor($vncportInt, $http, $fqdn, 'vnc.html?resize=remote&autoconnect=1');
            $novncport = $novnc['port'];
            $novncurl  = $novnc['url'];
            // spice
            [$spiceexists, $spiceportInt] = $this->graphicsInfo($domXml, 'spice');
            $spiceport = $this->fmtPort($spiceportInt);
            $spicehtml5 = $this->webUrlFor($spiceportInt, $http, $fqdn, 'spice_auto.html?resize=remote');
            $spicehtml5port = $spicehtml5['port'];
            $spicehtml5url  = $spicehtml5['url'];
            // snapshots
            $snaps = count($this->virshGetSnapshotList($name));
            // notes and arch from dumpxml
            $notes = (string)($domXml->xpath('//description')[0] ?? '');
            $archNodes = $domXml->xpath('//os/type/@arch');
            $arch = $archNodes ? (string)$archNodes[0] : '';
            // object to return
            $objects[] = [
                'vmname'       => $name,
                'virttype'     => 'vm',
                'mem'          => $info['memory_kb'] * 1024,
                'cpu'          => $info['ncpus'],
                'state'        => $info['state'],
                'disks'        => $disks,
                'arch'         => $arch,
                'autostart'    => $info['autostart'],
                'vncexists'    => $vncexists,
                'spiceexists'  => $spiceexists,
                'vncport'      => $vncport,
                'spiceport'    => $spiceport,
                'novncport'    => $novncport,
                'novncurl'     => $novncurl,
                'spicehtml5port' => $spicehtml5port,
                'spicehtml5url'  => $spicehtml5url,
                'termurl'      => '-',
                'snaps'        => $snaps,
                'notes'        => str_replace(PHP_EOL, '<br/>', $notes),
            ];
        }
        // lxc
        foreach ($this->virshGetDomains(true) as $name) {
            $xml = $this->virshDumpXml($name, true);
            if (!$xml) {
                continue;
            }
            $domXml = new SimpleXMLElement($xml);
            $info = $this->virshGetDomInfo($name, true);
            $disks = 'n/a';
            // vnc
            [$vncexists, $vncportInt] = $this->graphicsInfo($domXml, 'vnc');
            $vncport = $this->fmtPort($vncportInt);
            $novnc = $this->webUrlFor($vncportInt, $http, $fqdn, 'vnc.html?resize=remote&autoconnect=1');
            $novncport = $novnc['port'];
            $novncurl  = $novnc['url'];
            // spice
            [$spiceexists, $spiceportInt] = $this->graphicsInfo($domXml, 'spice');
            $spiceport = $this->fmtPort($spiceportInt);
            $spicehtml5 = $this->webUrlFor($spiceportInt, $http, $fqdn, 'spice_auto.html?resize=remote');
            $spicehtml5port = $spicehtml5['port'];
            $spicehtml5url  = $spicehtml5['url'];
            // snapshots
            $snaps = count($this->doLxcSnapshot($name, 'list', ''));
            // notes and arch from dumpxml
            $notes = (string)($domXml->xpath('//description')[0] ?? '');
            $archNodes = $domXml->xpath('//os/type/@arch');
            $arch = $archNodes ? (string)$archNodes[0] : '';
            // terminal
            $tstate = $info['state'];
            if (!$cterm || $tstate !== 'running' || ($cterm && (int)$cterm->get("enable") === 0)) {
                $termurl = gettext("disabled");
            } else {
                $termurl = sprintf(
                    '<a href="%s/%s/lxc?user=%s&hmac=%s&ts=%s" target="_blank">exec terminal</a>',
                    $execurl, $name, $autouser, $hmac, $ts
                );
            }
            // object to return
            $objects[] = [
                'vmname'       => $name,
                'virttype'     => 'lxc',
                'mem'          => $info['memory_kb'] * 1024,
                'cpu'          => $info['ncpus'],
                'state'        => $tstate,
                'disks'        => $disks,
                'arch'         => $arch,
                'autostart'    => $info['autostart'],
                'vncexists'    => $vncexists,
                'spiceexists'  => $spiceexists,
                'vncport'      => $vncport,
                'spiceport'    => $spiceport,
                'novncport'    => $novncport,
                'novncurl'     => $novncurl,
                'spicehtml5port' => $spicehtml5port,
                'spicehtml5url'  => $spicehtml5url,
                'termurl'      => $termurl,
                'snaps'        => $snaps,
                'notes'        => str_replace(PHP_EOL, '<br/>', $notes),
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
        $objects= [];
        foreach ($this->virshGetDomains(false) as $name) {
            $info = $this->virshGetDomInfo($name);
            $objects[] = [
                'vmname' => $name,
                'mem' => $info['memory_kb'] * 1024,
                'cpu' => $info['ncpus'],
                'state' => $info['state']
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
        $objects= [];
        foreach ($this->virshGetDomains(true) as $name) {
            $info = $this->virshGetDomInfo($name, true);
            $objects[] = [
                'vmname' => $name,
                'mem' => $info['memory_kb'] * 1024,
                'cpu' => $info['ncpus'],
                'state' => $info['state']
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
        $objects= [];
        foreach ($this->virshGetDomains(false) as $name) {
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
        // backup old xml using virsh dumpxml
        $backup = tempnam(sys_get_temp_dir(), $vmname . '_backup_');
        $cmdArgs = [];
        if ($params['virttype'] == 'lxc') {
            $cmdArgs[] = '--connect lxc:///';
        }
        $cmdArgs[] = 'dumpxml';
        $cmdArgs[] = sprintf('--domain "%s"', $vmname);
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->setRedirect1toFile($backup);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        if ($exitStatus > 0) {
            throw new \OMV\Exception(
              gettext("Unable to backup existing XML for VM :: ") . $vmname
            );
        }
        // create temp xml file
        $xml = tempnam(sys_get_temp_dir(), $vmname . '_new_');
        file_put_contents($xml, $params['vmxml']);
        try {
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
        } catch (\Exception $e) {
            // On any failure, restore the old definition
            $cmdArgs = [];
            if ($params['virttype'] == 'lxc') {
                $cmdArgs[] = '--connect lxc:///';
            }
            $cmdArgs[] = 'define';
            $cmdArgs[] = sprintf('--file "%s"', $backup);
            $err = gettext("Failed to apply new XML, and unable to restore backup :: ") . $vmname;
            $this->virshCommand($cmdArgs, $err);
            throw new \OMV\Exception($e);
        }
        unlink($xml);
        unlink($backup);
    }

    public function getVmDetails($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        $vmname = $this->escapeName($params['vmname']);
        // vm info
        $output = [];
        $cmd = new \OMV\System\Process('virsh', 'dominfo', $vmname);
        $cmd->execute($output, $exitStatus);
        $vminfo = implode(PHP_EOL, array_filter($output));
        // nic info
        $output = [];
        $cmd = new \OMV\System\Process('virsh', 'domiflist', $vmname);
        $cmd->execute($output, $exitStatus);
        $nicinfo = implode(PHP_EOL, array_filter($output));
        // net info
        $netinfo = gettext("VM is not running!");
        if (preg_match("/State:\s+running/", $vminfo)) {
            $output = [];
            $cmd = new \OMV\System\Process('virsh', 'domifaddr', $vmname, '--source agent');
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
        $cmd = new \OMV\System\Process('virsh', 'domblklist', $vmname, '--details');
        $cmd->execute($output, $exitStatus);
        $disklist = implode(PHP_EOL, array_filter($output));
        // disk info
        $output = [];
        $cmd = new \OMV\System\Process('virsh', 'domblkinfo', $vmname, '--all', '--human');
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $diskinfo = ($exitStatus === 0) ? implode(PHP_EOL, array_filter($output)) : '';
        // disk stats
        $output = [];
        $cmd = new \OMV\System\Process('virsh', 'domblkstat', $vmname, '--human');
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $diskstats = implode(PHP_EOL, array_filter($output));
        // dom stats
        $output = [];
        $cmd = new \OMV\System\Process('virsh', 'domstats', $vmname);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $domstats = ($exitStatus === 0) ? implode(PHP_EOL, array_filter($output)) : '';
        // return results
        return [
            "vminfo" => $vminfo,
            "nicinfo" => $nicinfo,
            "netinfo" => $netinfo,
            "disklist" => $disklist,
            "diskinfo" => $diskinfo,
            "diskstats" => $diskstats,
            "domstats" => $domstats
        ];
    }

    public function setVm($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $lxc = $params['lxc'];
        $os = '';
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
            $cpu = explode(' ', trim($params['cpu']));
            $cpuArch = $cpu[0] ?? 'host';
            if ($cpuArch === 'other') {
                $cpuModel = $params['otherCpu'];
            } else {
                $cpuModel = $cpu[1] ?? $cpuArch; // fallback
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
                throw new \OMV\Exception( gettext('Incompatible cpu and architecture!'));
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
        $isWindows = (preg_match('/^win/', $os) === 1);
        if ($isWindows) {
            $vcpuArgs = sprintf('%d,sockets=1,cores=%d,threads=1', $vcpu, $vcpu);
            $clockArgs = 'offset=localtime';
        } else {
            $vcpuArgs = $vcpu;
            $clockArgs = 'offset=utc';
        }

        if (!$lxc) {
            // arch-based arguments
            switch ($arch) {
                case 'aarch64':
                case 'arm':
                    $fd = '/usr/share/AAVMF/AAVMF_CODE.fd';
                    break;
                case 'x86_64':
                    if ($secure) {
                        $fd = '/usr/share/OVMF/OVMF_CODE_4M.ms.fd';
                    } else {
                        $fd = '/usr/share/OVMF/OVMF_CODE_4M.fd';
                    }
                    break;
            }

            // disk arguments
            $volbus = $params['volbus'];
            $voldisk = $params['voldisk'];
            $volformat = $params['volformat'];
            $voliso = $params['voliso'];
            $voliso2 = $params['voliso2'];
            $volname = $params['volname'];
            $volpool = $params['volpool'];
            $volsize = $params['volsize'];
            $volunit = $params['volunit'];

            if ($voldisk == 'Create new disk') {
                if (strlen($volname) < 1) {
                    $volname = $this->generateDiskName($vmname, $volpool, $volformat);
                    if (!$volname) {
                        throw new \OMV\Exception(gettext("Failed to generate a disk name!"));
                    }
                }
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
                    $diskpath = sprintf("'/dev/%s/%s'", $vg, $this->escapeName($volname));
                } else {
                    $poolPath = $this->virshGetPoolPath($volpool);
                    $diskpath = sprintf("'%s/%s.%s'", $poolPath, $volname, $volformat);
                    $cmdArgs = [];
                    $cmdArgs[] = 'create';
                    $cmdArgs[] = sprintf('-f %s', $volformat);
                    $cmdArgs[] = $diskpath;
                    $cmdArgs[] = sprintf('%d%s', $volsize, $volunit);
                    $err = gettext("Failed to create new disk :: ") . $params['command'];
                    $this->virshCommand($cmdArgs, $err, 'qemu-img');
                }
                $voldisk = $diskpath;
            }
            $diskArgs = sprintf("'%s',bus=%s,cache=none,io=native", $voldisk, $volbus);
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
            $brname = $params['bridge'];
            $brmodel = $params['brmodel'];
            $netArgs = sprintf('bridge=%s,model=%s,mac=%s', $brname, $brmodel, $mac);
        } else {
            $netArgs = sprintf('network=%s,model=%s,mac=%s', $net, $model, $mac);
        }

        // build virt-install arguments
        $cmdArgs = [];
        if ($lxc) {
            $cmdArgs[] = '--connect lxc:///';
            $cmdArgs[] = '--container';
        } else {
            if ($params['uefi']) {
                if ($secure) {
                    $cmdArgs[] = sprintf('--boot hd,cdrom,uefi,loader_secure=yes,loader=%s,loader_type=pflash', $fd);
                    $cmdArgs[] = '--features smm=on';
                } else {
                    $cmdArgs[] = sprintf('--boot hd,cdrom,loader=%s,loader_type=pflash', $fd);
                }
            } else {
                $cmdArgs[] = '--boot hd,cdrom';
            }
            $cmdArgs[] = sprintf('--arch %s', $arch);
            $cmdArgs[] = sprintf('--cpu %s,check=none,migratable=off', $cpuModel);
            $cmdArgs[] = sprintf('--disk %s', $diskArgs);
            $cmdArgs[] = sprintf('--os-variant name=%s', $params['os']);
        }
        $cmdArgs[] = sprintf('--memory %s', $memory);
        $cmdArgs[] = sprintf('--metadata description="%s"', htmlspecialchars($params['notes'], ENT_XML1));
        $cmdArgs[] = sprintf('--name %s', $vmname);
        $cmdArgs[] = sprintf('--network %s', $netArgs);
        $cmdArgs[] = sprintf('--vcpus %s', $vcpuArgs);
        $cmdArgs[] = sprintf('--clock %s', $clockArgs);
        $cmdArgs[] = '--memballoon none';

        $printStep = '';
        if ($lxc) {
            $cmdArgs[] = sprintf('--filesystem %s,/', $path);
        } else {
            // Since version 3.1.0 of virt-installer, a sound device is
            // being added unconditionally. Therefore, we just have to
            // use --sound in case we want no sound device at all.
            if (!$params['audio']) {
                $cmdArgs[] = '--sound none';
            }

            // treat Win7+ and Server 2008+ as modern
            $isModernWindows = preg_match('/^win(7|8(\.1)?|10|11|2k8(r2)?|2k(12(r2)?|16|19|22|25))$/', $os) === 1;
            $cdbus = ($isWindows && !$isModernWindows) ? 'ide' : 'sata';

            // drive 1 for OS install
            if (!empty($voliso) && strtolower(trim($voliso)) !== 'none') {
                $cmdArgs[] = sprintf(
                    "--disk path='%s',device=cdrom,bus=%s,readonly=on", $voliso, $cdbus
                );
                $printStep = ' 1';
            }

            // drive 2 for windows only - usually virtio-win.iso
            if (!empty($voliso2) && strtolower(trim($voliso2)) !== 'none') {
                $cmdArgs[] = sprintf(
                    "--disk path='%s',device=cdrom,bus=%s,readonly=on", $voliso2, $cdbus
                );
            }

            // Since version 4.0.0 of virt-installer, a TPM is
            // being created automatically when using UEFI.
            if($params['uefi'] && !$params['tpm']) {
                $cmdArgs[] = '--tpm none';
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

    private function addVmBridgeNic($vmname, $mac, $net, $lxc = false)
    {
        // add a new bridge nic XML
        $domXml = new SimpleXMLElement($this->virshDumpXml($vmname, $lxc));

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

        $this->virshChangeXml($vmname, $newXml, $lxc);
    }

    public function getVmVolumes($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
        $vols = $this->virshGetDiskInfo($params['vmname']);
        $objects = [];
        foreach ($vols as $vol) {
            $cap = $vol['capacity'];
            $alloc = $vol['allocation'];
            $objects[] = [
                'file' => $vol['file'],
                'type' => $vol['type'],
                'device' => $vol['device'],
                'capacity' => $cap,
                'allocation' => $alloc,
                'percentage' => $cap > 0 ? ($alloc / $cap) * 100 : 0,
                'physical' => $vol['physical']
            ];
        }
        // Filter the result.
        return $this->applyFilter($objects, $params['start'], $params['limit'],
            $params['sortfield'], $params['sortdir']);
    }

    private function generateDiskName($vmname, $volpool, $volformat)
    {
        $base = $this->escapeName($vmname) . '_d';
        $poolType = $this->getPoolType($volpool);
        // Precompute path parts
        $isLogical = ($poolType === 'logical');
        if ($isLogical) {
            $vg = $volpool;
            $dir = null;
            $fmt = null;
        } else {
            $dir = rtrim($this->virshGetPoolPath($volpool), '/');
            $vg = null;
            $fmt = ltrim($volformat, '.');
        }
        for ($i = 1; $i < 999; $i++) {
            if ($isLogical) {
                $path = sprintf('/dev/%s/%s%d', $vg, $base, $i);
            } else {
                $path = sprintf('%s/%s%d.%s', $dir, $base, $i, $fmt);
            }
            if (!file_exists($path)) {
                return $base . $i;
            }
        }
        return null;
    }

    public function addDisk($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.adddisk');
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
        $formatArg = '';
        // create new disk (if specified)
        if ($voldisk == "Create new disk") {
            if (strlen($volname) < 1) {
                $volname = $this->generateDiskName($vmname, $volpool, $volformat);
                if (!$volname) {
                    throw new \OMV\Exception(gettext("Failed to generate a disk name!"));
                }
            }
            $volname = str_replace(['.qcow2', '.raw'], '', $volname);
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
                $diskpath = sprintf("'/dev/%s/%s'", $vg, $this->escapeName($volname));
            } else {
                $poolPath = $this->virshGetPoolPath($volpool);
                $diskpath = sprintf("'%s/%s.%s'", $poolPath, $volname, $volformat);
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
        $vmname = $params['vmname'];
        $domXml = new SimpleXMLElement($this->virshDumpXml($vmname));
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
        // enumerate host optical devices via sysfs
        $objects = [];
        foreach (glob('/sys/class/block/sr*', GLOB_ONLYDIR) as $path) {
            $dev = '/dev/' . basename($path);
            if (file_exists($dev)) {
                $objects[] = [
                    'device' => $dev,
                    'description' => sprintf('[host] %s', $dev)
                ];
            }
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
        $listCmd = new \OMV\System\Process('virsh', ['list', '--name']);
        $listCmd->setQuiet(true);
        $listCmd->execute($vmNames, $exitStatus);
        foreach (array_filter(array_map('trim', $vmNames)) as $runningVm) {
            $blkCmd = new \OMV\System\Process('virsh', ['domblklist', '--details', $this->escapeName($runningVm)]);
            $blkCmd->setQuiet(true);
            $blkCmd->execute($blkOutput, $exitStatus);
            foreach (array_slice($blkOutput, 2) as $line) {
                $parts = preg_split('/\s+/', trim($line), -1, PREG_SPLIT_NO_EMPTY);
                if (isset($parts[1]) && $parts[1] === 'disk' && isset($parts[3])) {
                    $disks[] = $parts[3];
                }
            }
        }
        $pools = $this->virshListPools();
        $optical = $params["optical"];
        $objects= [];
        foreach ($pools as $pool) {
            $this->virshCommand(['pool-refresh', $this->escapeName($pool)], '', 'virsh', true);
            $volumes = $this->virshGetVolumeList($pool);
            foreach ($volumes as $name => $volume) {
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
                        'name' => $name,
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
        $pools = $this->virshListPools();
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
            $volumes = $this->virshGetVolumeList($pool);
            foreach ($volumes as $name => $volume) {
                if (is_dir($volume['path'])) continue;
                $ext = strtolower(pathinfo($volume['path'], PATHINFO_EXTENSION));
                if (((($ext != 'iso') && (!$optical)) || (($ext == 'iso') && ($optical)))) {
                    $cap = sprintf('(%s :: %s)',
                        $this->formatBytes($volume['capacity']),
                        $this->formatBytes($volume['allocation'])
                    );
                    $objects[] = [
                        'name' => sprintf('[%s] %s %s', $pool, $name, $cap),
                        'capacity' => $volume['capacity'],
                        'path' => $volume['path']
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
        $cmdArgs = ['domblklist', $this->escapeName($params['vmname']), '--details'];
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output, $exitStatus);
        $objects = [];
        foreach (array_slice($output, 2) as $line) {
            if (empty(trim($line))) continue;
            $parts = preg_split('/\s+/', trim($line), 4, PREG_SPLIT_NO_EMPTY);
            if (count($parts) < 3) continue;
            $dev = $parts[1]; // disk or cdrom
            $tgt = $parts[2]; // target device name (vda, sda, ...)
            $src = trim($parts[3] ?? ''); // source path (may be empty for empty cdroms)
            if ($optical && $dev !== 'cdrom') continue;
            if (!$optical && $dev === 'cdrom') continue;
            if (empty($src)) continue;
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
        $pool = $params['pool'];
        $name = $params['name'];
        $format = $params['format'];
        $image = sprintf('%s.%s', $name, $format);
        $size = sprintf('%d%s', $params['volsize'], $params['volunit']);
        $cmdArgs = [];
        $cmdArgs[] = 'vol-create-as';
        $cmdArgs[] = $this->escapeName($pool);
        $cmdArgs[] = $this->escapeName($image);
        $cmdArgs[] = $size;
        $cmdArgs[] = sprintf('--format %s', $format);
        $cmdArgs[] = sprintf('--prealloc-metadata');
        $msg = sprintf("%s on %s of type %s and size %s", $name, $pool, $format, $size);
        $this->virshCommand($cmdArgs, gettext("Unable to create volume - ") . $msg);
    }

    public function deleteVolume($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.kvm.path");
        $cmdArgs = [];
        $cmdArgs[] = 'vol-delete';
        $cmdArgs[] = sprintf("'%s'", $params['path']);
        $cmdArgs[] = '--by-path';
        $this->virshCommand($cmdArgs, gettext("Unable to delete volume - ") . $params['path']);
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
            $newPath = preg_replace('/\.' . preg_quote($ext, '/') . '$/', '.' . $format, $path);
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

    public function volumeShrink($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.kvm.volumeshrink");
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($params) {
                $cmdArgs = [];
                $cmdArgs[] = '/usr/sbin/omv-shrink-disk';
                $cmdArgs[] = sprintf("'%s'", $params['path']);
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
        // Get current XML (inactive config)
        $xmlStr = $this->virshDumpXml($vmname, $lxc, true);
        if (!$xmlStr) {
            return false;
        }
        $domXml = new \SimpleXMLElement($xmlStr);
        if ($lxc) {
            $xpath = $domXml->xpath('//filesystem');
            $xpathSize = sizeof($xpath);
            for ($i = 0; $i < $xpathSize; $i++) {
                $dst = $domXml->devices->filesystem[$i]->target['dir'];
                if ($dst == "/") {
                    $src = $domXml->devices->filesystem[$i]->source['dir'];
                    if ($src !== "/" && file_exists($src)) {
                        $cmdLine = sprintf('rm -rf %s', escapeshellarg($src));
                        $cmd = new \OMV\System\Process($cmdLine);
                        $cmd->execute($output, $exitStatus);
                        return ($exitStatus === 0);
                    }
                    return (true);
                }
            }
        } else {
            // Make sure VM is shut off – disk removal on running VM is asking for pain.
            if ($this->virshIsDomainActive($vmname)) {
                throw new \OMV\Exception(
                    gettext("VM must be powered off before deleting disks.")
                );
            }
            // Remove disk devices + unlink backing files
            $xpath = $domXml->xpath('//devices/disk');
            foreach ($xpath as $idx => $disk) {
                if ((string)$disk['device'] !== 'disk') {
                    continue;
                }
                $file = (string)$disk->source['file'];
                if ($file !== '' && file_exists($file)) {
                    @unlink($file);
                }
                // Remove this disk element from XML
                unset($domXml->devices->disk[$idx]);
            }
            $newXml = $domXml->asXML();
            $newXml = preg_replace('/<\?xml[^>]+>\s*/', '', $newXml);
            // Redefine domain with updated XML (no undefine)
            $this->virshChangeXml($vmname, $newXml);
            return true;
        }
        return false;
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
                $snap_short = trim(preg_replace('/\s*\(.*\)$/', '', $snap));
                $objects[] = [
                    'snapname' => $snap_short,
                    'description'=> $snap
                ];
            }
        } else {
            $snaps = $this->virshGetSnapshotList($params['vmname']);
            foreach ($snaps as $snap) {
                $objects[] = [
                    'snapname' => $snap['name'],
                    'description' => sprintf("%s :: %s (%s)", $snap['ctime'], $snap['state'], $snap['name'])
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
            $this->virshCommand(
                ['snapshot-create-as', $this->escapeName($params['vmname'])],
                gettext("Unable to create snapshot")
            );
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
            $cmdArgs = [];
            $cmdArgs[] = 'snapshot-delete';
            $cmdArgs[] = $this->escapeName($params['vmname']);
            $cmdArgs[] = $this->escapeName($params['snapname']);
            $cmdArgs[] = '--metadata';
            $err = gettext("Failed to delete the snapshot!");
            $this->virshCommand($cmdArgs, $err);
        }
    }

    public function deleteAllSnapshots($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        if ($params['virttype'] == 'lxc') {
            $snaps = $this->doLxcSnapshot($params['vmname'], 'list', '');
            foreach (array_reverse($snaps) as $snap) {
                $this->doLxcSnapshot($params['vmname'], 'delete', $snap);
            }
        } else {
            $snaps = $this->virshGetSnapshotList($params['vmname']);
            // delete in reverse order so children are removed before parents
            foreach (array_reverse($snaps) as $snap) {
                $this->virshCommand(
                    ['snapshot-delete', $this->escapeName($params['vmname']), $this->escapeName($snap['name']), '--metadata'],
                    gettext("Failed to delete snapshot: ") . $snap['name'],
                    'virsh',
                    true
                );
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
            $this->virshCommand(
                ['snapshot-revert', $this->escapeName($params['vmname']), $this->escapeName($params['snapname'])],
                gettext("Unable to revert to snapshot")
            );
        }
    }

    public function enumerateUsbByHost($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $cmd = new \OMV\System\Process('lsusb', ['-v']);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $objects = [];
        $busLine = '';
        foreach ($output as $line) {
            if (preg_match('/^Bus\s/', $line)) {
                $busLine = $line;
            } elseif (!empty($busLine) && preg_match('/iSerial\s+\d+\s+(\S+)/', $line, $m)) {
                $desc = $busLine . ' Serial: ' . $m[1];
                $objects[] = ['description' => $desc, 'device' => $desc];
                $busLine = '';
            }
        }
        return ($objects);
    }

    public function enumerateUsbByVm($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.vmname');
        $domXml = new SimpleXMLElement($this->virshDumpXml($params['vmname']));
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
        $domXml = new SimpleXMLElement($this->virshDumpXml($params['vmname']));
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
        $lxc = ($params['virttype'] == 'lxc');
        $domXml = new SimpleXMLElement($this->virshDumpXml($params['vmname'], $lxc));
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
        $lxc = ($params['virttype'] === 'lxc');
        $vmname = $params['name'];
        $conn = $lxc ? ['--connect lxc:///'] : [];
        // check for nvram
        $domXml = new SimpleXMLElement($this->virshDumpXml($vmname, $lxc));
        $hasNvram = isset($domXml->os->nvram) && file_exists((string) $domXml->os->nvram);
        switch ($params['command']) {
            case 'poweron':
                $this->virshCommand(array_merge($conn, ['start', $this->escapeName($vmname)]),
                    gettext("Failed to power on VM."));
                break;
            case 'reboot':
                $this->virshCommand(array_merge($conn, ['reboot', $this->escapeName($vmname)]),
                    gettext("Failed to reboot VM."));
                break;
            case 'reset':
                $this->virshCommand(array_merge($conn, ['reset', $this->escapeName($vmname)]),
                    gettext("Failed to reset VM."));
                break;
            case 'poweroff':
                $this->shutdownWeb($params['vncport'], $params['spiceport'], $params['hostport'], $params['hostport2']);
                $this->virshCommand(array_merge($conn, ['shutdown', $this->escapeName($vmname)]),
                    gettext("Failed to power off VM."));
                break;
            case 'force':
                $this->shutdownWeb($params['vncport'], $params['spiceport'], $params['hostport'], $params['hostport2']);
                $this->virshCommand(array_merge($conn, ['destroy', $this->escapeName($vmname)]),
                    gettext("Failed to force stop VM."));
                break;
            case 'pause':
                $this->virshCommand(array_merge($conn, ['suspend', $this->escapeName($vmname)]),
                    gettext("Failed to pause VM."));
                break;
            case 'resume':
                $this->virshCommand(array_merge($conn, ['resume', $this->escapeName($vmname)]),
                    gettext("Failed to resume VM."));
                break;
            case 'undefine': {
                $this->shutdownWeb($params['vncport'], $params['spiceport'], $params['hostport'], $params['hostport2']);
                $cmdArgs = array_merge($conn, ['undefine']);
                if (!$lxc && $hasNvram) {
                    $cmdArgs[] = '--nvram';
                }
                $cmdArgs[] = $this->escapeName($vmname);
                $this->virshCommand($cmdArgs, gettext("Failed to delete VM."));
                break;
            }
            case 'undefineplus': {
                $this->shutdownWeb($params['vncport'], $params['spiceport'], $params['hostport'], $params['hostport2']);
                if (!$this->deleteDisksByVm($vmname, $lxc)) {
                    throw new \OMV\Exception(gettext("Unable to delete disk(s) from VM."));
                }
                $cmdArgs = array_merge($conn, ['undefine']);
                if (!$lxc && $hasNvram) {
                    $cmdArgs[] = '--nvram';
                }
                $cmdArgs[] = $this->escapeName($vmname);
                $this->virshCommand($cmdArgs, gettext("Failed to delete VM."));
                break;
            }
            case 'autostartdisable':
                $this->virshCommand(array_merge($conn, ['autostart', '--disable', $this->escapeName($vmname)]),
                    gettext("Failed to disable autostart."));
                break;
            case 'autostartenable':
                $this->virshCommand(array_merge($conn, ['autostart', $this->escapeName($vmname)]),
                    gettext("Failed to enable autostart."));
                break;
        }
    }

    private function findFreePort($startPort)
    {
        for ($port = $startPort; $port <= 65535; $port++) {
            $errno = 0;
            $errstr = '';
            $sock = @stream_socket_server(
                sprintf('tcp://0.0.0.0:%d', $port),
                $errno,
                $errstr,
                STREAM_SERVER_BIND | STREAM_SERVER_LISTEN
            );
            if ($sock !== false) {
                fclose($sock);
                return $port;
            }
        }
        return -1;
    }

    public function doWeb($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $command = $params['command'];
        $hostport = intval($params['hostport']);
        if ($hostport < 1024) {
            $hostport = $this->findFreePort(8081);
            if ($hostport == -1) {
                throw new \OMV\Exception("Failed to find an open port");
            }
        }
        $hostport2 = intval($params['hostport2']);
        if ($hostport2 < 1024) {
            $hostport2 = $this->findFreePort(8091);
            if ($hostport2 == -1) {
                throw new \OMV\Exception("Failed to find an open port");
            }
        }
        // shutdown existing consoles if running
        $this->shutdownWeb($params['vncport'], $params['spiceport'], $hostport, $hostport2);
        if ($command != "start") {
            return;
        }
        // Get configuration data.
        $db = \OMV\Config\Database::getInstance();
        $webadmin = $db->get("conf.webadmin");
        $ssl = boolval($webadmin->get("enablessl"));
        $forcessl = boolval($webadmin->get("forcesslonly"));
        $crt = '';
        $key = '';
        if ($ssl) {
            $certref = $webadmin->get("sslcertificateref");
            $crt = sprintf("/etc/ssl/certs/openmediavault-%s.crt", $certref);
            $key = sprintf("/etc/ssl/private/openmediavault-%s.key", $certref);
        }
        // create websockify process for noVNC
        $this->startWebsockify($hostport, $params['vncport'],
            '/usr/share/novnc/', $ssl, $forcessl, $crt, $key, 'noVNC');
        // create websockify process for spice html5
        $this->startWebsockify($hostport2, $params['spiceport'],
            '/usr/share/spice-html5/', $ssl, $forcessl, $crt, $key, 'spice-html5');
    }

    private function startWebsockify($hostport, $vmport, $webRoot, $ssl, $forcessl, $crt, $key, $label)
    {
        if ($hostport < 1024) {
            return;
        }
        $vmport = intval($vmport);
        $runDir = '/run/kvmweb';
        if (!is_dir($runDir)) {
            mkdir($runDir, 0755, true);
            chown($runDir, 'openmediavault-kvmweb');
        }
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
        $cmdArgs[] = sprintf('--web %s', $webRoot);
        $cmdArgs[] = $hostport;
        $cmdArgs[] = sprintf('localhost:%d', $vmport);
        $cmd = new \OMV\System\Process($cmdArgs);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        if (0 !== $exitStatus) {
            throw new \OMV\Exception(
                gettext("Failed to start websockify for ") . $label
            );
        }
        // Find the PID and write tracking file for this VM port
        $pidOutput = [];
        $pidCmd = new \OMV\System\Process(
            sprintf('pgrep -u openmediavault-kvmweb,root -f "websockify.*localhost:%d"', $vmport)
        );
        $pidCmd->setQuiet(true);
        $pidCmd->execute($pidOutput, $pidExit);
        $pid = intval($pidOutput[0] ?? 0);
        if ($pid > 0) {
            file_put_contents(
                sprintf('%s/ws-%d', $runDir, $vmport),
                sprintf("%d\n%d", $pid, $hostport)
            );
        }
    }

    private function getWebsockifyInfo($vmport)
    {
        $trackFile = sprintf('/run/kvmweb/ws-%d', $vmport);
        if (!file_exists($trackFile)) {
            return null;
        }
        $lines = file($trackFile, FILE_IGNORE_NEW_LINES);
        $pid = intval($lines[0] ?? 0);
        $hostport = intval($lines[1] ?? 0);
        if ($pid > 0 && file_exists(sprintf('/proc/%d', $pid)) && $hostport >= 1024) {
            return ['pid' => $pid, 'hostport' => $hostport];
        }
        return null;
    }

    private function shutdownWeb($vncport, $spiceport, $vnchostport, $spicehostport)
    {
        // kill by VM port (the port websockify bridges to)
        $this->killWebsockifyByPort($vncport);
        $this->killWebsockifyByPort($spiceport);
        // kill by host port (the port websockify listens on)
        $this->killWebsockifyByHostPort($vnchostport);
        $this->killWebsockifyByHostPort($spicehostport);
    }

    private function killWebsockifyByPort($vmport)
    {
        if (intval($vmport) < 1024) {
            return;
        }
        // Try tracking file first
        $trackFile = sprintf('/run/kvmweb/ws-%d', $vmport);
        if (file_exists($trackFile)) {
            $lines = file($trackFile, FILE_IGNORE_NEW_LINES);
            $pid = intval($lines[0] ?? 0);
            if ($pid > 0) {
                $cmd = new \OMV\System\Process(sprintf('kill %d 2>/dev/null', $pid));
                $cmd->setQuiet(true);
                $cmd->execute($output, $exitStatus);
            }
            @unlink($trackFile);
        }
        // Fallback: kill by pattern match (handles processes without tracking files)
        $output = [];
        $cmdLine = sprintf(
            'pgrep -u openmediavault-kvmweb,root -f "websockify.*localhost:%d" | xargs -r kill 2>/dev/null',
            intval($vmport)
        );
        $cmd = new \OMV\System\Process($cmdLine);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
    }

    private function killWebsockifyByHostPort($hostport)
    {
        if (intval($hostport) < 1024) {
            return;
        }
        // Scan tracking files for matching host port
        $runDir = '/run/kvmweb';
        if (is_dir($runDir)) {
            foreach (glob(sprintf('%s/ws-*', $runDir)) as $trackFile) {
                $lines = file($trackFile, FILE_IGNORE_NEW_LINES);
                $storedHostport = intval($lines[1] ?? 0);
                if ($storedHostport === intval($hostport)) {
                    $pid = intval($lines[0] ?? 0);
                    if ($pid > 0) {
                        $cmd = new \OMV\System\Process(sprintf('kill %d 2>/dev/null', $pid));
                        $cmd->setQuiet(true);
                        $cmd->execute($output, $exitStatus);
                    }
                    @unlink($trackFile);
                }
            }
        }
        // Fallback: kill by pattern match (handles processes without tracking files)
        $output = [];
        $cmdLine = sprintf(
            'pgrep -u openmediavault-kvmweb,root -f "websockify.* %d localhost:" | xargs -r kill 2>/dev/null',
            intval($hostport)
        );
        $cmd = new \OMV\System\Process($cmdLine);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
    }

    public function doChangeCpuMemory($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.dochangecpumemory');
        $lxc = ($params['virttype'] == 'lxc');
        $conn = $lxc ? ['--connect', 'lxc:///'] : [];
        $memory = $params['memory'];
        $memoryunit = $params['memoryunit'];
        $vcpu = $params['vcpu'];
        $vmname = $params['vmname'];
        $vmname_esc = $this->escapeName($vmname);
        $skip = false;
        if ($vcpu == -1) {
            switch ($memoryunit) {
                case 'GiB':  $multi = 1024 * 1024; break;
                case 'MiB':  $multi = 1024; break;
            }
            $newMem = $memory * $multi;
            $cmdLine = array_merge($conn, ['setmaxmem', $vmname_esc, $newMem, '--config']);
            $cmdLine2 = array_merge($conn, ['setmem', $vmname_esc, $newMem, '--config']);
        } else if ($memory == -1) {
            $cmdLine = array_merge($conn, ['setvcpus', $vmname_esc, $vcpu, '--config', '--maximum']);
            $cmdLine2 = array_merge($conn, ['setvcpus', $vmname_esc, $vcpu, '--config']);
            $domXml = new SimpleXMLElement($this->virshDumpXml($vmname, $lxc));
            if (isset($domXml->cpu->topology)) {
                $domXml->cpu->topology['dies'] = 1;
                $domXml->cpu->topology['sockets'] = 1;
                $domXml->cpu->topology['threads'] = 1;
                $domXml->cpu->topology['cores'] = $vcpu;
                $domXml->vcpu = $vcpu;
                $newXml = $domXml->asXML();
                $newXml = str_replace('<?xml version="1.0"?>', '', $newXml);
                $this->virshChangeXml($vmname, $newXml, $lxc);
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
        $lxc = ($params['virttype'] == 'lxc');
        $info = $this->virshGetDomInfo($params['vmname'], $lxc);
        return ["vcpu" => $info['ncpus']];
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
        $cmdArgs[] = '--new-desc';
        $cmdArgs[] = escapeshellarg($params['notes']);
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

    public function doMove($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.kvm.domove');
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($params) {
                $cmdArgs = [];
                $cmdArgs[] = '/usr/sbin/omv-move-vm';
                $cmdArgs[] = sprintf("-v '%s'", $params['vmname']);
                $cmdArgs[] = sprintf("-d '%s'", $params['pool']);
                if ($params['poweroff']) {
                    $cmdArgs[] = '--force-off';
                }
                $cmd = new \OMV\System\Process($cmdArgs);
                $cmd->setRedirect2to1();
                $cmdLine = $cmd->getCommandLine();
                file_put_contents($bgOutputFilename, $cmdLine . PHP_EOL . PHP_EOL, FILE_APPEND);
                $cmd->setQuiet(true);
                $this->exec($cmdLine, $output, $bgOutputFilename);
                return $output;
            }
        );
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
                $restorefrom = trim($backup[2]);
                $newpath = trim($params['newpath']);
                $vmname = trim($backup[0]);
                $cmdArgs = [];
                $cmdArgs[] = '/usr/sbin/omv-restore-vm';
                $cmdArgs[] = sprintf("-d '%s'", $restorefrom);
                $cmdArgs[] = sprintf('-r %s', $params['newname']);
                $cmdArgs[] = sprintf('-t %s', $date);
                $cmdArgs[] = sprintf('-v %s', $vmname);
                if (strlen($newpath) > 1) {
                    $cmdArgs[] = sprintf("-l '%s'", $newpath);
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
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        // Get the configuration object.
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get("conf.service.kvm");
        $dbPath = $object->get('monitor_dbpath');
        // Remove useless properties from the object.
        $object->remove("jobs");
        // get db size
        if (file_exists($dbPath)) {
            $dbSize = number_format(filesize($dbPath) / (1024 * 1024), 2) . ' MiB';
        } else {
            $dbSize = 'Not found';
        }
        $object->add('monitor_db_size', 'string', $dbSize);
        // Return the configuration object.
        return $object->getAssoc();
    }

    public function setSettings($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, "rpc.kvm.setsettings");
        // Get the existing configuration object.
        $db = \OMV\Config\Database::getInstance();
        $object = $db->get("conf.service.kvm");
        $object->setAssoc($params);
        $db->set($object);
        // Remove useless properties from the object.
        $object->remove("jobs");
        // Return the configuration object.
        return $object->getAssoc();
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

    private function virshDumpXml(string $vmname, bool $lxc = false, bool $inactive = false): string
    {
        $cmdArgs = [];
        if ($lxc) $cmdArgs[] = '--connect lxc:///';
        $cmdArgs[] = 'dumpxml';
        if ($inactive) $cmdArgs[] = '--inactive';
        $cmdArgs[] = $this->escapeName($vmname);
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        if ($exitStatus !== 0 || empty($output)) return '';
        return implode(PHP_EOL, $output);
    }

    private function virshGetDomains(bool $lxc = false): array
    {
        $cmdArgs = [];
        if ($lxc) $cmdArgs[] = '--connect lxc:///';
        $cmdArgs[] = 'list --all --name';
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->execute($output);
        return array_values(array_filter(array_map('trim', $output)));
    }

    private function virshNormalizeState(string $state): string
    {
        $map = [
            'shut off'    => 'shutoff',
            'in shutdown' => 'shutdown',
            'idle'        => 'blocked',
        ];
        return $map[$state] ?? $state;
    }

    private function virshGetDomInfo(string $vmname, bool $lxc = false): array
    {
        $cmdArgs = [];
        if ($lxc) $cmdArgs[] = '--connect lxc:///';
        $cmdArgs[] = 'dominfo';
        $cmdArgs[] = $this->escapeName($vmname);
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        $info = ['state' => 'unknown', 'memory_kb' => 0, 'ncpus' => 0, 'autostart' => false];
        foreach ($output as $line) {
            $line = trim($line);
            if (preg_match('/^State:\s+(.+)$/i', $line, $m)) {
                $info['state'] = $this->virshNormalizeState(trim($m[1]));
            } elseif (preg_match('/^CPU\(s\):\s+(\d+)$/i', $line, $m)) {
                $info['ncpus'] = (int)$m[1];
            } elseif (preg_match('/^Max memory:\s+(\d+)\s+KiB/i', $line, $m)) {
                $info['memory_kb'] = (int)$m[1];
            } elseif (preg_match('/^Autostart:\s+(\w+)$/i', $line, $m)) {
                $info['autostart'] = (strtolower(trim($m[1])) === 'enable');
            }
        }
        return $info;
    }

    private function virshIsDomainActive(string $vmname, bool $lxc = false): bool
    {
        $cmdArgs = [];
        if ($lxc) $cmdArgs[] = '--connect lxc:///';
        $cmdArgs[] = 'domstate';
        $cmdArgs[] = $this->escapeName($vmname);
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        if ($exitStatus !== 0 || empty($output)) return false;
        $state = strtolower(trim($output[0]));
        return !in_array($state, ['shut off', 'shutoff', 'crashed']);
    }

    private function virshListPools(): array
    {
        $cmd = new \OMV\System\Process('virsh', ['pool-list', '--all', '--name']);
        $cmd->execute($output);
        return array_values(array_filter(array_map('trim', $output)));
    }

    private function virshGetPoolInfo(string $pool): array
    {
        $info = ['capacity' => 0, 'allocation' => 0, 'available' => 0, 'path' => '',
                 'active' => false, 'state' => 'unknown', 'volume_count' => 0, 'type' => ''];
        // bytes + path from pool-dumpxml
        $cmd = new \OMV\System\Process('virsh', ['pool-dumpxml', $this->escapeName($pool)]);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        if ($exitStatus === 0 && !empty($output)) {
            try {
                $xml = new SimpleXMLElement(implode(PHP_EOL, $output));
                $info['capacity']   = (int)(string)$xml->capacity;
                $info['allocation'] = (int)(string)$xml->allocation;
                $info['available']  = (int)(string)$xml->available;
                $info['path']       = (string)($xml->target->path ?? '');
                $info['type']       = (string)($xml->attributes()['type'] ?? '');
            } catch (\Exception $e) {}
        }
        // state/active from pool-info
        $cmd2 = new \OMV\System\Process('virsh', ['pool-info', $this->escapeName($pool)]);
        $cmd2->setQuiet(true);
        $cmd2->execute($output2, $exitStatus2);
        foreach (($output2 ?? []) as $line) {
            if (preg_match('/^State:\s+(.+)$/i', trim($line), $m)) {
                $info['state']  = trim($m[1]);
                $info['active'] = (strtolower(trim($m[1])) === 'running');
            }
        }
        // volume count from vol-list
        $cmd3 = new \OMV\System\Process('virsh',
            ['vol-list', $this->escapeName($pool), '| tail -n +3', '| awk NF', '| wc -l']);
        $cmd3->setQuiet(true);
        $cmd3->execute($output3, $exitStatus3);
        if ($exitStatus3 === 0 && !empty($output3)) {
            $info['volume_count'] = (int)trim($output3[0]);
        }
        return $info;
    }

    private function virshGetPoolPath(string $pool): string
    {
        $info = $this->virshGetPoolInfo($pool);
        return $info['path'];
    }

    private function virshGetNetworkInfo(string $net): array
    {
        $info = ['name' => $net, 'active' => false, 'ip' => '', 'ip_range' => '',
                 'forwarding' => '', 'forward_dev' => '', 'dhcp_start' => '', 'dhcp_end' => ''];
        // active status from net-info
        $cmd = new \OMV\System\Process('virsh', ['net-info', $this->escapeName($net)]);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        foreach (($output ?? []) as $line) {
            if (preg_match('/^Active:\s+(yes|no)/i', trim($line), $m)) {
                $info['active'] = (strtolower($m[1]) === 'yes');
            }
        }
        // details from net-dumpxml
        $cmd2 = new \OMV\System\Process('virsh', ['net-dumpxml', $this->escapeName($net)]);
        $cmd2->setQuiet(true);
        $cmd2->execute($output2, $exitStatus2);
        if ($exitStatus2 !== 0 || empty($output2)) return $info;
        try {
            $xml = new SimpleXMLElement(implode(PHP_EOL, $output2));
            if (isset($xml->forward)) {
                $info['forwarding']  = (string)($xml->forward['mode'] ?? 'None');
                $info['forward_dev'] = (string)($xml->forward->interface['dev'] ?? '');
            }
            if (isset($xml->ip)) {
                $addr = (string)($xml->ip['address'] ?? '');
                $mask = (string)($xml->ip['netmask'] ?? '');
                $info['ip']       = $addr;
                $info['ip_range'] = $mask ? sprintf('%s/%s', $addr, $mask) : $addr;
                if (isset($xml->ip->dhcp->range)) {
                    $info['dhcp_start'] = (string)($xml->ip->dhcp->range['start'] ?? '');
                    $info['dhcp_end']   = (string)($xml->ip->dhcp->range['end'] ?? '');
                }
            }
        } catch (\Exception $e) {}
        return $info;
    }

    private function virshGetSnapshotList(string $vmname): array
    {
        $cmd = new \OMV\System\Process('virsh', ['snapshot-list', $this->escapeName($vmname)]);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        if ($exitStatus !== 0) return [];
        $snaps = [];
        foreach (array_filter(array_map('trim', $output)) as $line) {
            if (str_starts_with($line, 'Name') || str_starts_with($line, '---')) continue;
            // columns: Name  Creation Time  State (separated by 2+ spaces)
            $parts = preg_split('/\s{2,}/', $line, 3);
            if (count($parts) < 3) continue;
            $name  = trim($parts[0]);
            $ts    = strtotime(trim($parts[1]));
            $ctime = $ts ? date("Y.m.d H:i:s", $ts) : '';
            $state = trim($parts[2]);
            $snaps[] = ['name' => $name, 'ctime' => $ctime, 'state' => $state];
        }
        return $snaps;
    }

    private function virshGetVolumeList(string $pool): array
    {
        // vol-list --details returns Name, Path, Type, Capacity, Allocation in one shot,
        // avoiding a separate vol-dumpxml call per volume.
        $cmdArgs = ['vol-list', '--details', $this->escapeName($pool)];
        $cmd = new \OMV\System\Process('virsh', $cmdArgs);
        $cmd->setQuiet(true);
        $cmd->execute($output, $exitStatus);
        if ($exitStatus !== 0) return [];
        $volumes = [];
        foreach (array_filter(array_slice($output, 2), 'strlen') as $line) {
            $parts = preg_split('/\s{2,}/', trim($line), -1, PREG_SPLIT_NO_EMPTY);
            if (count($parts) < 5) continue;
            $name     = trim($parts[0]);
            $path     = trim($parts[1]);
            $type     = trim($parts[2]);
            $capacity = $this->parseVirshSize($parts[3]);
            $alloc    = $this->parseVirshSize($parts[4]);
            if (empty($name) || empty($path)) continue;
            // fall back to filesize for allocation if virsh reported zero;
            // filesize() returns actual on-disk usage, not virtual capacity
            if ($alloc <= 0 && file_exists($path)) {
                $alloc = filesize($path);
            }
            $volumes[$name] = [
                'path'       => $path,
                'capacity'   => $capacity,
                'allocation' => $alloc,
                'physical'   => $alloc,
                'type'       => $type,
            ];
        }
        return $volumes;
    }

    private function parseVirshSize(string $size): int
    {
        $size = trim($size);
        if (!preg_match('/^([\d.]+)\s*(\w+)$/', $size, $m)) return 0;
        $multipliers = [
            'B'   => 1,
            'KIB' => 1024,
            'MIB' => 1024 ** 2,
            'GIB' => 1024 ** 3,
            'TIB' => 1024 ** 4,
            'KB'  => 1000,
            'MB'  => 1000 ** 2,
            'GB'  => 1000 ** 3,
            'TB'  => 1000 ** 4,
        ];
        return (int)((float)$m[1] * ($multipliers[strtoupper($m[2])] ?? 1));
    }

    private function virshGetDiskInfo(string $vmname): array
    {
        $xmlStr = $this->virshDumpXml($vmname);
        if (!$xmlStr) return [];
        $domXml = new SimpleXMLElement($xmlStr);
        $diskNodes = $domXml->xpath('//devices/disk[@device="disk"]');
        $disks = [];
        foreach ($diskNodes as $disk) {
            $file   = (string)($disk->source['file'] ?? $disk->source['dev'] ?? '');
            $target = (string)($disk->target['dev'] ?? '');
            $type   = (string)($disk->driver['type'] ?? '');
            $capacity   = 0;
            $allocation = 0;
            // Use virsh domblkinfo for accurate sizes (in bytes)
            if ($target) {
                $blkCmd = new \OMV\System\Process('virsh',
                    ['domblkinfo', $this->escapeName($vmname), $target]);
                $blkCmd->setQuiet(true);
                $blkCmd->execute($blkOutput, $blkExit);
                if ($blkExit === 0) {
                    foreach ($blkOutput as $line) {
                        if (preg_match('/^Capacity:\s+(\d+)/i', trim($line), $m)) {
                            $capacity = (int)$m[1];
                        } elseif (preg_match('/^Allocation:\s+(\d+)/i', trim($line), $m)) {
                            $allocation = (int)$m[1];
                        }
                    }
                }
            }
            $disks[] = [
                'file'       => $file,
                'type'       => $type,
                'device'     => $target,
                'capacity'   => $capacity,
                'allocation' => $allocation,
                'physical'   => $allocation,
            ];
        }
        return $disks;
    }

    private function virshNetDefineXml(string $xmlStr): void
    {
        $tmp = tempnam(sys_get_temp_dir(), 'virsh_net_');
        file_put_contents($tmp, $xmlStr);
        $this->virshCommand(['net-define', $tmp],
            gettext("Unable to define network"));
        unlink($tmp);
    }

    private function virshPoolDefineXml(string $xmlStr): void
    {
        $tmp = tempnam(sys_get_temp_dir(), 'virsh_pool_');
        file_put_contents($tmp, $xmlStr);
        $this->virshCommand(['pool-define', $tmp],
            gettext("Unable to define storage pool"));
        unlink($tmp);
    }

    private function virshChangeXml(string $vmname, string $xmlStr, bool $lxc = false): void
    {
        $tmp = tempnam(sys_get_temp_dir(), 'virsh_define_');
        file_put_contents($tmp, $xmlStr);
        $cmdArgs = [];
        if ($lxc) $cmdArgs[] = '--connect lxc:///';
        $cmdArgs[] = 'define';
        $cmdArgs[] = sprintf('--file "%s"', $tmp);
        $this->virshCommand($cmdArgs, gettext("Unable to update domain XML"));
        unlink($tmp);
    }

    private function generateMacAddr(): string
    {
        $mac = [0x52, 0x54, 0x00,
            rand(0x00, 0x7f),
            rand(0x00, 0xff),
            rand(0x00, 0xff)
        ];
        return implode(':', array_map(fn($b) => sprintf('%02x', $b), $mac));
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
            $errMsg = sprintf('%s%s%s',
                gettext("Failed to download image."), PHP_EOL, $outMsg
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

    public function getMonitorStats($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $ch = curl_init('http://127.0.0.1:8889/api/vms');
        curl_setopt_array($ch, [
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_CONNECTTIMEOUT => 2,
            CURLOPT_TIMEOUT        => 5,
        ]);
        $json = curl_exec($ch);
        $err  = curl_error($ch);
        curl_close($ch);
        if ($json === false || $json === '') {
            return [];
        }
        $data = json_decode($json, true);
        return is_array($data) ? $data : [];
    }

    public function clearMonitorDb($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $ch = curl_init('http://127.0.0.1:8889/api/db/clear');
        curl_setopt_array($ch, [
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_POST           => true,
            CURLOPT_CONNECTTIMEOUT => 2,
            CURLOPT_TIMEOUT        => 15,
        ]);
        $json = curl_exec($ch);
        $err  = curl_error($ch);
        curl_close($ch);
        if ($json === false) {
            throw new \OMV\Exception("Failed to clear monitor database: %s", $err);
        }
    }
}
