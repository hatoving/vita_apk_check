# What is this?
This is a program you can use to check if an Android game you'd wanna see on the PS Vita is able to be ported or not. Based on withLogic's original Python script, this is a precompiled program, meaning you can just run it and go.

# Usage
The program will ask you where your installation of VitaSDK is. If you haven't installed it yet, go do so here: https://vitasdk.org/
After that, you're good to go. Type in the path of the .APK you want to check and watch it do the work.
Alternatively, you can set a path as an argument when you're launching the executable, like this: ``vita_apk_check PATH_HERE``.

# Build Instructions
Just make sure you have `zlib` installed and just execute `make` in the project directory.
If you want to build for Windows, make sure to run `make windows`. Otherwise, if you are on Windows and are using mingw32, I think `make` alone will suffice.

# Credits
withLogic for making vitaApkCheck (https://github.com/withLogic/vitaApkCheck)
