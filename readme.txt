Raven City source code version 1.0.

Code by:
Andrew "Highlander" Lucas
Yriy "BUzer" Sitnikov 
Claudio "SysOp" Ficara
Moritz "Ryokeen" Ulte
Laurie Cheers,
Andrew "Confused" Hamilton
VALVe Software, Xaerox 
BattleGrounds Team.

Additional credit goes to:
Max "Protector" Vollmer
Aaron "aaron_da_killa" Zimmerman
James "Minuit" 
Members of VERC and the SOHL forums.

These people aren't properly credited within the code, so that's
why I try to credit all those who helped me in this file. I'm
thankfull for all of their help and that they shared their work
with those who needed it, that's why I'm also sharing my creations
with those like me. I started programming because I wanted to
extend Half-Life's features with new things, including fog, shadows,
a working particle system, and numerous bugfixes to the original
code base.

Anyone can use this code freely as long as they credit all the 
original creators properly.

Features of Raven City:

OpenGL Fog: 
Code by Highlander.

Motion Blur: 
Code by Ryokeen, modified by Highlander.

Stencil Shadows: 
Code based on BUzer's stencil shadow 
code, extended by Highlander and SysOp.

Dynamic Lighting: 
Code by Highlander and BUzer.

Dynamic NPC Step Code:
Code by Highlander.

Dynamic Decalling and Bullet Impact sounds:
Code by Highlander.

BG Particle System:
Code by BattleGrounds team, modified by
SysOp and Highlander.

CountDown Timer:
Based on code by Ryokeen, modified by Highlander.

SunGlare:
Code by Ryokeen, modified by Highlander.

Rain System:
Code by BUzer.

Weapons:

 - Spore Launcher: Code by Demiurge and Highlander.
 - M249 SAW: Code by Highlander.
 - M4A1 Sniper Rifle: Code by Highlander.
 - Desert Eagle: Code by Highlander.
 - Beretta: By Highlander.
 - Shock Rifle: By Demiurge and Highlander.
 - Knife: By Highlander.
 - Displacer: By G-Cont and Highlander.
 - Shotgun: Modified heavily by Highlander.
 - M4A1 Carbine: By Highlander.
 - Pipe Wrench: By Highlander.

NPC's:

 - Opposing Forces Human Grunt: By Highlander.
 - Opposing Forces Torch Grunt: By Highlander and G-Cont.
 - Opposing Forces Medic Grunt: By Highlander.
 - Special Ops Grunts: By Highlander.
 - Male Assassins: By Highlander.
 - SuperZombie: By Highlander.
 - Maria: By Highlander.
 - Boris: By Highlander.
 - Harrison: By Highlander.
 - Otis: By Highlander.

There's probably people that I forgot to credit in this file. I'm sorry
for that, and if perhaps someone notices his own work in the code and
he has recieved no credit, please inform me.

Andrew Steven Lucas, at highlander2nd@freemail.hu.

About stencil shadows:
Stencil shadows by default are quite slow with higher quality meshes, even
though these are optimized well. The point of these shadows is that an
optimized mesh is first loaded into the game, the shadow renderer exports
it's data into an external .dat file, and then you put your normal model
back to have that be used when you render the shadow volume. This saves
on performance with minimal quality loss. One thing to remember though, is
that the model used to render shadows must be closed, and that triangles
can't share more than two edges, otherwise visual bugs might occur. Also,
studiomdl seems to mess this up, but it's crucial that the arrangement of
bones must match between the two models, or the vertex transformations on
the shadow model might become corrupted, because the code tries to transform
with the wrong bone.

Readme file by Highlander, words by Highlander, file created by Highlander.
