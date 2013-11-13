DPfix by Durante -- README
==========================

**Please read this whole document before running anything!**

What is it?
===========
It's an interception DINPUT8.dll that you place in the same folder as the game executable. It intercepts the game's calls to the DirectX 9 API and changes them as necessary to enable a higher internal rendering resolution and various other features.

How do I use it?
================
1) Delete previous version of the mod if you have any
2) Place the contents of the .zip into the game's binary directory. (The place where DP.exe is)
(this may be something like C:\Program Files (x86)\Steam\steamapps\common\Deadly Premonition The Director's Cut)
3) Adjust the settings in DPfix.ini as desired
4) Adjust the keybindings in DPfixKeys.ini as desired

Will it work?
=============
It works for many, however...
*I can not and will not guarantee that it will work for anyone else, or not have any adverse effect on your game/computer. Use at your own risk!*

Are there known issues?
=======================
Nvidia Optimus doesn't enable the dedicated GPU when using DPfix.

Will it cause performance problems?
===================================
That depends on your system configuration. 
Usually, performance scales rather linearly with framebuffer size, and so far this game does not seem different. 

Can I donate?
=============
If you really want to donate I won't say no, I'm not particularly rich.
You can find donation links on my blog, where I'll also release new versions:
http://blog.metaclassofnil.com/
You can also simply Paypal to peter@metaclassofnil.com

How can I contribute?
=====================
I'll probably make the source code available at some point, for now it's just a hacked up DSfix, which is on Github.

It crashes, help!
=================
First, make sure that the .ini files are present in the correct location.
Turn off tools such as MSI Afterburner or other overlays that manipulate D3D.
Then try restoring the default settings in the .ini file
If none of these help then check if the problem still occurs when you remove/rename d3d9.dll
Finally, try rebooting
If the issue still persists, then report the problem, otherwise it has nothing to do with DPfix.

How can I uninstall the mod?
============================
Simply remove or rename d3d9.dll
The mod makes *no* permanent changes to *anything*.

Contact information
===================
Contact me at peter@metaclassofnil.com
