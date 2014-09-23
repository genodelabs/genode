#ifndef ___VBox_com_ptr_h
#define ___VBox_com_ptr_h

#include <VBox/com/defs.h>

template <typename T>
class ComPtr {

	protected:

		T * _obj;

	public:

		ComPtr<T> () : _obj(nullptr) { }

		/* copy constructor */
		ComPtr<T> (T *obj) : _obj(obj) { }
		template<typename X>
		ComPtr<T> (X *obj) : _obj(nullptr) { }
		template <class T2>
		ComPtr(const ComPtr<T2> &that) : _obj(nullptr) { }

		/* operators */
		T * operator->() const  { return _obj; }
        operator T*() const     { return _obj; }

		bool isNull () const    { return _obj == nullptr; }

		T ** asOutParam() { return &_obj; }

		template <class T2>
		HRESULT queryInterfaceTo(T2 **pp) const {
			if (pp == nullptr)
				return E_INVALIDARG;

			*pp = _obj;
			return S_OK;
		}

		void setNull() { _obj = nullptr; }
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
