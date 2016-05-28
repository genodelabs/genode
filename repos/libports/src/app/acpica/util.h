namespace Acpica {
	template<typename> class Buffer;
	template<typename> class Callback;
}

template <typename T>
class Acpica::Buffer : public ACPI_BUFFER
{
	public:
		T object;
		Buffer() {
			Pointer = &object;
			Length = sizeof(object);
		}

} __attribute__((packed));

template <typename T>
class Acpica::Callback : public Genode::List<Acpica::Callback<T> >::Element
{
	public:

		static void handler(ACPI_HANDLE h, UINT32 value, void * context)
		{
			reinterpret_cast<T *>(context)->handle(h, value);
		}

		void generate(Genode::Xml_generator &xml) {
			reinterpret_cast<T *>(this)->generate(xml);
		}
};
