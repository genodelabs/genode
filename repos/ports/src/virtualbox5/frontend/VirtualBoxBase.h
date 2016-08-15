#ifndef ____H_VIRTUALBOXBASEIMPL
#define ____H_VIRTUALBOXBASEIMPL

#include <base/log.h>

#include <iprt/cdefs.h>
#include <iprt/thread.h>

#include <list>
#include <map>

#include "ObjectState.h"

#include "VBox/com/AutoLock.h"
#include "VBox/com/string.h"
#include "VBox/com/Guid.h"

#include "VBox/com/VirtualBox.h"

namespace com
{
	class ErrorInfo;
}

using namespace com;
using namespace util;

class AutoInitSpan;
class AutoUninitSpan;

class VirtualBox;
class Machine;
class Medium;

typedef std::list<ComObjPtr<Medium> > MediaList;
typedef std::list<Utf8Str> StringsList;

class VirtualBoxTranslatable : public util::Lockable
{
	public:

		/* should be used for translations */
		inline static const char *tr(const char *pcszSourceText,
		                             const char *aComment = NULL)
		{
			return pcszSourceText;
		}
};

class VirtualBoxBase : public VirtualBoxTranslatable
{

	private:

		RWLockHandle *_lock;


		/** Primary state of this object */
		ObjectState mState;

		/** Thread that caused the last state change */
		RTTHREAD mStateChangeThread;
		/** Total number of active calls to this object */
		unsigned mCallers;
		/** Posted when the number of callers drops to zero */
		RTSEMEVENT mZeroCallersSem;
		/** Posted when the object goes from InInit/InUninit to some other state */
		RTSEMEVENTMULTI mInitUninitSem;
		/** Number of threads waiting for mInitUninitDoneSem */
		unsigned mInitUninitWaiters;

		/** User-level object lock for subclasses */
		mutable RWLockHandle *mObjectLock;

		friend class AutoInitSpan;
		friend class AutoReinitSpan;
		friend class AutoUninitSpan;

	protected:

		HRESULT   BaseFinalConstruct() { return S_OK; }

		void   BaseFinalRelease() { }

	public:

		VirtualBoxBase(); // : _lock(nullptr) { }
		~VirtualBoxBase();

		virtual void uninit() { }

		ObjectState &getObjectState()
		{
			return mState;
		}

		virtual const char* getComponentName() const = 0;

		static HRESULT handleUnexpectedExceptions(VirtualBoxBase *const aThis, RT_SRC_POS_DECL);
		static HRESULT initializeComForThread(void);
		static void uninitializeComForThread(void);
		static void clearError(void);

		HRESULT setError(HRESULT aResultCode);
		HRESULT setError(HRESULT aResultCode, const char *pcsz, ...);
		HRESULT setError(const com::ErrorInfo &ei);
		HRESULT setErrorVrc(int vrc);
		HRESULT setErrorVrc(int vrc, const char *pcszMsgFmt, ...);
		HRESULT setErrorBoth(HRESULT hrc, int vrc);
		HRESULT setErrorBoth(HRESULT hrc, int vrc, const char *pcszMsgFmt, ...);
		HRESULT setErrorNoLog(HRESULT aResultCode, const char *pcsz, ...);

		static HRESULT setErrorInternal(HRESULT aResultCode,
		                                const GUID &aIID,
		                                const char *aComponent,
		                                Utf8Str aText,
		                                bool aWarning,
		                                bool aLogIt);

		virtual VBoxLockingClass getLockingClass() const
		{
			return LOCKCLASS_OTHEROBJECT;
		}

		RWLockHandle * lockHandle() const;
};


/**
 * Dummy macro that is used to shut down Qt's lupdate tool warnings in some
 * situations. This macro needs to be present inside (better at the very
 * beginning) of the declaration of the class that inherits from
 * VirtualBoxTranslatable, to make lupdate happy.
 */
#define Q_OBJECT


template <typename T>
class Shareable
{

	private:

		bool _verbose;
		T *  _obj;

	public:

		Shareable<T> () : _verbose(false), _obj(nullptr) { }

		/* operators */
		T * operator->() const  { if (_verbose) Genode::log(__PRETTY_FUNCTION__, " called"); return _obj; }

		bool isNull() const { return _obj == nullptr; }
		bool operator!() const { return isNull(); }

