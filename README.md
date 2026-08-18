# M4KCU

I took sashakoshka's M4KC project and ported it to the wii u!


Since her code used C for everything, and ONLY SDL2 and nothing else to render with,
it was pretty straightforward to port... Though I did do more than just port it.
I dont think Im going to work on it anymore, since its slow and I cant really do anything to make it faster
because it ray-traces every pixel and the wii u's cpu isnt that awful powerful...


Im including the latest rpx and wuhb so you shouldnt have to figure out how to build it unless you have to


** If you know what you are doing you could probably figure this out but I have a ton of crap installed within devkitpro msys2 I dont know how to make it print out what it needs...

## BUILDING:

To build on windows you need to have \DevKitPro MSYS2\ Installed as well as

-WUT (Wii U Toolchain).

-DevKitPPC (PowerPc aka Wii U CPU)

-GCC to compile (powerpc-eabi-gcc)

-ELF2RPl

-SDL2 Stuff

-WUHBTOOL


## Heres all the changes I made:

-Made saving and loading work using the sd card and not pc junk

-Added vpad (gamepad) controls 

-Rewrote most of menus.c so you can navigate without the mouse (and you cant access the config menu)

-I distributed the ray-tracing across the wii u's three cpu cores

-Instead of calling sdl to render each pixel separately, it now waits until the entire frame has been traced before rendering

-Leaf blocks now have dark green instead of transparent holes

## |

It kinda runs like crap... It runs great if you are in your house or underground (15-30fps) but if you are just wandering around or staring at the horizon its not going to run very well.

Im honestly surprised I managed to port it because I dont really know what I am doing but have fun...

If you know how to make it run faster and want to work on it contact me at spacerobot6152@proton.me and I can add you to the github to edit it or whatever...

If you fork this you are free to do so just give credit to me as well as sashakoshka and whoever she said to credit, as well as notch for making the original game.

\/ \/ \/ sashakoshka's original readme \/ \/ \/

# M4KC

![Grass block icon](icons/icon.png)

*Minecraft 4K - C Rewrite*

For those who don't know, Minecraft 4K was a stripped down version of Minecraft submitted by Notch to the [Java 4K Game Programming Contest](https://en.wikipedia.org/wiki/Java_4K_Game_Programming_Contest), where each submission had to be under 4 kilobytes in size. Its wiki page can be found [here](https://minecraft.fandom.com/wiki/Minecraft_4k).

Being so small, the game proved somewhat easy to de-compile and edit. [Multiple people have given this a go, including me](https://www.minecraftforum.net/forums/mapping-and-modding-java-edition/minecraft-mods/1290821-minecraft-4k-improved-by-crunchycat-download-now).

This project is an attempt to translate the game into C in order to increase its performance, and to provide a platform upon which to add new features.

## Some goals for this project

* Maintaining the original look and feel as closely as possible. ✅️
* Keeping the final executable under 20 KB (on Linux, with the system I have set up in `build.sh`) ✅️
* More blocks 🏗️
* Perlin noise terrain generation ✅️
  * Water ✅️
  * Biomes 🏗️
  * Caves ✅️
* Infinite worlds, horizontally and vertically too 🏗️
* Mobs and multiplayer (this would require changing the rendering engine to some degree) 🏗️
* Day/night ✅️

*✅️ - got that!*

*🏗️ - not yet...*

## Dependencies

### Bare minimum to make this code run
* SDL2
* A C compiler, such as gcc or clang

### To get it down to a small size, you need
* gzexe

### On windows, you will need
* MSYS2 installed
* mingw-w64-x86_64-gcc installed through MSYS2

## Build instructions

Visit [the wiki](https://github.com/sashakoshka/m4kc/wiki/Building-From-Source) for more detailed build instructions.

### Linux, unix, etc
* To just get a binary, run `./build.sh small` or `./build.sh all small`
* To run an uncompressed version, run `./build.sh` or `./build.sh all`
* To install the program, run `./build.sh install`
* To uninstall, run `./build.sh uninstall`
* To clean, run `./build.sh clean`

### Windows
The exact same as above, but you need to do it within an MSYS2/MINGW64 shell. Instructions on how to do this can be found [here](https://www.msys2.org/docs/terminals/).

### macOS with Xcode
Open Xcode and do a Product -> Build or Command+B. You can also Archive and then export the archive with Organizer -> Distribute App -> Copy App.

Command-line version of the build:
```
# cd apple
# xcodebuild
```

Note: This app won't be distributable without code signing.

## Places

There is a forum thread for this project [here](https://www.minecraftforum.net/forums/mapping-and-modding-java-edition/minecraft-mods/3081789-minecraft-4k-c-rewrite)

I will be uploading binaries [here](https://holanet.xyz/soft/m4kc/)

## FAQ

I've either been asked these, or I expect to be at some point.

> What's with the cryptic variable names like `f22` and `i6`?

A lot of this code is decompiled from the original java version, and those are names the decompiler assigned to the variables. Much of the code is extremely obfuscated due to what are probably compiler optimizations, and some variables have not been deciphered and renamed yet. This is why they mostly appear in the `gameLoop` function.

> Why is it so slow?

The game uses a 3D voxel raycaster to render things, which is a lot slower than more traditional methods of rendering. Luckily, C provides more powerful ways to optimize something like this than Java - and optimizations will keep coming.

> Will you add in \_\_\_\_\_ from Minecraft?

I plan to port over a lot of user interface features, controls, and gameplay mechanics from the official game. However, this game will be taken in its own direction content-wise. For example, crafting will be a part of the game eventually, but creepers will not. New features will be added if they fit with the aesthetic and feel of the game, and make sense from a technichal and gameplay perspective.

## Screenshots

What this actually looks like.

<img alt="Main menu" src="screenshots/0.png" width="428"> <img alt="Cool house" src="screenshots/1.bmp" width="428"> <img alt="World selection" src="screenshots/2.png" width="428"> <img alt="Rolling hills" src="screenshots/3.bmp" width="428"> <img alt="A beach" src="screenshots/4.bmp" width="428">
