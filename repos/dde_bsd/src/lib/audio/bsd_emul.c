/*
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
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

int hz = HZ;


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
	{&eap_ca,    &eap_cd,    0, 0, 0, 0, pv+1, 0, 0},
	{&auich_ca,  &auich_cd,  0, 0, 0, 0, pv+1, 0, 0},
};


int enodev(void) { return ENODEV; }


/* global character device switch table */
struct cdevsw cdevsw[] = {
	/* cdev_audio_init */
	{
		audioopen,
		audioclose,
		audioread,
		audiowrite,
		audioioctl,
		(int (*)(struct tty*, int)) enodev,
		0,
		audiopoll,
		0,
		0,
		0,
		0,
	},
};


/* needed by dev/audio.c:522 */
int nchrdev = sizeof(cdevsw) / sizeof(struct cdevsw);


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


struct device *config_found_sm(struct device *parent, void *aux, cfprint_t print,
                               cfmatch_t submatch)
{
	struct cfdata *cf = &cfdata[0];
	struct cfattach *ca = cf->cf_attach;
	struct cfdriver *cd = cf->cf_driver;

	int rv = ca->ca_match(parent, NULL, aux);
	if (rv) {
		struct device *dev = (struct device *) malloc(ca->ca_devsize, M_DEVBUF,
		                                              M_NOWAIT|M_ZERO);

		snprintf(dev->dv_xname, sizeof(dev->dv_xname), "%s%d", cd->cd_name,
		         dev->dv_unit);
		printf("%s at %s\n", dev->dv_xname, parent->dv_xname);

		dev->dv_cfdata = cf;

		ca->ca_attach(parent, dev, aux);

		audio_cd.cd_ndevs = 1;
		audio_cd.cd_devs = malloc(sizeof(void*), 0, 0);
		audio_cd.cd_devs[0] = dev;

		return dev;
	}

	return 0;
}


struct device *device_lookup(struct cfdriver *cd, int unit)
{
	if (unit >= audio_cd.cd_ndevs || audio_cd.cd_devs[unit] == NULL)
		return NULL;

	return audio_cd.cd_devs[unit];
}

/*****************
 ** sys/ucred.h **
 *****************/

int suser(struct proc *p)
{
	(void)p;

	/* we always have special user powers */
	return 0;
};
