#
# This script is used to test the shared folder support of Vbox@Genode/NOVA.
#
# Requirements to run this script:
# - hard disk with 4 primary partitions
# - on the 4. partition an ext2 filesystem is expected with following files:
# -- /win7.vdi
# -- /ram/overlay_win7.vdi
#
# The overlay file must be generated beforehand, e.g.:
#
#    vboxmanage showhdinfo win7.vdi
#
# to find out the size of the vdi and then to create the overlay:
#
#   vboxmanage createhd --filename overlay_win7.vdi --size [vdi_size] --format vdi
#
#
#
#
# The Windows image (win7.vdi) is prepared to contain a bat file with following
# content:
#
#     :check
#     IF EXIST E:\test.bat (GOTO start) ELSE (timeout /t 3 >nul)
#     GOTO check
#
#     :start
#
#     call E:\test.bat
#
# This file must be configured to be autostarted by Windows, e.g change the
# value in the registry entry at
#
# HKEY_CURRENT_USER\Software/Microsoft\Windows\CurrentVersion\Run
#
# to the batch file in the Windows vdi image as described above.
#
#
# What this script does:
#
# A VM containing a Win7 image is started configured with two different
# Virtualbox fileshares (D: and E:). One contains a test file (test.bin),
# which will be copied by the Windows VM from the one file share over to the
# second one.
# On Genode side a Noux instance is running a bash shell with the two 
# filesystems mounted. The shell is scripted by this script to wait for
# completion of the copy operation performed by the Windows VM. If this is
# detected, the bash scripts use sha1sum to generate hashes of the original
# test file of the one file system share and of the copy on the second file
# system share. This run scripts then compares the hashes and reports
# "Test succeeded" if they are identical.
#
# This script generates the test file out of /dev/urandom and generates a
# batch file test.bat which is placed in the shared folder for the VM. test.bat
# is invoked by the batch file running already in the Windows VM as described
# beforehand. test.bat contains the actual instructions to be performed by
# the VM - for this script to copy the test.bin file from D: to E:.

if {[have_include "power_on/qemu"]} {
	puts "\nRun script does not support Qemu.\n"
	exit 0
}

# Tested for nova
if {![have_spec nova]} {
	puts "Platform is unsupported."
	exit 0
}

set virtualbox_binary "virtualbox-rem"
if {[have_spec nova]} { set virtualbox_binary "virtualbox-nova" }
set virtualbox5_binary "virtualbox5-rem"
if {[have_spec nova]} { set virtualbox5_binary "virtualbox5-nova" }

#
# Create .bat file to be executed by Win VM
#
set template_bat_fd [open "bin/template.bat" w]
puts $template_bat_fd {:check
IF EXIST E:\start.txt (GOTO start) ELSE (timeout /t 3 >nul)
GOTO check

:start

copy D:\test.bin E:\test.bin
copy D:\template.bat E:\done.txt
shutdown /s /t 00}

close $template_bat_fd

# Convert bat file to dos format
catch { exec unix2dos bin/template.bat }

#
# Create random test binary to be copied via shared folder of vbox
#
catch { exec dd if=/dev/urandom of=bin/test.bin bs=4096 count=8160 }

#
# Step 1: prepare and start the actual VM
#
set build_components {
	lib/vfs lib/libc lib/posix lib/vfs_lwip lib/vfs_pipe lib/vfs_import
	server/event_filter server/report_rom server/fs_rom server/vfs
	server/tcp_terminal server/nic_router
	drivers/nic
}

#
# Build Noux packages only once
#
foreach pkg {bash coreutils} {
	lappend_if [expr ![file exists bin/$pkg]] build_components noux-pkg/$pkg }

lappend boot_modules bash.tar coreutils.tar test.bin template.bat $vbox_file

