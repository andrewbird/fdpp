/*
 *  FDPP - freedos port to modern C++
 *  Copyright (C) 2018  Stas Sergeev (stsp)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FARPTR_HPP
#define FARPTR_HPP

#include <type_traits>
#include <new>
#include <memory>
#include <cstring>
#include <unordered_set>
//#include "cppstubs.hpp"
#include "thunks_priv.h"
#include "farhlp.hpp"

static inline far_s __lookup_far(fh1 *fh, const void *ptr)
{
    _assert(ptr == fh->ptr);
    far_s f = fh->f;
    _assert(f.seg || f.off);
    return f;
}

static inline far_s _lookup_far(const void *ptr)
{
    return __lookup_far(&g_farhlp1, ptr);
}

static inline void __store_far(fh1 *fh, const void *ptr, far_s fptr)
{
    fh->ptr = ptr;
    fh->f = fptr;
}

static inline void _store_far(const void *ptr, far_s fptr)
{
    __store_far(&g_farhlp1, ptr, fptr);
}

static inline far_s do_lookup_far(const void *ptr)
{
    return __lookup_far(&g_farhlp2, ptr);
}

static inline void do_store_far(far_s fptr)
{
    __store_far(&g_farhlp2, resolve_segoff(fptr), fptr);
}

#define _MK_S(s, o) (far_s){o, s}

#define _P(T1) std::is_pointer<T1>::value
#define _C(T1) std::is_const<T1>::value
#define _RP(T1) typename std::remove_pointer<T1>::type
#define _RC(T1) typename std::remove_const<T1>::type
template<typename> class SymWrp;
template<typename> class SymWrp2;

template<typename T>
class WrpType {
public:
    using type = typename std::conditional<std::is_class<T>::value,
        SymWrp<T>, SymWrp2<T>>::type;
    using ref_type = typename std::add_lvalue_reference<type>::type;
};

#define ALLOW_CNV(T0, T1) (( \
        std::is_void<T0>::value || \
        std::is_void<T1>::value || \
        std::is_same<_RC(T0), char>::value || \
        std::is_same<_RC(T1), char>::value || \
        std::is_same<_RC(T0), unsigned char>::value || \
        std::is_same<_RC(T1), unsigned char>::value) && \
        !std::is_same<T0, T1>::value && \
        (_C(T1) || !_C(T0)))

template<typename T>
class FarPtrBase {
protected:
    far_s ptr;

    using wrp_type = typename WrpType<T>::type;
    void do_adjust_far() {
        if (ptr.seg == 0xffff)
            return;
        ptr.seg += ptr.off >> 4;
        ptr.off &= 0xf;
    }
public:
    FarPtrBase() = default;
    FarPtrBase(uint16_t s, uint16_t o) : ptr(_MK_S(s, o)) {}
    FarPtrBase(std::nullptr_t) : ptr(_MK_S(0, 0)) {}
    explicit FarPtrBase(const far_s& f) : ptr(f) {}

    template<typename T0, typename T1 = T,
        typename std::enable_if<ALLOW_CNV(T0, T1)>::type* = nullptr>
    FarPtrBase(const FarPtrBase<T0>& f) : ptr(_MK_S(f.seg(), f.off())) {}

    T* operator ->() {
        do_store_far(get_far());
        return (T*)resolve_segoff_fd(ptr);
    }
    operator T*() {
        static_assert(std::is_standard_layout<T>::value ||
                std::is_void<T>::value, "need std layout");
        if (!ptr.seg && !ptr.off)
            return NULL;
        return (T*)resolve_segoff_fd(ptr);
    }
    template<typename T0>
    explicit operator T0*() {
        if (!ptr.seg && !ptr.off)
            return NULL;
        return (T0*)resolve_segoff_fd(ptr);
    }

    wrp_type& get_wrp() {
        wrp_type *s = new(get_buf()) wrp_type;
        _store_far(s, get_far());
        return *s;
    }
    wrp_type& operator [](int idx) {
        return FarPtrBase<T>(*this + idx).get_wrp();
    }

    FarPtrBase<T> operator ++(int) {
        FarPtrBase<T> f = *this;
        ptr.off += sizeof(T);
        return f;
    }
    FarPtrBase<T> operator --(int) {
        FarPtrBase<T> f = *this;
        ptr.off -= sizeof(T);
        return f;
    }
    FarPtrBase<T> operator ++() {
        ptr.off += sizeof(T);
        return *this;
    }
    FarPtrBase<T> operator --() {
        ptr.off -= sizeof(T);
        return *this;
    }
    FarPtrBase<T> operator +=(int inc) { ptr.off += inc * sizeof(T); return *this; }
    FarPtrBase<T> operator -=(int dec) { ptr.off -= dec * sizeof(T); return *this; }
    FarPtrBase<T> operator +(int inc) { return FarPtrBase<T>(ptr.seg, ptr.off + inc * sizeof(T)); }
    FarPtrBase<T> operator -(int dec) { return FarPtrBase<T>(ptr.seg, ptr.off - dec * sizeof(T)); }
    bool operator == (std::nullptr_t) { return (!ptr.seg && !ptr.off); }
    bool operator != (std::nullptr_t) { return (ptr.seg || ptr.off); }
    uint16_t seg() const { return ptr.seg; }
    uint16_t off() const { return ptr.off; }
    uint32_t get_fp32() const { return ((ptr.seg << 16) | ptr.off); }
    far_s get_far() const { return ptr; }
    far_s& get_ref() { return ptr; }
    const far_s *get_addr() { return &ptr; }
    T* get_ptr() { return (T*)resolve_segoff(ptr); }
    void *get_buf() { return (void*)resolve_segoff(ptr); }
    explicit operator uint32_t () const { return get_fp32(); }
};

class ObjIf {
public:
    virtual far_s get_obj() = 0;
    virtual void ref(const void *owner) = 0;
    virtual ~ObjIf() = default;
};

template <typename T> class FarObj;
#define __Sx(x) #x
#define _Sx(x) __Sx(x)
#define NM __FILE__ ":" _Sx(__LINE__)
#define _RR(t) typename std::remove_reference<t>::type
#define _MK_FAR(o) std::make_shared<FarObj<_RR(decltype(o))>>(o, NM)
#define _MK_FAR_S(o) std::make_shared<FarObj<char>>(o, strlen(o) + 1, false, NM)
#define _MK_FAR_CS(o) std::make_shared<FarObj<const char>>(o, strlen(o) + 1, true, NM)

template<typename T>
class FarPtr : public FarPtrBase<T>
{
    typedef std::shared_ptr<ObjIf> sh_obj;
    sh_obj obj;
    bool nonnull = false;

public:
    /* first, tell a few secret phrases to the compiler :) */
    template <typename> friend class FarPtr;
    using FarPtrBase<T>::FarPtrBase;
    FarPtr() = default;

    FarPtr(const FarPtrBase<T>& f) : FarPtrBase<T>(f) {}
    explicit FarPtr(uint32_t f) : FarPtrBase<T>(f >> 16, f & 0xffff) {}
    explicit FarPtr(const sh_obj& o) :
            FarPtrBase<T>(o->get_obj()), obj(o) {}
    FarPtr(const FarPtr<T>& f) = default;
    FarPtr(uint16_t s, uint16_t o, bool nnull) :
            FarPtrBase<T>(_MK_S(s, o)), nonnull(nnull) {}

    template<typename T0, typename T1 = T,
        typename std::enable_if<ALLOW_CNV(T0, T1)>::type* = nullptr>
    FarPtr(const FarPtrBase<T0>& f) : FarPtrBase<T1>(f.seg(), f.off()) {}
    template<typename T0, typename T1 = T,
        typename std::enable_if<ALLOW_CNV(T0, T1)>::type* = nullptr>
    FarPtr(const FarPtr<T0>& f) : FarPtrBase<T1>(f._seg_(), f._off_()),
        obj(f.obj), nonnull(f.nonnull) {}

    template<typename T0, typename T1 = T,
        typename std::enable_if<!ALLOW_CNV(T0, T1)>::type* = nullptr>
    explicit FarPtr(const FarPtrBase<T0>& f) : FarPtrBase<T1>(f.seg(), f.off()) {}
    template<typename T0, typename T1 = T,
        typename std::enable_if<!ALLOW_CNV(T0, T1)>::type* = nullptr>
    explicit FarPtr(const FarPtr<T0>& f) : FarPtrBase<T1>(f._seg_(), f._off_()),
        obj(f.obj), nonnull(f.nonnull) {}

    template<typename T0, typename T1 = T,
        typename std::enable_if<ALLOW_CNV(T1, T0) &&
        _C(T0) == _C(T1)>::type* = nullptr>
    operator FarPtrBase<T0>() const & {
        return FarPtrBase<T0>(this->_seg_(), this->_off_());
    }
    template<typename T0, typename T1 = T,
        typename std::enable_if<ALLOW_CNV(T1, T0) &&
        _C(T0) == _C(T1)>::type* = nullptr>
    operator FarPtrBase<T0>() && {
        _assert(!obj);
        return FarPtrBase<T0>(this->seg(), this->off());
    }

    template<typename T0, typename T1 = T,
        typename std::enable_if<ALLOW_CNV(T1, T0) &&
        /* adding constness is not allowed here for the quite tricky
         * reason that bothers only clang but not gcc:
         * https://bugs.llvm.org/show_bug.cgi?id=36131
         * The implicit cast to an arbitrary ptr can be triggered by
         * the dereference operator*. But in clang there are 2 of them:
         * - built-in candidate operator*(T *)
         * - built-in candidate operator*(const T *)
         * ... or even more.
         * We need to select the one that matches constness.
         * Hope this does not bring any limitations because the
         * missing constness will be implicitly added later as needed
         * (by operator= for example). */
        _C(T0) == _C(T1)>::type* = nullptr>
    operator T0*() { return (T0*)resolve_segoff_fd(this->ptr); }
    template<typename T0, typename T1 = T,
        typename std::enable_if<!ALLOW_CNV(T1, T0)>::type* = nullptr>
    explicit operator T0*() { return (T0*)resolve_segoff_fd(this->ptr); }

    using FarPtrBase<T>::operator ==;
    template <typename T0, typename T1 = T,
        typename std::enable_if<!std::is_void<T0>::value>::type* = nullptr,
        typename std::enable_if<std::is_void<T1>::value>::type* = nullptr>
    bool operator == (const FarPtrBase<T0>& f) const {
        return ((f.seg() == this->ptr.seg) && (f.off() == this->ptr.off));
    }

    /* below is a bit tricky. If we have a dosobj here, then we
     * should use the "owner" form to link the objects together.
     * But if the object is const, we don't have to link it to
     * the owner, because the fwd copy is already done and bwd
     * copy is not needed. So the simple form can be used then. */
    uint16_t seg() const {
        _assert(!obj || std::is_const<T>::value);
        return this->ptr.seg;
    }
    uint16_t off() const {
        _assert(!obj || std::is_const<T>::value);
        return this->ptr.off;
    }
    uint16_t seg(void *owner) const {
        _assert(obj);
        obj->ref(owner);
        return this->ptr.seg;
    }
    uint16_t off(void *owner) const {
        _assert(obj);
        obj->ref(owner);
        return this->ptr.off;
    }
    uint16_t _seg_() const { return this->ptr.seg; }
    uint16_t _off_() const { return this->ptr.off; }
    operator T*() {
        static_assert(std::is_standard_layout<T>::value ||
                std::is_void<T>::value, "need std layout");
        if (!nonnull && !this->ptr.seg && !this->ptr.off)
            return NULL;
        return (T*)resolve_segoff_fd(this->ptr);
    }
    FarPtr<T>& adjust_far() { this->do_adjust_far(); return *this; }
};

