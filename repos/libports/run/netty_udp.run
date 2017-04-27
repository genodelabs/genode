source ${genode_dir}/repos/libports/run/netty.inc

append build_components { test/netty/udp }

build $build_components

create_boot_directory

append config {
	<start name="netty-server-7">
		<binary name="test-netty_udp"/>
		<resource name="RAM" quantum="4M"/>
		<config ld_verbose="yes" port="7" read_write="no" nonblock="true">
			<vfs>
				<dir name="dev">    <log/> </dir>
				<dir name="socket"> <fs/>  </dir>
				<dir name="tmp">    <ram/> </dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"/>
		</config>
	</start>
	<start name="netty-server-7070">
		<binary name="test-netty_udp"/>
		<resource name="RAM" quantum="4M"/>
		<config ld_verbose="yes" mode="server" port="7070" nonblock="false">
			<vfs>
				<dir name="dev">    <log/> </dir>
				<dir name="socket"> <fs/>  </dir>
				<dir name="tmp">    <ram/> </dir>
			</vfs>
			<libc stdout="/dev/log" stderr="/dev/log" socket="/socket"/>
		</config>
	</start>
</config>
}

install_config $config

append boot_modules { test-netty_udp }

build_boot_image $boot_modules

run_genode_until forever

# vi: set ft=tcl :
