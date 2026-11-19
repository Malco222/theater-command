"""Every scenery model of the Asian sets by name: which builder makes it and with what."""
import flora

BUILDERS = dict(flora.BUILDERS)
try:
    import houses
    BUILDERS.update(houses.BUILDERS)
except ImportError:
    pass
try:
    import rocks
    BUILDERS.update(rocks.BUILDERS)
except ImportError:
    pass


def build(name, lod=0, **kw):
    return BUILDERS[name](lod=lod, **kw)