template<typename, int, typename P, char (P::*)[0]> class ArMemb;

template<typename T>
class XFarPtr : public FarPtr<T>
{
public:

    using FarPtr<T>::FarPtr;
    XFarPtr() = delete;
    XFarPtr(FarPtr<T>& f) : FarPtr<T>(f) {}

    template<typename T1 = T, int max_len, typename P, char (P::*M)[0]>
    XFarPtr(ArMemb<T1, max_len, P, M> &p) : FarPtr<T>(p) {}

    /* The below ctors are safe wrt implicit conversions as they take
     * the lvalue reference to the pointer, not just the pointer itself.
     * This is the reason why they exist at all. */
    template<typename T1 = T,
        typename std::enable_if<
          !std::is_same<T1, char>::value &&
          !std::is_same<T1, const char>::value
        >::type* = nullptr>
    XFarPtr(T1 *&p) : FarPtr<T>(_MK_FAR(*p)) {}
    template<typename T1 = T,
        typename std::enable_if<
          std::is_same<T1, char>::value ||
          std::is_same<T1, const char>::value ||
          std::is_void<T1>::value
        >::type* = nullptr>
    XFarPtr(char *&s) : FarPtr<T>(_MK_FAR_S(s)) {}
    template<typename T1 = T,
        typename std::enable_if<
          std::is_same<T1, const char>::value ||
          std::is_same<T1, const void>::value
        >::type* = nullptr>
    XFarPtr(const char *&s) : FarPtr<T>(_MK_FAR_CS(s)) {}
    template<typename T1 = T, int N,
        typename std::enable_if<!std::is_void<T1>::value>::type* = nullptr>
    XFarPtr(T1 (&p)[N]) : FarPtr<T>(_MK_FAR(p)) {}
};

