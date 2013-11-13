DPfix Developer Readme
======================

General
=======

This file is intended for developers who want to improve DPfix or use it as a base for other modifications.

All the source code is released under the conditions of the GPLv3, except for
- SuperFastHash
- SMAA
which have their own licensing terms included.

Please note that the code base is currently in quite a bad state, and much of it is lifted wholesale from DSfix.
This is in no way representative of how I'd work in a professional setting, or even for a long-term personal project.
 
Also note that I would prefer for no one except myself to make binary releases of DPfix for now, 
simply to keep things easier to track for people and reduce confusion.

Additionally, if you start working on some major feature, I'd appreciate if you contacted me, so that we don't duplicate effort.

Finally, if you want to contribute bug fixes or features I'd prefer it if you used the github mechanisms (that is, pull requests).

- Peter


The Deadly Premonition Rendering Pipeline
=========================================

I don't have time to fully write out everything I've discovered, but I'd
like to at least give everyone interested in working on this a rough idea of what to expect,
and hopefully relieve them of the basic groundwork of understanding anything at all about what 
the game is doing.
Obviously, all of this is based purely on observation, deduction and speculation, so some/all
of what you read here might be wrong or incomplete.

Rendertargets
-------------

Deadly Premonition uses deferred rendering. There are 4 main rendertargets, in addition to
the normal backbuffer. One is used to store the screen-space normals, one is used to store 
depth (as 24 bits in the RGB components), one stores the lightmap and one is used to store 
the initial HDR rendering result. The former 3 are 4 bytes per pixel, while the latter is 
8 bytes per pixel (A16R16G16B16F). All of them are 720p by default.

In addition to those main targets, there are the following:
- 640x360 and 320x180 rendertargets used for reflections (depending on the expected screen
  size of the reflection, as far as I can tell), two A8R8G8B8 and one D24S8 each.
- 512x512 and 1024x1024 depth (D16) maps for shadow rendering.
- 896x504 and 448x252 rendertargets in A8R8G8B8 to store the frames for dual-scene rendering.
- An A16B16G16R16F pyramid from 1x1 over 2x1, 4x2, 8x4, ..., up to 448x252. This is used to
  reduce the HDR to get an overall value for the final tone mapping. However, the top level
  is *also* used for DoF.
- Some 128x128 and 256x256 RTs apparently used for texture animation effects. I haven't really
  looked into these much.

Standard Rendering Pipeline
---------------------------

The "standard" rendering process for DP roughly follows these steps:

1. [optional] Render some of the texture RTs identified above.
2. Use MRT to render the screen-space normals to RT #1, and depth to RT #0.
3. Render the light map to RT #0.
4. Render the shadow depth map.
5. Add shadows to the screen depth RT in the (previously unused) alpha channels. 
   This may loop back to 4. a few times for additional shadow maps.
6. [optional] Render the reflection maps.
7. Take the normal, depth (+shadow) and light RTs and render the (preliminary) HDR output.
8. Add alpha blended effects on top of the HDR output.
9. Downsample HDR output along the pyramid.
10. Blur HDR light glow on 224x126 RT.
11. Blur on 448x252 (pyramid top-level) RT for DoF.
12. Tone-map/resolve HDR buffer + DoF + glow into the backbuffer.
13. Render HuD elements on top.

Special Cases
-------------

Here are some special cases that cause a deviation from the above:

- The menu uses a slightly simplified version of the above to render the 3D background, 
  which skips some steps.
- Scenes which render multiple viewpoints work quite similarly, alternating between
  the views each full frame. However, there is an additional composition pass
  injected on the final backbuffer rendering, and the frames pre-HuD are stored in
  the "storage" RTs defined earlier.
- For some reason, some locations in the game (I found only one so far, the gallery)
  use a quite different rendering path. Crucially, they *do not* use MRT to render the 
  initial depth/normal combination. This requires an entirely separate path in DPfix
  to identify the main surfaces in order to e.g. apply AA or SSAO.


My Development Process
======================

I get the feeling that sometimes people don't know where to start when trying to contribute,
or perhaps even start their own fix or mod for some game, despite generally being qualified
(that is, having a solid understanding of C/C++ programming and DirectX).
To hopefully alleviate this issue I'll give a short description of my personal process during 
development of these fixes.

Initial Setup
-------------

