#include <initguid.h>
#include <dplay8.h>
#include <dpaddr.h>
#include <netinet/in.h>

#include <atomic>

#include "DirectPlay8Address.hpp"
#include "DirectPlay8Peer.hpp"

DEFINE_GUID(GUID_NULL, 0x00000000,0x0000,0x0000,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
DEFINE_GUID(IID_IUnknown, 0x00000000,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);

static std::atomic<unsigned int> global_refcount;

__attribute__ ((visibility ("default")))
HRESULT DirectPlay8Create(const GUID* pcIID, LPVOID* ppvInterface, IUnknown*)
{
	if (!IsEqualIID(*pcIID, IID_IDirectPlay8Peer) && !IsEqualIID(*pcIID, CLSID_DirectPlay8Peer))
		return E_NOINTERFACE;

	*(IDirectPlay8Peer**)ppvInterface = new DirectPlay8Peer(&global_refcount);
	return S_OK;
}

__attribute__ ((visibility ("default")))
HRESULT DirectPlay8AddressCreate(const GUID* pcIID, LPVOID* ppvInterface, IUnknown*)
{
	if (!IsEqualIID(*pcIID, IID_IDirectPlay8Address) && !IsEqualIID(*pcIID, CLSID_DirectPlay8Address))
		return E_NOINTERFACE;

	*(IDirectPlay8Address**)ppvInterface = new DirectPlay8Address(&global_refcount);
	return S_OK;
}