#define _MK_F(f, s) f(s)

/* These SymWrp are tricky, and are needed only because we
 * can't provide 'operator.':
 * https://isocpp.org/blog/2016/02/a-bit-of-background-for-the-operator-dot-proposal-bjarne-stroustrup
 * Consider the following code (1):
 * void FAR *f = &fp[idx];
 * In this case & should create "void FAR *" from ref, not just "void *".
 * Wrapper helps with this.
 * And some other code (2) does this:
 * int a = fp[idx].a_memb;
 * in which case we should better have no wrapper.
 * Getting both cases to work together is challenging.
 * Note that the simplest solution "operator T &()" for case 2
 * currently doesn't work, but it may work in the future:
 * http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0352r1.pdf
 * But I don't think waiting for this document to materialize in
 * gcc/g++ is a good idea, as it is not even a part of the upcoming C++20.
 * So what I do is a public inheritance of T. This kind of works,
 * but puts an additional restrictions on the wrapper, namely, that
 * it should be a POD, it should not add any data members and it should
 * be non-copyable.
 * Fortunately the POD requirement is satisfied with 'ctor = default'
 * trick, and non-copyable is simple, but having no data members means
 * I can't easily retrieve the far ptr for case 1.
 * So the only solution I can come up with, is to put the static
 * map somewhere that will allow to look up the "lost" far pointers.
 * Thus farhlp.cpp
 */
