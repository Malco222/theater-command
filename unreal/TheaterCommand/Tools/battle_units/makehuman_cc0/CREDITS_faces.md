# Credits: the face shapes

The 65 `.target` files in `targets/` that `fetch_faces.sh` lists are MakeHuman face shape targets by the
MakeHuman team (Manuel Bastioni and the MakeHuman community; Data Collection AB, Joel Palmius, Jonas Hauquier),
released CC0 (public domain dedication) in 2020: http://www.makehumancommunity.org, source
https://github.com/makehumancommunity/makehuman. CC0 asks for no credit; we give it anyway.

`variety.py` mixes them into eight faces per army and stores each face as a small move of the soldier's own head
points, so the game never reads these files. They sit beside the base man's files (`fetch.sh`, `CREDITS.md`,
`LICENSE.md` on the battle-units branch), which `variety.py` also needs. No MakeHuman code is used or shipped.
