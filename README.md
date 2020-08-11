# ray-caster
A simple ray casting engine I'm building using two languages:
* Python, with pygame and numpy
* C++, with SDL for windows API support (basically the backend of pygame)

The python engine is way ahead of the C++ engine, as I started work on it first.
I will only be working on the C++ engine from now on. I may work on the python engine on request.

Enemy agents are now added to the engine. As of right now they keep following the player unless hardcoded not to.
Once the enemy/s touch the player, the game is over.
Agent sprite rendering is also now supported. The sprites clip around walls.

Textured walls have finally been added. I realized very late about the horizontal distortion that was happening due to
a constant angular step size taken during ray casting. I've fixed it with an arctan calculation for each column of pixels
on the screen. The wall textures now look realistic, except when you go very close to them and they distort.

There is nothing to do in the map as of right now, you can only roam around and be followed by the one enemy.
Apart from that, some bug testing / play testing is needed.

I'm also planning on adding GPU rendering in the next commit. (oops, worked on wall textures instead!)
I want to keep the CPU and GPU rendering repos separate, so I'll be creating a different repo or a different branch
for it. Maybe fork it to a different repo altogether.

Thanks for reading this.