template<typename T>
class SymWrp : public T {
public:
    SymWrp() = default;
    SymWrp(const SymWrp&) = delete;
    SymWrp<T>& operator =(T& f) { *(T *)this = f; return *this; }
    FarPtr<T> operator &() const { return _MK_F(FarPtr<T>, _lookup_far(this)); }
};

template<typename T>
class SymWrp2 {
    /* remove const or default ctor will be deleted */
    _RC(T) sym;

public:
    SymWrp2() = default;
    SymWrp2(const SymWrp2&) = delete;
    SymWrp2<T>& operator =(const T& f) { sym = f; return *this; }
    FarPtr<T> operator &() const { return _MK_F(FarPtr<T>, _lookup_far(this)); }
    operator T &() { return sym; }
    /* for fmemcpy() etc that need const void* */
    template <typename T1 = T,
        typename std::enable_if<_P(T1) &&
        !std::is_void<_RP(T1)>::value>::type* = nullptr>
    operator FarPtr<const void> () const {
        return _MK_F(FarPtr<const void>, _lookup_far(this));
    }
};

template<typename T> class AsmSym;
template<typename T>
class AsmRef {
    FarPtr<T> *sym;

public:
    AsmRef(FarPtr<T> *s) : sym(s) {}
    T* operator ->() { return *sym; }
    operator FarPtr<T> () { return *sym; }
    template <typename T1 = T,
        typename std::enable_if<!std::is_void<T1>::value>::type* = nullptr>
    operator FarPtr<void> () { return FarPtr<void>(*sym); }
    uint16_t seg() const { return sym->seg(); }
    uint16_t off() const { return sym->off(); }
};

template<typename T>
class AsmSym {
    FarPtr<T> sym;

public:
    using sym_type = typename WrpType<T>::ref_type;
    sym_type get_sym() { return sym.get_wrp(); }
    AsmRef<T> get_addr() { return AsmRef<T>(&sym); }

    /* everyone with get_ref() method should have no copy ctor */
    AsmSym() = default;
    AsmSym(const AsmSym<T> &) = delete;
    far_s* get_ref() { return &sym.get_ref(); }
};

template<typename T>
class AsmFSym {
    FarPtr<T> sym;

public:
    using sym_type = decltype(sym);
    sym_type get_sym() { return sym; }

    AsmFSym() = default;
    AsmFSym(const AsmFSym<T> &) = delete;
    far_s* get_ref() { return &sym.get_ref(); }
};

class CallSym {
    FarPtrBase<void> ptr;

public:
    CallSym(const FarPtrBase<void>& f) { ptr = f; }
    void operator()() { thunk_call_void_noret(ptr.get_far()); }
};

class AsmCSym {
    FarPtrBase<void> ptr;

public:
    AsmCSym(const FarPtrBase<void>& f) : ptr(f) {}
    CallSym operator *() { return CallSym(ptr); }
    void operator()() { thunk_call_void(ptr.get_far()); }
};

