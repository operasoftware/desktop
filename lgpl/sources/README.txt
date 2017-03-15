This README explains how to build a custom native library for Opera Desktop.

Due to platform limitations, we have different build instructions for different
desktop platforms.

Windows build instructions
--------------------------

To be able to run the final product of Windows build, you need to obtain a
dynamically linked Opera build (shared library version) provided along with this
source code package. From provided sources you may build dlls for the open
source parts, and replace the ones from Opera with them.

  cd chromium\src
  gn args out\Release

When the editor shows up, you need to add "is_component_build=true".

Open all.sln in Visual Studio. From this solution you are able to modify and
build dll libraries in "Release" configuration and then replace them with the
ones in Opera shared library installation.

Mac/Linux build instructions
----------------------------

To be able to run the final product of Linux build, you need to obtain a
dynamically linked Opera build (shared library version) provided along with this
source code package. From provided sources you may build dynamic libraries
for the open source parts, and replace the ones from Opera with them.

  cd chromium/src
  gn args out/Release

When the editor shows up, you need to add "is_component_build=true".

Then you can build your custom library.
  ninja -C out/Release <custom_lib>

The custom library can be exchanged with the one used in dynamically linked
Opera build.
