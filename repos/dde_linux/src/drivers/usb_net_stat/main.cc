/*
 * \brief  Startup USB driver library
 * \author Sebastian Sumpf
 * \date   2013-02-20
 */

int main()
{
	extern void start_usb_driver();
	start_usb_driver();
	return 0;
}
