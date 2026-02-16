create_boot_directory

import_from_depot [depot_user]/pkg/[drivers_interactive_pkg] \
                  [depot_user]/pkg/themed_wm \
                  [depot_user]/raw/qt5_dejavusans \
                  [depot_user]/src/[base_src] \
                  [depot_user]/src/expat \
                  [depot_user]/src/global_keys_handler \
                  [depot_user]/src/init \
                  [depot_user]/src/libdrm \
                  [depot_user]/src/libc \
                  [depot_user]/src/mesa \
                  [depot_user]/src/nitpicker \
                  [depot_user]/src/qt5_base \
                  [depot_user]/src/report_rom \
                  [depot_user]/src/rom_filter \
                  [depot_user]/src/stdcxx \
                  [depot_user]/src/vfs \
                  [depot_user]/src/vfs_gpu \
                  [depot_user]/src/vfs_pipe

#
# Configuration
#

proc qt5_layouter_config { } {
	return {
    + config | rules: rom
      + report | rules: yes
      + rules
        + screen screen
        + assign | label_prefix: | target: screen | xpos: any | ypos: any
      + press | key: BTN_LEFT | action: drag
        + release | key: BTN_LEFT | action: drop}
}

proc qt5_decorator_binary { } { return "decorator" }

proc qt5_decorator_config { } {
	return {
    + config
      . this is an example config for the themed decorator
      . + vfs
      .   + tar plain_decorator_theme.tar
      . + libc
      + default-policy}
}

proc qt5_parent_provides { } {

	set parent_provides { }

	append parent_provides {
    + service ROM
    + service IRQ
    + service IO_MEM
    + service IO_PORT
    + service PD
    + service RM
    + service CPU
    + service LOG}

	return $parent_provides
}

#
# Keyboard layout - this function can be overridden in a run script
#
proc language_chargen { } { return "en_us" }

exec mkdir -p bin
exec cp -f [genode_dir]/repos/os/src/server/event_filter/[language_chargen].chargen bin/
exec cp -f [genode_dir]/repos/os/src/server/event_filter/special.chargen bin/

set qt5_event_filter_config {config
  + output
    + chargen
      + remap
        + include | rom: numlock.remap
        + merge
          + input ps2
          + input usb
          + input sdl
      + mod1
        + key KEY_LEFTSHIFT
        + key KEY_RIGHTSHIFT
      + mod2
        + key KEY_LEFTCTRL
        + key KEY_RIGHTCTRL
      + mod3
        + key KEY_RIGHTALT
        . AltGr
      + mod4
        + rom capslock
      + repeat | delay_ms: 500 | rate_ms: 50 }
append qt5_event_filter_config "
      + include | rom: [language_chargen].chargen"
append qt5_event_filter_config {
      + include | rom: special.chargen
  + policy | label: ps2 | input: ps2
  + policy | label: usb | input: usb
  + policy | label: sdl | input: sdl
-
}

set fd [open bin/qt5_event_filter.config w]
puts $fd $qt5_event_filter_config
close $fd


