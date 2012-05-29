LIBCRYPTO     = libcrypto-1.0.0
LIBCRYPTO_DIR = $(REP_DIR)/contrib/openssl-1.0.1b/crypto

#
# ARM is not supported currently (needs testing)
#
REQUIRES = x86

SHARED_LIB = yes

LIBS += libc

CC_OPT += -DDSO_DLFCN -DHAVE_DLFCN_H -Wa,--noexecstack -DL_ENDIAN -DTERMIOS

#
# crypto base source
#
# Removed 'mem_clr.c' from the list because this function is provided by
# 'src/lib/openssl/x86_32/cpuid.s'.
#
SRC_C = cryptlib.c mem.c mem_dbg.c cversion.c ex_data.c cpt_err.c \
        ebcdic.c uid.c o_time.c o_str.c o_dir.c o_fips.c o_init.c fips_ers.c

#
# Excluded 'aes_x86core.c'
#
SRC_C_aes      = aes_core.c aes_misc.c aes_ecb.c aes_cbc.c aes_cfb.c \
                 aes_ofb.c aes_ctr.c aes_ige.c aes_wrap.c
SRC_C_asn1     = a_object.c a_bitstr.c a_utctm.c a_gentm.c a_time.c \
                 a_int.c a_octet.c a_print.c a_type.c a_set.c a_dup.c \
                 a_d2i_fp.c a_i2d_fp.c a_enum.c a_utf8.c a_sign.c \
                 a_digest.c a_verify.c a_mbstr.c a_strex.c x_algor.c \
                 x_val.c x_pubkey.c x_sig.c x_req.c x_attrib.c \
                 x_bignum.c x_long.c x_name.c x_x509.c x_x509a.c \
                 x_crl.c x_info.c x_spki.c nsseq.c x_nx509.c d2i_pu.c \
                 d2i_pr.c i2d_pu.c i2d_pr.c t_req.c t_x509.c \
                 t_x509a.c t_crl.c t_pkey.c t_spki.c t_bitst.c \
                 tasn_new.c tasn_fre.c tasn_enc.c tasn_dec.c \
                 tasn_utl.c tasn_typ.c tasn_prn.c ameth_lib.c f_int.c \
                 f_string.c n_pkey.c f_enum.c x_pkey.c a_bool.c \
                 x_exten.c bio_asn1.c bio_ndef.c asn_mime.c \
                 asn1_gen.c asn1_par.c asn1_lib.c asn1_err.c \
                 a_bytes.c a_strnid.c evp_asn1.c asn_pack.c p5_pbe.c \
                 p5_pbev2.c p8_pkey.c asn_moid.c)
SRC_C_bf       = bf_skey.c bf_ecb.c bf_enc.c bf_cfb64.c bf_ofb64.c bf_enc.c
SRC_C_bio      = bio_lib.c bio_cb.c bio_err.c \
                 bss_mem.c bss_null.c bss_fd.c \
                 bss_file.c bss_sock.c bss_conn.c \
                 bf_null.c bf_buff.c b_print.c b_dump.c \
                 b_sock.c bss_acpt.c bf_nbio.c bss_log.c bss_bio.c \
                 bss_dgram.c
SRC_C_bn       = bn_add.c bn_div.c bn_exp.c bn_lib.c bn_ctx.c bn_mul.c \
                 bn_mod.c bn_print.c bn_rand.c bn_shift.c bn_word.c \
                 bn_blind.c bn_kron.c bn_sqrt.c bn_gcd.c bn_prime.c \
                 bn_err.c bn_sqr.c bn_asm.c bn_recp.c bn_mont.c \
                 bn_mpi.c bn_exp2.c bn_gf2m.c bn_nist.c bn_depr.c \
                 bn_const.c bn_x931p.c bn_asm.c
SRC_C_buffer   = buffer.c buf_err.c buf_str.c

#
# camellia (x86 only?)
#
SRC_C_camellia = camellia.c cmll_misc.c cmll_ecb.c cmll_cbc.c \
                 cmll_ofb.c cmll_cfb.c cmll_ctr.c cmll_utl.c
