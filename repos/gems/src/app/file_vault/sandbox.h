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

namespace File_vault {

	void gen_arg(Generator &g, auto const &arg)
	{
		g.node("arg", [&] { g.attribute("value", arg); });
	}

	void gen_common_start_content(Generator &g, char const *name, Cap_quota caps, Ram_quota ram)
	{
		g.attribute("name", name);
		g.attribute("caps", caps.value);
		gen_named_node(g, "resource", "RAM", [&] {
			g.attribute("quantum", String<64>(Number_of_bytes(ram.value))); });
	}

	void gen_route(Generator &g, char const *service, char const *label, auto const &fn)
	{
		gen_named_node(g, "service", service, [&] {
			if (strcmp(label, ""))
				g.attribute("label", label);
			fn(); });
	}

	void gen_child_route(Generator &g, char const *child, char const *service, char const *label = "")
	{
		gen_route(g, service, label, [&] { gen_named_node(g, "child", child, [] { }); });
	}

	void gen_parent_route(Generator &g, char const *service, char const *src_label = "", char const *dst_label = "")
	{
		gen_route(g, service, src_label, [&] {
			g.node("parent", [&] {
				if (strcmp(dst_label, ""))
					g.attribute("label", dst_label); }); });
	};

	void gen_local_route(Generator &g, char const *service, char const *label = "")
	{
		gen_route(g, service, label, [&] { g.node("local", [] { }); });
	};

	void gen_service(Generator &g, char const *service)
	{
		gen_named_node(g, "service", service, [] { });
	};

	void gen_provides(Generator &g, char const *service)
	{
		g.node("provides", [&] { gen_service(g, service); });
	}

	void gen_common_routes(Generator &g)
	{
		gen_parent_route(g, "PD");
		gen_parent_route(g, "ROM");
		gen_parent_route(g, "CPU");
		gen_parent_route(g, "LOG");
	}

	void gen_vfs_policy(Generator &g, char const *label_prefix,
	                    char const *root, bool writeable)
	{
		g.node("policy", [&] {
			g.attribute("label_prefix", label_prefix);
			g.attribute("root", root);
			g.attribute("writeable", writeable ? "yes" : "no");
		});
	};

	void gen_parent_provides_and_report_nodes(Generator &g)
	{
		g.node("report", [&] {
			g.attribute("provided", "yes");
			g.attribute("child_ram",  "yes");
			g.attribute("child_caps", "yes");
			g.attribute("delay_ms", 500);
		});
		g.node("parent-provides", [&] {
			gen_service(g, "ROM");
			gen_service(g, "CPU");
			gen_service(g, "PD");
			gen_service(g, "LOG");
			gen_service(g, "RM");
			gen_service(g, "File_system");
			gen_service(g, "Gui");
			gen_service(g, "Timer");
			gen_service(g, "Report");
		});
	}

	void gen_mke2fs_start_node(Generator &g, Child_state const &child)
	{
		child.gen_start_node(g, [&] {
			g.node("config", [&] {
				g.node("libc", [&] {
					g.attribute("stdout", "/dev/log");
					g.attribute("stderr", "/dev/log");
					g.attribute("stdin", "/dev/null");
					g.attribute("rtc", "/dev/rtc");
				});
				g.node("vfs", [&] {
					gen_named_node(g, "dir", "dev", [&] {
						gen_named_node(g, "block", "block", [&] {
							g.attribute("label", "default");
						});
						gen_named_node(g, "inline", "rtc", [&] {
							g.append_quoted("2018-01-01 00:01");
						});
						g.node("null", [&] {});
						g.node("log", [&] {});
					});
				});
				gen_arg(g, "mkfs.ext2");
				gen_arg(g, "-F");
				gen_arg(g, "/dev/block");
			});
			g.node("route", [&] {
				gen_child_route(g, "vfs_block", "Block");
				gen_parent_route(g, "Timer");
				gen_common_routes(g);
			});
		});
	}

	void gen_e2fsck_start_node(Generator &g, Child_state const &child)
	{
		child.gen_start_node(g, [&] {
			g.node("config", [&] {
				g.node("libc", [&] {
					g.attribute("stdout", "/dev/log");
					g.attribute("stderr", "/dev/log");
					g.attribute("stdin", "/dev/null");
					g.attribute("rtc", "/dev/rtc");
				});
				g.node("vfs", [&] {
					gen_named_node(g, "dir", "dev", [&] {
						gen_named_node(g, "block", "block", [&] {
							g.attribute("label", "default");
						});
						gen_named_node(g, "inline", "rtc", [&] {
							g.append_quoted("2018-01-01 00:01");
						});
						g.node("null", [&] {});
						g.node("log", [&] {});
					});
				});
				gen_arg(g, "fsck.ext2");
				gen_arg(g, "-yv");
				gen_arg(g, "/dev/block");
			});
			g.node("route", [&] {
				gen_child_route(g, "vfs_block", "Block");
				gen_parent_route(g, "Timer");
				gen_common_routes(g);
			});
		});
	}