		void allocate() { if (_verbose) Genode::log(__PRETTY_FUNCTION__, " called"); _obj = new T; }
		void attach(T * t) { if (_verbose) Genode::log(__PRETTY_FUNCTION__, " called"); }
		void attach(Shareable &s) { if (_verbose) Genode::log(__PRETTY_FUNCTION__, " called"); }
		void share(const Shareable &s) { if (_verbose) Genode::log(__PRETTY_FUNCTION__, " called"); _obj = s._obj; }
		void share(T * obj) { if (_verbose) Genode::log(__PRETTY_FUNCTION__, " called"); _obj = obj; }
		void free() { if (_verbose) Genode::log(__PRETTY_FUNCTION__, " called"); }
		void attachCopy(const T *) { if(_verbose) Genode::log(__PRETTY_FUNCTION__, " called"); }
		void attachCopy(const Shareable &) { if (_verbose) Genode::log(__PRETTY_FUNCTION__, " called"); }

		T *data() const { if (_verbose) Genode::log(__PRETTY_FUNCTION__, " called"); return _obj; }

		bool isShared() const { if (_verbose) Genode::log(__PRETTY_FUNCTION__, " called"); return false; }
};

template <typename T>
class Backupable : public Shareable<T>
{

	public:

	    Backupable() : Shareable<T>() { }

		void backup() { }
		void rollback() { }
		void commit() { }
		void commitCopy() { }
		void assignCopy(const T *) { }
		void assignCopy(const Backupable &) { }

		HRESULT backupEx() { return S_OK; }

		T *backedUpData() const { Genode::log(__PRETTY_FUNCTION__, " called"); return nullptr; }
		bool isBackedUp() const { return false; }
};

/**
 *  Special version of the Assert macro to be used within VirtualBoxBase
 *  subclasses.
 *
 *  In the debug build, this macro is equivalent to Assert.
 *  In the release build, this macro uses |setError(E_FAIL, ...)| to set the
 *  error info from the asserted expression.
 *
 *  @see VirtualBoxBase::setError
 *
 *  @param   expr    Expression which should be true.
 */
