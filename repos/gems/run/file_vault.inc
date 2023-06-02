proc ui_is { arg } {

	global ui
	return [string equal $arg $ui ]
}

build { app/file_vault }

create_boot_directory

append archives "
	[depot_user]/src/[base_src]
	[depot_user]/src/init
	[depot_user]/src/libc
	[depot_user]/src/zlib
	[depot_user]/src/fs_query
	[depot_user]/src/tresor
	[depot_user]/src/vfs_block
	[depot_user]/src/vfs_jitterentropy
	[depot_user]/src/vfs
	[depot_user]/src/openssl
	[depot_user]/src/fs_tool
	[depot_user]/src/fs_utils
	[depot_user]/src/posix
	[depot_user]/src/rump
	[depot_user]/src/sandbox
"

lappend_if [ui_is menu_view] archives [depot_user]/src/nitpicker
lappend_if [ui_is menu_view] archives [depot_user]/src/menu_view
lappend_if [ui_is menu_view] archives [depot_user]/src/libpng
lappend_if [ui_is menu_view] archives [depot_user]/pkg/fonts_fs
lappend_if [ui_is menu_view] archives [depot_user]/pkg/[drivers_interactive_pkg]

lappend_if [ui_is config_and_report] archives [depot_user]/src/report_rom
lappend_if [ui_is config_and_report] archives [depot_user]/src/dynamic_rom

lappend_if [have_board linux] archives [depot_user]/src/lx_fs

import_from_depot $archives

