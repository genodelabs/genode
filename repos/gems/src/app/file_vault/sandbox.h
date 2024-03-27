/*
 * \brief  Local utilities for the sandbox API
 * \author Martin Stein
 * \date   2021-02-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SANDBOX_H_
#define _SANDBOX_H_

/* Genode includes */
#include <util/string.h>
#include <sandbox/sandbox.h>

/* local includes */
#include <input.h>

namespace File_vault {

	template <typename ARG>
	void gen_arg(Xml_generator &xml, ARG const &arg)
	{
		xml.node("arg", [&] () { xml.attribute("value", arg); });
	}

	template <typename FN>
	static inline void gen_named_node(Xml_generator &xml,
	                                  char const *type, char const *name, FN const &fn)
	{
		xml.node(type, [&] () {
			xml.attribute("name", name);
			fn();
		});
	}

	static inline void gen_common_start_content(Xml_generator   &xml,
	                                            char      const *name,
	                                            Cap_quota const  caps,
	                                            Ram_quota const  ram)
	{
		xml.attribute("name", name);
		xml.attribute("caps", caps.value);
		gen_named_node(xml, "resource", "RAM", [&] () {
			xml.attribute("quantum", String<64>(Number_of_bytes(ram.value))); });
	}

	void route_to_child_service(Genode::Xml_generator &xml,
	                            char const            *child_name,
	                            char const            *service_name,
	                            char const            *service_label = "")
	{
		xml.node("service", [&] () {
			xml.attribute("name", service_name);
			if (Genode::strcmp(service_label, "")) {
				xml.attribute("label", service_label);
			}
			xml.attribute("name", service_name);
			xml.node("child", [&] () {
				xml.attribute("name", child_name);
			});
		});
	};

	void route_to_parent_service(Genode::Xml_generator &xml,
	                             char const            *service_name,
	                             char const            *src_label = "",
	                             char const            *dst_label = "")
	{
		xml.node("service", [&] () {
			xml.attribute("name", service_name);
			if (Genode::strcmp(src_label, "")) {
				xml.attribute("label", src_label);
			}
			xml.node("parent", [&] () {
				if (Genode::strcmp(dst_label, "")) {
					xml.attribute("label", dst_label);
				}
			});
		});
	};

	void route_to_local_service(Genode::Xml_generator &xml,
	                            char const            *service_name,
	                            char const            *service_label = "")
	{
		xml.node("service", [&] () {
			xml.attribute("name", service_name);
			if (Genode::strcmp(service_label, "")) {
				xml.attribute("label", service_label);
			}
			xml.node("local", [&] () { });
		});
	};

	void service_node(Genode::Xml_generator &xml,
	                  char const            *service_name)
	{
		xml.node("service", [&] () {
			xml.attribute("name", service_name);
		});
	};

	void gen_provides_service(Xml_generator &xml,
	                          char    const *service_name)
	{
		xml.node("provides", [&] () {
			xml.node("service", [&] () {
				xml.attribute("name", service_name);
			});
		});
	}

	void gen_parent_routes_for_pd_rom_cpu_log(Xml_generator &xml)
	{
		route_to_parent_service(xml, "PD");
		route_to_parent_service(xml, "ROM");
		route_to_parent_service(xml, "CPU");
		route_to_parent_service(xml, "LOG");
	}

	void gen_parent_provides_and_report_nodes(Xml_generator &xml)
	{
		xml.attribute("verbose", "no");

		xml.node("report", [&] () {
			xml.attribute("provided", "yes");
			xml.attribute("child_ram",  "yes");
			xml.attribute("child_caps", "yes");
			xml.attribute("delay_ms", 500);
		});

		xml.node("parent-provides", [&] () {
			service_node(xml, "ROM");
			service_node(xml, "CPU");
			service_node(xml, "PD");
			service_node(xml, "LOG");
			service_node(xml, "RM");
			service_node(xml, "File_system");
			service_node(xml, "Gui");
			service_node(xml, "Timer");
			service_node(xml, "Report");
		});
	}