set config_of_app {

	<start name="nic" priority="-1">
		<binary name="pc_nic"/>
		<resource name="RAM" quantum="8M"/>
		<route>
			<service name="Uplink"><child name="nic_router"/></service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="nic_router" caps="200">
		<resource name="RAM" quantum="10M"/>
		<provides>
			<service name="Nic"/>
			<service name="Uplink"/>
		</provides>
		<config verbose_domain_state="yes">

			<policy label_prefix="nic"          domain="uplink"/>
			<policy label_prefix="tcp_terminal" domain="downlink"/>

			<domain name="uplink">

				<nat domain="downlink"
				     tcp-ports="16384"
				     udp-ports="16384"
				     icmp-ids="16384"/>

				<tcp-forward port="8888" domain="downlink" to="10.0.3.2"/>

			</domain>

			<domain name="downlink" interface="10.0.3.1/24">

				<dhcp-server ip_first="10.0.3.2" ip_last="10.0.3.2" dns_config_from="uplink"/>

				<tcp dst="0.0.0.0/0"><permit-any domain="uplink" /></tcp>
				<udp dst="0.0.0.0/0"><permit-any domain="uplink" /></udp>
				<icmp dst="0.0.0.0/0" domain="uplink"/>

			</domain>

		</config>
	</start>

	<start name="tcp_terminal" priority="-1" caps="200">
		<resource name="RAM" quantum="6M"/>
		<provides> <service name="Terminal"/> </provides>
		<config>
			<policy label_prefix="vfs" port="8888"/>
			<vfs>
				<dir name="dev"> <log/> </dir>
				<dir name="socket"> <lwip dhcp="yes"/> </dir>
				<dir name="pipe"> <pipe/> </dir>
			</vfs>
			<libc stdout="/dev/log" socket="/socket" pipe="/pipe"/>
		</config>
		<route>
			<service name="Nic"><child name="nic_router"/></service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="ram_fs_from" priority="-1">
		<binary name="vfs"/>
		<resource name="RAM" quantum="64M"/>
		<provides><service name="File_system"/></provides>
		<config>
			<vfs>
				<ram/>
				<import>
					<rom name="test.bin"/>
					<rom name="template.bat"/>
				</import>
			</vfs>
			<default-policy root="/" writeable="no"/>
		</config>
	</start>

	<start name="ram_fs_to" priority="-1">
		<binary name="vfs"/>
		<resource name="RAM" quantum="64M"/>
		<provides><service name="File_system"/></provides>
		<config>
			<vfs> <ram/> </vfs>
			<default-policy root="/" writeable="yes"/>
		</config>
	</start>

	<start name="vfs" caps="120" priority="-1">
		<resource name="RAM" quantum="30M"/>
		<provides><service name="File_system"/></provides>
		<config>
			<vfs>
				<tar name="coreutils.tar"/>
				<tar name="bash.tar"/>

				<dir name="from"> <fs label="share_from"/> </dir>
				<dir name="to">   <fs label="share_to"/> </dir>

				<dir name="tmp"> <ram/> </dir>
				<dir name="dev">
					<zero/> <null/> <terminal/>
					<inline name="rtc">2018-01-01 00:01</inline>
				</dir>
				<dir name="pipe"> <pipe/> </dir>
			</vfs>

			<policy label_prefix="vfs_rom" root="/"/>
			<default-policy root="/" writeable="yes"/>
		</config>
		<route>
			<service name="File_system" label="share_from">
				<child name="ram_fs_from"/>
			</service>
			<service name="File_system" label="share_to">
				<child name="ram_fs_to"/>
			</service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="vfs_rom" priority="-1">
		<resource name="RAM" quantum="30M"/>
		<binary name="fs_rom"/>
		<provides> <service name="ROM"/> </provides>
		<config/>
		<route>
			<service name="File_system"> <child name="vfs"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="/bin/bash" caps="600" priority="-1">
		<resource name="RAM" quantum="30M" />
		<config ld_verbose="yes">
			<libc stdin="/dev/terminal" stdout="/dev/terminal"
			      stderr="/dev/terminal" rtc="/dev/rtc" pipe="/pipe"/>
			<vfs> <fs/> </vfs>
			<arg value="bash"/>
			<env key="TERM" value="screen"/>
			<env key="PATH" value="/bin" />
		</config>
		<route>
			<service name="File_system"> <child name="vfs"/> </service>
			<service name="ROM" label_suffix=".lib.so"> <parent/> </service>
			<service name="ROM" label_last="/bin/bash"> <child name="vfs_rom"/> </service>
			<service name="ROM" label_prefix="/bin"> <child name="vfs_rom"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>

	<start name="report_rom" priority="-1">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Report"/> <service name="ROM"/> </provides>
		<config>
			<policy label="pointer -> hover"  report="nitpicker -> hover"/>
			<policy label="pointer -> xray"   report="nitpicker -> xray"/>
			<policy label="usb_hid -> report" report="usb_drv -> devices"/>
		</config>
	</start>

	<start name="event_filter" priority="-1">
		<resource name="RAM" quantum="1M" />
		<provides>
			<service name="Event" />
		</provides>
		<config>
			<output>
				<merge>}
