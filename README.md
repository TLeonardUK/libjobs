    .__   .__ ___.         __        ___.            
    |  |  |__|\_ |__      |__|  ____ \_ |__    ______
    |  |  |  | | __ \     |  | /  _ \ | __ \  /  ___/
    |  |__|  | | \_\ \    |  |(  <_> )| \_\ \ \___ \ 
    |____/|__| |___  //\__|  | \____/ |___  //____  >
                   \/ \______|            \/      \/ 

## About Libjobs
Libjobs is a simple C++ library that is designed to allow multi-threaded coroutine-style job management and scheduling (implemented using fibers). It currently runs on Windows, XboxOne, PS4, Nintendo Switch, and is fairly straight forward to port to other platforms as it uses relatively little platform-dependent code.

Implementing jobs using fibers provides a variety of benefits. Primarily it provides the developer the illusion they are working with threads, and allows them to do things that would typically block the cpu (waiting on sync primitives, sleeping, waiting for tasks to complete, wait for io, etc), without actually doing so. Allowing optimal usage of available processing power.

Architecturally libjobs is designed to run a fixed number of worker threads (preferably one per core), with each thread picking up and executing jobs. Jobs are cooperatively scheduled, so whenever a blocking event occurs, the worker thread stops executing it and starts running another job, while the waiting job is queued for re-execution when its wait condition completes. The library also supports different job and worker priorities so jobs can be appropriate ordered and split between different workers.

The library is designed for both high-performance and limited resources. Memory allocation (overridable with use defined allocation functions) is done during scheduler initialization and no memory is allocated beyond that point.

## Building
The project uses cmake for building the library and examples. It's also been setup with a CMakeSettings.json file so it can be opened as a folder project in visual studio.

Building is identical to most cmake projects, check a cmake tutorial if you are unsure. The only caveat is that we cross-compile various platform builds. To make cmake build these you need to use the appropriate toolchain file (which are stored in cmake/Toolchain). You can pass these as parameters when configuring the project with cmake. eg.

-DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain/PS4.cmake

## API Reference
The library uses doxygen for its api reference. Once the project is build the output will be stored on docs/html.

## Basic Usage
Examples of how to use the library are provided in docs/examples. Below is a brief explanation of the minimal steps required to run a job.

First a scheduler needs to be created, the scheduler is the responsible for managing the entire job system. Projects should use a single scheduler, though in theory multiple can be used concurrently.
```cpp
jobs::scheduler scheduler;
```

Next a thread pool should be added. A thread pool is a number of worker threads that will be spawned and will execute jobs which have a priority in thread-pools priority bit-mask. Multiple thread-pools can be created if you want to segregate specific jobs to specific workers (such as having a specific thread that only every takes time-critical jobs, or one that only takes long running jobs).
```cpp
scheduler.add_thread_pool(8, jobs::priority::all);
```

Next a fiber pool should be created. Fibers store the context of individually executing jobs. You should allocate enough fibers such that all concurrently active jobs are able to allocate one. Fibers have a fixed stack-size and jobs can be given a minimum stack-size they require, and will not use any fibers that have less than it. Fibers can be split up into different pools of varying stack-sizes to optimally allocate memory.
```cpp
scheduler.add_fiber_pool(10, 16 * 1024);
```

Next the scheduler should be initialized. It is at this point that all memory will be allocated and all fibers and thread workers will be created.
```cpp
scheduler.init();
```

Jobs can then be created, assigned a work-function and dispatched for execution.
```cpp
jobs::job_handle job_1;
scheduler.create_job(job_1);

job_1.set_work([=]() { printf("Example job executed\n"); });
job_1.dispatch();
```

Various API's exist for waiting for individual jobs, counters or other syncronization primitives. At a minimum in this example we wait until all jobs have finished by waiting for the scheduler to go idle.
```cpp
scheduler.wait_until_idle();
```

## Syncronization
Syncronization is primarily done in three ways, waiting for jobs, waiting for counters and other sync primitives, and hard-set dependencies.

### Hard-Set Dependencies
@todo

### Waiting For Jobs
@todo

### Waiting For Counters
@todo

## Contact Details
Any questions you are welcome to send me an email;

Tim Leonard
me@timleonard.uk

## Credits / Further Reading
Naughty Dog use a similar fiber based job system in their engine, and gave a great GDC talk a few years ago that covers their implementation. It's one of the inspirations for this library.
https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine

Housemarque also have an very nice fiber based in-house engine which I had the pleasure of working with when porting resogun/dead-nation, which was heavily instrumental in me experimenting further with fibers.
