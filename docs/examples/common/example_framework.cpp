/*
  libjobs - Simple coroutine based job scheduling.
  Copyright (C) 2019 Tim Leonard <me@timleonard.uk>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <jobs.h>
#include <stdio.h>

#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)
#   include <pix/include/WinPixEventRuntime/pix3.h>
#elif defined(JOBS_PLATFORM_PS4)
#   include <razorcpu.h>
#endif

#if defined(JOBS_PLATFORM_XBOX_ONE)
#   include <xdk.h>
#   include <wrl.h>
#   include <ppltasks.h>
    using namespace concurrency;
    using namespace Windows::ApplicationModel;
    using namespace Windows::ApplicationModel::Core;
    using namespace Windows::ApplicationModel::Activation;
    using namespace Windows::UI::Core;
    using namespace Windows::Foundation;
#endif

#if defined(JOBS_PLATFORM_PS4)

// Ensure we set the libc heap to grow on ps4, otherwise we will just oom
// after creating a few fiber stacks. Ideally the memory alloc functions should
// be overriden and allocations should take place through garlic/onion memory pools.
size_t sceLibcHeapSize = SCE_LIBC_HEAP_SIZE_EXTENDED_ALLOC_NO_LIMIT;
unsigned int sceLibcHeapExtendedAlloc = 1;

#endif

void jobsMain();

// ----------------------------------------------------------------------------
#if defined(JOBS_PLATFORM_XBOX_ONE)

ref class ViewProvider sealed : public IFrameworkView
{
public:
    virtual void Initialize(CoreApplicationView^ applicationView)
    {
        applicationView->Activated += ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &ViewProvider::OnActivated);
        CoreApplication::DisableKinectGpuReservation = true;
    }

    virtual void Uninitialize() {}
    virtual void SetWindow(CoreWindow^ window) { }
    virtual void Load(Platform::String^ entryPoint) {}

    virtual void Run()
    {
        jobsMain();
    }

protected:
    void OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
    {
        CoreWindow::GetForCurrentThread()->Activate();
    }

private:
};

ref class ViewProviderFactory : IFrameworkViewSource
{
public:
    virtual IFrameworkView^ CreateView()
    {
        return ref new ViewProvider();
    }
};

[Platform::MTAThread]
int __cdecl main(Platform::Array<Platform::String^>^ /*argv*/)
{
    JOBS_PRINTF("==== libjobs example ====\n");
    auto viewProviderFactory = ref new ViewProviderFactory();
    CoreApplication::Run(viewProviderFactory);
    JOBS_PRINTF("Finished\n");
    return 0;
}

// ----------------------------------------------------------------------------
#elif defined(JOBS_PLATFORM_SWITCH)

extern "C" void nnMain()
{
    JOBS_PRINTF("==== libjobs example ====\n");
    jobsMain();
    JOBS_PRINTF("Finished\n");
}

// ----------------------------------------------------------------------------
#else

int main()
{
    JOBS_PRINTF("==== libjobs example ====\n");
    jobsMain();
    JOBS_PRINTF("Finished\n");
    return 0;
}

// ----------------------------------------------------------------------------
#endif

void framework_enter_scope(jobs::profile_scope_type type, const char* tag)
{
#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)

    UINT64 color;

    // We choose different colors dependending on the type of scope we are emitting.
    if (type == jobs::profile_scope_type::worker)
    {
        color = PIX_COLOR(255, 0, 0);
    }
    else if (type == jobs::profile_scope_type::fiber)
    {
        color = PIX_COLOR(0, 0, 255);
    }
    else
    {
        color = PIX_COLOR(0, 255, 0);
    }

    // For this example we emit a pix event. You should replace this with your own profiler
    // API, vtune, razor, whatever.
    PIXBeginEvent(color, "%s", tag);

#elif defined(JOBS_PLATFORM_PS4)

    uint32_t color = 0xFF00FF00;
    if (type == jobs::profile_scope_type::worker)
    {
        color = 0xFF0000FF;
    }
    else if (type == jobs::profile_scope_type::fiber)
    {
        color = 0xFFFF0000;
    }
    else
    {
        color = 0xFF00FF00;
    }

    sceRazorCpuPushMarker(tag, color, 0);

#endif
}

void framework_leave_scope()
{
#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)

    // Leave the pix event at the top of the stack.
    PIXEndEvent();

#elif defined(JOBS_PLATFORM_PS4)

    sceRazorCpuPopMarker();

#endif
}