#if defined(DEBUG)
#define ComAssert(expr)    Assert(expr)
#else
#define ComAssert(expr)    \
    do { \
        if (RT_UNLIKELY(!(expr))) \
            setError(E_FAIL, \
                     "Assertion failed: [%s] at '%s' (%d) in %s.\nPlease contact the product vendor!", \
                     #expr, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    } while (0)
#endif

/**
 *  Special version of the AssertFailed macro to be used within VirtualBoxBase
 *  subclasses.
 *
 *  In the debug build, this macro is equivalent to AssertFailed.
 *  In the release build, this macro uses |setError(E_FAIL, ...)| to set the
 *  error info from the asserted expression.
 *
 *  @see VirtualBoxBase::setError
 *
 */
#if defined(DEBUG)
#define ComAssertFailed()    AssertFailed()
#else
#define ComAssertFailed()    \
    do { \
        setError(E_FAIL, \
                 "Assertion failed: at '%s' (%d) in %s.\nPlease contact the product vendor!", \
                 __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    } while (0)
#endif

/**
 *  Special version of the AssertMsg macro to be used within VirtualBoxBase
 *  subclasses.
 *
 *  See ComAssert for more info.
 *
 *  @param   expr    Expression which should be true.
 *  @param   a       printf argument list (in parenthesis).
 */
#if defined(DEBUG)
#define ComAssertMsg(expr, a)  AssertMsg(expr, a)
#else
#define ComAssertMsg(expr, a)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
            setError(E_FAIL, \
                     "Assertion failed: [%s] at '%s' (%d) in %s.\n%s.\nPlease contact the product vendor!", \
                     #expr, __FILE__, __LINE__, __PRETTY_FUNCTION__, Utf8StrFmt a .c_str()); \
    } while (0)
#endif

/**
 *  Special version of the AssertComRC macro to be used within VirtualBoxBase
 *  subclasses.
 *
 *  See ComAssert for more info.
 *
 *  @param rc   COM result code
 */
#if defined(DEBUG)
#define ComAssertComRC(rc)  AssertComRC(rc)
#else
#define ComAssertComRC(rc)  ComAssertMsg(SUCCEEDED(rc), ("COM RC = %Rhrc (0x%08X)", (rc), (rc)))
#endif

/**
 *  Special version of the AssertMsgRC macro to be used within VirtualBoxBase
 *  subclasses.
 *
 *  See ComAssert for more info.
 *
 *  @param   vrc    VBox status code.
 *  @param   msg    printf argument list (in parenthesis).
 */
#if defined(DEBUG)
#define ComAssertMsgRC(vrc, msg)    AssertMsgRC(vrc, msg)
#else
#define ComAssertMsgRC(vrc, msg)    ComAssertMsg(RT_SUCCESS(vrc), msg)
#endif

/**
 *  Special version of the AssertRC macro to be used within VirtualBoxBase
 *  subclasses.
 *
 *  See ComAssert for more info.
 *
 * @param   vrc     VBox status code.
 */
#if defined(DEBUG)
#define ComAssertRC(vrc)    AssertRC(vrc)
#else
#define ComAssertRC(vrc)    ComAssertMsgRC(vrc, ("%Rra", vrc))
#endif

/** Special version of ComAssert that returns ret if expr fails */
#define ComAssertRet(expr, ret)             \
    do { ComAssert(expr); if (!(expr)) return (ret); } while (0)
/** Special version of ComAssertMsg that returns ret if expr fails */
#define ComAssertMsgRet(expr, a, ret)       \
    do { ComAssertMsg(expr, a); if (!(expr)) return (ret); } while (0)
/** Special version of ComAssertComRC that returns ret if rc does not succeed */
#define ComAssertComRCRet(rc, ret)          \
    do { ComAssertComRC(rc); if (!SUCCEEDED(rc)) return (ret); } while (0)
/** Special version of ComAssertMsg that evaluates eval and breaks if expr fails */
#define ComAssertMsgBreak(expr, a, eval)          \
    if (1) { ComAssertMsg(expr, a); if (!(expr)) { eval; break; } } else do {} while (0)
/** Special version of ComAssert that evaluates eval and throws it if expr fails */
#define ComAssertThrow(expr, eval)                \
    do { ComAssert(expr); if (!(expr)) { throw (eval); } } while (0)
/** Special version of ComAssertRC that evaluates eval and throws it if vrc does not succeed */
#define ComAssertRCThrow(vrc, eval)               \
    do { ComAssertRC(vrc); if (!RT_SUCCESS(vrc)) { throw (eval); } } while (0)
/** Special version of ComAssertFailed that returns ret */
#define ComAssertFailedRet(ret)             \
    do { ComAssertFailed(); return (ret); } while (0)
/** Special version of ComAssertFailed that returns ret */
#define ComAssertFailedRet(ret)             \
    do { ComAssertFailed(); return (ret); } while (0)
/** Special version of ComAssertRC that returns ret if vrc does not succeed */
#define ComAssertRCRet(vrc, ret)            \
    do { ComAssertRC(vrc); if (!RT_SUCCESS(vrc)) return (ret); } while (0)
/** Special version of ComAssertComRC that just throws rc if rc does not succeed */
#define ComAssertComRCThrowRC(rc)                 \
    do { ComAssertComRC(rc); if (!SUCCEEDED(rc)) { throw rc; } } while (0)
/** Special version of ComAssertComRC that returns rc if rc does not succeed */
#define ComAssertComRCRetRC(rc)             \
    do { ComAssertComRC(rc); if (!SUCCEEDED(rc)) return (rc); } while (0)

/**
 * Checks that the pointer argument is not NULL and returns E_INVALIDARG +
 * extended error info on failure.
 * @param arg   Input pointer-type argument (strings, interface pointers...)
 */
#define CheckComArgNotNull(arg) \
    do { \
        if (RT_UNLIKELY((arg) == NULL)) \
            return setError(E_INVALIDARG, tr("Argument %s is NULL"), #arg); \
    } while (0)

/**
 * Checks that the given expression (that must involve the argument) is true and
 * returns E_INVALIDARG + extended error info on failure.
 * @param arg   Argument.
 * @param expr  Expression to evaluate.
 */
#define CheckComArgExpr(arg, expr) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
            return setError(E_INVALIDARG, \
                tr("Argument %s is invalid (must be %s)"), #arg, #expr); \
    } while (0)

/**
 * Checks that the given pointer to an output argument is valid and returns
 * E_POINTER + extended error info otherwise.
 * @param arg   Pointer argument.
 */
#define CheckComArgOutPointerValid(arg) \
    do { \
        if (RT_UNLIKELY(!VALID_PTR(arg))) \
            return setError(E_POINTER, \
                tr("Output argument %s points to invalid memory location (%p)"), \
                #arg, (void *)(arg)); \
    } while (0)

/**
 * Checks that a string input argument is valid (not NULL or obviously invalid
 * pointer), returning E_INVALIDARG + extended error info if invalid.
 * @param a_bstrIn  Input string argument (IN_BSTR).
 */
#define CheckComArgStr(a_bstrIn) \
    do { \
        IN_BSTR const bstrInCheck = (a_bstrIn); /* type check */ \
        if (RT_UNLIKELY(!RT_VALID_PTR(bstrInCheck))) \
            return setError(E_INVALIDARG, tr("Argument %s is an invalid pointer"), #a_bstrIn); \
    } while (0)

/**
 * Checks that the string argument is not a NULL, a invalid pointer or an empty
 * string, returning E_INVALIDARG + extended error info on failure.
 * @param a_bstrIn  Input string argument (BSTR etc.).
 */
#define CheckComArgStrNotEmptyOrNull(a_bstrIn) \
    do { \
        IN_BSTR const bstrInCheck = (a_bstrIn); /* type check */ \
        if (RT_UNLIKELY(!RT_VALID_PTR(bstrInCheck) || *(bstrInCheck) == '\0')) \
            return setError(E_INVALIDARG, tr("Argument %s is empty or an invalid pointer"), #a_bstrIn); \
    } while (0)

/**
 * Checks that the given pointer to an output safe array argument is valid and
 * returns E_POINTER + extended error info otherwise.
 * @param arg   Safe array argument.
 */
#define CheckComArgOutSafeArrayPointerValid(arg) \
    do { \
        if (RT_UNLIKELY(ComSafeArrayOutIsNull(arg))) \
            return setError(E_POINTER, \
                            tr("Output argument %s points to invalid memory location (%p)"), \
                            #arg, (void*)(arg)); \
    } while (0)

/**
 * Checks that safe array argument is not NULL and returns E_INVALIDARG +
 * extended error info on failure.
 * @param arg   Input safe array argument (strings, interface pointers...)
 */
#define CheckComArgSafeArrayNotNull(arg) \
    do { \
        if (RT_UNLIKELY(ComSafeArrayInIsNull(arg))) \
            return setError(E_INVALIDARG, tr("Argument %s is NULL"), #arg); \
    } while (0)

/**
 * Sets the extended error info and returns E_NOTIMPL.
 */
#define ReturnComNotImplemented() \
    do { \
        return setError(E_NOTIMPL, tr("Method %s is not implemented"), __FUNCTION__); \
    } while (0)


#define DECLARE_EMPTY_CTOR_DTOR(X) public: X(); ~X();

#define DEFINE_EMPTY_CTOR_DTOR(X)  X::X() {} X::~X() {}


#define VIRTUALBOXBASE_ADD_VIRTUAL_COMPONENT_METHODS(cls, iface) \
    virtual const GUID& getClassIID() const \
    { \
        return cls::getStaticClassIID(); \
    } \
    static const GUID& getStaticClassIID() \
    { \
        return COM_IIDOF(iface); \
    } \
    virtual const char* getComponentName() const \
    { \
        return cls::getStaticComponentName(); \
    } \
    static const char* getStaticComponentName() \
    { \
        return #cls; \
    }

/**
 * VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT:
 * This macro must be used once in the declaration of any class derived
 * from VirtualBoxBase. It implements the pure virtual getClassIID() and
 * getComponentName() methods. If this macro is not present, instances
 * of a class derived from VirtualBoxBase cannot be instantiated.
 *
 * @param X The class name, e.g. "Class".
 * @param IX The interface name which this class implements, e.g. "IClass".
 */
  #define VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(cls, iface) \
    VIRTUALBOXBASE_ADD_VIRTUAL_COMPONENT_METHODS(cls, iface)

#include "GenodeImpl.h"

#endif // !____H_VIRTUALBOXBASEIMPL
