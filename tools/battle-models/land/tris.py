"""Triangles by group of parts: python tris.py <Name>"""
import os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa
import vehicles
for fine in (True, False):
    v = vehicles.BUILDERS[sys.argv[1]](fine)
    m = v.m
    c = collections.Counter()
    for f, p in zip(m.faces, m.fpart):
        n = m.parts[p].name
        key = n.split(".")[0].rstrip("0123456789")
        c[key] += len(f) - 2
    print("fine" if fine else "plain", m.triangles(), dict(c.most_common()))
