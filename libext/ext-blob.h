/*######   Copyright (c) 2014-2018 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include EXT_HEADER_SYSTEM_ERROR

#if UCFG_COM
typedef WCHAR OLECHAR;
typedef OLECHAR* BSTR;
typedef OLECHAR* LPOLESTR;
typedef double DATE;

typedef struct tagVARIANT VARIANT;
class _bstr_t;
typedef unsigned short VARTYPE;
#endif

#include "ext-basic-cpp.h"
//!!!#include "libext.h"

#include EXT_HEADER_SPAN

namespace Ext {

using std::atomic;
using std::span;

class CStringBlobBuf;
class COleVariant;

#if !UCFG_MINISTL
using std::exchange;
#endif

#if UCFG_FULL && !UCFG_MINISTL
template <class T, size_t MAXSIZE> class vararray : public std::array<T, MAXSIZE> {
	typedef std::array<T, MAXSIZE> base;

public:
	using base::begin;
	using base::data;

	vararray(size_t size = 0)
		: m_size(size) {}

	vararray(const T* b, const T* e)
		: m_size(e - b) {
		std::copy_n(b, m_size, begin());
	}

	vararray(const T* b, size_t size)
		: m_size(size) {
		std::copy_n(b, m_size, begin());
	}

	size_t max_size() const { return MAXSIZE; }

	size_t size() const { return m_size; }
	bool empty() const { return size() == 0; }
	void resize(size_t n) { m_size = n; }

	void push_back(const T& v) { (*this)[m_size++] = v; }

	const T* constData() const { return data(); }

	typename base::const_iterator end() const { return begin() + size(); }
	typename base::iterator end() { return begin() + size(); }

private:
	size_t m_size;
};
#endif // UCFG_FULL && !UCFG_MINISTL

class ConstBuf : public span<const uint8_t> {
    typedef span<const uint8_t> base;
public:
	ConstBuf() {}
	ConstBuf(const void *p, size_t size) : base((const uint8_t*)p, size) {}
};

typedef span<const uint8_t> Span;
typedef const Span& RCSpan;

const uint8_t *Find(RCSpan a, RCSpan b);

EXTAPI bool AFXAPI Equal(RCSpan x, RCSpan y);

class CBlobBufBase {
public:
	atomic<int> m_aRef;

#if UCFG_STRING_CHAR == 16
	typedef Char16 Char;
#elif UCFG_STRING_CHAR == 32
	typedef Char32 Char;
#else
	typedef wchar_t Char;
#endif

	CBlobBufBase() : m_aRef(1) {}

	void AddRef() EXT_FAST_NOEXCEPT { ++m_aRef; }

#if UCFG_BLOB_POLYMORPHIC
	virtual ~CBlobBufBase() {}
	virtual Char* GetBSTR() = 0; //!!!was BSTR
	virtual CBlobBufBase* Clone() = 0;
	virtual size_t GetSize() = 0;
	virtual CBlobBufBase* SetSize(size_t size) = 0;

	virtual CStringBlobBuf* AsStringBlobBuf();
	virtual void Attach(Char* bstr); //!!!was BSTR
	virtual Char* Detach();			 //!!!was BSTR
#endif
};

class EXTAPI CStringBlobBuf : public CBlobBufBase {
public:
#ifdef WDM_DRIVER
	UNICODE_STRING m_us;
#endif
	atomic<char*> m_apChar;
#ifdef _WIN64 //!!!
	uint32_t m_pad;
#endif
	uint32_t m_size;

	CStringBlobBuf(size_t len = 0);
	CStringBlobBuf(const void* p, size_t len);
	CStringBlobBuf(size_t len, const void* buf, size_t copyLen);

	~CStringBlobBuf() noexcept {
		if (char* p = m_apChar)
			free(p);
	}

	void* AFXAPI operator new(size_t sz);
	__forceinline void operator delete(void* p) { free(p); }
	void* AFXAPI operator new(size_t sz, size_t len, bool);
	__forceinline void operator delete(void* p, size_t, bool) { free(p); }

	CStringBlobBuf* AsStringBlobBuf() { return this; }

	Char* GetBSTR() EXT_FAST_NOEXCEPT { return (Char*)(this + 1); } //!!! was BSTR
	size_t GetSize() EXT_FAST_NOEXCEPT { return m_size; }

	CStringBlobBuf* Clone();
	CStringBlobBuf* SetSize(size_t size);
	static CStringBlobBuf* AFXAPI RefEmptyBlobBuf();
};

#if UCFG_BLOB_POLYMORPHIC
	inline void Release(CBlobBufBase* bb) EXT_FAST_NOEXCEPT { // Allowed only if destructor is virtual
#else
	inline void Release(CStringBlobBuf* bb) EXT_FAST_NOEXCEPT {
#endif
		if (bb && !--(bb->m_aRef))
			delete bb;
	}

#if UCFG_WIN32 && UCFG_BLOB_POLYMORPHIC && UCFG_COM
class COleBlobBuf : public CBlobBufBase {
public:
	COleBlobBuf();
	~COleBlobBuf();
	void Init(size_t len, const void* buf = 0);
	void Init2(size_t len, const void* buf, size_t copyLen);
	void Empty();
	void Attach(BSTR bstr);
	BSTR Detach();

	Char* GetBSTR() { return m_bstr; }

	size_t GetSize() { return *((uint32_t*)GetBSTR() - 1); }

	CBlobBufBase* Clone();
	CBlobBufBase* SetSize(size_t size);

  private:
	BSTR m_bstr;
};
#endif

class EXTAPI Blob {
	typedef Blob class_type;

public:
	typedef const uint8_t* const_iterator;

#if UCFG_BLOB_POLYMORPHIC
	typedef CBlobBufBase impl_class;
#else
	typedef CStringBlobBuf impl_class;
#endif

	//!!!?	impl_class * volatile m_pData;				//!!!  not optimal  atomic only for some rare
	//!operations
	impl_class* m_pData; // atomic only for some rare operations

	Blob() : m_pData(CStringBlobBuf::RefEmptyBlobBuf()) {}

	Blob(const Blob& blob) noexcept;

	Blob(impl_class* pData) : m_pData(pData) {}

	Blob(std::nullptr_t) : m_pData(0) {}

	Blob(const void* buf, size_t len);
	Blob(RCSpan mb);
	Blob(const span<uint8_t>& mb);
#if UCFG_COM
	Blob(BSTR bstr);
#endif

#if UCFG_COM
	__forceinline Blob(const VARIANT& v) : m_pData(new CStringBlobBuf) { SetVariant(v); }

	operator COleVariant() const;
#endif

	~Blob();

	Blob(EXT_RV_REF(Blob) rv) : m_pData(exchange(rv.m_pData, nullptr)) {}

	Blob& operator=(EXT_RV_REF(Blob) rv) {
		Release(exchange(m_pData, exchange(rv.m_pData, nullptr)));
		return *this;
	}

	void swap(Blob& x) noexcept { std::swap(m_pData, x.m_pData); }

	operator Span() const noexcept {
		return m_pData ? Span(constData(), Size) : Span();
	}

	void AssignIfNull(const Blob& val);
	Blob& operator=(const Blob& val);
	Blob& operator=(RCSpan mb) { return operator=(Blob(mb)); }

	bool operator==(RCSpan cbuf) const { return Equal(operator Span(), cbuf); } //!!!O

	bool operator==(const Blob& blob) const noexcept;
	bool operator<(const Blob& blob) const noexcept;

	bool operator!=(const Blob& blob) const { return !operator==(blob); }

	Blob& operator+=(RCSpan mb);

	bool operator!() const { return !m_pData; }

	static Blob AFXAPI FromHexString(RCString s);

	size_t get_Size() const EXT_FAST_NOEXCEPT { return m_pData->GetSize(); }
	void put_Size(size_t size);
	DEFPROP_CONST(size_t, Size);

    size_t size() const { return m_pData->GetSize(); }

	size_t max_size() const noexcept { return SIZE_MAX - 2; }

	// we don't use property feature to explicit call constData() for efficiency
	uint8_t* data();
	__forceinline const uint8_t* data() const noexcept { return (const uint8_t*)m_pData->GetBSTR(); }
	__forceinline const uint8_t* constData() const noexcept { return (const uint8_t*)m_pData->GetBSTR(); }

	const uint8_t* begin() const { return constData(); }
	const uint8_t* end() const { return constData() + Size; }

	uint8_t operator[](size_t idx) const {
		if (idx >= Size)
			Throw(ExtErr::IndexOutOfRange);
		return constData()[idx];
	}

	uint8_t& operator[](size_t idx) {
		if (idx >= Size)
			Throw(ExtErr::IndexOutOfRange);
		return data()[idx];
	}

	void Replace(size_t offset, size_t size, RCSpan mb);

protected:
	void Cow();
#if UCFG_COM
	void SetVariant(const VARIANT& v);
#endif
	friend class String;
};

typedef const Blob& RCBlob;

inline void swap(Blob& x, Blob& y) noexcept {
	x.swap(y);
}

inline Blob operator+(RCSpan mb1, RCSpan mb2) {
	Blob r(0, mb1.size() + mb2.size());
	memcpy(r.data(), mb1.data(), mb1.size());
	memcpy(r.data() + mb1.size(), mb2.data(), mb2.size());
	return r;
}

template <class T> class StaticList : noncopyable {
  public:
	static T* Root;

	T* Next;

	StaticList() : Next(0) {}

  protected:
	explicit StaticList(bool) : Next(Root) { Root = static_cast<T*>(this); }
};

class ErrorCategoryBase : public std::error_category, public StaticList<ErrorCategoryBase> {
	typedef StaticList<ErrorCategoryBase> base;

public:
	const char* Name;
	int Facility;

	ErrorCategoryBase(const char* name, int facility = FACILITY_UNKNOWN);

	const char* name() const noexcept override { return Name; }

	static ErrorCategoryBase* AFXAPI GetRoot();
	static const error_category* AFXAPI Find(int fac);
};

} // namespace Ext

namespace std {
    inline size_t size(const Ext::Blob& blob) { return blob.Size; }

	EXT_API ostream& __stdcall operator<<(ostream& os, Ext::RCSpan cbuf);
	EXT_API wostream& __stdcall operator<<(wostream& os, Ext::RCSpan cbuf);

	inline ostream& __stdcall operator<<(ostream& os, const Ext::Blob& blob) { return os << Ext::Span(blob); }
	inline wostream& __stdcall operator<<(wostream& os, const Ext::Blob& blob) { return os << Ext::Span(blob); }
}

namespace EXT_HASH_VALUE_NS {
inline size_t hash_value(const Ext::Blob& blob) {
	return Ext::hash_value(blob.constData(), blob.Size);
}
} // namespace EXT_HASH_VALUE_NS

EXT_DEF_HASH(Ext::Blob)

namespace EXT_HASH_VALUE_NS {
inline size_t hash_value(Ext::RCSpan mb) {
	return Ext::hash_value(mb.data(), mb.size());
}
} // namespace EXT_HASH_VALUE_NS