SRC_C_cast     = c_skey.c c_ecb.c c_enc.c c_cfb64.c c_ofb64.c
SRC_C_cmac     = cm_ameth.c cm_pmeth.c cmac.c
SRC_C_cms      = cms_lib.c cms_asn1.c cms_att.c cms_io.c cms_smime.c \
                 cms_err.c cms_sd.c cms_dd.c cms_cd.c cms_env.c \
                 cms_enc.c cms_ess.c cms_pwri.c
SRC_C_comp     = comp_lib.c comp_err.c c_rle.c c_zlib.c
SRC_C_conf     = conf_err.c conf_lib.c conf_api.c conf_def.c conf_mod.c \
                 conf_mall.c conf_sap.c
SRC_C_des      = cbc_cksm.c cbc_enc.c  cfb64enc.c cfb_enc.c \
                 ecb3_enc.c ecb_enc.c  enc_read.c enc_writ.c fcrypt.c \
                 ofb64enc.c ofb_enc.c pcbc_enc.c qud_cksm.c rand_key.c \
                 rpc_enc.c  set_key.c  des_enc.c fcrypt_b.c xcbc_enc.c \
                 str2key.c  cfb64ede.c ofb64ede.c ede_cbcm_enc.c \
                 des_old.c des_old2.c read2pwd.c
SRC_C_dh       = dh_asn1.c dh_gen.c dh_key.c dh_lib.c dh_check.c \
                 dh_err.c dh_depr.c dh_ameth.c dh_pmeth.c dh_prn.c
SRC_C_dsa      = dsa_gen.c dsa_key.c dsa_lib.c dsa_asn1.c dsa_vrf.c \
                 dsa_sign.c dsa_err.c dsa_ossl.c dsa_depr.c \
                 dsa_ameth.c dsa_pmeth.c dsa_prn.c
SRC_C_dso      = dso_dl.c dso_dlfcn.c dso_err.c dso_lib.c dso_null.c \
                 dso_openssl.c
SRC_C_ec       = ec_lib.c ecp_smpl.c ecp_mont.c ecp_nist.c ec_cvt.c \
                 ec_mult.c ec_err.c ec_curve.c ec_check.c ec_print.c \
                 ec_asn1.c ec_key.c ec2_smpl.c ec2_mult.c ec_ameth.c \
                 ec_pmeth.c eck_prn.c ecp_nistp224.c ecp_nistp256.c \
                 ecp_nistp521.c ecp_nistputil.c ecp_oct.c ec2_oct.c \
                 ec_oct.c
SRC_C_ecdh     = ech_lib.c ech_ossl.c ech_key.c ech_err.c
SRC_C_ecdsa    = ecs_lib.c ecs_asn1.c ecs_ossl.c ecs_sign.c \
                 ecs_vrf.c ecs_err.c
SRC_C_engine   = eng_err.c eng_lib.c eng_list.c eng_init.c \
                 eng_ctrl.c eng_table.c eng_pkey.c eng_fat.c \
                 eng_all.c tb_rsa.c tb_dsa.c tb_ecdsa.c tb_dh.c \
                 tb_ecdh.c tb_rand.c tb_store.c tb_cipher.c \
                 tb_digest.c tb_pkmeth.c tb_asnmth.c eng_openssl.c \
                 eng_cnf.c eng_dyn.c eng_cryptodev.c eng_rsax.c \
                 eng_rdrand.c
SRC_C_err      = err.c err_all.c err_prn.c

