# Integra Framework

The Integra Framework consists of libIntegra, a shared [dynamically loaded library](https://en.wikipedia.org/wiki/Dynamic_loading) and a set of data files consumed by the library. Further information about libIntegra can be found [here](http://birminghamconservatoire.github.io/integra-framework/api-documentation/).

The additional files that form part of the framework are:

- The Integra core modules
- Schemas for libIntegra's data files
- Any other dependencies required by libIntegra
- The libIntegra public headers (macOS only)

### Source Code

-   Download source tree from Git
	[https://github.com/BirminghamConservatoire/integra-framework](https://github.com/BirminghamConservatoire/integra-framework "https://github.com/BirminghamConservatoire/integra-framework")

The repository contains a submodule for certain dependencies.  To download these submodules, use the git command:

`git submodule update --init --recursive`


## Building on macOS

On macOS the Integra framework builds as a [framework](https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPFrameworks/Concepts/WhatAreFrameworks.html). The built framework includes the libIntegra dynamically linked library, any non-system dependencies, the core Integra module library and any assets or data files required by libIntegra.

To build the Integra framework on macOS:

- Open libIntegra/xcode/Integra/Integra.xcodeproj
- Select the Integra.framework build target from the target selector
- Select Product -> Build

This will create a **Debug** build of the Integra.framework in the built products output directory

To build a **Release** build, either select "Build for Profiling" from the Product menu or change the Build Configuration in the Scheme Editor

### Testing

To run the test suite:

- Select the UnitTests target from the target selector
- Select Product -> Run


## Building in Windows

On Windows the framework builds as a collection of [dll](https://en.wikipedia.org/wiki/Dynamic-link_library)s and other files that are copied to a common directory at the end of the build process. All files in this output directory must be available to the libIntegra.dll at runtime.

### Prerequisites

-   Install Visual Studio (2017 or later)
-   Install the [Test Adapter for Google Test](https://docs.microsoft.com/en-us/visualstudio/test/how-to-use-google-test-for-cpp?view=vs-2017)
-   Install the [Windows WDK](https://developer.microsoft.com/en-us/windows/hardware/windows-driver-kit
)
-   Install [mingw](http://www.mingw.org) (including msys, gcc and pthreads). The easiest way to do this is to download the mingw installer and install the mingw32-base-bin, msys-base and mingw32-pthreads-w32 meta packages

### Environment Variables

-   C:\MinGW\msys\1.0\bin should be in the PATH variable
-   C:\MinGW\bin should be in the PATH variable

### ASIO SDK

The ASIO SDK is needed in order provide low-latency audio I/O in windows.  The ASIO SDK is published by Steinberg, who do not permit the redistribution of its source code.  So you need to download it from [http://www.steinberg.net/nc/en/company/developer/sdk_download_portal.html](http://www.steinberg.net/nc/en/company/developer/sdk_download_portal.html "http://www.steinberg.net/nc/en/company/developer/sdk_download_portal.html"), and extract it to libIntegra/externals/portaudio/src/hostapi/asio/ASIOSDK.

### Set Flext build properties

In order for the libIntegra build system to locate required headers for the Flext dependencies:

- Copy the file libIntegra/msvc/flext_settings_VS.props to libIntegra/externals/flext

**This will overwrite the existing flext_settings_VS.props in libIntegra/externals/flext**. Because this file is tracked by git, it will therefore show up as modified in the git staging area. To ignore these local changes, use:

`git update-index --skip-worktree libIntegra/externals/flext/flext_settings_VS.props`

The changes will then be ignored by git.

### Building the Visual Studio solution

Once the above requirements have been satisfied, it should be trivial to build the Integra Framework.

- Open libIntegra/msvc/libIntegra.sln
- Build the solution

The output files will be placed in libIntegra/bin

## License

The Integra Framework is licensed under the GNU GPL version 2. For further details see [here](libIntegra/LICENSE.txt)

The Integra [module library](modules) is public domain under the terms of the Unlicense. For further details see [here](http://unlicense.org)

## Reporting issues

Issues should be reported on the project [issue tracker](https://github.com/BirminghamConservatoire/integra-framework/issues)






