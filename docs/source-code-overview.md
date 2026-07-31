Source Code: First contact
The source code has never been officially released but it is everywhere on the internet. I will obviously not provide a link but I think to provide a roadmap for historical value is acceptable and will probably help fellow code readers.

The solution is for Visual Studio 6.0 (development started on VS 4.1 but C++ support was not satisfactory: The team updated the dev tools as soon as a new version became available). It can be automatically converted to a modern version of Visual Studio (2008/2010/2012). Attempting to compile shows that the source code has heavily suffered from erosion: C++ has evolved a lot since 1998 and certain things are not permited anymore (like things that now require the typename keyword).



Before starting to open files and read, I ran a few metrics since cloc can give an idea of the volume of code to read:



    fabiensanglard$ cloc jp2_pc
    1596 text files.
    1555 unique files.                                          
     418 files ignored.

    http://cloc.sourceforge.net v 1.60  T=14.08 s (88.1 files/s, 40859.1 lines/s)
    -------------------------------------------------------------------------------
    Language                     files          blank        comment           code
    -------------------------------------------------------------------------------
    C++                            469          64881          86607         195287
    C/C++ Header                   762          32170          89234         105606
    C                                2            253            177            884
    DOS Batch                        7              5              0            174
    Teamcenter def                   1              0              0              8
    -------------------------------------------------------------------------------
    SUM:                          1241          97309         176018         301959
    -------------------------------------------------------------------------------

      
Standing alone, those numbers don't mean much. But comparing them to others games (Quake,Doom,Duke3D,...) helps a lot:



 
    -------------------------------------------------------------------------------
    Game             Year         files         blank         comment          code
    -------------------------------------------------------------------------------
    Wolfenstein3D    1992           80           7223           7516          27311
    Doom             1993          149          10213          10234          39080
    Duke 3D          1996          182          18537          11364          77166
    Quake 1          1996          543          45550          38262         171065
    Quake 2          1997          399          37743          31735         163928 
    Trespasser       1998         1241          97309         176018         301959 
    Quake 3          1999          799          65988          93977         313311
    Doom  3          2004         1466         137589         164894         593899
    Doom iOS         2009          237          15080          24054          70766
    Wolfenstein iOS  2009          326          20403          20170          63609
    Doom  3 BFG      2012          977          92735         119965         363239
    -------------------------------------------------------------------------------

    
The volume of code is twice more than other titles released at the same time. The volume is very representative of the complexity of the engine and the collosal scope of features. What really stands out is the volume of comment: Trespasser code base is exceptionnaly well commented and that is usually an indication of high skills.

The solution contains 23 projects which clearly show the different subsystems but I suspect was done to make compile time bearable. Even on a modern machine a full built take close to 10mn. I would not be surprised if 1998 and its lack of precompiled header took an hour for a full build.

An interview of Seamus Blackley (Lead developer on Trespasser) shows that compile time was a problem back in the days:



   Qtrescom.org    : What fraction of the development time was spent waiting on the C++ compiler?

   Seamus Blackley : It felt like 103%

  

Details of each projects :

