from utils import TestCase, run_executor


class LargeOutputsTestCase(TestCase):
    """Test executing tasks producing large outputs."""

    def test_save_last_1m_outputs(self):
        N = 1000000
        save_length = 1048576
        total_output = ('\n'.join(str(i) for i in range(N)) + '\n').encode('utf-8')
        discarded_length = len(total_output) - save_length
        expected_output = '[{} ({:.2f}M) bytes discarded]\n'.format(
            discarded_length, discarded_length / 1048576.).encode('utf-8') + total_output[-save_length:]

        self.assertEqual(
            run_executor(
                ['python',
                 '-u',
                 '-c',
                 'print("\\n".join(str(i) for i in range({})))'.format(N)],
                buffer_size=save_length
            )[0],
            expected_output
        )