	void gen_resize2fs_start_node(Generator &g, Child_state const &child)
	{
		child.gen_start_node(g, [&] {
			g.node("config", [&] {
				g.node("libc", [&] {
					g.attribute("stdout", "/dev/log");
					g.attribute("stderr", "/dev/log");
					g.attribute("stdin", "/dev/null");
					g.attribute("rtc", "/dev/rtc");
				});
				g.node("vfs", [&] {
					gen_named_node(g, "dir", "dev", [&] {
						gen_named_node(g, "block", "block", [&] {
							g.attribute("label", "default");
						});
						gen_named_node(g, "inline", "rtc", [&] {
							g.append_quoted("2018-01-01 00:01"); });
						g.node("null", [&] {});
						g.node("log", [&] {});
					});
				});
				gen_arg(g, "resize2fs");
				gen_arg(g, "-f");
				gen_arg(g, "-p");
				gen_arg(g, "/dev/block");
			});

			g.node("route", [&] {
				gen_child_route(g, "vfs_block", "Block");
				gen_parent_route(g, "Timer");
				gen_common_routes(g);
			});
		});
	}

	void gen_tresor_vfs_start_node(Generator &g, Child_state const &child, File_path const &image)
	{
		child.gen_start_node(g, [&] {
			gen_provides(g, "File_system");
			g.node("config", [&] {
				g.node("vfs", [&] {
					g.node("fs", [&] {
						g.attribute("buffer_size", "1M");
						g.attribute("label", "tresor_fs -> /");
					});
					gen_named_node(g, "tresor_crypto_aes_cbc", "crypto", [] { });
					gen_named_node(g, "dir", "trust_anchor", [&] {
						g.node("fs", [&] {
							g.attribute("buffer_size", "1M");
							g.attribute("label", "trust_anchor -> /");
						});
					});
					gen_named_node(g, "dir", "dev", [&] {
						gen_named_node(g, "tresor", "tresor", [&] {
							g.attribute("block", File_path("/", image));
							g.attribute("crypto", "/crypto");
							g.attribute("trust_anchor", "/trust_anchor");
						});
					});
				});
				gen_vfs_policy(g, "extend_fs_tool ->", "/dev", true);
				gen_vfs_policy(g, "rekey_fs_tool ->", "/dev", true);
				gen_vfs_policy(g, "lock_fs_tool ->", "/dev", true);
				gen_vfs_policy(g, "extend_fs_query ->", "/dev", true);
				gen_vfs_policy(g, "rekey_fs_query ->", "/dev", true);
				gen_vfs_policy(g, "lock_fs_query ->", "/dev", true);
				gen_vfs_policy(g, "vfs_block ->", "/dev/tresor/current", true);
				gen_vfs_policy(g, "client_fs_query ->", "/dev/tresor/current", false);
				gen_vfs_policy(g, "sync_to_tresor_vfs_init ->", "/dev", true);
			});
			g.node("route", [&] {
				gen_child_route(g, "tresor_trust_anchor_vfs", "File_system", "trust_anchor -> /");
				gen_parent_route(g, "File_system", "tresor_fs -> /");
				gen_common_routes(g);
			});
		});
	}