#
# evp (e_camellia.c x86 only?)
#
SRC_C_evp      = encode.c digest.c evp_enc.c evp_key.c evp_acnf.c \
                 e_des.c e_bf.c e_idea.c e_des3.c e_camellia.c e_rc4.c \
                 e_aes.c names.c e_seed.c e_xcbc_d.c e_rc2.c e_cast.c \
                 e_rc5.c m_null.c m_md2.c m_md4.c m_md5.c m_sha.c \
                 m_sha1.c m_wp.c m_dss.c m_dss1.c m_mdc2.c m_ripemd.c \
                 m_ecdsa.c p_open.c p_seal.c p_sign.c p_verify.c \
                 p_lib.c p_enc.c p_dec.c bio_md.c bio_b64.c bio_enc.c \
                 evp_err.c e_null.c c_all.c c_allc.c c_alld.c \
                 evp_lib.c bio_ok.c evp_pkey.c evp_pbe.c p5_crpt.c \
                 p5_crpt2.c e_old.c pmeth_lib.c pmeth_fn.c pmeth_gn.c \
                 m_sigver.c evp_fips.c e_aes_cbc_hmac_sha1.c \
                 e_rc4_hmac_md5.c

SRC_C_hmac     = hmac.c hm_ameth.c hm_pmeth.c
SRC_C_idea     = i_cbc.c i_cfb64.c i_ecb.c i_ofb64.c i_skey.c
SRC_C_krb5     = krb5_asn.c
SRC_C_lhash    = lhash.c lh_stats.c
SRC_C_md2      = md2_dgst.c md2_one.c
SRC_C_md4      = md4_dgst.c md4_one.c
SRC_C_md5      = md5_dgst.c md5_one.c
SRC_C_mdc2     = mdc2_one.c mdc2dgst.c
SRC_C_objects  = o_names.c obj_dat.c obj_err.c obj_lib.c obj_xref.c
SRC_C_modes    = cbc128.c ctr128.c cts128.c cfb128.c ofb128.c \
                 gcm128.c ccm128.c xts128.c
SRC_C_ocsp     = ocsp_asn.c ocsp_ext.c ocsp_ht.c ocsp_lib.c \
                 ocsp_cl.c ocsp_srv.c ocsp_prn.c ocsp_vfy.c \
                 ocsp_err.c
SRC_C_pem      = pem_sign.c pem_seal.c pem_info.c pem_lib.c \
                 pem_all.c pem_err.c pem_x509.c pem_xaux.c \
                 pem_oth.c pem_pk8.c pem_pkey.c pvkfmt.c
SRC_C_pkcs12   = p12_add.c p12_asn.c p12_attr.c p12_crpt.c \
                 p12_crt.c p12_decr.c p12_init.c p12_key.c \
                 p12_kiss.c p12_mutl.c p12_utl.c p12_npas.c \
                 pk12err.c p12_p8d.c p12_p8e.c
SRC_C_pkcs7    = pk7_asn1.c pk7_lib.c pkcs7err.c pk7_doit.c \
                 pk7_smime.c pk7_attr.c pk7_mime.c bio_pk7.c
SRC_C_pqueue   = pqueue.c
SRC_C_rand     = md_rand.c randfile.c rand_lib.c rand_err.c \
                 rand_egd.c rand_win.c rand_unix.c rand_os2.c \
                 rand_nw.c
SRC_C_rc2      = rc2_ecb.c rc2_skey.c rc2_cbc.c rc2cfb64.c rc2ofb64.c
SRC_C_rc4      = rc4_skey.c rc4_enc.c rc4_utl.c
SRC_C_rc5      = rc5_skey.c rc5_ecb.c rc5_enc.c rc5cfb64.c rc5ofb64.c
SRC_C_ripemd   = rmd_dgst.c rmd_one.c
SRC_C_rsa      = rsa_eay.c rsa_gen.c rsa_lib.c rsa_sign.c rsa_saos.c \
                 rsa_err.c rsa_pk1.c rsa_ssl.c rsa_none.c rsa_oaep.c \
                 rsa_chk.c rsa_null.c rsa_pss.c rsa_x931.c rsa_asn1.c \
                 rsa_depr.c rsa_ameth.c rsa_prn.c rsa_pmeth.c \
                 rsa_crpt.c
SRC_C_seed     = seed.c seed_ecb.c seed_cbc.c seed_cfb.c seed_ofb.c
SRC_C_sha      = sha_dgst.c sha1dgst.c sha_one.c sha1_one.c sha256.c \
                 sha512.c
