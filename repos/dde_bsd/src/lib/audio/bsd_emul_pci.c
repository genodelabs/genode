/*
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-09
 */

/*
 * Copyright (C) 2014-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <bsd_emul.h>

#include <sys/device.h>
#include <dev/audio_if.h>


/******************
 ** sys/kernel.h **
 ******************/

/* ioconf.c */
extern struct cfdriver audio_cd;
extern struct cfattach audio_ca;
extern struct cfdriver azalia_cd;
extern struct cfattach azalia_ca;
extern struct cfdriver eap_cd;
extern struct cfattach eap_ca;
extern struct cfdriver auich_cd;
extern struct cfattach auich_ca;


/* original value */
enum { PCI_BUS_PARENT = 56 };
short pv[] = { -1, PCI_BUS_PARENT };


struct cfdata cfdata[] = {
	{&audio_ca,  &audio_cd,  0, 0, 0, 0, pv+0, 0, 0},
	{&azalia_ca, &azalia_cd, 0, 0, 0, 0, pv+1, 0, 0},
};


struct device pci_bus = { DV_DULL, { 0, 0 }, 0, 0, { 'p', 'c', 'i', '0'}, 0, 0, 0 };

/**
 * This function is our little helper that matches and attaches
 * the driver to the device.
 */
int probe_cfdata(struct pci_attach_args *pa)
{
	size_t ncd = sizeof(cfdata) / sizeof(struct cfdata);

	size_t i;
	for (i = 0; i < ncd; i++) {
		struct cfdata *cf = &cfdata[i];
		struct cfdriver *cd = cf->cf_driver;

		if (*cf->cf_parents != PCI_BUS_PARENT)
			continue;

		struct cfattach *ca = cf->cf_attach;
		if (!ca->ca_match)
			continue;

		int rv = ca->ca_match(&pci_bus, 0, pa);

		if (rv) {
			struct device *dev = (struct device *) malloc(ca->ca_devsize,
			                                              M_DEVBUF, M_NOWAIT|M_ZERO);

			dev->dv_cfdata = cf;

			snprintf(dev->dv_xname, sizeof(dev->dv_xname), "%s%d", cd->cd_name,
			         dev->dv_unit);
			printf("%s at %s\n", dev->dv_xname, pci_bus.dv_xname);
			ca->ca_attach(&pci_bus, dev, pa);

			return 1;
		}
	}

	return 0;
}
