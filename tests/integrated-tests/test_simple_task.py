import os

from utils import TestCase, run_executor


class SimpleTaskTestCase(TestCase):
    """Test executing simple tasks."""

    def test_capture_outputs(self):
        self.assertEqual(
            run_executor(
                ['python', '-u', '-c',
                 'import sys; print("output1"); print("output2", file=sys.stderr); print("output3")']
            )[0],
            b'output1\noutput2\noutput3\n'
        )

    def test_default_env_vars(self):
        expected_default_env = {
            b'PYTHONUNBUFFERED': b'1'
        }

        def parse_env(content):
            ret = {}
            for line in content.split(b'\n'):
                pos = line.find(b'=')
                if pos > 0:
                    k, v = line[:pos], line[pos+1:]
                    ret[k] = v
            return ret

        # If we do not have the above environmental variables configured,
        # the executor should set them with the default values.
        os_env = os.environb.copy()
        for key in expected_default_env:
            os_env.pop(key, None)
        output, _ = run_executor(['env'], subprocess_kwargs={'env': os_env})
        output_env = parse_env(output)
        for key, value in expected_default_env.items():
            self.assertEqual(output_env[key], value)

        # If we already have the above environmental variables, the executor should
        # left them untouched.
        plan = {k: k + b'_is_set' for k in expected_default_env}
        os_env = os.environb.copy()
        os_env.update(plan)
        output, _ = run_executor(['env'], subprocess_kwargs={'env': os_env})
        output_env = parse_env(output)
        for key, value in expected_default_env.items():
            self.assertEqual(output_env[key], plan[key])

        # If we set the above environmental variables by specifying arguments to the
        # executor, the default values should not be used.
        plan = {k: k + b'_is_set_by_arg' for k in expected_default_env}
        os_env = os.environb.copy()
        for key in expected_default_env:
            os_env.pop(key, None)
        output, _ = run_executor(['env'], env=plan, subprocess_kwargs={'env': os_env})
        output_env = parse_env(output)
        for key, value in expected_default_env.items():
            self.assertEqual(output_env[key], plan[key])
