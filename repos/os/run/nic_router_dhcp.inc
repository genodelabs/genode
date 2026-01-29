#
# See os/src/test/nic_router_dhcp/README for a documentation.
#

create_boot_directory

import_from_depot [depot_user]/src/[base_src]

set build_components {

	init
	server/dynamic_rom
	test/nic_router_dhcp/client
	server/nic_router
}

lappend_if [nic_router_2_managed] build_components test/nic_router_dhcp/manager
lappend_if [nic_router_2_managed] build_components server/report_rom

build $build_components

append config {

config
+ parent-provides
  + service ROM
  + service IRQ
  + service IO_MEM
  + service IO_PORT
  + service PD
  + service RM
  + service CPU
  + service LOG

+ default-route
  + any-service
    + parent
    + any-child

+ default | caps: 200 | ram: 1M

+ start timer
  + provides | + service Timer

+ start dynamic_rom | ram: 4M
  + provides | + service ROM
  + config | verbose: yes
    + rom nic_router_1.config
      + inline
      | + config
      |   + policy | label: nic_router_2 ->  | domain: downlink
      |   + domain downlink | interface: 10.2.3.1/24
      |     + dhcp-server | ip_first: 10.2.3.2 | ip_last: 10.2.3.2
      |       + dns-server | ip: 1.2.3.4
      |       + dns-server | ip: 2.3.4.5
      |       + dns-server | ip: 3.4.5.6
      |       + dns-domain genode.org
      + sleep | milliseconds: 4000
      + inline
      | + config
      |   + policy | label: nic_router_2 ->  | domain: downlink
      |   + domain downlink | interface: 10.2.3.1/24
      |     + dhcp-server | ip_first: 10.2.3.2 | ip_last: 10.2.3.2
      |       + dns-server | ip: 4.5.6.7
      |       + dns-server | ip: 5.6.7.8
      + sleep | milliseconds: 4000
      + inline
      | + config
      |   + policy | label: nic_router_2 ->  | domain: downlink
      |   + domain downlink | interface: 10.2.4.1/24
      |     + dhcp-server | ip_first: 10.2.4.2 | ip_last: 10.2.4.2
      |       + dns-server | ip: 6.7.8.9
      |       + dns-domain this_is_a_way_to_long_dns_domain_name_that_should_be_rejected_by_the_router_and_cause_the_corresponding_dhcp_option_to_disappear_but_you_never_know_and_so_we_better_test_it................................................................................Xooooo
      + sleep | milliseconds: 2000
      + inline
      | + config
      |   + policy | label: nic_router_2 ->  | domain: downlink
      |   + domain downlink | interface: 10.2.4.1/24
      |     + dhcp-server | ip_first: 10.2.4.200 | ip_last: 10.2.4.200
      |       + dns-domain genodians.org
      + sleep | milliseconds: 2000
      + inline
      | + config
      |   + policy | label: nic_router_2 ->  | domain: downlink
      |   + domain downlink | interface: 10.2.3.1/24
      |     + dhcp-server | ip_first: 10.2.3.2 | ip_last: 10.2.3.2
      |       + dns-domain genodians.org
      + sleep | milliseconds: 2000

+ start nic_router_1 | ram: 10M
  + binary nic_router
  + provides
    + service Nic
    + service Uplink
  + route
    + service ROM | label: config | + child dynamic_rom | label: nic_router_1.config
    + any-service
      + parent
      + any-child

+ start nic_router_2 | ram: 10M
  + binary nic_router
  + provides
    + service Nic
    + service Uplink
}

append_if [expr ![nic_router_2_managed]] config {
  + config | verbose_packets: no
    + policy | label: test_client ->  | domain: downlink
    + nic-client | domain: uplink
    + domain uplink
    + domain downlink | interface: 10.0.3.1/24
      + dhcp-server
                    ip_first:          10.0.3.2
                    ip_last:           10.0.3.2
                    ip_lease_time_sec: 6
                    dns_config_from:   uplink}

append config {
  + route
    + service Nic | + child nic_router_1
}