proc qt5_start_nodes { } {

	set start_nodes { }

	append start_nodes {

  + start timer | ram: 1M
    + provides | + service Timer

  + start numlock_remap_rom | ram: 1M
    .
    . toggle key mappings depending on the numlock state
    .
    + binary rom_filter
    + provides | + service ROM
    + config
      + input numlock_enabled | rom: numlock | node: numlock
        + attribute enabled
      + output | node: remap
        + if
          + has_value | input: numlock_enabled | value: no
          + then
            + inline
              + key KEY_KP0   | to: KEY_INSERT
              + key KEY_KP1   | to: KEY_END
              + key KEY_KP2   | to: KEY_DOWN
              + key KEY_KP3   | to: KEY_PAGEDOWN
              + key KEY_KP4   | to: KEY_LEFT
              + key KEY_KP5   | to: KEY_RESERVED
              + key KEY_KP6   | to: KEY_RIGHT
              + key KEY_KP7   | to: KEY_HOME
              + key KEY_KP8   | to: KEY_UP
              + key KEY_KP9   | to: KEY_PAGEUP
              + key KEY_KPDOT | to: KEY_DELETE
    + route
      + service ROM | label: numlock | + child wm_report_rom
      + any-service | + parent

  + start drivers | caps: 1500 | ram: 64M | managing_system: yes
    + binary init
    + route
      + service ROM     | label: config                        | + parent                  | label: drivers.config
      + service ROM     | label: event_filter.config           | + parent                  | label: qt5_event_filter.config
      + service ROM     | label: event_filter -> capslock      | + child wm_report_rom
      + service ROM     | label: event_filter -> numlock.remap | + child numlock_remap_rom
      + service Timer                                          | + child timer
      + service Capture                                        | + child nitpicker
      + service Event                                          | + child nitpicker
      + any-service                                            | + parent

  + start nitpicker | caps: 200 | ram: 2M
    + provides
      + service Gui
      + service Capture
      + service Event
    + route
      + service Report | + child wm_report_rom
      + any-service
        + parent
        + any-child
    + config
      + capture
      + event

      + report | focus: yes | hover: yes | xray: yes

      + domain pointer | layer: 1 | label: no | content: client | origin: pointer
      + domain default | layer: 2 | label: no | content: client
                       | focus: click | hover: always | width: 1024 | height: 768

      + policy | label_prefix: pointer | domain: pointer
      + default-policy | domain: default

      + global-key KEY_CAPSLOCK | label: global_keys_handler -> input
      + global-key KEY_NUMLOCK  | label: global_keys_handler -> input

  + start pointer | ram: 2M
    + provides | + service Report
    + config | shapes: yes
    + route
      + service Gui                | + child nitpicker
      + service ROM | label: hover | + child wm_report_rom
      + service ROM | label: xray  | + child wm_report_rom
      + any-service
        + parent
        + any-child

  + start wm_report_rom | ram: 4M
    + binary report_rom
    + provides
      + service Report
      + service ROM
    + config | verbose: no
      + policy | label: layouter -> focus_request           | report: wm -> focus_request
      + policy | label: decorator -> pointer                | report: wm -> pointer
      + policy | label: layouter -> window_list             | report: wm -> window_list
      + policy | label: wm -> focus                         | report: layouter -> focus
      + policy | label: wm -> resize_request                | report: layouter -> resize_request
      + policy | label: decorator -> window_layout          | report: layouter -> window_layout
      + policy | label: layouter -> rules                   | report: layout_rules
      + policy | label: layouter -> decorator_margins       | report: decorator -> decorator_margins
      + policy | label: layouter -> hover                   | report: decorator -> hover
      + policy | label: clipboard -> focus                  | report: nitpicker -> focus
      + policy | label: pointer -> hover                    | report: nitpicker -> hover
      + policy | label: pointer -> xray                     | report: nitpicker -> xray
      + policy | label: drivers -> event_filter -> capslock | report: global_keys_handler -> capslock
      + policy | label: numlock_remap_rom -> numlock        | report: global_keys_handler -> numlock
      + policy | label: ps2 -> capslock                     | report: global_keys_handler -> capslock
      + policy | label: ps2 -> numlock                      | report: global_keys_handler -> numlock
      + policy | label: usb -> capslock                     | report: global_keys_handler -> capslock
      + policy | label: usb -> numlock                      | report: global_keys_handler -> numlock

  + start global_keys_handler | ram: 1M
    + config
      + bool capslock | initial: no
      + bool numlock  | initial: no

      + press KEY_CAPSLOCK | bool: capslock | change: toggle
      + press KEY_NUMLOCK  | bool: numlock  | change: toggle

      + report capslock | + bool capslock
      + report numlock  | + bool numlock
    + route
      + service Report | + child wm_report_rom
      + service Gui    | + child nitpicker
      + service Timer  | + child timer
      + any-service
        + parent
        + any-child

  + start wm | caps: 250 | ram: 8M
    + provides
      + service Gui
      + service Report
      + service ROM
    + config
      + policy | label_prefix: decorator | role: decorator
      + policy | label_prefix: layouter  | role: layouter
      + default-policy
    + route
      + service ROM    | label: focus          | + child wm_report_rom
      + service ROM    | label: resize_request | + child wm_report_rom
      + service Report | label_last: shape     | + child pointer
      + service Report                         | + child wm_report_rom
      + any-service
         + child nitpicker
         + parent
         + any-child

  + start layouter | caps: 150 | ram: 4M
    + binary window_layouter
    } [qt5_layouter_config] {
    + route
      + service ROM    | label: window_list       | + child wm_report_rom
      + service ROM    | label: focus_request     | + child wm_report_rom
      + service ROM    | label: hover             | + child wm_report_rom
      + service ROM    | label: decorator_margins | + child wm_report_rom
      + service ROM    | label: rules             | + child wm_report_rom
      + service Report | label: rules             | + child wm_report_rom | label: layout_rules
      + service Report                            | + child wm_report_rom
      + any-service
        + child wm
        + parent
        + any-child

  + start decorator | caps: 300 | ram: 32M
    + binary } [qt5_decorator_binary] {
    } [qt5_decorator_config] {
    + route
      + service ROM    | label: window_layout | + child wm_report_rom
      + service ROM    | label: pointer       | + child wm_report_rom
      + service Report                        | + child wm_report_rom
      + any-service
        + child wm
        + parent
        + any-child
    }

	return $start_nodes
}


proc qt5_boot_modules { } {

	set boot_modules [build_artifacts]

	lappend boot_modules [language_chargen].chargen
	lappend boot_modules special.chargen
	lappend boot_modules qt5_event_filter.config
}