append_if [expr $use_ps2] config_of_app {
					<input name="ps2"/>}
append_if [expr $use_usb] config_of_app {
					<input name="usb_hid"/>}
append config_of_app {
				</merge>
			</output>}
append_if [expr $use_ps2] config_of_app {
			<policy label="ps2" input="ps2"/>}
append_if [expr $use_usb] config_of_app {
			<policy label="usb_hid" input="usb_hid"/>}
append config_of_app {
		</config>
		<route>
			<service name="Event"> <child name="nitpicker"/> </service>
			<any-service> <parent /> <any-child /> </any-service>
		</route>
	</start>

	<start name="nitpicker" priority="-1">
		<resource name="RAM" quantum="12M"/>
		<provides>
			<service name="Gui"/> <service name="Capture"/> <service name="Event"/>
		</provides>
		<route>
			<service name="Report"> <child name="report_rom" /> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
		<config>
			<capture/> <event/>
			<report focus="yes" hover="yes" />

			<domain name="pointer"  layer="1" content="client" label="no" origin="pointer" />
			<domain name="cpu_load" layer="2" content="client" label="no" />
			<domain name=""         layer="3" content="client" label="no" focus="click" hover="always" />

			<policy label_prefix="pointer"          domain="pointer"/>
			<policy label_prefix="cpu_load_display" domain="cpu_load"/>
			<default-policy domain=""/>
		</config>
	</start>

	<start name="pointer" priority="-1">
		<resource name="RAM" quantum="2M"/>
		<provides> <service name="Report"/> </provides>
		<config shapes="yes"/>
		<route>
			<service name="Gui"> <child name="nitpicker"/>  </service>
			<service name="ROM" label="hover"> <child name="report_rom"/> </service>
			<service name="ROM" label="xray"> <child name="report_rom"/> </service>
			<any-service> <parent/> </any-service>
		</route>
	</start>

	<start name="vbox1" priority="-2" caps="600">}
append_if [expr $use_vbox5] config_of_app "
		<binary name=\"$virtualbox5_binary\" />"
append config_of_app {
		<resource name="RAM" quantum="1280M"/>
		<config vbox_file="} $vbox_file {" vm_name="AutoDisk">
			<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/rtc" pipe="/pipe"/>
			<vfs>
				<dir name="dev">
					<log/> <rtc/>
				 </dir>
				<dir name="pipe"> <pipe/> </dir>
				<rom name="} $vbox_file {"/>}

append_if [expr $use_ram_fs] config_of_app {
				<dir name="ram">  <fs label="from_ram_fs"/> </dir>}

