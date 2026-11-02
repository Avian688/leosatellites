"""Setup regression checks; no compilation or package installation."""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('leo_setup', Path(__file__).resolve().parents[2]/'setup.py')
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


class SetupTests(unittest.TestCase):
    def detected(self, root):
        return (root/'inet', root/'os3', '/tools/pkg-config',
                {'PKG_CONFIG_PATH': '/packages with spaces/lib/pkgconfig'},
                '-I/packages/include', '-ligraph -lcurl', '/tools/opp_makemake')

    def test_check_has_no_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(setup, 'ROOT', root), patch.object(setup, 'discover', return_value=self.detected(root)), \
                 patch.object(setup, 'run') as run, contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(setup.main(['--check']), 0)
            run.assert_not_called()
            self.assertEqual(list(root.iterdir()), [])

    def test_generation_and_failure_rollback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(setup, 'ROOT', root), patch.object(setup, 'discover', return_value=self.detected(root)), \
                 patch.object(setup, 'run') as run, contextlib.redirect_stdout(io.StringIO()):
                setup.main([])
                self.assertEqual(run.call_args.args[0], ['/tools/opp_makemake', '-f', '--make-so', '--deep', '-o', 'leosatellites'])
                self.assertEqual(run.call_args.args[2], root/'src')
                config = root/'config.local.mk'
                self.assertIn("'PKG_CONFIG_PATH=/packages with spaces/lib/pkgconfig'", config.read_text())
                config.write_text('previous settings\n')
                run.side_effect = RuntimeError('generation failed')
                with self.assertRaises(RuntimeError):
                    setup.main([])
                self.assertEqual(config.read_text(), 'previous settings\n')
                config.unlink()
                with self.assertRaises(RuntimeError):
                    setup.main([])
                self.assertFalse(config.exists())

    def test_missing_dependency_checkout(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, 'Missing'):
                setup.main(['--inet', directory, '--os3', directory, '--check'])


if __name__ == '__main__':
    unittest.main()
