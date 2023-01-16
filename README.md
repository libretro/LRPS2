# LRPS2

Hard fork / port of PCSX2. Will target only Libretro specifically. Is currently in development. No support provided.

Do not bother PCSX2 mainline project with questions about LRPS2. This is not doing either project any favors. LRPS2 operates independently of PCSX2.

Changes (in no particular order):
- We only target Libretro
- Removed as much cross-platform code as possible, reduces a significant chunk of the code
  - (SPU2)   Removed all sound API dependent code, we only target Libretro audio
  - (GS/OGL) Removed all OpenGL context code, we only target the Libretro GL renderpath
  - (Input)  Removed all input API dependent code, we only target Libretro audio
- (GS/OGL) Don't use GLAD for GL extension loading purposes, try to keep a few limited function pointers for calling GL functions
- (GS) Embed shaders into the core itself instead of having to read them from disk. Reduces file I/O and makes it so we have as few file dependencies as possible
- (GS) Rip out all swapchain management, is not needed for Libretro and gets handled at the frontend side
- The entire GameDB YAML file is embedded into the core. Reduces file I/O and makes it so we have as few file dependencies as possible
- Widescreen and 60fps patches is embedded into the core. Reduces file I/O and makes it so we have as few file dependencies as possible.
  - Aim is an out of the box experience to make games run as the user would expect with minimal interlacing/ghosting issues. Not quite there yet but making progress.
- All plugins unified, LRPS2 now builds as a single dynamic library with as few static libraries linked in near the end

TODO/FIXME:
- File I/O code should all go through libretro VFS
- Get rid of remaining threading issues at context teardown. Has been reduced but can still happen at times.
- Get rid of wxWidgets entirely. Made a lot of progress here but not done yet
- (Dependent on wxWidgets removal) Move as much of the gui/ code as possible
- Get software renderer up and running
  - Make software renderer not dependent on OpenGL or another hardware renderer like mainline, should be renderer agnostic.
- Backport Vulkan renderer
- Backport D3D12 renderer
- Backport Metal renderer
- Aim at providing better backwards compatibility (for older OpenGL versions, bring back Direct3D 10 rendering option perhaps, aim at keeping the 32bit x86 build around)
- Reimplement DEV9, should get a complete bottoms-up reimplementation for Libretro
- Reimplement USB, shold get a complete bottoms-up reimplementation for Libretro
- Use more of libretro-common instead of relying on Common/ code
- Reduce some of the threading overhead right now - there might be some threads being spun that are unneeded with no workloads or outright fake threads.
- Maybe reimplement how MTGS works.
