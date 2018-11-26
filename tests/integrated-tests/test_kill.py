import signal
import time

import pytest

from utils import *


class KillTestCase(TestCase):

    def test_force_kill(self):
        with TemporaryDirectory() as tmpdir:
            status_file = os.path.join(tmpdir, 'status.json')
            output_file = os.path.join(tmpdir, 'output.log')
            after_log = os.path.join(tmpdir, 'after.json')
            work_dir = os.path.join(tmpdir, 'work_dir')
            if not work_dir.endswith('/'):
                work_dir += '/'
            args = ['python', '-c', 'import time\n'
                                    'i = 0\n'
                                    'while True:\n'
                                    '  try:\n'
                                    '    while True:\n'
                                    '      print(i)\n'
                                    '      i += 1\n'
                                    '      time.sleep(1)\n'
                                    '  except KeyboardInterrupt:\n'
                                    '    print("keyboard interrupt")\n']
            env = os.environb.copy()
            env.update({
                b'ML_GRIDENGINE_KILL_PROGRAM_FIRST_WAIT_SECONDS': b'1',
                b'ML_GRIDENGINE_KILL_PROGRAM_SECOND_WAIT_SECONDS': b'2',
                b'ML_GRIDENGINE_KILL_PROGRAM_FINAL_WAIT_SECONDS': b'3'
            })
            proc = start_executor(args, status_file=status_file, output_file=output_file, work_dir=work_dir,
                                  run_after=get_after_script(after_log), subprocess_kwargs={'env': env})
            try:
                time.sleep(.5)
                proc.send_signal(signal.SIGINT)
                with pytest.raises(subprocess.TimeoutExpired):
                    _ = proc.wait(1)
                self.assertEqual(proc.wait(2.5), 0)

                status = json.loads(file_content(status_file, binary=False))
                print(status)
                self.assertEqual(status['status'], 'SIGNALLED')
                self.assertEqual(status['exitSignal'], signal.SIGKILL)

                after_log = get_after_log(after_log)
                print(after_log)
                self.assertDictEqual(after_log, {
                    'workDir': work_dir,
                    'exitStatus': 'SIGNALLED',
                    'exitSignal': str(int(signal.SIGKILL))
                })
            finally:
                proc.wait()