append config {

	<config>

		<parent-provides>
			<service name="ROM"/>
			<service name="LOG"/>
			<service name="RM"/>
			<service name="CPU"/>
			<service name="PD"/>
			<service name="IRQ"/>
			<service name="IO_MEM"/>
			<service name="IO_PORT"/>
		</parent-provides>

		<start name="timer" caps="200">
			<resource name="RAM" quantum="1M"/>
			<resource name="CPU" quantum="5"/>
			<provides> <service name="Timer"/> </provides>
			<route>
				<service name="PD">      <parent/> </service>
				<service name="ROM">     <parent/> </service>
				<service name="LOG">     <parent/> </service>
				<service name="CPU">     <parent/> </service>
				<service name="IO_PORT"> <parent/> </service>
				<service name="IRQ">     <parent/> </service>
			</route>
		</start>
}
append_if [ui_is menu_view] config {

		<start name="drivers" caps="1500" managing_system="yes">
			<resource name="RAM" quantum="64M"/>
			<binary name="init"/>
			<route>
				<service name="Timer">              <child name="timer"/> </service>
				<service name="Capture">            <child name="nitpicker"/> </service>
				<service name="Event">              <child name="nitpicker"/> </service>
				<service name="ROM" label="config"> <parent label="drivers.config"/> </service>
				<service name="PD">                 <parent/> </service>
				<service name="ROM">                <parent/> </service>
				<service name="LOG">                <parent/> </service>
				<service name="CPU">                <parent/> </service>
				<service name="RM">                 <parent/> </service>
				<service name="IO_MEM">             <parent/> </service>
				<service name="IO_PORT">            <parent/> </service>
				<service name="IRQ">                <parent/> </service>
			</route>
		</start>

		<start name="nitpicker" caps="100">
			<resource name="RAM" quantum="4M"/>
			<provides>
				<service name="Gui"/> <service name="Capture"/> <service name="Event"/>
			</provides>
			<config focus="rom">
				<capture/> <event/>
				<background color="#123456"/>
				<domain name="pointer" layer="1" content="client" label="no" origin="pointer" />
				<domain name="default" layer="3" content="client" label="no" hover="always" />
				<domain name="second" layer="2" xpos="200" ypos="300" content="client" label="no" hover="always" />

				<policy label_prefix="pointer" domain="pointer"/>
				<policy label_prefix="text_area.2" domain="second"/>
				<default-policy domain="default"/>
			</config>
			<route>
				<service name="Timer">  <child name="timer" /> </service>
				<service name="PD">     <parent/> </service>
				<service name="ROM">    <parent/> </service>
				<service name="LOG">    <parent/> </service>
				<service name="CPU">    <parent/> </service>
			</route>
		</start>

		<start name="pointer" caps="100">
			<resource name="RAM" quantum="1M"/>
			<route>
				<service name="Gui"> <child name="nitpicker" /> </service>
				<service name="PD">  <parent/> </service>
				<service name="ROM"> <parent/> </service>
				<service name="LOG"> <parent/> </service>
				<service name="CPU"> <parent/> </service>
			</route>
		</start>

		<start name="fonts_fs" caps="150">
			<binary name="vfs"/>
			<resource name="RAM" quantum="4M"/>
			<provides>
				<service name="File_system"/>
			</provides>
			<config>
				<vfs>
					<rom name="Vera.ttf"/>
					<rom name="VeraMono.ttf"/>
					<dir name="fonts">
						<dir name="title">
							<ttf name="regular" path="/Vera.ttf" size_px="18" cache="256K"/>
						</dir>
						<dir name="text">
							<ttf name="regular" path="/Vera.ttf" size_px="14" cache="256K"/>
						</dir>
						<dir name="annotation">
							<ttf name="regular" path="/Vera.ttf" size_px="11" cache="256K"/>
						</dir>
						<dir name="monospace">
							<ttf name="regular" path="/VeraMono.ttf" size_px="14" cache="256K"/>
						</dir>
					</dir>
				</vfs>

				<policy label="file_vault -> fonts" root="/fonts" />
			</config>
			<route>
				<service name="ROM" label="config"> <parent label="fonts_fs.config"/> </service>
				<service name="PD">                 <parent/> </service>
				<service name="ROM">                <parent/> </service>
				<service name="LOG">                <parent/> </service>
				<service name="CPU">                <parent/> </service>
			</route>
		</start>
}
append_if [ui_is config_and_report] config {

		<start name="dynamic_rom" caps="100">
			<resource name="RAM" quantum="4M"/>
			<provides><service name="ROM"/> </provides>
			<config verbose="no">
				<rom name="file_vault_ui_config">
					<inline>

						<ui_config version="step_1_wait"/>

					</inline>
					<sleep milliseconds="6000"/>
					<inline>

						<ui_config version="step_2_init" passphrase="abcdefgh"
						                                 client_fs_size="1M"
						                                 journaling_buf_size="1M"/>

					</inline>
					<sleep milliseconds="10000"/>
					<inline>

						<ui_config version="step_3_lock"/>

					</inline>
					<sleep milliseconds="6000"/>
					<inline>

						<ui_config version="step_4_bad_unlock" passphrase="00000001"/>

					</inline>
					<sleep milliseconds="6000"/>
					<inline>

						<ui_config version="step_5_wait"/>

					</inline>
					<sleep milliseconds="6000"/>
					<inline>

						<ui_config version="step_6_unlock" passphrase="abcdefgh"/>

					</inline>
					<sleep milliseconds="600000"/>

				</rom>
			</config>
			<route>
				<service name="Timer"> <child name="timer"/> </service>
				<service name="PD">  <parent/> </service>
				<service name="ROM"> <parent/> </service>
				<service name="LOG"> <parent/> </service>
				<service name="CPU"> <parent/> </service>
			</route>
		</start>

		<start name="report_rom" caps="70">
			<resource name="RAM" quantum="1M"/>
			<provides>
				<service name="ROM" />
				<service name="Report" />
			</provides>
			<config verbose="yes"/>
			<route>
				<service name="LOG"> <parent/> </service>
				<service name="PD">  <parent/> </service>
				<service name="CPU"> <parent/> </service>
				<service name="ROM"> <parent/> </service>
			</route>
		</start>
}
if {[have_board linux]} {

	append config {

		<start name="data_fs" caps="200" ld="no">
			<binary name="lx_fs"/>
			<resource name="RAM" quantum="4M"/>
			<provides>
				<service name="File_system"/>
			</provides>
			<config>
				<policy label="file_vault -> data"
				        root="/file_vault_dir/data"
				        writeable="yes"/>
			</config>
			<route>
				<service name="Timer"> <child name="timer"/> </service>
				<service name="PD">  <parent/> </service>
				<service name="ROM"> <parent/> </service>
				<service name="LOG"> <parent/> </service>
				<service name="CPU"> <parent/> </service>
			</route>
		</start>

		<start name="trust_anchor_fs" caps="200" ld="no">
			<binary name="lx_fs"/>
			<resource name="RAM" quantum="4M"/>
			<provides>
				<service name="File_system"/>
			</provides>
			<config>
				<policy label="file_vault -> trust_anchor"
				        root="/file_vault_dir/trust_anchor"
				        writeable="yes"/>
			</config>
			<route>
				<service name="Timer"> <child name="timer"/> </service>
				<service name="PD">  <parent/> </service>
				<service name="ROM"> <parent/> </service>
				<service name="LOG"> <parent/> </service>
				<service name="CPU"> <parent/> </service>
			</route>
		</start>
	}

} else {

	append config {

		<start name="data_fs" caps="2000">
			<binary name="vfs"/>
			<resource name="RAM" quantum="200M"/>
			<provides><service name="File_system"/></provides>
			<config>
				<vfs>
					<dir name="data">
						<ram/>
					</dir>
				</vfs>
				<policy label="file_vault -> data" root="/data" writeable="yes"/>
			</config>
			<route>
				<service name="PD">  <parent/> </service>
				<service name="ROM"> <parent/> </service>
				<service name="LOG"> <parent/> </service>
				<service name="CPU"> <parent/> </service>
			</route>
		</start>

		<start name="trust_anchor_fs" caps="100">
			<binary name="vfs"/>
			<resource name="RAM" quantum="5M"/>
			<provides><service name="File_system"/></provides>
			<config>
				<vfs>
					<dir name="trust_anchor">
						<ram/>
					</dir>
				</vfs>
				<policy label="file_vault -> trust_anchor" root="/trust_anchor" writeable="yes"/>
			</config>
			<route>
				<service name="PD">  <parent/> </service>
				<service name="ROM"> <parent/> </service>
				<service name="LOG"> <parent/> </service>
				<service name="CPU"> <parent/> </service>
			</route>
		</start>
	}
}
append config {

		<start name="file_vault" caps="2000">
			<resource name="RAM" quantum="200M"/>
			<config user_interface="} $ui {">
				<vfs>
					<dir name="tresor">
						<fs label="tresor"/>
					</dir>
				</vfs>
			</config>
			<route>
}
append_if [ui_is menu_view] config {

				<service name="File_system" label="menu_view -> fonts">                  <child name="fonts_fs"        label="file_vault -> fonts"/> </service>
				<service name="Gui">                                                     <child name="nitpicker"/> </service>
}
append_if [ui_is config_and_report] config {

				<service name="ROM"         label="ui_config">                           <child name="dynamic_rom"     label="file_vault_ui_config"/> </service>
				<service name="Report"      label="ui_report">                           <child name="report_rom"/> </service>
}
append config {
				<service name="File_system" label="tresor_trust_anchor_vfs -> storage_dir"> <child name="trust_anchor_fs" label="file_vault -> trust_anchor"/> </service>
				<service name="File_system" label="tresor_init -> ">                        <child name="data_fs"         label="file_vault -> data"/> </service>
				<service name="File_system" label="tresor">                                 <child name="data_fs"         label="file_vault -> data"/> </service>
				<service name="File_system" label="fs_query -> ">                        <child name="data_fs"         label="file_vault -> data"/> </service>
				<service name="File_system" label="image_fs_query -> ">                  <child name="data_fs"         label="file_vault -> data"/> </service>
				<service name="File_system" label="tresor_vfs -> tresor_fs">                   <child name="data_fs"         label="file_vault -> data"/> </service>
				<service name="File_system" label="truncate_file -> tresor">                <child name="data_fs"         label="file_vault -> data"/> </service>
				<service name="Timer">                                                   <child name="timer"/> </service>
				<service name="PD">                                                      <parent/> </service>
				<service name="ROM">                                                     <parent/> </service>
				<service name="CPU">                                                     <parent/> </service>
				<service name="LOG">                                                     <parent/> </service>
				<service name="RM">                                                      <parent/> </service>
			</route>
		</start>

	</config>
}