	void gen_menu_view_start_node(Xml_generator     &xml,
	                              Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.attribute("xpos", "100");
				xml.attribute("ypos", "50");

				xml.node("report", [&] () {
					xml.attribute("hover", "yes"); });

				xml.node("libc", [&] () {
					xml.attribute("stderr", "/dev/log"); });

				xml.node("vfs", [&] () {
					xml.node("tar", [&] () {
						xml.attribute("name", "menu_view_styles.tar"); });
					xml.node("dir", [&] () {
						xml.attribute("name", "dev");
						xml.node("log", [&] () { });
					});
					xml.node("dir", [&] () {
						xml.attribute("name", "fonts");
						xml.node("fs", [&] () {
							xml.attribute("label", "fonts");
						});
					});
				});
			});

			xml.node("route", [&] () {
				route_to_local_service(xml, "ROM", "dialog");
				route_to_local_service(xml, "Report", "hover");
				route_to_local_service(xml, "Gui");
				route_to_parent_service(xml, "File_system", "fonts");
				route_to_parent_service(xml, "Timer");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_mke2fs_start_node(Xml_generator     &xml,
	                           Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.node("libc", [&] () {
					xml.attribute("stdout", "/dev/log");
					xml.attribute("stderr", "/dev/log");
					xml.attribute("stdin",  "/dev/null");
					xml.attribute("rtc",    "/dev/rtc");
				});
				xml.node("vfs", [&] () {
					gen_named_node(xml, "dir", "dev", [&] () {
						gen_named_node(xml, "block", "block", [&] () {
							xml.attribute("label", "default");
							xml.attribute("block_buffer_count", 128);
						});
						gen_named_node(xml, "inline", "rtc", [&] () {
							xml.append("2018-01-01 00:01");
						});
						xml.node("null", [&] () {});
						xml.node("log",  [&] () {});
					});
				});
				gen_arg(xml, "mkfs.ext2");
				gen_arg(xml, "-F");
				gen_arg(xml, "/dev/block");
			});

			xml.node("route", [&] () {
				route_to_child_service(xml, "vfs_block", "Block");
				route_to_parent_service(xml, "Timer");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_resize2fs_start_node(Xml_generator     &xml,
	                              Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.node("libc", [&] () {
					xml.attribute("stdout", "/dev/log");
					xml.attribute("stderr", "/dev/log");
					xml.attribute("stdin",  "/dev/null");
					xml.attribute("rtc",    "/dev/rtc");
				});
				xml.node("vfs", [&] () {
					gen_named_node(xml, "dir", "dev", [&] () {
						gen_named_node(xml, "block", "block", [&] () {
							xml.attribute("label", "default");
							xml.attribute("block_buffer_count", 128);
						});
						gen_named_node(xml, "inline", "rtc", [&] () {
							xml.append("2018-01-01 00:01");
						});
						xml.node("null", [&] () {});
						xml.node("log",  [&] () {});
					});
				});
				gen_arg(xml, "resize2fs");
				gen_arg(xml, "-f");
				gen_arg(xml, "-p");
				gen_arg(xml, "/dev/block");
			});

			xml.node("route", [&] () {
				route_to_child_service(xml, "vfs_block", "Block");
				route_to_parent_service(xml, "Timer");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_tresor_vfs_start_node(Xml_generator     &xml,
	                               Child_state const &child,
	                               File_path   const &tresor_img_file_name)
	{
		child.gen_start_node(xml, [&] () {

			gen_provides_service(xml, "File_system");
			xml.node("config", [&] () {

				xml.node("vfs", [&] () {

					xml.node("fs", [&] () {
						xml.attribute("buffer_size", "1M");
						xml.attribute("label", "tresor_fs");
					});
					xml.node("tresor_crypto_aes_cbc", [&] () {
						xml.attribute("name", "crypto");
					});
					xml.node("dir", [&] () {
						xml.attribute("name", "trust_anchor");

						xml.node("fs", [&] () {
							xml.attribute("buffer_size", "1M");
							xml.attribute("label", "trust_anchor");
						});
					});
					xml.node("dir", [&] () {
						xml.attribute("name", "dev");

						xml.node("tresor", [&] () {
							xml.attribute("name", "tresor");
							xml.attribute("verbose", "no");
							xml.attribute("block", File_path { "/", tresor_img_file_name });
							xml.attribute("crypto", "/crypto");
							xml.attribute("trust_anchor", "/trust_anchor");
						});
					});
				});

				xml.node("policy", [&] () {
					xml.attribute("label", "resizing_fs_tool -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "rekeying_fs_tool -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "lock_fs_tool -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "create_snap_fs_tool -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "discard_snap_fs_tool -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "snapshots_fs_query -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "resizing_fs_query -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "rekeying_fs_query -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "lock_fs_query -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "vfs_block -> ");
					xml.attribute("root", "/dev/tresor/current");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "client_fs_fs_query -> ");
					xml.attribute("root", "/dev/tresor/current");
					xml.attribute("writeable", "no");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "sync_to_tresor_vfs_init -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_child_service(xml, "tresor_trust_anchor_vfs", "File_system", "trust_anchor");
				route_to_parent_service(xml, "File_system", "tresor_fs");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_tresor_trust_anchor_vfs_start_node(Xml_generator     &xml,
	                                            Child_state const &child,
	                                            bool               jent_avail)
	{
		child.gen_start_node(xml, [&] () {

			gen_provides_service(xml, "File_system");
			xml.node("config", [&] () {

				xml.node("vfs", [&] () {

					xml.node("dir", [&] () {
						xml.attribute("name", "storage_dir");

						xml.node("fs", [&] () {
							xml.attribute("buffer_size", "1M");
							xml.attribute("label", "storage_dir");
						});
					});
					xml.node("dir", [&] () {
						xml.attribute("name", "dev");

						xml.node("tresor_trust_anchor", [&] () {
							xml.attribute("name", "tresor_trust_anchor");
							xml.attribute("storage_dir", "/storage_dir");
						});

						if (jent_avail) {
							xml.node("jitterentropy", [&] () {
								xml.attribute("name", "jitterentropy");
							});
						} else {
							xml.node("inline", [&] () {
								xml.attribute("name", "jitterentropy");
								xml.append_content(String<33> { "0123456789abcdefghijklmnopqrstuv" });
							});
							warning("Insecure mode, no entropy source!");
						}
					});
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "tresor_init_trust_anchor -> trust_anchor");
					xml.attribute("root", "/dev/tresor_trust_anchor");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "tresor_init -> trust_anchor");
					xml.attribute("root", "/dev/tresor_trust_anchor");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "tresor_vfs -> trust_anchor");
					xml.attribute("root", "/dev/tresor_trust_anchor");
					xml.attribute("writeable", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_parent_service(xml, "File_system", "storage_dir");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_rump_vfs_start_node(Xml_generator     &xml,
	                             Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			gen_provides_service(xml, "File_system");
			xml.node("config", [&] () {

				xml.node("vfs", [&] () {
					xml.node("rump", [&] () {
						xml.attribute("fs", "ext2fs");
						xml.attribute("ram", "20M");
					});
				});

				xml.node("default-policy", [&] () {
					xml.attribute("root", "/");
					xml.attribute("writeable", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_child_service(xml, "vfs_block", "Block");
				route_to_parent_service(xml, "Timer");
				route_to_parent_service(xml, "RM");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_truncate_file_start_node(Xml_generator     &xml,
	                                  Child_state const &child,
	                                  char        const *path,
	                                  uint64_t           size)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.attribute("size", size);
				xml.attribute("path", path);

				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "tresor");

						xml.node("fs", [&] () {
							xml.attribute("label", "tresor");
						});
					});
				});
			});
			xml.node("route", [&] () {
				route_to_parent_service(xml, "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_sync_to_tresor_vfs_init_start_node(Xml_generator     &xml,
	                                         Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.attribute("ld_verbose", "no");

				xml.node("libc", [&] () {
					xml.attribute("stdin", "/dev/log");
					xml.attribute("stdout", "/dev/log");
					xml.attribute("stderr", "/dev/log");
				});
				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "dev");
						xml.node("log", [&] () { });
					});
					xml.node("dir", [&] () {
						xml.attribute("name", "tresor");

						xml.node("fs",  [&] () {
							xml.attribute("writeable", "yes");
						});
					});
				});
			});
			xml.node("route", [&] () {
				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_tresor_vfs_block_start_node(Xml_generator     &xml,
	                                     Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			gen_provides_service(xml, "Block");
			xml.node("config", [&] () {
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("buffer_size", "1M");
					});
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "mke2fs -> default");
					xml.attribute("block_size", "512");
					xml.attribute("file", "/data");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "resize2fs -> default");
					xml.attribute("block_size", "512");
					xml.attribute("file", "/data");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "rump_vfs -> ");
					xml.attribute("block_size", "512");
					xml.attribute("file", "/data");
					xml.attribute("writeable", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_image_fs_query_start_node(Xml_generator     &xml,
	                                   Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("writeable", "no");
					});
				});
				xml.node("query", [&] () {
					xml.attribute("path", "/");
					xml.attribute("content", "no");
					xml.attribute("size", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_local_service(xml, "Report");
				route_to_parent_service(xml, "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_client_fs_fs_query_start_node(Xml_generator     &xml,
	                                       Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("writeable", "no");
					});
				});
				xml.node("query", [&] () {
					xml.attribute("path", "/");
					xml.attribute("content", "no");
					xml.attribute("size", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_local_service(xml, "Report");
				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_fs_query_start_node(Xml_generator     &xml,
	                             Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("writeable", "yes");
					});
				});
				xml.node("query", [&] () {
					xml.attribute("path", "/file_vault");
					xml.attribute("content", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_local_service(xml, "Report");
				route_to_parent_service(xml, "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_tresor_init_trust_anchor_start_node(Xml_generator &xml,
	                                             Child_state const &child,
	                                             Passphrase const &passphrase)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {

				xml.attribute("passphrase", passphrase);
				xml.attribute("trust_anchor_dir", "/trust_anchor");
				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "trust_anchor");
						xml.node("fs", [&] () {
							xml.attribute("label", "trust_anchor");
						});
					});
				});
			});
			xml.node("route", [&] () {
				route_to_child_service(xml, "tresor_trust_anchor_vfs", "File_system", "trust_anchor");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_tresor_init_start_node(Xml_generator &xml,
	                                Child_state const &child,
	                                Tresor::Superblock_configuration sb_config)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {

				xml.node("trust-anchor", [&] () {
					xml.attribute("path", "/trust_anchor");
				});
				xml.node("block-io", [&] () {
					xml.attribute("type", "vfs");
					xml.attribute("path", "/tresor.img");
				});
				xml.node("crypto", [&] () {
					xml.attribute("path", "/crypto");
				});
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("buffer_size", "1M");
					});
					xml.node("tresor_crypto_aes_cbc", [&] () {
						xml.attribute("name", "crypto");
					});
					xml.node("dir", [&] () {
						xml.attribute("name", "trust_anchor");
						xml.node("fs", [&] () {
							xml.attribute("label", "trust_anchor");
						});
					});
				});
				sb_config.generate_xml(xml);
			});
			xml.node("route", [&] () {
				route_to_child_service(xml, "tresor_trust_anchor_vfs", "File_system", "trust_anchor");
				route_to_parent_service(xml, "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_policy_for_child_service(Xml_generator     &xml,
	                                  char        const *service_name,
	                                  Child_state const &child)
	{
		xml.node("service", [&] () {
			xml.attribute("name", service_name);
			xml.node("default-policy", [&] () {
				xml.node("child", [&] () {
					xml.attribute("name", child.start_name());
				});
			});
		});
	}

	void gen_snapshots_fs_query_start_node(Xml_generator     &xml,
	                                       Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("writeable", "yes");
					});
				});
				xml.node("query", [&] () {
					xml.attribute("path", "/tresor/snapshots");
					xml.attribute("content", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_local_service(xml, "Report");
				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_resizing_fs_tool_start_node(Xml_generator     &xml,
	                                     Child_state const &child,
	                                     char        const *tree,
	                                     unsigned long      nr_of_blocks)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.attribute("exit",    "yes");
				xml.attribute("verbose", "no");

				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "tresor");

						xml.node("fs",  [&] () {
							xml.attribute("writeable", "yes");
						});
					});
				});
				xml.node("new-file", [&] () {
					xml.attribute("path", "/tresor/tresor/control/extend");
					xml.append_content("tree=", tree, ",blocks=", nr_of_blocks);
				});
			});
			xml.node("route", [&] () {

				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_resizing_fs_query_start_node(Xml_generator     &xml,
	                                      Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("writeable", "yes");
					});
				});
				xml.node("query", [&] () {
					xml.attribute("path", "/tresor/control");
					xml.attribute("content", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_local_service(xml, "Report");
				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_lock_fs_tool_start_node(Xml_generator     &xml,
	                                      Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.attribute("exit",    "yes");
				xml.attribute("verbose", "no");

				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "tresor");

						xml.node("fs",  [&] () {
							xml.attribute("writeable", "yes");
						});
					});
				});
				xml.node("new-file", [&] () {
					xml.attribute("path", "/tresor/tresor/control/deinitialize");
					xml.append_content("true");
				});
			});
			xml.node("route", [&] () {

				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_rekeying_fs_tool_start_node(Xml_generator     &xml,
	                                     Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.attribute("exit",    "yes");
				xml.attribute("verbose", "no");

				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "tresor");

						xml.node("fs",  [&] () {
							xml.attribute("writeable", "yes");
						});
					});
				});
				xml.node("new-file", [&] () {
					xml.attribute("path", "/tresor/tresor/control/rekey");
					xml.append_content("true");
				});
			});
			xml.node("route", [&] () {

				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_lock_fs_query_start_node(Xml_generator     &xml,
	                                  Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("writeable", "yes");
					});
				});
				xml.node("query", [&] () {
					xml.attribute("path", "/tresor/control");
					xml.attribute("content", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_local_service(xml, "Report");
				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_rekeying_fs_query_start_node(Xml_generator     &xml,
	                                      Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("writeable", "yes");
					});
				});
				xml.node("query", [&] () {
					xml.attribute("path", "/tresor/control");
					xml.attribute("content", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_local_service(xml, "Report");
				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_create_snap_fs_tool_start_node(Xml_generator     &xml,
	                                        Child_state const &child)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.attribute("exit",    "yes");
				xml.attribute("verbose", "no");

				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "tresor");

						xml.node("fs",  [&] () {
							xml.attribute("writeable", "yes");
						});
					});
				});
				xml.node("new-file", [&] () {
					xml.attribute("path", "/tresor/tresor/control/create_snapshot");
					xml.append_content("true");
				});
			});
			xml.node("route", [&] () {

				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}

	void gen_discard_snap_fs_tool_start_node(Xml_generator     &xml,
	                                         Child_state const &child,
	                                         Generation        generation)
	{
		child.gen_start_node(xml, [&] () {

			xml.node("config", [&] () {
				xml.attribute("exit",    "yes");
				xml.attribute("verbose", "no");

				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "tresor");

						xml.node("fs",  [&] () {
							xml.attribute("writeable", "yes");
						});
					});
				});
				xml.node("new-file", [&] () {
					xml.attribute("path", "/tresor/tresor/control/discard_snapshot");
					xml.append_content(Generation_string(generation));
				});
			});
			xml.node("route", [&] () {

				route_to_child_service(xml, "tresor_vfs", "File_system");
				gen_parent_routes_for_pd_rom_cpu_log(xml);
			});
		});
	}
}


#endif /*  SANDBOX_H_ */
