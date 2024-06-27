#include <genode_c_api/usb_client.h>

#ifdef __cplusplus
extern "C" {
#endif

int   lx_emul_usb_client_set_configuration(genode_usb_client_dev_handle_t, void *data, unsigned long config);
void  lx_emul_usb_client_init(void);
void  lx_emul_usb_client_rom_update(void);
void  lx_emul_usb_client_ticker(void);

struct usb_device;
void  lx_emul_usb_client_device_unregister_callback(struct usb_device *);

#ifdef __cplusplus
} /* extern "C" */
#endif