template<typename T, uint16_t (*SEG)(void)>
class NearPtr {
    uint16_t _off;

public:
    explicit NearPtr(uint16_t o) : _off(o) {}    // for farobj only
    NearPtr(std::nullptr_t) : _off(0) {}
    explicit operator uint16_t () { return _off; }
    operator T *() { return FarPtr<T>(SEG(), _off); }
    NearPtr<T, SEG> operator - (const NearPtr<T, SEG>& n) const {
        return NearPtr<T, SEG>(_off - n.off());
    }
    NearPtr<T, SEG> operator +=(const int inc) {
        _off += inc * sizeof(T);
        return *this;
    }
    NearPtr<T, SEG> operator -=(const int dec) {
        _off -= dec * sizeof(T);
        return *this;
    }
    bool operator == (std::nullptr_t) { return !_off; }
    bool operator != (std::nullptr_t) { return !!_off; }
    uint16_t off() const { return _off; }

    NearPtr() = default;
};

template<typename T, int max_len = 0>
class ArSymBase {
protected:
    FarPtrBase<T> sym;

    using wrp_type = typename WrpType<T>::type;
public:
    wrp_type& operator [](int idx) {
        _assert(!max_len || idx < max_len);
        return sym[idx];
    }

    far_s* get_ref() { return &sym.get_ref(); }
};

template <typename T, char (T::*M)[0]>
struct offset_of {
    constexpr operator size_t() const {
        return (std::uintptr_t)&(((T*)nullptr)->*M);
    }
};
template<typename T, typename P, char (P::*M)[0], int O = 0>
class MembBase {
protected:
    FarPtr<T> lookup_sym() const {
        FarPtr<T> fp;
        int off = offset_of<P, M>() + O;
        /* find parent first */
        const uint8_t *ptr = (const uint8_t *)this - off;
        far_s f = lookup_far_st(ptr);
        if (!f.seg && !f.off)
            f = do_lookup_far(ptr);
        _assert(f.seg || f.off);
        fp = _MK_F(FarPtr<uint8_t>, f) + off;
        return fp;
    }
};

template<typename T, int max_len, typename P, char (P::*M)[0]>
class ArMemb : public MembBase<T, P, M> {
    T sym[max_len];

    using wrp_type = typename WrpType<T>::type;
public:
    using type = T;
    static constexpr decltype(max_len) len = max_len;

    template <typename T1 = T,
        typename std::enable_if<!_C(T1)>::type* = nullptr>
    operator FarPtr<void> () { return this->lookup_sym(); }
    template <typename T1 = T,
        typename std::enable_if<!_C(T1)>::type* = nullptr>
    operator FarPtr<const T1> () { return this->lookup_sym(); }
    operator FarPtr<T> () { return this->lookup_sym(); }
    template <uint16_t (*SEG)(void)>
    operator NearPtr<T, SEG> () {
        FarPtr<T> f = this->lookup_sym();
        _assert(f.seg() == SEG());
        return NearPtr<T, SEG>(f.off());
    }
    operator T *() { return sym; }
    template <typename T0, typename T1 = T,
        typename std::enable_if<!std::is_same<T0, T1>::value>::type* = nullptr>
    explicit operator T0 *() { return (T0 *)sym; }
    FarPtr<T> operator +(int inc) { return this->lookup_sym() + inc; }
    FarPtr<T> operator -(int dec) { return this->lookup_sym() - dec; }

    wrp_type& operator [](int idx) {
        _assert(!max_len || idx < max_len);
        FarPtr<T> f = this->lookup_sym();
        return f[idx];
    }

    ArMemb() = default;
};

template<typename T, int max_len = 0>
class AsmArNSym : public ArSymBase<T, max_len> {
public:
    using sym_type = T*;
    sym_type get_sym() { return this->sym.get_ptr(); }

    AsmArNSym() = default;
    AsmArNSym(const AsmArNSym<T> &) = delete;
};

template<typename T, int max_len = 0>
class AsmArFSym : public ArSymBase<T, max_len> {
public:
    using sym_type = FarPtr<T>;
    sym_type get_sym() { return this->sym; }

    AsmArFSym() = default;
    AsmArFSym(const AsmArFSym<T> &) = delete;
};

