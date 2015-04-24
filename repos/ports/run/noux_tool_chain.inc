#
# Configuration variables for the platform-specific run scripts:
#
# $platform          - name of the platform passed to the 'create_builddir' script
# $platform_base_dir - name of the platform-specific Genode base directory
# $platform_cmds     - platform-specific shell commands to be executed at startup
# $platform_pkgs     - names of platform-specific tar archives without the '.tar' extension
#

#
# The Linux version of Noux lacks the support for the fork system call. Hence,
# the run script is expected to fail.
#
if {[have_spec linux]} {
	puts "Linux is unsupported."
	exit 0
}

set use_platform_driver [expr [have_spec platform_arndale] || [have_spec platform_imx53] || [have_spec platform_rpi]]
set use_usb_input [expr ![have_spec ps2] && ![have_spec sdl] && [have_spec usb]]

set build_components {
	core init drivers/timer noux/minimal lib/libc_noux
	drivers/framebuffer drivers/pci drivers/input
	server/terminal server/ram_fs
	test/libports/ncurses
}

#
# Disable syntax check for config file
#
# The syntax check fails because the content of the '.bash_profile' is
# specified using an '<inline>' node but it is no valid XML. I.e., xmllint
# complains about the sequence '&&'.
#
proc check_xml_syntax {xml_file} { }


lappend_if [have_spec acpi]     build_components drivers/acpi
lappend_if [have_spec pci]      build_components drivers/pci/device_pd
lappend_if $use_platform_driver build_components drivers/platform
lappend_if $use_usb_input       build_components drivers/usb

set cc_march ""

if {[have_spec arm]} {
	set binutils "binutils_arm"
	set gcc "gcc_arm"
	set tool_prefix "genode-arm-"
}

if {[have_spec x86]} {
	set binutils "binutils_x86"
	set gcc "gcc_x86"
	set tool_prefix "genode-x86-"
	if {[have_spec x86_32]} {
		set cc_march "-m32"
        }
}

#
# Build Noux packages only once
#
set noux_pkgs "bash coreutils vim grep sed findutils make which $binutils $gcc"

foreach pkg $noux_pkgs {
	lappend_if [expr ![file exists bin/$pkg]] build_components noux-pkg/$pkg }

build $build_components

# write default vimrc file
set vimrc_fd [open "bin/vim/share/vim/vimrc" w]
puts $vimrc_fd {
set noloadplugins
set hls
set nocompatible
set laststatus=2
set noswapfile
set viminfo=}
close $vimrc_fd

# strip all binaries prior archiving
set find_args ""
foreach pkg $noux_pkgs { append find_args " bin/$pkg" }
exec sh -c "find $find_args -type f | (xargs [cross_dev_prefix]strip -g || true) 2>/dev/null"

exec ln -sf bash bin/bash/bin/sh

foreach pkg $noux_pkgs {
	exec tar cfv bin/$pkg.tar -h -C bin/$pkg . }

exec tar cvf bin/genode.tar -C $genode_dir tool repos/base repos/$platform_base_dir repos/os repos/demo

create_boot_directory

append config {
	<config verbose="yes">
		<parent-provides>
			<service name="ROM"/>
			<service name="LOG"/>
			<service name="CAP"/>
			<service name="RAM"/>
			<service name="RM"/>
			<service name="CPU"/>
			<service name="PD"/>
			<service name="IRQ"/>
			<service name="IO_PORT"/>
			<service name="IO_MEM"/>
			<service name="SIGNAL"/>
		</parent-provides>
		<default-route>
			<any-service> <any-child/> <parent/> </any-service>
		</default-route>
		<start name="timer">
			<resource name="RAM" quantum="1M"/>
			<provides><service name="Timer"/></provides>
		</start> }

append_if $use_platform_driver config {
		<start name="platform_drv">
			<resource name="RAM" quantum="1M" />
			<provides><service name="Regulator"/></provides>
			<provides><service name="Platform"/></provides>
		</start>}

append_if [have_spec sdl] config {
	<start name="fb_sdl">
		<resource name="RAM" quantum="4M"/>
		<provides>
			<service name="Input"/>
			<service name="Framebuffer"/>
		</provides>
	</start>}

append_if [have_spec acpi] config {
	<start name="acpi">
		<resource name="RAM" quantum="5M" constrain_phys="yes"/>
		<binary name="acpi_drv"/>
		<provides>
			<service name="PCI"/>
			<service name="IRQ" />
		</provides>
		<config>
			<policy label="acpi_drv"> <pci class="ALL"/> </policy>
			<policy label="ps2_drv">  <device name="PS2"/> </policy>
			<policy label="usb_drv">  <pci class="USB"/> </policy>
			<policy label="fb_drv">   <pci class="VGA"/> </policy>
		</config>
		<route>
			<service name="PCI"> <any-child /> </service>
			<any-service> <parent/> <any-child /> </any-service>
		</route>
	</start>}

