#ifndef ____H_CLIENTWATCHER
#define ____H_CLIENTWATCHER

class VirtualBox::ClientWatcher
{
	public:

		ClientWatcher(VirtualBox* const) { }

		bool isReady() { return true; }
		void update() { }
		void addProcess(RTPROCESS pid);
};
#endif /* !____H_CLIENTWATCHER */