template<typename T>
class FarPtrAsm {
    FarPtr<FarPtrBase<T>> ptr;

public:
    explicit FarPtrAsm(const FarPtr<FarPtrBase<T>>& f) : ptr(f) {}
    operator FarPtrBase<T> *() { return ptr.get_ptr(); }
    /* some apps do the following: *(UWORD *)&f_ptr = new_offs; */
    explicit operator uint16_t *() { return &ptr->get_ref().off; }
    uint16_t seg() const { return ptr.seg(); }
    uint16_t off() const { return ptr.off(); }
};

template<typename T>
class AsmFarPtr {
    FarPtr<FarPtrBase<T>> ptr;

public:
    using sym_type = FarPtrBase<T>&;
    sym_type get_sym() { return *ptr.get_ptr(); }
    FarPtrAsm<T> get_addr() { return FarPtrAsm<T>(ptr); }

    AsmFarPtr() = default;
    AsmFarPtr(const AsmFarPtr<T> &) = delete;
    far_s* get_ref() { return &ptr.get_ref(); }
};

template<typename T, uint16_t (*SEG)(void)>
class AsmNearPtr {
    FarPtr<NearPtr<T, SEG>> ptr;

public:
    using sym_type = NearPtr<T, SEG>&;
    sym_type get_sym() { return *ptr.get_ptr(); }

    AsmNearPtr() = default;
    AsmNearPtr(const AsmNearPtr<T, SEG> &) = delete;
    far_s* get_ref() { return &ptr.get_ref(); }
};

template<typename T, typename P, char (P::*M)[0], int O = 0>
class SymMemb : public T, public MembBase<T, P, M, O> {
public:
    SymMemb() = default;
    SymMemb(const SymMemb&) = delete;
    T& operator =(const T& f) { *(T *)this = f; return *this; }
    FarPtr<T> operator &() const { return this->lookup_sym(); }
};

template<typename T, typename P, char (P::*M)[0], int O = 0>
class SymMemb2 : public MembBase<T, P, M, O> {
    T sym;

public:
    SymMemb2() = default;
    SymMemb2(const SymMemb2&) = delete;
    T& operator =(const T& f) { sym = f; return sym; }
    FarPtr<T> operator &() const { return this->lookup_sym(); }
    operator T &() { return sym; }
};

#undef _P
#undef _C
#undef _RP
#undef _RC

#define __ASMSYM(t) AsmSym<t>
#define __ASMFSYM(t) AsmFSym<t>
#define __ASMARSYM(t, l) AsmArFSym<t, l>
#define __ASMARISYM(t) AsmArNSym<t>
#define __ASMARIFSYM(t) AsmArFSym<t>
#define __FAR(t) FarPtr<t>
#define __XFAR(t) XFarPtr<t>
#define __ASMFAR(t) AsmFarPtr<t>
#define __ASMNEAR(t, s) AsmNearPtr<t, s>
#define __ASMREF(f) f.get_ref()
#define __ASMADDR(v) __##v.get_addr()
#define __ASMCALL(f) AsmCSym f
#define __ASYM(x) x.get_sym()
#define ASMREF(t) AsmRef<t>
#define DUMMY_MARK(n) char off_##n[0]
#define AR_MEMB(p, t, n, l) \
    DUMMY_MARK(n); \
    ArMemb<t, l, p, &p::off_##n> n
#define SYM_MEMB(p, t, n) \
    DUMMY_MARK(n); \
    SymMemb<t, p, &p::off_##n> n
#define SYM_MEMB2(p, m, o, t, n) \
    SymMemb<t, p, &p::off_##m, o> n
#define SYM_MEMB_T(p, t, n) \
    DUMMY_MARK(n); \
    SymMemb2<t, p, &p::off_##n> n
#define FP_SEG(fp) ((fp).seg())
#define FP_OFF(fp) ((fp).off())
#define FP_SEG_OBJ(o, fp) ((fp).seg(o))
#define FP_OFF_OBJ(o, fp) ((fp).off(o))
#define MK_FP(seg, ofs) ({ \
    uint16_t _s = seg; \
    uint16_t _o = ofs; \
    __FAR(void)(_s, _o); \
})
#define MK_FP_N(seg, ofs) (__FAR(void)(seg, ofs, true))
#define __DOSFAR(t) FarPtrBase<t>
#define _MK_DOS_FP(t, s, o) __FAR(t)(MK_FP(s, o))
#define GET_FP32(f) (f).get_fp32()
#define GET_FAR(f) (f).get_far()
#define GET_PTR(f) (f).get_ptr()
#define ADJUST_FAR(f) (f).adjust_far()

#undef NULL
#define NULL           nullptr

#endif