SRC_C_srp      = srp_lib.c srp_vfy.c
SRC_C_stack    = stack.c
SRC_C_store    = str_err.c str_lib.c str_meth.c str_mem.c
#SRC_C_threads  = th-lock.c
SRC_C_ts       = ts_err.c ts_req_utils.c ts_req_print.c ts_rsp_utils.c \
                 ts_rsp_print.c ts_rsp_sign.c ts_rsp_verify.c \
                 ts_verify_ctx.c ts_lib.c ts_conf.c ts_asn1.c
SRC_C_txt_db   = txt_db.c
SRC_C_ui       = ui_err.c ui_lib.c ui_openssl.c ui_util.c ui_compat.c
SRC_C_whrlpool = wp_dgst.c wp_block.c
SRC_C_x509     = x509_def.c x509_d2.c x509_r2x.c x509_cmp.c \
                 x509_obj.c x509_req.c x509spki.c x509_vfy.c \
                 x509_set.c x509cset.c x509rset.c x509_err.c \
                 x509name.c x509_v3.c x509_ext.c x509_att.c \
                 x509type.c x509_lu.c x_all.c x509_txt.c x509_trs.c \
                 by_file.c by_dir.c x509_vpm.c
SRC_C_x509v3   = v3_bcons.c v3_bitst.c v3_conf.c v3_extku.c \
                 v3_ia5.c v3_lib.c v3_prn.c v3_utl.c v3err.c \
                 v3_genn.c v3_alt.c v3_skey.c v3_akey.c v3_pku.c \
                 v3_int.c v3_enum.c v3_sxnet.c v3_cpols.c v3_crld.c \
                 v3_purp.c v3_info.c v3_ocsp.c v3_akeya.c \
                 v3_pmaps.c v3_pcons.c v3_ncons.c v3_pcia.c \
                 v3_pci.c pcy_cache.c pcy_node.c pcy_data.c \
                 pcy_map.c pcy_tree.c pcy_lib.c v3_asid.c \
                 v3_addr.c

SRC_DIR = aes asn1 bf bio bn buffer camellia cast cmac cms comp conf des dh \
          dsa dso ec ecdh ecdsa engine err evp hmac idea krb5 lhash md2 md4 md5 \
          mdc2 objects modes ocsp pem pkcs12 pkcs7 pqueue rand rc2 rc4 rc5 \
          ripemd rsa seed sha srp stack store ts txt_db ui whrlpool x509 x509v3

SRC_C += $(foreach dir,$(SRC_DIR),$(addprefix $(dir)/,$(SRC_C_$(dir))))

INC_DIR += $(LIBCRYPTO_DIR)/asn1
INC_DIR += $(LIBCRYPTO_DIR)/evp
INC_DIR += $(LIBCRYPTO_DIR)/modes
INC_DIR += $(LIBCRYPTO_DIR)
INC_DIR += $(LIBCRYPTO_DIR)/../

CC_OPTS += -DL_ENDIAN

ifeq ($(filter-out $(SPECS),x86_32),)
TARGET_CPUARCH=x86_32
else ifeq ($(filter-out $(SPECS),x86_64),)
TARGET_CPUARCH=x86_64

SRC_S += modexp512.s
SRC_S += rc4_md5.s
endif

INC_DIR += $(REP_DIR)/src/lib/openssl/$(TARGET_CPUARCH)/

#SRC_S += $(REP_DIR)/src/lib/openssl/$(TARGET_CPUARCH)/aes_enc.s
SRC_S += cpuid.s


#
# Generate buildinf.h
#
$(SRC_C:.c=.o): buildinf.h

# Rules taken from FreeBSDs libcrypto/Makefile
buildinf.h:
	$(VERBOSE)( echo "#ifndef MK1MF_BUILD"; \
	echo "  /* auto-generated by libcrypto.mk for crypto/cversion.c */"; \
	echo "  #define CFLAGS \"$(CC)\""; \
	echo "  #define PLATFORM \"FreeBSD-$(TARGET_CPUARCH)\""; \
	echo "#endif" ) > $@

vpath %.s $(REP_DIR)/src/lib/openssl/$(TARGET_CPUARCH)
vpath %.c $(LIBCRYPTO_DIR)