	void gen_tresor_trust_anchor_vfs_start_node(Generator &g, Child_state const &child,
	                                            bool jent_avail)
	{
		child.gen_start_node(g, [&] {
			gen_provides(g, "File_system");
			g.node("config", [&] {
				g.node("vfs", [&] {
					gen_named_node(g, "dir", "storage_dir", [&] {
						g.node("fs", [&] {
							g.attribute("buffer_size", "1M");
							g.attribute("label", "storage_dir -> /");
						});
					});
					gen_named_node(g, "dir", "dev", [&] {
						gen_named_node(g, "tresor_trust_anchor", "tresor_trust_anchor", [&] {
							g.attribute("storage_dir", "/storage_dir"); });
						if (jent_avail)
							gen_named_node(g, "jitterentropy", "jitterentropy", [&] { });
						else {
							warning("Insecure mode, no entropy source!");
							gen_named_node(g, "inline", "jitterentropy", [&] {
								g.append_quoted(String<33> { "0123456789abcdefghijklmnopqrstuv" }.string()); });
						}
					});
				});
				gen_vfs_policy(g, "tresor_init_trust_anchor -> trust_anchor", "/dev/tresor_trust_anchor", true);
				gen_vfs_policy(g, "tresor_init -> trust_anchor", "/dev/tresor_trust_anchor", true);
				gen_vfs_policy(g, "tresor_vfs -> trust_anchor", "/dev/tresor_trust_anchor", true);
			});
			g.node("route", [&] {
				gen_parent_route(g, "File_system", "storage_dir -> /");
				gen_common_routes(g);
			});
		});
	}

	void gen_rump_vfs_start_node(Generator &g, Child_state const &child)
	{
		child.gen_start_node(g, [&] {
			gen_provides(g, "File_system");
			g.node("config", [&] {
				g.node("vfs", [&] {
					g.node("rump", [&] {
						g.attribute("fs", "ext2fs");
						g.attribute("ram", "20M");
					});
				});
				g.node("default-policy", [&] {
					g.attribute("root", "/");
					g.attribute("writeable", "yes");
				});
			});
			g.node("route", [&] {
				gen_child_route(g, "vfs_block", "Block");
				gen_parent_route(g, "Timer");
				gen_parent_route(g, "RM");
				gen_common_routes(g);
			});
		});
	}

	void gen_truncate_file_start_node(Generator &g, Child_state const &child,
	                                  char const *path, uint64_t size)
	{
		child.gen_start_node(g, [&] {
			g.node("config", [&] {
				g.attribute("size", size);
				g.attribute("path", path);
				g.node("vfs", [&] {
					gen_named_node(g, "dir", "tresor", [&] {
						g.node("fs", [&] { g.attribute("label", "tresor -> /"); }); }); });
			});
			g.node("route", [&] {
				gen_parent_route(g, "File_system");
				gen_common_routes(g);
			});
		});
	}

	void gen_sync_to_tresor_vfs_init_start_node(Generator &g, Child_state const &child)
	{
		child.gen_start_node(g, [&] {
			g.node("config", [&] {
				g.node("libc", [&] {
					g.attribute("stdin", "/dev/log");
					g.attribute("stdout", "/dev/log");
					g.attribute("stderr", "/dev/log");
				});
				g.node("vfs", [&] {
					gen_named_node(g, "dir", "dev", [&] {
						g.node("log", [] { }); });
					gen_named_node(g, "dir", "tresor", [&] {
						g.node("fs", [&] { g.attribute("writeable", "yes"); }); });
				});
			});
			g.node("route", [&] {
				gen_child_route(g, "tresor_vfs", "File_system");
				gen_common_routes(g);
			});
		});
	}

	void gen_tresor_vfs_block_start_node(Generator &g, Child_state const &child)
	{
		auto gen_policy = [&] (char const *label) {
			g.node("policy", [&] {
				g.attribute("label", label);
				g.attribute("block_size", "512");
				g.attribute("file", "/data");
				g.attribute("writeable", "yes");
			});
		};
		child.gen_start_node(g, [&] {
			gen_provides(g, "Block");
			g.node("config", [&] {
				g.node("vfs", [&] {
					g.node("fs", [&] { g.attribute("buffer_size", "1M"); }); });
				gen_policy("mke2fs -> default");
				gen_policy("e2fsck -> default");
				gen_policy("resize2fs -> default");
				gen_policy("rump_vfs ->");
			});
			g.node("route", [&] {
				gen_child_route(g, "tresor_vfs", "File_system");
				gen_common_routes(g);
			});
		});
	}

	void gen_tresor_init_trust_anchor_start_node(Generator &g, Child_state const &child,
	                                             Passphrase const &passphrase)
	{
		child.gen_start_node(g, [&] {
			g.node("config", [&] {
				g.attribute("passphrase", passphrase);
				g.attribute("trust_anchor_dir", "/trust_anchor");
				g.node("vfs", [&] {
					gen_named_node(g, "dir", "trust_anchor", [&] {
						g.node("fs", [&] { g.attribute("label", "trust_anchor -> /"); }); }); });
			});
			g.node("route", [&] {
				gen_child_route(g, "tresor_trust_anchor_vfs", "File_system", "trust_anchor -> /");
				gen_common_routes(g);
			});
		});
	}

