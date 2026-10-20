"""Decisions about the PlayStation executable's symbol table that tools/gendata.py applies.

symbols.tsv names every address the code refers to, so a global that is really a field or an
element of a bigger object appears twice: as the object and as a name inside it. The original
sources had one object; the port must too. For each such name there is a decision here, taken from
the machine code (every access to the addresses involved, with the width and stride it uses) and
the declarations in game/. The names are macros onto the real storage (or their uses were rewritten,
marked "Port:") in the shared headers listed, so no definition is generated for them and
`--check` verifies, for the 32-bit layout, that the storage the macro reaches lies exactly where the
executable has the name.

An Alias is: `name` (symbol of symbols.tsv), `container` (symbol that is defined), `expr` (a C
lvalue, in the headers `includes`, that is the first byte of the aliased storage) and the evidence.
"""
from collections import namedtuple

Alias = namedtuple('Alias', 'name container expr includes sdk evidence pair')


def A(name, container, expr, includes, evidence, sdk=False, pair=None):
    """`pair` (optional) is two lvalues that must be the same object: an element further into a
    strided alias and the container element it stands for."""
    return Alias(name, container, expr, tuple(includes), sdk, evidence, pair)


ALIASES = [
]

# Symbols defined under another C name: the executable's symbol -> the identifier in the data file.
RENAMES = {}

# Symbols declared with different types in different files: use the declaration with this spelling.
PREFER = {}

# Entries of incomplete arrays that the extent cannot give because the names inside them cut it short.
SIZES = {}

# Containers whose declared type is taken even though it is bigger than the space up to the next
# symbol of the table (the other names are inside it); everything in ALIASES counts as well.
BIGGER_THAN_EXTENT = set()

SYNTHETIC = []

SEPARATE = {}

ARENAS = {}