append_if [have_spec framebuffer] config {
	<start name="fb_drv">
		<resource name="RAM" quantum="8M"/>
		<provides><service name="Framebuffer"/></provides>
	</start>}

append_if [expr ![have_spec acpi] && [have_spec pci]] config {
	<start name="pci_drv">
		<resource name="RAM" quantum="2M" constrain_phys="yes"/>
		<provides> <service name="PCI"/> </provides>
		<config>
			<policy label="ps2_drv">  <device name="PS2"/> </policy>
			<policy label="usb_drv">  <pci class="USB"/> </policy>
			<policy label="fb_drv">   <pci class="VGA"/> </policy>
		</config>
	</start> }

append_if [have_spec ps2] config {
	<start name="ps2_drv">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Input"/></provides>
	</start> }

append_if $use_usb_input config {
	<start name="usb_drv">
		<resource name="RAM" quantum="12M"/>
		<provides><service name="Input"/></provides>
		<config uhci="yes" ehci="no" xhci="no"> <hid/> </config>
	</start> }

append config {
	<start name="terminal">
		<resource name="RAM" quantum="2M"/>
		<provides><service name="Terminal"/></provides>
		<config>
			<keyboard layout="de"/>
		</config>
	</start>
	<start name="ram_fs">
		<resource name="RAM" quantum="100M"/>
		<provides><service name="File_system"/></provides>
		<config>
			<content>
				<dir name="home">
					<dir name="user">
						<inline name=".bash_profile">
							cp /bin/make /usr/bin/
							cp /bin/echo /usr/bin/
							cp /bin/mkdir /usr/bin/
							echo "creating build directory..."
}
append config "						/genode/tool/create_builddir $platform BUILD_DIR=/home/build"
append config {
							cd /home/build
}
append config "						echo \"CROSS_DEV_PREFIX=$tool_prefix\" > etc/tools.conf"
append config "                                         $platform_cmds"
append config {
							echo "now you can type, for example, \"make core\""
						</inline>
					</dir>
				</dir>
				<dir name="tmp" />
				<dir name="usr">
					<dir name="bin" />
				</dir>
			</content>

			<!-- constrain sessions according to their labels -->
			<policy label="noux -> root" root="/" />
			<policy label="noux -> home" root="/home/user" writeable="yes" />
			<policy label="noux -> tmp"  root="/tmp"       writeable="yes" />
			<policy label="noux -> usr"  root="/usr"       writeable="yes" />
		</config>
	</start>
}

append config {
	<start name="noux">
		<resource name="RAM" quantum="1G" />
}

if {![info exists verbose_mode]} {set verbose_mode "yes"}
append config " <config verbose=\"$verbose_mode\">"

append config {
			<fstab>
}

foreach pkg $noux_pkgs {
	append config "		<tar name=\"$pkg.tar\" />" }

append config {			<dir name="platform"> }
foreach pkg $platform_pkgs {
	append config "			<dir name=\"$pkg\">
						<tar name=\"$pkg.tar\" />
					</dir>" }

append config {			</dir>

				<dir name="genode">
					<tar name="genode.tar" />
				</dir>

				<dir name="dev">
					<null />
				</dir>

				<dir name="home">
					<fs label="home" />
				</dir>

				<dir name="tmp">
					<fs label="tmp" />
				</dir>

				<dir name="usr">
					<fs label="usr" />
				</dir>
			</fstab>
			<start name="/bin/bash">
				<env name="TERM" value="linux" />
				<env name="HOME" value="/home" />
				<env name="PATH" value="/bin" />
				<arg value="--login" />
			</start>
		</config>
	</start>
</config>
}

install_config $config


#
# Boot modules
#

# generic modules
set boot_modules {
	core init timer ld.lib.so noux terminal ram_fs
	libc.lib.so libm.lib.so libc_noux.lib.so ncurses.lib.so }

append boot_modules { gmp.lib.so mpfr.lib.so mpc.lib.so }

foreach pkg $noux_pkgs {
	lappend boot_modules "$pkg.tar" }

append boot_modules { genode.tar }

foreach pkg $platform_pkgs { append boot_modules "$pkg.tar" }

# platform-specific modules
lappend_if [have_spec       linux] boot_modules fb_sdl
lappend_if [have_spec         pci] boot_modules pci_drv
lappend_if [have_spec framebuffer] boot_modules fb_drv
lappend_if [have_spec         ps2] boot_modules ps2_drv
lappend_if [have_spec        acpi] boot_modules acpi_drv
lappend_if [have_spec        nova] boot_modules pci_device_pd
lappend_if $use_platform_driver    boot_modules platform_drv
lappend_if $use_usb_input          boot_modules usb_drv

build_boot_image $boot_modules

# vi: set ft=tcl :
