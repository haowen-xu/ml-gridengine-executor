import os
import subprocess
from tempfile import TemporaryDirectory

from utils import TestCase, file_content


class LargeOutputsTestCase(TestCase):
    """Test executing tasks producing large outputs."""

    def test_save_last_1m_outputs(self):
        N = 1000000
        save_length = 1048576
        total_output = '\n'.join(str(i) for i in range(N)) + '\n'
        discarded_length = len(total_output) - save_length
        expected_output = '[{} ({:.2f}M) bytes discarded]\n{}'.format(
            discarded_length,
            discarded_length / 1048576.,
            total_output[-save_length:]
        )

        with TemporaryDirectory() as tmpdir:
            output_file = os.path.join(tmpdir, 'output.log')
            subprocess.check_call(
                [
                    './ml-gridengine-executor',
                    '--server-host=127.0.0.1',
                    '--output-file={}'.format(output_file),
                    '--buffer-size={}'.format(save_length),
                    '--',
                    'python',
                    '-u',
                    '-c',
                    'print("\\n".join(str(i) for i in range({})))'.format(N),
                ]
            )
            self.assertEqual(file_content(output_file), expected_output)
