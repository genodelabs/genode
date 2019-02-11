#ifndef ___VBox_com_ptr_h
#define ___VBox_com_ptr_h

#include <VBox/com/defs.h>

#include <base/log.h>

template <typename T>
class ComPtr {

	protected:

		T * _obj;

	public:

		ComPtr<T> () : _obj(nullptr) { }

		/* copy constructor */
		ComPtr<T> (T *obj) : _obj(obj) { }

		template<typename X>
		ComPtr<T> (X *obj) : _obj(dynamic_cast<T*>(obj))
		{
			if (!_obj)
				Genode::log(__func__, ": dynamic cast failed");
		}

		template <class T2>
		ComPtr<T>(const ComPtr<T2> &that) : ComPtr<T>((T2*)that) { }

		/* operators */
		T * operator->() const  { return _obj; }
        operator T*() const     { return _obj; }

		template <class T2>
		ComPtr& operator=(const ComPtr<T2> &that)
		{
			return operator=((T2*)that);
		}

		ComPtr& operator=(const ComPtr &that)
		{
			return operator=((T*)that);
		}

		ComPtr& operator=(T *p)
		{
			_obj = p;
			return *this;
		}

		bool isNull () const    { return _obj == nullptr; }
		bool isNotNull() const  { return _obj != nullptr; }

		T ** asOutParam() { return &_obj; }

		template <class T2>
		HRESULT queryInterfaceTo(T2 **pp) const {
			if (pp == nullptr)
				return E_INVALIDARG;

			*pp = _obj;
			return S_OK;
		}

		void setNull() { _obj = nullptr; }

		template <class T2>
		bool operator==(T2* p)
		{
			return (p == _obj);
		}

};


template <class T>
class ComObjPtr : public ComPtr<T> {

	public:

		ComObjPtr<T> () : ComPtr<T>() { }

		/* copy constructor */
		ComObjPtr<T> (T *obj) : ComPtr<T>(obj) { }

		HRESULT createObject()
		{
			T * obj = new T();
			if (!obj)
				return E_OUTOFMEMORY;

			ComPtr<T>::_obj = obj;
			return obj->FinalConstruct();
		}
};

#endif /* ___VBox_com_ptr_h */
