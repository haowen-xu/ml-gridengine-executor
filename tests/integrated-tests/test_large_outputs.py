from utils import *


class LargeOutputsTestCase(TestCase):
    """Test executing tasks producing large outputs."""

    def test_save_last_1m_outputs(self):
        N = 1000000
        save_length = 1048576
        total_output = get_count_output(N)
        discarded_length = len(total_output) - save_length
        expected_output = '[{} ({:.2f}M) bytes discarded]\n'.format(
            discarded_length, discarded_length / 1048576.).encode('utf-8') + total_output[-save_length:]

        self.assertEqual(
            run_executor([get_count_exe(), str(N)], buffer_size=save_length)[0],
            expected_output
        )