append config_of_app {
				<dir name="from"> <fs label="share_ram_fs_from"/> </dir>
				<dir name="to">   <fs label="share_ram_fs_to"/> </dir>
				<fs/>
			</vfs>
		</config>
		<route>
			<service name="Report"> <child name="report_rom"/> </service>}

append_if [expr $use_ram_fs] config_of_app {
			<service name="File_system" label="from_ram_fs">
				<child name="ram_fs"/>
			</service>}

append config_of_app {
			<service name="File_system" label="share_ram_fs_from">
				<child name="ram_fs_from"/>
			</service>
			<service name="File_system" label="share_ram_fs_to">
				<child name="ram_fs_to"/>
			</service>
			<service name="File_system"> <child name="rump_fs"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>
}

source ${genode_dir}/repos/ports/run/virtualbox_auto.inc

exec cp ${genode_dir}/repos/ports/run/$vbox_file bin/.

build_boot_image $boot_modules

set ip_match_string "nic_router\\\] \\\[uplink\\\] dynamic IP config: interface (\[0-9]{1,3}.\[0-9]{1,3}.\[0-9]{1,3}.\[0-9]{1,3}).*\n"

#
# Step 2: Read out TCP/IP address of tcp_terminal running on Genode target
#
run_genode_until $ip_match_string 20
set serial_id [output_spawn_id]

regexp $ip_match_string $output -> serial_ip_addr

#
# Step 3: Wait until Windows is up for sure
#


run_genode_until {\[init -\> vbox1\].*Guest Additions capability report:.*seamless: yes, hostWindowMapping: no, graphics: yes} 300 $serial_id


#
# Step 4 : connect to Noux of the running Genode target and issue bash commands
#          via netcat
#
puts "\nTest shared folder\n"

spawn netcat $serial_ip_addr 8888
set netcat_id $spawn_id
set spawn_id_list [list $netcat_id $serial_id]

run_genode_until {\[init -> tcp_terminal\] connection established} 20 $spawn_id_list

#
# Step 5 : interact with netcat -> ... -> tcp_terminal -> /bin/bash
#

# Windows does not like trailing zeros introduced by our ROM service.
# -> .bat script does not find labels like 'check' with zero bytes
# so - delete zero bytes
puts $netcat_id "cat from/template\.bat | tr -d \"\\0\" >to/test\.bat"

# SHA1 of original file
puts $netcat_id "sha1sum from/test.bin"

# Tell Windows VM to start copying
puts $netcat_id "echo \"start\" > to/start\.txt"

# Wait until Windows finished copying
puts $netcat_id "while \[ ! -f to/done\.txt ]"
puts $netcat_id "do"
puts $netcat_id "sleep 5"
puts $netcat_id "done"

# Wait until VM signaled shutdown state
run_genode_until {\[init\] child "vbox1" exited with exit value 0} 120 $spawn_id_list

# SHA1 of copied file
puts $netcat_id "sha1sum to/test.bin"

# Wait for output of bash shell until last SHA1 sum is calculated
run_genode_until {[[:xdigit:]]+  to/test\.bin} 50 $spawn_id_list

# cleanup created files
exec rm -f bin/test.bin
exec rm -f bin/template.bat
exec rm -f bin/$vbox_file


#
# Step 5: Compare sha1sum of original file and of copy made by .bat file in VM
#
set sha1sum_original [regexp -inline {[[:xdigit:]]+  from/test\.bin} $output]
set sha1sum_copy     [regexp -inline {[[:xdigit:]]+  to/test\.bin}   $output]
set sha1sum_original [regexp -inline {[[:xdigit:]]+} $sha1sum_original]
set sha1sum_copy     [regexp -inline {[[:xdigit:]]+} $sha1sum_copy]

puts -nonewline "\n$sha1sum_original ?= $sha1sum_copy --> "
if {$sha1sum_original eq ""} {
	puts " empty -> no "
} else {
	if {$sha1sum_original != $sha1sum_copy} {
		puts "no"
		exit -1
	} else {
		puts "yes"
	}
}