1. Put on some music. Metal and classical work well, but sometimes it could also be trance. Or JPOP.
2. Use dumpbin (VS tools) to check out the .exe of the game, see which imports it uses, find one
   suitable for injection.
3. Get your code to a state where its loaded into the game, forwards everything correctly, and you
   can conveniently log what's going on.

Most of the above (well not the music) is already done for you if you start from DPfix.

Gaining Understanding
---------------------

Here I'll try to describe how I arrived at the insight into the DP rendering process
summarized in the rendering pipeline section above. Note that this is the most time consuming part
of doing something like this, not the actual coding (not even by a long shot).

1. Get to a point in the game where the rendering you want to understand.
2. Use the "dump full frame" functionality. This will capture each render target as the game
   switches *away* from it, and also temporarily increase the logging level for this one frame.
3. Look through the framedump and the associated parts of the text log and try to understand what
   is going on.
4. If you still are unable to understand how something works, add additional logging / dumping code
   and try again from 1).

To give you an idea, I currently (as of 0.8) have "framecap1" to "framecap31" folders in my DP
directory, with a total size of 9.8 GB. In one case, I dumped 134 separate states of a single 
rendertarget to figure out what is going on.

Additional information can be obtained by enabling the shader decompilation feature, associating
shaders with steps in the rendering process and starting to read the ASM. MS pixel/vertex shader
ASM is actually rather OK to read, of course it still takes time.

An alternative option is to *purposely sabotage* the rendering, by e.g. setting the size of a
rendertarget to 1x1. In many cases, doing so can quickly give you an idea of what it is used for.

An important point is to always challenge your assumptions. Nothing costs more time than trying
to track a bug down a completely wrong lane simply because you did not consider the most obvious 
reason since you assumed that it was impossible. This is generally true in development, but *even
more so* when you directly interact with a program you can only incompletely characterize by its
IO behaviour.

Iterative Development
---------------------

If you have achieved a general understanding of what the game does, it's time to actually add
a feature or fix an issue. However, before that some further prerequisites are helpful:

- Keep multiple save files around, particularly in vastly different/challenging rendering
  scenarios. In this complex development environment this is the best approximation of
  unit/integration tests you can get. Take particular care to keep scenes accessible that
  gave you trouble before, they may do so again.
- Code versioning systems are your friend -- again, this is a general programming advice,
  but doubly useful if you have complex interactions with code you don't fully understand.

That out of the way, this is my usual process for adding a feature or fixing an issue:

1. Find a scene in the game which is both easily accessible and clearly demonstrates the 
   feature / exhibits the issue.
2. Perform a full frame dump, and investigate it to figure out *where* you need to intervene.
3. Determine *what* needs to be done.
4. Find a unique event or sequence of events which identifies the point where you need to 
   perform your modifications.
5. Actually write the code to do so.
6. Test it, and go back to 2. if it goes wrong.
7. Test it in your other saved locations, and see if it still work or breaks anything else.
   If the latter, refine 4.

Points 2-5 can all be challenging, but I find that often 4 is actually the hardest to get
right. Ideally, you should strive for your indicators to be as *stateless* as possible, e.g.
ideally a unique set of parameters for exactly the call you want to modify. Sadly, reality is 
rarely so kind, and you often need to track some state. If so, still try to keep it to a 
minimum, and don't be afraid to replace complex indicators with simpler ones if you find them.


Requirements
============

- Visual C++
- The DirectX SDK
- Microsoft Detours (Express) 3
- DPfix must be compiled in the Release/Win32 config to work with the game


File Overview
=============

General
-------

- The "pack" folder contains the files for distribution, including end-user documentation, .inis, effects and folders
- "main.*" includes the main function and a few utilities

Wrapping
--------

- The "d3d9*" files implement d3d wrapping
- "Detouring.*" files implement function overriding using the Detours library

Utilities
---------

- "Settings.*" files implement reading settings (defined in Settings.def) from .ini files and querying them
- "KeyActions.*" files implement keybindings, together with the Xmacro files "Keys.def" and "Actions.def"
- "WindowManager.*" files implement window management (e.g. borderless fullscreen)

Rendering
---------

- "RenderstateManager.*" is where most of the magic happens, implements detection and rerouting of the games' rendering pipeline state
- "SMAA.*", "VSSAO.*" and "GAUSS.*" are effects optionally used during rendering (derive from the base Effect)
- "Textures.def" is a database of known texture hashes