append_if [nic_router_2_managed] config {
    + service ROM | label: config | + child report_rom
}

append config {
    + any-service
      + parent
      + any-child

+ start test_client | ram: 10M
  + binary test-nic_router_dhcp-client
  + config | verbose: no
  + route
    + service Nic | + child nic_router_2
    + any-service
      + parent
      + any-child
}

append_if [nic_router_2_managed] config {

+ start report_rom
  + provides
    + service Report
    + service ROM
  + config | verbose: no
    + policy | label:  test_manager -> router_state | report: nic_router_2 -> state
    + policy | label:  nic_router_2 -> config       | report: test_manager -> router_config
+ start test_manager
  + binary test-nic_router_dhcp-manager
  + route
    + service ROM | label: router_state | + child report_rom
    + service Report                    | + child report_rom
    + any-service
      + parent
      + any-child
}

append config {
-
}

install_config $config

build_boot_image [build_artifacts]

append qemu_args " -nographic "
append_qemu_nic_args

if {[nic_router_2_managed]} {

append done_string ".*Event 1, DHCP request completed:.*\n"
append done_string ".*  IP lease time: 3600 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 1.2.3.4.*\n"
append done_string ".*  DNS server #2: 2.3.4.5.*\n"
append done_string ".*  DNS server #3: 3.4.5.6.*\n"
append done_string ".*  DNS domain name: genode.org.*\n"
append done_string ".*Event 2, DHCP request completed:.*\n"
append done_string ".*  IP lease time: 3600 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 4.5.6.7.*\n"
append done_string ".*  DNS server #2: 5.6.7.8.*\n"
append done_string ".*Event 3, DHCP request completed:.*\n"
append done_string ".*  IP lease time: 3600 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 6.7.8.9.*\n"
append done_string ".*Event 4, DHCP request completed:.*\n"
append done_string ".*  IP lease time: 3600 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS domain name: genodians.org.*\n"
append done_string ".*Event 5, DHCP request completed:.*\n"
append done_string ".*  IP lease time: 3600 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 1.2.3.4.*\n"
append done_string ".*  DNS server #2: 2.3.4.5.*\n"
append done_string ".*  DNS server #3: 3.4.5.6.*\n"
append done_string ".*  DNS domain name: genode.org.*\n"

} else {

append done_string ".*Event 1, DHCP request completed:.*\n"
append done_string ".*  IP lease time: 6 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 1.2.3.4.*\n"
append done_string ".*  DNS server #2: 2.3.4.5.*\n"
append done_string ".*  DNS server #3: 3.4.5.6.*\n"
append done_string ".*  DNS domain name: genode.org.*\n"
append done_string ".*Event 2, DHCP request completed:.*\n"
append done_string ".*  IP lease time: 6 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 4.5.6.7.*\n"
append done_string ".*  DNS server #2: 5.6.7.8.*\n"
append done_string ".*Event 3, DHCP renew completed:.*\n"
append done_string ".*  IP lease time: 6 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 4.5.6.7.*\n"
append done_string ".*  DNS server #2: 5.6.7.8.*\n"
append done_string ".*Event 4, DHCP request completed:.*\n"
append done_string ".*  IP lease time: 6 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 6.7.8.9.*\n"
append done_string ".*Event 5, DHCP request completed:.*\n"
append done_string ".*  IP lease time: 6 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS domain name: genodians.org.*\n"
append done_string ".*Event 6, DHCP request completed:.*\n"
append done_string ".*  IP lease time: 6 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS domain name: genodians.org.*\n"
append done_string ".*Event 7, DHCP request completed:.*\n"
append done_string ".*  IP lease time: 6 seconds.*\n"
append done_string ".*  Interface: 10.0.3.2/24.*\n"
append done_string ".*  Router: 10.0.3.1.*\n"
append done_string ".*  DNS server #1: 1.2.3.4.*\n"
append done_string ".*  DNS server #2: 2.3.4.5.*\n"
append done_string ".*  DNS server #3: 3.4.5.6.*\n"
append done_string ".*  DNS domain name: genode.org.*\n"

}

run_genode_until $done_string 60