Projects	Generates	Notes:
AI	Artificial Intelligence subsystem	A lot of code unused since most traits had to be disabled.
AI Test	JP2_PC.exe	A standalone program allowing to test AI with graphics.
Audio	Audio.lib	The audio sub-system static library featuring the "real time Foley".
Bug	Bugs.exe	A project concentrating all compiler errors. Since the team switched from VS4 to VS4.1 to VS4.2 to VS6.0 it was usefull
CollisionEditor	CollisionEditor.exe	Sound effects editor to test the audio engine (very powerful at the time)
EntityDBase	EntityDBase.lib	Classes representing all objects in the game.
File	File.lib	Abstraction classes for File and Images, used to build the Groff archives.
File Test	File	Test for the file and image abstractions.
Game	Game.lib	Glue, triggers, Player, Gun classes.
GeomDBase	GeomDBase.lib	The 3D representation (Geometry) of all objects defined in EntityDBase.
GroffBuild	GroffBuild.exe	The tool in charge of gathering all game assets (3d,sounds,maps) in one GOFF file.
GroffExp	GroffExp.dle	The DLL loaded by 3DS Max that export all data to GOFF sections. This was originally outsourced to an other dev and is standalone.
GUIApp	GUIApp.exe	A wrapper around the game. The GUI allows to change the game values at runtime for testing. Like the console allowing to change the cvar in Quake engines.
Loader	Loader.lib	The library loading GOFF assets to RAM.
Math	Math.lib	The math library (features a fInvSqrt that is not as good as QuakeIII's InvSqrt since it uses a lookup table but also uses Newton-Raphson).
Math Test	MathTest.exe	A few functions to test the speed of the math routines.
Physics	Physics.lib	The pelvis heavy, penalty force based Physic engine library.
PhysicsTest	PhysicsTest.exe	A sandbox level where physic can be tested.
PipeLineTest	PipeLineTest.exe	Testbed for the rendering pipeline
Processor	Processor.dll	Uses CPUID to detect 8086, 80286, 28386 or a 80486, Pentium, K6-3and K7, Detect Floating Point Unit and CPU speed. Loaded at runtime by System project in order to set automatically details level (based on CPU Mhz).
QuantizerTool	QuantizerTool.exe	Aborted project. Does nothing.
Render3D	Render3D.lib	The hybrid software/Direct3D renderer.
ScreenRenderDWI	ScreenRenderDWI.lib	Pentium, PentiumPro and K6_3D specific code ASM optimized code for scanline and cache rendering. Direct3D code.
Std	Std.lib	Extension of STL. An horrible mess of specific containers of containers of set of hashmap. Arg.
System	System.lib	Contains scheduler, Virtual Memory. Thread control. SetupForSelfModifyingCode (via modify the page tables associated with the application). Many things are not used.
trespass	trespass.exe	The game we played.
View	View.lib	Raster to window code. Blitter, DirectDraw, Direct3D, software palette viewers.
WaveTest	WaveTest.exe	Shell to test wave modeling.
WinShell	WinShell.lib	win32 windows creation and management library


The project names and the post-mortem are giving us a good idea of the production pipeline :

┌───────────────────────────────────────────┐      ┌────────────────────────┐
│                GUIApp.exe                 │      │     3D Studio MAX      │
├───────────────────────────────────────────┤      │ ┌────────────────────┐ │
│               trespass.exe                │      │ │    GROFFEXP.DLE    │ │
├───────────────────────────────────────────┤      │ └─────────┬──────────┘ │
│ AUDIO   GEOMDB   ENTITYDB                 │      └───────────┼────────────┘
│ PHYSIC  GAME     RENDER3D   AI            │                  │
│ STD     MATH     SYSTEM     PHYSIC        │                  ▼
├───────────────────────────────────────────┤              ┌───────┐
│                    STL                    │◄─────────────┤ GROFF │
├───────────────────────────────────────────┤              │Archive│
│                    C++                    │              └───────┘
└───────────────────────────────────────────┘                  ▲
                                                               │
                                                   ┌───────────┴────────────┐
                                                   │     GROFFBUILD.EXE     │
                                                   └───────────▲────────────┘
                                                               │
                                                           ┌───┴───┐
                                                           │Assets │
                                                           └───────┘


------------------------------

Here is the structured text representation of the architecture diagram:
## Left Column: Core Executable Stack (GUIApp.exe / trespass.exe)

* GUIApp.exe (Top Layer)
* trespass.exe (Core Engine Layer)
* Engine Subsystems Layer:
* AUDIO
   * GEOMDB
   * ENTITYDB
   * PHYSIC
   * GAME
   * RENDER3D
   * AI
* Foundation Utilities Layer:
* STD
   * MATH
   * SYSTEM
   * PHYSIC (Note: Listed twice in the original diagram)
* STL (Standard Template Library Layer)
* C++ (Base Language Layer)

------------------------------
## Right Column: Asset Pipeline & Tools

* 3D Studio MAX (Green Block)
* Contains plugin: GROFFEXP.DLE
   * Outputs to: GROFF Archive
* Assets (Database Cylinder at bottom right)
* Feeds into: GROFFBUILD.EXE
* GROFFBUILD.EXE (Tool Block)
* Outputs to: GROFF Archive

------------------------------
## Central Data Link

* GROFF Archive (Central Database Cylinder)
* Receives data from both GROFFEXP.DLE and GROFFBUILD.EXE.
   * Feeds data directly into: The engine subsystems layer (GEOMDB / ENTITYDB) of trespass.exe.

------------------------------


Artist and game designer used 3D Studio Max.
When a level was ready, it was exported (GroffExp).
All assets were compiled into a resource file GROFF (GroffBuild).
The engine worked on all subsystems and generated the game (trespass).
Debugging and research was done with a secondary screen displaying GUIAPP, allowing to change the engine settings at runtime.

Renderer
The 3D Renderer is based on Depth Sorting Algorithm coupled with a quadtree sliced map to help remove as many polygons as possible. There is actually a comment in the code which indicate the implementation is taken straight from "pages 673 to 675 in "Computer Graphics: Principles and Practices by Foley, Van Dam et al". Here are the pages:

-----------------------

Here is the structured transcription of the core concepts and steps detailed in the document:

## Core Concept
The algorithm functions like an artist painting a canvas: it renders polygons into the frame buffer in order of decreasing distance from the viewpoint (back-to-front rendering), drawing closer objects directly on top of distant ones.
------------------------------
## The Three Conceptual Steps

   1. Initial Sort: Sort all polygons according to the smallest (farthest) $z$ coordinate of each.
   2. Resolve Overlaps: Resolve any ambiguities this may cause when the polygons' $z$ extents overlap, splitting polygons if necessary.
   3. Scan Conversion: Scan-convert each polygon in ascending order of smallest $z$ coordinate (from back to front).

------------------------------
## Overlap Resolution (The 5 Ambiguity Tests)
When the $z$ depth extents of a farther polygon $P$ and a closer polygon $Q$ overlap, the algorithm must prove that $P$ cannot obscure $Q$ (meaning $P$ can safely be drawn first). To verify this, up to five tests are performed in order of increasing mathematical complexity. As soon as one test succeeds, $P$ is proven safe to draw.
The five tests are:

   1. X-Extent Test: Do the polygons' $x$ extents not overlap?
   2. Y-Extent Test: Do the polygons' $y$ extents not overlap?
   3. P-Plane Test: Is $P$ entirely on the opposite side of $Q$'s plane from the viewpoint?
   4. Q-Plane Test: Is $Q$ entirely on the same side of $P$'s plane as the viewpoint?
   5. 2D Projection Test: Do the projections of the polygons onto the $(x, y)$ screen plane not overlap? (Determined by comparing polygon edges).

Note at the bottom: If all five tests fail, the algorithm assumes $P$ actually obscures $Q$, triggering a calculation to see if $Q$ can be split or scan-converted before $P$.

This second page describes what happens when the initial five depth tests fail, introducing reversed tests and the polygon clipping/splitting logic required to prevent rendering loops.

Here is the structured transcription of the concepts on this page:

## 1. The Reversed Tests (3' and 4')
If the initial five tests are inconclusive, the algorithm attempts to reverse the order of polygons $P$ and $Q$ by running modified versions of the plane tests:

* Test 3': Is $Q$ entirely on the opposite side of $P$'s plane from the viewpoint?
* Test 4': Is $P$ entirely on the same side of $Q$'s plane as the viewpoint?

If Test 3' or 4' succeeds, $Q$ is moved to the very end of the rendering list, becoming the new farthest polygon ($P$).
------------------------------
## 2. Polygon Splitting & Clipping
If the reversed tests also fail, the spatial ambiguity cannot be resolved by sorting alone.

* The Solution: Either polygon $P$ or $Q$ must be physically split into pieces using the plane of the other polygon as a knife.
* The Action: The original, unsplit polygon is discarded from memory. Its newly sliced fragments are individually re-inserted back into the sorted list based on their new proper $z$ depths.

------------------------------
## 3. Preventing Infinite Loops (Cyclic Overlap)
A major flaw in the standard painter's algorithm is that three or more polygons ($P, Q, R$) can overlap in a cyclic loop (where $P$ covers $Q$, $Q$ covers $R$, and $R$ covers $P$).

* Without intervention, the engine will infinitely shuffle polygons to the back of the list, crashing or locking the frame loop.
* The Fix: The algorithm flags or marks any polygon moved to the end of the list. If a marked polygon fails the tests again, the engine skips further shifting and forces an immediate split to break the loop.

------------------------------
## Fig. 1.25 & Fig. 1.26 Visual Breakdown

* Fig. 15.25 (Test 3 is True): Shows polygon $P$ completely separated behind the infinite plane projected by polygon $Q$, allowing $P$ to be drawn first safely.
* Fig. 15.26 (Test 3 is False, Test 4 is True): Shows $P$ crossing $Q$'s projected plane (failing Test 3). However, polygon $Q$ is entirely on the same side of $P$'s plane as the camera viewpoint, so Test 4 passes and resolves the overlap.

The third final page concludes the depth-sort discussion with a flaw scenario and introduces Binary Space-Partitioning (BSP) Trees as a highly efficient alternative for static 3D environments.
------------------------------
## 1. The Flaw: Unnecessary Polygon Splitting (Fig. 15.27)
The text details a specific mathematical scenario where the depth-sort algorithm is forced to perform an unnecessary split.

* The Scenario: In Figure 15.27(b), polygon $P$ and polygon $Q$ are already correctly ordered for rendering.
* The Failure: They fail all five basic tests and the reversed tests ($3'$ and $4'$) because their $x, y,$ and $z$ extents interlock in a specific spatial orientation.
* The Result: The engine is forced to slice one of the polygons into pieces anyway, proving that the painter's algorithm sometimes wastes processing cycles on geometry that does not actually obscure each other.

------------------------------
## 2. Introduction to BSP Trees (Section 15.5.2)
To solve the sorting and splitting bottlenecks of the depth-sort method, the text introduces Binary Space-Partitioning (BSP).

* The Core Strategy: It shifts the math burden from real-time calculation to an initial preprocessing step.
* The Trade-Off: It trades an expensive, one-time spatial calculation during development for a lightning-fast, linear sorting display loop during gameplay.
* The Best Use Case: It is perfectly optimized for applications where the player's viewpoint changes constantly, but the surrounding 3D world objects remain static.

------------------------------
## 3. How the BSP Tree Divides Space
The text traces the origins of BSP logic back to Schumacker's concept of spatial clusters.

* Spatial Clusters: The 3D world is treated as collections of intersecting polygons and faces.
* Plane Separation: If a dividing plane cleanly separates one set of clusters from another, the clusters on the camera's side of the plane can obscure, but can never be obscured by, the clusters on the opposite side.
* Recursive Trees: The algorithm continuously and recursively subdivides these subsets of geometry until the entire 3D map is represented as a clean binary tree structure.

-----------------------

The engine walks the map quadtree recursively, discarding as much geometry (terrain and objects) as possible via backface culling and frustum culling. The list of surviving polygon is then sorted by depth and the scene is sent to the rasterizer: Back to front in one batch as illustrated by the wikipedia drawing :


This method generates a visually correct scene but can potentially lead to overdraw that waste a lot of CPU cycles. A typical worse case scenario would be the player in front of mountains but inside a house :

The engine would first rasterize all the mountains in the back:



Only to overwrite every single pixel with the front objects such as walls, stairs and crates resulting in a terrible waste :


Even though the rasterizer was fast hand optimized ASM (even exploiting floating point SIMD from AMD processor K6), something had to be done in order to limit overdraw. The developers asked the 3DS Max designer to place "occluders" on the map : Invisible surfaces that were used to create occluding volumes. Here they are in gray on the right image:
 

Those occluders were used to construct occluding volume and discard more primitives during the quadtree rundown. Each frame the quadtree was used to find all the occluders intersecting the view frustum. By connecting the player's point of view through each occluder vertice, an occluding volume was created. Each primitive retrieved via the quadtree was hence tested not only against the view frustum but also against each occluding volume. This is a technique that reminded me a lot of shadow volumes.

This technique was instrumental when the player was inside a building but it also to important to limit overdraw in pure outdoor scenes. Items behind a hill could be discarded this way so occluders are also placed within hills and terrain curves :

there was not important picture, but this is text version

This mechanism worked well but it could be problem if the designers placed them poorly. In the following scene, at first sight things looks fine but if we look closer we can see that occluder are "sticking out": Crossing that part would result in objects suddenly appearing:

there was not important picture, but this is text version

A other occurence at the end of the first level :

there was not important picture, but this is text version

Not to mention the problems related to generating the terrain on the fly based on wavelet: Sometimes occluders did not match the geometry :/ !

The engine also used two other big tricks to lower the poly count: The most famous was "object caching": While rendering objects, the distant ones would be rendered to a cache and drawn as sprite on a quad. This trick originally resulted in mixed results since the mesh would remain a sprite too close from the player and changed from 2D to 3D in a very noticeable way as see in the two next images :

there was not important picture, but this is text version

The pixelating problems were partly corrected in a patch 1.1 (see comparative in this no so kind review). There were still issues related to depth correctness since the trick turned a volume turned into a plane but visual results were much more acceptable as seen on the right image (Trespasser v1.1):

there was not important picture, but this is text version

An other less known trick to reduce the number of primitive to rasterize was to generate the terrain poly on the fly via wavelets. Wavelets can be used in order to generate lower detail version of a mesh. Depending on the distance of the terrain to draw, a specific Level Of Detail wavelet was selected. If you want to read deeper into this topic, here are two very good PDFs:

SIGGRAPH ’95.course: Wavelets and their Applications in Computer Graphics
Wavelets for Computer Graphics: A Primer Part 1

The renderer high level pipeline is a follow.

For each frame :
Update the terrain and generate appropriate distance related LOD polygons using wavelet.
Draw skybox (called backdrop).
Walk the quadTree and find occluders within the view frustum, use them to build occlusion volume tetrahedron.
Walk the quadTree to select all terrain polygons: Discard via backface culling, frustum culling and occlusion culling.
Walk the quadTree to select all objects meshes: Discard via backface culling, frustum culling and occlusion culling. For distant objects, generate a proxy and render to a cache that is then rendered as a sprite.
If shadow is enabled: Generate a shadowmap .
Sort all polygons far to near using the Depth Sorting algorithm.
Rasterize the full scene. Use bumpmapping and shadowmapping.

Here is the first scene with each steps. In this scene, overdraw is severely limited (the mountains polygon are generated but not rasterized, some overdraw occurs where crates and walls are in front of each others):

there was not important picture, but this is text version

Note : The C++ polymorphism really was beautiful when shadow was enabled: An instance of the renderer pipeline templated with a depth only rasterizer generated the shadow map. That part of the code was extremely elegant.


The same scene viewed from above (notice the occluders placed within the mountains in the distance):

there was a picture, but this is text version


The rasterizer code was hard to read since it features a lot of ASM and "old" AMD K6-2 3DNow! instructions but it was state of the art especially when bumpmapping T-Rex skin :


In the end, the renderer did no have the elegance of Quake II software renderer "zero overdraw (video)" but thanks to a few tricks, it worked well enough.
The rest and the C++ box of chocolate
The rest of the engine looked interesting (I.A, Physics, "real time Foley" Sound) and I would have loved to read and write more about it. Unfortunately the code is hard to read for me and the experimental/research/release date rush aspects of it felt very real. Once I figured the 3D renderer, my motivation vanished.

On top of time constraints and research aspects, I felt like the team was pushing C++ at a time it should't have:

The code is full of cursing against Visual Studio bugs.
The code is full of cursing against slow generated code.
A full build took forever.
Trivia : The first things one should run on a new codebase are the following greps :


    find . -exec grep --with-filename --ignore-case fuck   {}   \; 2>/dev/null
    find . -exec grep --with-filename --ignore-case shit   {}   \; 2>/dev/null
    find . -exec grep --with-filename --ignore-case hack   {}   \; 2>/dev/null
    find . -exec grep --with-filename --ignore-case lame   {}   \; 2>/dev/null
    find . -exec grep --with-filename --ignore-case stupid {}   \; 2>/dev/null

    
I was not a game developer in the 90s but I wonder if C++ was a good choice (many did not recommend it) when the solid C with Watcom was available.

My second issue with C++ is that a C++ codebase it is like a box of chocolate: You never know what flavor you are going to get :



    I think C++ was pushed well beyond its complexity threshold and yet there are a lot of people programming it. But what you do is you
    force people to subset it. So almost every shop that I know of that uses C++ says, “Yes, we’re using C++ but we’re not doing 
    multiple-implementation inheritance and we’re not using operator overloading.” There are just a bunch of features that you’re not going
    to use because the complexity of the resulting code is too high. And I don’t think it’s good when you have to start doing that. You 
    lose this programmer portability where everyone can read everyone else’s code, which I think is such a good thing.

                                                                                                                     - Joshua Bloch (source)

  
Or more simply :

        

          With C++, it’s possible to make code that isn’t understandable by anyone, with C, this is very hard. 

                                                                                                                     — Mike Abrash (source)

    
In the case of Trespasser the subset selected was STL, smart pointers and extreme metaprogramming which is not a flavor I really enjoy. I will probably get back to it one day when I am more comfortable with it.

For now I will feel better thinking I am not alone. Really Really not alone :) !

Closing up
In the end, the source code of Jurassic Park: Trespasser re-inforced the sense of a missed opportunity which Seamus Blackley summarized in his an interview with trecom.org:



   Q: Trespasser is unfortunately known for not bringing in the sales it deserved. If you could change one thing about the production what 
      would you do differently?

   A: I would have assigned the 25-year-old Seamus a strong producer, who would have bullied him to restrict the scope of innovation to 
      something manageable. I wanted to make this beautiful vision that I had for this amazing island become real, and I was too young 
      and stupid to realize that less is more.


  
Less is more. Enough said.