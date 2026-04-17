//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Display/Rtt_CPUResourcePool.h"
#include "Renderer/Rtt_CPUResource.h"

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

CPUResourcePool::CPUResourcePool()
{
	
}

CPUResourcePool::~CPUResourcePool()
{
	// Detach all resources to prevent UAF when CPUResource destructors call
	// DetachObserver() after this pool is freed. This happens during shutdown:
	// ~Display deletes Renderer (owns this pool) before deleting Scene (owns
	// CPUResource orphanage arrays). We swap to a local copy because
	// DetachObserver() calls back into DetachResource() which modifies the map.
	std::map<const CPUResource*,CPUResource*> resources;
	resources.swap(fCPUResources);
	for(auto iter = resources.begin(); iter != resources.end(); ++iter)
	{
		// Clear both observer and renderer pointers to prevent UAF.
		// The Renderer that owns this pool is being destroyed, so any
		// CPUResource that outlives us (e.g. in Scene's orphanage) must
		// not reference either this pool or the Renderer.
		iter->second->DetachObserver();
		iter->second->SetRenderer(NULL);
	}
}
void CPUResourcePool::ReleaseGPUResources()
{
	for(std::map<const CPUResource*,CPUResource*>::iterator iter = fCPUResources.begin(); iter != fCPUResources.end(); ++iter)
	{
		iter->second->ReleaseGPUResource();
	}
}
void CPUResourcePool::AttachResource(CPUResource *resource)
{
	const CPUResource *ref = static_cast<const CPUResource*>(resource);
	fCPUResources[ref] = resource;

}
void CPUResourcePool::DetachResource(CPUResource *resource)
{
	fCPUResources.erase(resource);
}

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

