import os
import subprocess
from tempfile import TemporaryDirectory

from utils import TestCase, file_content


class SimpleTaskTestCase(TestCase):
    """Test executing simple tasks."""

    def test_capture_outputs(self):
        with TemporaryDirectory() as tmpdir:
            output_file = os.path.join(tmpdir, 'output.log')
            subprocess.check_call(
                [
                    './ml-gridengine-executor',
                    '--server-host=127.0.0.1',
                    '--output-file={}'.format(output_file),
                    '--',
                    'python',
                    '-u',
                    '-c',
                    'import sys; print("output1"); print("output2", file=sys.stderr); print("output3")',
                ]
            )
            self.assertEqual(file_content(output_file), 'output1\noutput2\noutput3\n')

    def test_default_env_vars(self):
        expected_default_env = {
            'PYTHONUNBUFFERED': '1'
        }

        # If we do not have the above environmental variables configured,
        # the executor should set them with the default values.
        with TemporaryDirectory() as tmpdir:
            output_file = os.path.join(tmpdir, 'output.log')
            env = os.environ.copy()
            for k in expected_default_env:
                env.pop(k, None)
            subprocess.check_call(
                [
                    './ml-gridengine-executor',
                    '--server-host=127.0.0.1',
                    '--output-file={}'.format(output_file),
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

        # If we already have the above environmental variables, the executor should
        # left them untouched.
        with TemporaryDirectory() as tmpdir:
            output_file = os.path.join(tmpdir, 'output.log')
            env = os.environ.copy()
            for k in expected_default_env:
                env[k] = '{}_is_set'.format(k)
            subprocess.check_call(
                [
                    './ml-gridengine-executor',
                    '--server-host=127.0.0.1',
                    '--output-file={}'.format(output_file),
                    '--',
                    'env'
                ],
                env=env
            )
            output = file_content(output_file)
            parsed_env = {}
            for key, value in [l.split('=', 1) for l in output.split('\n') if '=' in l]:
                parsed_env[key] = value
            for key in expected_default_env:
                self.assertEqual(parsed_env[key], '{}_is_set'.format(key))

        # If we set the above environmental variables by specifying arguments to the
        # executor, the default values should not be used.
        with TemporaryDirectory() as tmpdir:
            output_file = os.path.join(tmpdir, 'output.log')
            env = os.environ.copy()
            env_args = []
            for k in expected_default_env:
                env.pop(k, None)
                env_args.append('--env={key}={key}_is_set_2'.format(key=k))
            subprocess.check_call(
                [
                    './ml-gridengine-executor',
                    '--server-host=127.0.0.1',
                    '--output-file={}'.format(output_file)
                ] + env_args + [
                    '--',
                    'env'
                ],
                env=env
            )
            output = file_content(output_file)
            parsed_env = {}
            for key, value in [l.split('=', 1) for l in output.split('\n') if '=' in l]:
                parsed_env[key] = value
            for key in expected_default_env:
                self.assertEqual(parsed_env[key], '{}_is_set_2'.format(key))
