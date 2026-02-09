#pragma once

#include "windows_base.h"
#include "unknwn.h"

#define __objidl_h__
typedef interface IMalloc IMalloc;
typedef interface IMallocSpy IMallocSpy;
typedef IMalloc *LPMALLOC;
typedef IMallocSpy *LPMALLOCSPY;

DEFINE_GUID(IID_IMalloc, 0x00000002, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(IID_IMallocSpy, 0x0000001d, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);
#if defined(__cplusplus) && !defined(CINTERFACE)
struct IMalloc : public IUnknown
{
    virtual void * STDMETHODCALLTYPE Alloc(
        SIZE_T cb) = 0;

    virtual void * STDMETHODCALLTYPE Realloc(
        void *pv,
        SIZE_T cb) = 0;

    virtual void STDMETHODCALLTYPE Free(
        void *pv) = 0;

    virtual SIZE_T STDMETHODCALLTYPE GetSize(
        void *pv) = 0;

    virtual int STDMETHODCALLTYPE DidAlloc(
        void *pv) = 0;

    virtual void STDMETHODCALLTYPE HeapMinimize(
        ) = 0;

};

struct IMallocSpy : public IUnknown
{
    virtual SIZE_T STDMETHODCALLTYPE PreAlloc(
        SIZE_T cbRequest) = 0;

    virtual void * STDMETHODCALLTYPE PostAlloc(
        void *pActual) = 0;

    virtual void * STDMETHODCALLTYPE PreFree(
        void *pRequest,
        WINBOOL fSpyed) = 0;

    virtual void STDMETHODCALLTYPE PostFree(
        WINBOOL fSpyed) = 0;

    virtual SIZE_T STDMETHODCALLTYPE PreRealloc(
        void *pRequest,
        SIZE_T cbRequest,
        void **ppNewRequest,
        WINBOOL fSpyed) = 0;

    virtual void * STDMETHODCALLTYPE PostRealloc(
        void *pActual,
        WINBOOL fSpyed) = 0;

    virtual void * STDMETHODCALLTYPE PreGetSize(
        void *pRequest,
        WINBOOL fSpyed) = 0;

    virtual SIZE_T STDMETHODCALLTYPE PostGetSize(
        SIZE_T cbActual,
        WINBOOL fSpyed) = 0;

    virtual void * STDMETHODCALLTYPE PreDidAlloc(
        void *pRequest,
        WINBOOL fSpyed) = 0;

    virtual int STDMETHODCALLTYPE PostDidAlloc(
        void *pRequest,
        WINBOOL fSpyed,
        int fActual) = 0;

    virtual void STDMETHODCALLTYPE PreHeapMinimize(
        ) = 0;

    virtual void STDMETHODCALLTYPE PostHeapMinimize(
        ) = 0;

};
#else
typedef struct IMallocSpyVtbl {
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IMallocSpy *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        IMallocSpy *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        IMallocSpy *This);

    SIZE_T (STDMETHODCALLTYPE *PreAlloc)(
        IMallocSpy *This,
        SIZE_T cbRequest);

    void * (STDMETHODCALLTYPE *PostAlloc)(
        IMallocSpy *This,
        void *pActual);

    void * (STDMETHODCALLTYPE *PreFree)(
        IMallocSpy *This,
        void *pRequest,
        WINBOOL fSpyed);

    void (STDMETHODCALLTYPE *PostFree)(
        IMallocSpy *This,
        WINBOOL fSpyed);

    SIZE_T (STDMETHODCALLTYPE *PreRealloc)(
        IMallocSpy *This,
        void *pRequest,
        SIZE_T cbRequest,
        void **ppNewRequest,
        WINBOOL fSpyed);

    void * (STDMETHODCALLTYPE *PostRealloc)(
        IMallocSpy *This,
        void *pActual,
        WINBOOL fSpyed);

    void * (STDMETHODCALLTYPE *PreGetSize)(
        IMallocSpy *This,
        void *pRequest,
        WINBOOL fSpyed);

    SIZE_T (STDMETHODCALLTYPE *PostGetSize)(
        IMallocSpy *This,
        SIZE_T cbActual,
        WINBOOL fSpyed);

    void * (STDMETHODCALLTYPE *PreDidAlloc)(
        IMallocSpy *This,
        void *pRequest,
        WINBOOL fSpyed);

    int (STDMETHODCALLTYPE *PostDidAlloc)(
        IMallocSpy *This,
        void *pRequest,
        WINBOOL fSpyed,
        int fActual);

    void (STDMETHODCALLTYPE *PreHeapMinimize)(
        IMallocSpy *This);

    void (STDMETHODCALLTYPE *PostHeapMinimize)(
        IMallocSpy *This);

    END_INTERFACE
} IMallocSpyVtbl;

