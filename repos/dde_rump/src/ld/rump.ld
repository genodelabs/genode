/*
 * \brief  Additonial sections required by all rump libraries
 *
 *         Modified version of original linker script
 *
 * \author Sebastiasn Sumpf
 * \date   2013-12-12
 */

SECTIONS
{
	.rump_modules :
	{
		__start_link_set_modules = .;
		KEEP(*(link_set_modules));
		__stop_link_set_modules = .;
	} : rw

	.rump_domains :
	{
		__start_link_set_domains = .;
		KEEP(*(link_set_domains));
		__stop_link_set_domains = .;
	} : rw

	.rump_components :
	{
		__start_link_set_rump_components = .;
		KEEP(*(link_set_rump_components));
		__stop_link_set_rump_components = .;
	} : rw
}