	void gen_tresor_init_start_node(Generator &g, Child_state const &child,
	                                Tresor::Superblock_configuration sb_config)
	{
		child.gen_start_node(g, [&] {
			g.node("config", [&] {
				g.node("trust-anchor", [&] { g.attribute("path", "/trust_anchor"); });
				g.node("block-io", [&] {
					g.attribute("type", "vfs");
					g.attribute("path", "/tresor.img");
				});
				g.node("crypto", [&] { g.attribute("path", "/crypto"); });
				g.node("vfs", [&] {
					g.node("fs", [&] { g.attribute("buffer_size", "1M"); });
					gen_named_node(g, "tresor_crypto_aes_cbc", "crypto", [] { });
					gen_named_node(g, "dir", "trust_anchor", [&] {
						g.node("fs", [&] { g.attribute("label", "trust_anchor -> /"); }); });
				});
				sb_config.generate(g);
			});
			g.node("route", [&] {
				gen_child_route(g, "tresor_trust_anchor_vfs", "File_system", "trust_anchor -> /");
				gen_parent_route(g, "File_system");
				gen_common_routes(g);
			});
		});
	}

	void gen_child_service_policy(Generator &g, char const *service, Child_state const &child)
	{
		gen_named_node(g, "service", service, [&] {
			g.node("default-policy", [&] {
				gen_named_node(g, "child", child.start_name(), [&] { }); }); });
	}

	void gen_fs_tool_start_node(Generator &g, Child_state const &child, char const *server,
	                            char const *path, auto const &content)
	{
		child.gen_start_node(g, [&] {
			g.node("config", [&] {
				g.attribute("exit", "yes");
				g.node("vfs", [&] {
					gen_named_node(g, "dir", "root", [&] {
						g.node("fs", [&] { g.attribute("writeable", "yes"); }); }); });
				g.node("new-file", [&] {
					g.attribute("path", String<64>("/root", path));
					g.append_quoted(content);
				});
			});
			g.node("route", [&] {
				gen_child_route(g, server, "File_system");
				gen_common_routes(g);
			});
		});
	}

	void gen_fs_query_start_node(Generator &g, Child_state const &child, char const *server,
	                             char const *path, bool content)
	{
		child.gen_start_node(g, [&] {
			g.node("config", [&] {
				g.node("vfs", [&] {
					g.node("fs", [&] { g.attribute("writeable", "no"); }); });
				g.node("query", [&] {
					g.attribute("path", path);
					g.attribute("size", "yes");
					g.attribute("content", content ? "yes" : "no");
				});
			});
			g.node("route", [&] {
				gen_local_route(g, "Report");
				if (!strcmp(server, "parent"))
					gen_parent_route(g, "File_system");
				else
					gen_child_route(g, server, "File_system");
				gen_common_routes(g);
			});
		});
	}

	void gen_extend_fs_tool_start_node(Generator &g, Child_state const &child,
	                                   char const *tree, Number_of_blocks num_blocks)
	{
		gen_fs_tool_start_node(
			g, child, "tresor_vfs", "/tresor/control/extend",
			String<64>("tree=", tree, ",blocks=", num_blocks).string());
	}

	void gen_lock_fs_tool_start_node(Generator &g, Child_state const &child) {
		gen_fs_tool_start_node(g, child, "tresor_vfs", "/tresor/control/deinitialize", "true"); }

	void gen_rekey_fs_tool_start_node(Generator &g, Child_state const &child) {
		gen_fs_tool_start_node(g, child, "tresor_vfs", "/tresor/control/rekey", "true"); }

	void gen_image_fs_query_start_node(Generator &g, Child_state const &child) {
		gen_fs_query_start_node(g, child, "parent", "/", false); }

	void gen_client_fs_query_start_node(Generator &g, Child_state const &child) {
		gen_fs_query_start_node(g, child, "tresor_vfs", "/", false); }

	void gen_extend_fs_query_start_node(Generator &g, Child_state const &child) {
		gen_fs_query_start_node(g, child, "tresor_vfs", "/tresor/control", true); }

	void gen_lock_fs_query_start_node(Generator &g, Child_state const &child) {
		gen_fs_query_start_node(g, child, "tresor_vfs", "/tresor/control", true); }

	void gen_rekey_fs_query_start_node(Generator &g, Child_state const &child) {
		gen_fs_query_start_node(g, child, "tresor_vfs", "/tresor/control", true); }
}

#endif /*  SANDBOX_H_ */
