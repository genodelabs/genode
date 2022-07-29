#include <linux/pci.h>

resource_size_t __weak pcibios_align_resource(void *data,
                          const struct resource *res,
                          resource_size_t size,
                          resource_size_t align)
{
       return res->start;
}


