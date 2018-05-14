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

# Tested for nova and muen.
if {(![have_spec nova] && ![have_spec muen])} {
	puts "Platform is unsupported."
	exit 0
}

set virtualbox_binary "virtualbox-rem"
if {[have_spec muen]} { set virtualbox_binary "virtualbox-muen" }
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
	server/input_merger
	server/ram_fs server/report_rom
	server/tcp_terminal drivers/nic
	lib/libc_noux
	noux/minimal
}

#
# Build Noux packages only once
#
foreach pkg {bash coreutils} {
	lappend_if [expr ![file exists bin/$pkg]] build_components noux-pkg/$pkg }

set boot_modules {
	ram_fs
	noux libc_noux.lib.so posix.lib.so bash.tar coreutils.tar
	tcp_terminal lwip_legacy.lib.so nic_drv report_rom
	input_merger
	test.bin template.bat
}

append boot_modules $vbox_file

set config_of_app {
	<start name="nic_drv" priority="-1">
		<resource name="RAM" quantum="4M"/>
		<provides> <service name="Nic"/> </provides>
	</start>

	<start name="tcp_terminal" priority="-1" caps="200">
		<resource name="RAM" quantum="5M"/>
		<provides> <service name="Terminal"/> </provides>
		<config>
			<policy label_prefix="noux" port="8888"/>
			<vfs> <dir name="dev"> <log/> </dir> </vfs>
			<libc stdout="/dev/log"/>
		</config>
	</start>

	<start name="noux" priority="-1" caps="600">
		<resource name="RAM" quantum="32M"/>
		<config>
			<fstab>
				<tar name="coreutils.tar"/>
				<tar name="bash.tar"/>

				<dir name="from"> <fs label="share_from"/> </dir>
				<dir name="to">   <fs label="share_to"/> </dir>
			</fstab>
			<start name="/bin/bash">
				<env name="TERM" value="screen"/>
			</start>
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

	<start name="ram_fs_from" priority="-1">
		<binary name="ram_fs"/>
		<resource name="RAM" quantum="64M"/>
		<provides><service name="File_system"/></provides>
		<config>
			<content>
				<rom name="test.bin"/>
				<rom name="template.bat"/>
			</content>
			<default-policy root="/" writeable="no"/>
		</config>
	</start>

	<start name="ram_fs_to" priority="-1">
		<binary name="ram_fs"/>
		<resource name="RAM" quantum="64M"/>
		<provides><service name="File_system"/></provides>
		<config>
			<default-policy root="/" writeable="yes"/>
		</config>
	</start>

	<start name="report_rom" priority="-1">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Report"/> <service name="ROM"/> </provides>
		<config>
			<policy label="usb_report_filter -> devices"        report="usb_drv -> devices"/>
			<policy label="usb_report_filter -> usb_drv_config" report="usb_drv -> config"/>
			<policy label="vbox1 -> usb_devices"                report="usb_report_filter -> usb_devices"/>
		</config>
	</start>

	<start name="input_merger" priority="-1">
		<resource name="RAM" quantum="1M" />
		<provides>
			<service name="Input" />
		</provides>
		<config>}
append_if [expr $use_ps2] config_of_app {
			<input label="ps2" /> }
append_if [expr $use_usb] config_of_app {
			<input label="usb_hid" />}
append config_of_app {
		</config>
		<route> }
append_if [expr $use_ps2] config_of_app {
			<service name="Input" label="ps2"> <child name="ps2_drv" /> </service> }
append_if [expr $use_usb] config_of_app {
			<service name="Input" label="usb_hid"> <child name="usb_drv" /> </service> }
append config_of_app {
			<any-service> <parent /> <any-child /> </any-service>
		</route>
	</start>

	<start name="vbox1" priority="-2" caps="600">}
append_if [expr $use_vbox4] config_of_app "
		<binary name=\"$virtualbox_binary\" />"
append_if [expr $use_vbox5] config_of_app "
		<binary name=\"$virtualbox5_binary\" />"
append config_of_app {
		<resource name="RAM" quantum="1280M"/>
		<config vbox_file="} $vbox_file {" vm_name="AutoDisk">
			<libc stdout="/dev/log" stderr="/dev/log" rtc="/dev/rtc"/>
			<vfs>
				<dir name="dev"> <log/> <rtc/> </dir>
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
			<service name="Input"> <child name="input_merger"/> </service>
			<any-service> <parent/> <any-child/> </any-service>
		</route>
	</start>
}

source ${genode_dir}/repos/ports/run/virtualbox_auto.inc

exec cp ${genode_dir}/repos/ports/run/$vbox_file bin/.

build_boot_image $boot_modules

#
# Step 2: Read out TCP/IP address of tcp_terminal running on Genode target
#
run_genode_until {\[init -> tcp_terminal\] got IP address [0-9]+\.[0-9]+\.[0-9]+\.[0-9]+} 20
set serial_id [output_spawn_id]

regexp {\[init -> tcp_terminal\] got IP address [0-9]+\.[0-9]+\.[0-9]+\.[0-9]+} $output serial_ip_addr
regexp {[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+} $serial_ip_addr serial_ip_addr

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
set noux_id $spawn_id
set spawn_id_list [list $noux_id $serial_id]

run_genode_until {\[init -> tcp_terminal\] connection established} 20 $spawn_id_list

#
# Step 5 : interact with netcat -> ... -> tcp_terminal -> Noux -> bash
#

# Windows does not like trailing zeros introduced by our ROM service.
# -> .bat script does not find labels like 'check' with zero bytes
# so - delete zero bytes
puts $noux_id "cat from/template\.bat | tr -d \"\\0\" >to/test\.bat"

# SHA1 of original file
puts $noux_id "sha1sum from/test.bin"

# Tell Windows VM to start copying
puts $noux_id "echo \"start\" > to/start\.txt"

# Wait until Windows finished copying
puts $noux_id "while \[ ! -f to/done\.txt ]"
puts $noux_id "do"
puts $noux_id "sleep 5"
puts $noux_id "done"

# Wait until VM signaled shutdown state
run_genode_until {\[init\] child "vbox1" exited with exit value 0} 120 $spawn_id_list

# SHA1 of copied file
puts $noux_id "sha1sum to/test.bin"

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
