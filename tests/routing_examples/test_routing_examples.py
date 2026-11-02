"""Configuration and launcher regression checks; never starts OMNeT++."""
import configparser
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[2] / 'simulations/pythonScripts/routingExamples'
sys.path.insert(0, str(SCRIPTS))
import generateRoutingExamples as generator
import routingSupport
import saveRoutes
import viewRoutes


class RoutingExamplesTests(unittest.TestCase):
    def test_shared_geometry(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.multiple(generator, EXPERIMENT_DIR=root, INI_FILE=root/'routing.ini',
                                VISUALIZATION_INI_FILE=root/'visualize.ini'):
                with contextlib.redirect_stdout(io.StringIO()):
                    generator.generate_ini(600)
            routing = configparser.ConfigParser(interpolation=None)
            viewer = configparser.ConfigParser(interpolation=None)
            routing.read(root/'routing.ini'); viewer.read(root/'visualize.ini')
            general = routing['General']; display = viewer['General']
            for kind, count in [('groundStation', int(general['**.numOfGS'])),
                                ('userTerminal', int(general['**.numOfUserTerminals']))]:
                for index in range(count):
                    for coordinate in ('latitude', 'longitude'):
                        prefix = '**' if kind == 'groundStation' else '*'
                        self.assertEqual(general[f'{prefix}.{kind}[{index}].mobility.{coordinate}'],
                                         display[f'*.{kind}[{index}].{coordinate}'])
            self.assertEqual(general['**.numOfUserTerminals'], '10')
            self.assertNotIn('.app[', (root/'topology.ini').read_text())
            self.assertEqual(routing['Config GenerateShortestPaths']['**.loadFiles'], 'false')
            self.assertEqual(routing['Config Generate_EdgeDisjoint']['*.configurator.kPathSnapshotMode'], '"generate"')
            self.assertEqual(viewer['Config View_ShortestPath']['*.pathVisualizer.shortestPathOnly'], 'true')

    def test_save_dispatch(self):
        for topology in ('ISL', 'GroundRelay'):
            for mode, viewer, expected in [('shortest', False, 1), ('shortest', True, 2), ('multiple', False, 2)]:
                argv = ['saveRoutes.py', '--mode', mode, '--topology', topology, '--cores', '3']
                if viewer: argv.append('--with-viewer')
                with patch.object(sys, 'argv', argv), patch.object(saveRoutes, 'generate_ini'), \
                     patch.object(saveRoutes, 'install_signal_handlers'), \
                     patch.object(saveRoutes, 'run_simulation_configs') as run, \
                     contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(saveRoutes.main(), 0)
                self.assertEqual(run.call_count, expected)
                suffix = '_GroundRelay' if topology == 'GroundRelay' else ''
                self.assertEqual(run.call_args_list[0].args[0][0].config_name, 'GenerateShortestPaths'+suffix)
                if expected == 2:
                    self.assertEqual(len(run.call_args_list[1].args[0]), 1 if viewer else 7)
                    self.assertEqual(run.call_args_list[1].args[2], 3)

    def test_viewer_dispatch_and_dependencies(self):
        for topology in ('ISL', 'GroundRelay'):
            with patch.object(sys, 'argv', ['viewRoutes.py', '--topology', topology, '--policy', 'ShortestPath']), \
                 patch.object(viewRoutes, 'generate_visualization_ini'), \
                 patch.object(Path, 'is_file', return_value=True), \
                 patch.object(viewRoutes.subprocess, 'call', return_value=0) as call, \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(viewRoutes.main(), 0)
            command = call.call_args.args[0]
            self.assertIn('View_ShortestPath' + ('_GroundRelay' if topology == 'GroundRelay' else ''), command)
        for path in routingSupport.common_ned_path().split(':'):
            self.assertTrue(Path(path).is_dir())
        self.assertEqual({Path(path).parts[-2] for path in routingSupport.common_ned_path().split(':')},
                         {'leosatellites', 'os3', 'inet4.5'})
        self.assertEqual(len(routingSupport.load_libraries()), 3)


if __name__ == '__main__':
    unittest.main()