interface IMallocSpy {
    CONST_VTBL IMallocSpyVtbl* lpVtbl;
};

typedef struct IMallocVtbl {
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IMalloc *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        IMalloc *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        IMalloc *This);

    void * (STDMETHODCALLTYPE *Alloc)(
        IMalloc *This,
        SIZE_T cb);

    void * (STDMETHODCALLTYPE *Realloc)(
        IMalloc *This,
        void *pv,
        SIZE_T cb);

    void (STDMETHODCALLTYPE *Free)(
        IMalloc *This,
        void *pv);

    SIZE_T (STDMETHODCALLTYPE *GetSize)(
        IMalloc *This,
        void *pv);

    int (STDMETHODCALLTYPE *DidAlloc)(
        IMalloc *This,
        void *pv);

    void (STDMETHODCALLTYPE *HeapMinimize)(
        IMalloc *This);

    END_INTERFACE
} IMallocVtbl;

interface IMalloc {
    CONST_VTBL IMallocVtbl* lpVtbl;
};

#define IMalloc_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IMalloc_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IMalloc_Release(This) (This)->lpVtbl->Release(This)
#define IMalloc_Alloc(This,cb) (This)->lpVtbl->Alloc(This,cb)
#define IMalloc_Realloc(This,pv,cb) (This)->lpVtbl->Realloc(This,pv,cb)
#define IMalloc_Free(This,pv) (This)->lpVtbl->Free(This,pv)
#define IMalloc_GetSize(This,pv) (This)->lpVtbl->GetSize(This,pv)
#define IMalloc_DidAlloc(This,pv) (This)->lpVtbl->DidAlloc(This,pv)
#define IMalloc_HeapMinimize(This) (This)->lpVtbl->HeapMinimize(This)

#define IMallocSpy_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define IMallocSpy_AddRef(This) (This)->lpVtbl->AddRef(This)
#define IMallocSpy_Release(This) (This)->lpVtbl->Release(This)
#define IMallocSpy_PreAlloc(This,cbRequest) (This)->lpVtbl->PreAlloc(This,cbRequest)
#define IMallocSpy_PostAlloc(This,pActual) (This)->lpVtbl->PostAlloc(This,pActual)
#define IMallocSpy_PreFree(This,pRequest,fSpyed) (This)->lpVtbl->PreFree(This,pRequest,fSpyed)
#define IMallocSpy_PostFree(This,fSpyed) (This)->lpVtbl->PostFree(This,fSpyed)
#define IMallocSpy_PreRealloc(This,pRequest,cbRequest,ppNewRequest,fSpyed) (This)->lpVtbl->PreRealloc(This,pRequest,cbRequest,ppNewRequest,fSpyed)
#define IMallocSpy_PostRealloc(This,pActual,fSpyed) (This)->lpVtbl->PostRealloc(This,pActual,fSpyed)
#define IMallocSpy_PreGetSize(This,pRequest,fSpyed) (This)->lpVtbl->PreGetSize(This,pRequest,fSpyed)
#define IMallocSpy_PostGetSize(This,cbActual,fSpyed) (This)->lpVtbl->PostGetSize(This,cbActual,fSpyed)
#define IMallocSpy_PreDidAlloc(This,pRequest,fSpyed) (This)->lpVtbl->PreDidAlloc(This,pRequest,fSpyed)
#define IMallocSpy_PostDidAlloc(This,pRequest,fSpyed,fActual) (This)->lpVtbl->PostDidAlloc(This,pRequest,fSpyed,fActual)
#define IMallocSpy_PreHeapMinimize(This) (This)->lpVtbl->PreHeapMinimize(This)
#define IMallocSpy_PostHeapMinimize(This) (This)->lpVtbl->PostHeapMinimize(This)

#endif // __cplusplus

DECLARE_UUIDOF_HELPER(IMalloc, 0x00000002, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46)
DECLARE_UUIDOF_HELPER(IMallocSpy, 0x0000001d, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46);
