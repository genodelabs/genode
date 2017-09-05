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

set use_usb_input [expr ![have_spec ps2] && ![have_spec sdl] && [have_spec usb]]

set build_components {
	core init drivers/timer noux/minimal lib/libc_noux
	drivers/framebuffer drivers/input
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
proc check_xml_with_schema {xml_file} { }

source ${genode_dir}/repos/base/run/platform_drv.inc
append_platform_drv_build_components

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
set vimrc_fd [open "bin/vimrc" w]
puts $vimrc_fd {
set noloadplugins
set hls
set nocompatible
set laststatus=2
set noswapfile
set viminfo=}
close $vimrc_fd

exec tar cvf bin/genode.tar -C $genode_dir tool repos/base repos/$platform_base_dir repos/os repos/demo

create_boot_directory

append config {
	<config verbose="yes">
		<parent-provides>
			<service name="ROM"/>
			<service name="LOG"/>
			<service name="RM"/>
			<service name="CPU"/>
			<service name="PD"/>
			<service name="IRQ"/>
			<service name="IO_PORT"/>
			<service name="IO_MEM"/>
		</parent-provides>
		<default-route>
			<any-service> <parent/> <any-child/> </any-service>
		</default-route>
		<default caps="100"/>
		<start name="timer">
			<resource name="RAM" quantum="1M"/>
			<provides><service name="Timer"/></provides>
		</start> }

append_if [have_spec sdl] config {
	<start name="fb_sdl">
		<resource name="RAM" quantum="4M"/>
		<provides>
			<service name="Input"/>
			<service name="Framebuffer"/>
		</provides>
	</start>}

append_platform_drv_config

append_if [have_spec framebuffer] config {
	<start name="fb_drv" caps="200">
		<resource name="RAM" quantum="8M"/>
		<provides><service name="Framebuffer"/></provides>
	</start>}

append_if [have_spec ps2] config {
	<start name="ps2_drv">
		<resource name="RAM" quantum="1M"/>
		<provides><service name="Input"/></provides>
	</start> }

append_if $use_usb_input config {
	<start name="usb_drv" caps="120">
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
	<start name="ram_fs" caps="200">
		<resource name="RAM" quantum="124M"/>
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
	<start name="noux" caps="2000">
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

				<dir name="bin"> <symlink name="sh" target="bash"/> </dir>

				<dir name="share"> <dir name="vim"> <rom name="vimrc"/> </dir> </dir>

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
	libc.lib.so libm.lib.so libc_noux.lib.so ncurses.lib.so stdcxx.lib.so
	pcre.lib.so posix.lib.so vimrc
}

append boot_modules { gmp.lib.so mpfr.lib.so mpc.lib.so }

foreach pkg $noux_pkgs {
	lappend boot_modules "$pkg.tar" }

append boot_modules { genode.tar }

foreach pkg $platform_pkgs { append boot_modules "$pkg.tar" }

# platform-specific modules
lappend_if [have_spec       linux] boot_modules fb_sdl
lappend_if [have_spec framebuffer] boot_modules fb_drv
lappend_if [have_spec         ps2] boot_modules ps2_drv
lappend_if $use_usb_input          boot_modules usb_drv

append_platform_drv_boot_modules

build_boot_image $boot_modules

# vi: set ft=tcl :
