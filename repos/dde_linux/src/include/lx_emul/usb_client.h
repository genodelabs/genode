#include <genode_c_api/usb_client.h>

#ifdef __cplusplus
extern "C" {
#endif

void *lx_emul_usb_client_register_device(genode_usb_client_handle_t handle, char const *label);
void  lx_emul_usb_client_unregister_device(genode_usb_client_handle_t handle, void *data);
int   lx_emul_usb_client_set_configuration(genode_usb_client_handle_t, void *data, unsigned long config);

#ifdef __cplusplus
} /* extern "C" */
#endif
