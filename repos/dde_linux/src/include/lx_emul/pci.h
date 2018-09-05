/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*****************
 ** linux/pci.h **
 *****************/

#include <linux/pci_ids.h>
#include <uapi/linux/pci_regs.h>

enum {
	PCI_DMA_BIDIRECTIONAL = 0,
	PCI_DMA_TODEVICE,
	PCI_DMA_FROMDEVICE,
	PCI_DMA_NONE
};


enum { PCI_ANY_ID = ~0U };


typedef enum {
	PCI_D0    = 0,
	PCI_D1    = 1,
	PCI_D2    = 2,
	PCI_D3hot = 3,
	PCI_D3cold = 4,
} pci_power_t;


/*
 * PCI types
 */
struct pci_bus;
struct pci_dev;

struct pci_driver {
	char                        *name;
	const struct pci_device_id  *id_table;
	int                        (*probe)  (struct pci_dev *dev,
                                        const struct pci_device_id *id);
	void                       (*remove) (struct pci_dev *dev);
	void                       (*shutdown) (struct pci_dev *dev);
	struct device_driver         driver;
};


static inline uint32_t PCI_DEVFN(unsigned slot, unsigned func) {
	return ((slot & 0x1f) << 3) | (func & 0x07); }

static inline uint32_t PCI_FUNC(unsigned devfn) { return devfn & 0x07; }
static inline uint32_t PCI_SLOT(unsigned devfn) { return ((devfn) >> 3) & 0x1f; }

int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn, int where, u8 *val);
int pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn, int where, u16 *val);
int pci_bus_read_config_dword(struct pci_bus *bus, unsigned int devfn, int where, u32 *val);
int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn, int where, u8 val);
int pci_bus_write_config_word(struct pci_bus *bus, unsigned int devfn, int where, u16 val);
int pci_bus_write_config_dword(struct pci_bus *bus, unsigned int devfn, int where, u32 val);

static inline
int pci_read_config_byte(struct pci_dev *dev, int where, u8 *val) {
return pci_bus_read_config_byte(dev->bus, dev->devfn, where, val); }

static inline
int pci_read_config_word(struct pci_dev *dev, int where, u16 *val) {
return pci_bus_read_config_word(dev->bus, dev->devfn, where, val); }

static inline
int pci_read_config_dword(struct pci_dev *dev, int where, u32 *val) {
return pci_bus_read_config_dword(dev->bus, dev->devfn, where, val); }

static inline
int pci_write_config_byte(struct pci_dev *dev, int where, u8 val) {
return pci_bus_write_config_byte(dev->bus, dev->devfn, where, val); }

static inline
int pci_write_config_word(struct pci_dev *dev, int where, u16 val) {
return pci_bus_write_config_word(dev->bus, dev->devfn, where, val); }

static inline
int pci_write_config_dword(struct pci_dev *dev, int where, u32 val) {
return pci_bus_write_config_dword(dev->bus, dev->devfn, where, val); }

size_t pci_resource_len(struct pci_dev *dev, unsigned bar);
size_t pci_resource_start(struct pci_dev *dev, unsigned bar);
size_t pci_resource_end(struct pci_dev *dev, unsigned bar);
void pci_dev_put(struct pci_dev *dev);
struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device, struct pci_dev *from);

int pci_enable_device(struct pci_dev *dev);
void pci_disable_device(struct pci_dev *dev);
int pci_register_driver(struct pci_driver *driver);
void pci_unregister_driver(struct pci_driver *driver);
const char *pci_name(const struct pci_dev *pdev);
bool pci_dev_run_wake(struct pci_dev *dev);
unsigned int pci_resource_flags(struct pci_dev *dev, unsigned bar);
void pci_set_master(struct pci_dev *dev);
int pci_set_mwi(struct pci_dev *dev);
bool pci_pme_capable(struct pci_dev *dev, pci_power_t state);
int pci_find_capability(struct pci_dev *dev, int cap);
struct pci_dev *pci_get_slot(struct pci_bus *bus, unsigned int devfn);
const struct pci_device_id *pci_match_id(const struct pci_device_id *ids, struct pci_dev *dev);
int pci_request_regions(struct pci_dev *dev, const char *res_name);
void pci_release_regions(struct pci_dev *dev);
void *pci_ioremap_bar(struct pci_dev *pdev, int bar);
void pci_disable_link_state(struct pci_dev *pdev, int state);

int pci_enable_msi(struct pci_dev *dev);
void pci_disable_msi(struct pci_dev *dev);

#define DEFINE_PCI_DEVICE_TABLE(_table) \
	const struct pci_device_id _table[] __devinitconst

#define to_pci_dev(n) container_of(n, struct pci_dev, dev)

int pci_register_driver(struct pci_driver *driver);

int pcie_capability_read_word(struct pci_dev *dev, int pos, u16 *val);

void *pci_get_drvdata(struct pci_dev *pdev);


#define dev_is_pci(d) (1)

int pci_num_vf(struct pci_dev *dev);

/* XXX will this cast ever work? */
#define dev_num_vf(d) ((dev_is_pci(d) ? pci_num_vf((struct pci_dev *)d) : 0))


/**********************
 ** linux/pci-aspm.h **
 **********************/

#define PCIE_LINK_STATE_L0S   1
#define PCIE_LINK_STATE_L1    2
#define PCIE_LINK_STATE_CLKPM 4
