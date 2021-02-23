/*
 * This is merely a stub to successfully compile Devices/PC/DrvACPI.cpp
 */

#ifndef _DEV__ACPICA__ACPIIO_H_
#define _DEV__ACPICA__ACPIIO_H_

union acpi_battery_ioctl_arg
{
	unsigned unit;

	struct {
		unsigned state;
		int cap;
	} battinfo;

	struct {
		unsigned units;
	} bif;

	struct {
		unsigned volt;
		unsigned rate;
	} bst;
};

/*
 * Note that these enum values are not meaningful.
 */
enum {
	ACPIIO_BATT_GET_BIF,
	ACPIIO_BATT_GET_BST,
	ACPIIO_BATT_GET_BATTINFO,
	ACPI_BIF_UNITS_MW,
	ACPI_BATT_STAT_NOT_PRESENT,
	ACPI_BATT_STAT_CHARGING,
	ACPI_BATT_STAT_DISCHARG,
	ACPI_BATT_STAT_CRITICAL,
};

#endif /* _DEV__ACPICA__ACPIIO_H_ */
