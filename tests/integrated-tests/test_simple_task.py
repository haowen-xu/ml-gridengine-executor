import os
import subprocess
from tempfile import TemporaryDirectory

from utils import TestCase, file_content


class SimpleTaskTestCase(TestCase):
    """Test executing simple tasks."""

    def test_default_environ(self):
        expected_default_env = {
            'PYTHONUNBUFFERED': '1'
        }
        with TemporaryDirectory() as tmpdir:
            output_file = os.path.join(tmpdir, 'output.log')
            env = os.environ.copy()
            for k in expected_default_env:
                env.pop(k, None)
            subprocess.check_call(
                [
                    './ml-gridengine-executor',
                    '--save-output={}'.format(output_file),
                    '--',
                    'env'
                ],
                env=env
            )
            output = file_content(output_file)
            parsed_env = {}
            for key, value in [l.split('=', 1) for l in output.split('\n') if '=' in l]:
                parsed_env[key] = value
            for key, value in expected_default_env.items():
                self.assertEqual(parsed_env[key], value)

    def test_capture_outputs(self):
        with TemporaryDirectory() as tmpdir:
            output_file = os.path.join(tmpdir, 'output.log')
            subprocess.check_call(
                [
                    './ml-gridengine-executor',
                    '--save-output={}'.format(output_file),
                    '--',
                    'python',
                    '-u',
                    '-c',
                    'import sys; print("output1"); print("output2", file=sys.stderr); print("output3")',
                ]
            )
            self.assertEqual(file_content(output_file), 'output1\noutput2\noutput3\n')