install_config $config

if {[have_board linux]} {

	if {[ui_is config_and_report]} {

		exec rm -rf bin/file_vault_dir
	}
	exec mkdir -p bin/file_vault_dir/data
	exec mkdir -p bin/file_vault_dir/trust_anchor
}

append boot_modules {

	file_vault
	file_vault-sync_to_tresor_vfs_init
	file_vault-truncate_file
}

lappend_if [have_board linux] boot_modules file_vault_dir

if {[ui_is menu_view]} {

	set fd [open [run_dir]/genode/focus w]
	puts $fd "<focus label=\"file_vault -> \"/>"
	close $fd

	append qemu_args " -display gtk "
	build_boot_image $boot_modules
	run_genode_until forever
}

if {[ui_is config_and_report]} {

	build_boot_image $boot_modules

	append exp_output {.*<ui_report version=\"step_.*\" state=\"uninitialized\"/>.*\n}
	append exp_output {.*<ui_report version=\"step_.*\" state=\"initializing\"/>.*\n}
	append exp_output {.*<ui_report version=\"step_.*\" state=\"unlocked\"/>.*\n}
	append exp_output {.*<ui_report version=\"step_.*\" state=\"locked\"/>.*\n}
	append exp_output {.*<ui_report version=\"step_.*\" state=\"unlocked\"/>.*\n}

	append qemu_args " -display none "
	run_genode_until $exp_output 70
}
