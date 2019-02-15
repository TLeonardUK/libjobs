    .__   .__ ___.         __        ___.            
    |  |  |__|\_ |__      |__|  ____ \_ |__    ______
    |  |  |  | | __ \     |  | /  _ \ | __ \  /  ___/
    |  |__|  | | \_\ \    |  |(  <_> )| \_\ \ \___ \ 
    |____/|__| |___  //\__|  | \____/ |___  //____  >
                   \/ \______|            \/      \/ 

# About Libjobs
Libjobs is a simple C++ library that is designed to allow coroutine-style job management and scheduling. It currently runs on Windows, XboxOne, PS4, Nintendo Switch, and is fairly straight forward to port to other platforms.

@todo

# Basic Usage
@todo

# Building
The project uses cmake for building the library and examples. It's also been setup with a CMakeSettings.json file so it can be opened as a folder project in visual studio.

Building is identical to most cmake projects, check a cmake tutorial if you are unsure. The only caveat is that we cross-compile various platform builds, to make cmake build these you need to use the appropriate toolchain file (which are stored in cmake/Toolchain). You can pass these as parameters when configuring the project with cmake. eg.

-DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain/PS4.cmake

# Contact Details
Any questions you are welcome to send me an email;

Tim Leonard
me@timleonard.uk

# Credits / Further Reading
Naughty Dog use a similar fiber based job system in their engine, and gave a great GDC talk a few years ago that covers their implementation. It's one of the inspirations for this library.
https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine

Housemarque also have an very nice fiber based in-house engine which I had the pleasure of working with when porting resogun/dead-nation, which was heavily instrumental in me experimenting further with fibers.
