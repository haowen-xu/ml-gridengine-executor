import signal
import time

import pytest

from utils import *


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

    def test_status_file(self):
        with run_executor_context(['sh', '-c', 'sleep 3']) as (proc, ctx):
            status_file = ctx['status_file']

            time.sleep(1.5)
            status = json.loads(file_content(status_file, binary=False))
            self.assertEqual(status['status'], 'RUNNING')

            proc.wait()
            status = json.loads(file_content(status_file, binary=False))
            self.assertEqual(status['status'], 'EXITED')
            self.assertEqual(status['exitCode'], 0)

    def test_work_dir_exit_code_and_run_after_and_no_exit(self):
        with TemporaryDirectory() as tmpdir:
            work_dir = os.path.join(tmpdir, 'work_dir')
            if not work_dir.endswith('/'):
                work_dir += '/'
            status_file = os.path.join(tmpdir, 'status.json')
            after_log = os.path.join(tmpdir, 'after.json')
            proc = start_executor(['sh', '-c', 'echo hello > message.txt; exit 123'],
                                  status_file=status_file, work_dir=work_dir, run_after=get_after_script(after_log),
                                  no_exit=True)
            try:
                with pytest.raises(subprocess.TimeoutExpired):
                    proc.wait(.5)
                status = json.loads(file_content(status_file, binary=False))
                self.assertEqual(status['status'], 'EXITED')
                self.assertEqual(status['exitCode'], 123)

                after_log = get_after_log(after_log)
                self.assertDictEqual(after_log, {
                    'workDir': work_dir,
                    'exitStatus': 'EXITED',
                    'exitCode': '123'
                })

                message = file_content(os.path.join(work_dir, 'message.txt'), binary=False)
                self.assertEqual(message, 'hello\n')

                proc.send_signal(signal.SIGINT)
                self.assertEqual(proc.wait(), 0)

            finally:
                proc.kill()
                proc.wait()
