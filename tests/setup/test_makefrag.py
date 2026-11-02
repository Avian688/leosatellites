"""Check Make target selection without compiling or loading real dependencies."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class MakeFragmentTests(unittest.TestCase):
    def test_default_target_links_library(self):
        make = shutil.which('gmake') or shutil.which('make')
        if not make:
            self.skipTest('GNU Make 4.x is required')
        version = subprocess.check_output([make, '--version'], text=True)
        if 'GNU Make 4.' not in version:
            self.skipTest('GNU Make 4.x is required')
        fragment = Path(__file__).resolve().parents[2] / 'src/makefrag'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            pkg = root / 'pkg-config'
            pkg.write_text('#!/bin/sh\ncase "$1" in\n--exists) exit 0;;\n--cflags) echo -I/test/include;;\n--libs) echo -ligraph -lcurl;;\nesac\n')
            pkg.chmod(0o755)
            (root / 'Makefile').write_text(f'''O := {root}/out
MKPATH := mkdir -p
OBJS := example.o
PLATFORM := darwin
WITH_OSG := no
LEO_PKG_CONFIG_COMMAND := {pkg}
include {fragment}
all: example.o
\t@echo LINK_LIBRARY
example.o:
\t@echo COMPILE_OBJECT
''')
            for targets in ([], ['all']):
                result = subprocess.run([make, '-n', *targets], cwd=root,
                                        text=True, capture_output=True, check=True)
                self.assertIn('LINK_LIBRARY', result.stdout)
                self.assertIn('COMPILE_OBJECT', result.stdout)
                self.assertFalse((root / 'example.o').exists())


if __name__ == '__main__':
    unittest.main()
