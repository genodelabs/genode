/* License Info */
static const char qt_configure_licensee_str          [256 + 12] = "qt_lcnsuser=Open Source";
static const char qt_configure_licensed_products_str [256 + 12] = "qt_lcnsprod=OpenSource";

/* Installation date */
static const char qt_configure_installation          [12+11]    = "qt_instdate=2013-05-23";

/* Installation Info */
static const char qt_configure_prefix_path_strs[][256 + 12] = {
    "qt_prfxpath=:/qt",
    "qt_docspath=:/qt/doc",
    "qt_hdrspath=:/qt/include",
    "qt_libspath=:/qt/lib",
    "qt_lbexpath=:/qt/libexec",
    "qt_binspath=:/qt/bin",
    "qt_plugpath=:/qt/plugins",
    "qt_impspath=:/qt/imports",
    "qt_qml2path=/qt5/qml",
    "qt_adatpath=:/qt",
    "qt_datapath=:/qt",
    "qt_trnspath=:/qt/translations",
    "qt_xmplpath=:/qt/examples",
    "qt_tstspath=:/qt/tests",
#ifdef QT_BUILD_QMAKE
    "qt_ssrtpath=",
    "qt_hpfxpath=:/qt",
    "qt_hbinpath=:/qt/bin",
    "qt_hdatpath=:/qt",
    "qt_targspec=genode-g++",
    "qt_hostspec=linux-g++",
#endif
};
static const char qt_configure_settings_path_str[256 + 12] = "qt_stngpath=:/qt/etc/xdg";

/* strlen( "qt_lcnsxxxx" ) == 12 */
#define QT_CONFIGURE_LICENSEE qt_configure_licensee_str + 12;
#define QT_CONFIGURE_LICENSED_PRODUCTS qt_configure_licensed_products_str + 12;

#define QT_CONFIGURE_SETTINGS_PATH qt_configure_settings_path_str + 12;
