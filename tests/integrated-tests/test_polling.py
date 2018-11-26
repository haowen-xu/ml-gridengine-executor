import requests

from utils import *


def poll_output(server_uri, on_data):
    def yield_content(response, buffer_size=8192):
        # buffer of the first chunk
        first_chunk_buffer = b''
        content_iter = response.iter_content(chunk_size=buffer_size)

        # reading off the first line
        for chunk in content_iter:
            first_chunk_buffer += chunk
            pos = first_chunk_buffer.find(b'\n')
            if pos > 0:
                yield int(first_chunk_buffer[:pos], 16)
                first_chunk_buffer = first_chunk_buffer[pos+1:]
                break

        if first_chunk_buffer:
            yield first_chunk_buffer

        for chunk in content_iter:
            yield chunk

    poll_uri = server_uri.rstrip('/') + '/output/_poll'
    begin = 0
    closed = False
    while not closed:
        r = requests.get('{}?begin={}&timeout=3'.format(poll_uri, begin), stream=True)
        if r.status_code == 410:
            closed = True
        elif r.status_code == 204:
            pass
        elif r.status_code == 200:
            content_iter = yield_content(r)
            r_begin = next(content_iter)
            for chunk in content_iter:
                on_data(r_begin, chunk)
                r_count = len(chunk)
                r_begin += r_count
            begin = r_begin
        else:
            raise RuntimeError('Error response: {} {}'.format(r.status_code, r.content))


class PollingTestCase(TestCase):

    def test_polling(self):
        outputs = []
        args = ['python', '-c', 'import time\n'
                                'for i in range(10):\n'
                                '  print(i)\n'
                                '  time.sleep(.5)']
        with run_executor_context(args, no_exit=True) as (proc, ctx):
            poll_output(ctx['uri'], lambda begin, data: outputs.append((time.time(), begin, data)))
        begin_time = outputs[0][0]
        for i in range(10):
            self.assertLessEqual(abs(outputs[i][0] - (begin_time + .5 * i)), .2)
            self.assertEqual(outputs[i][1], i * 2)
            self.assertEqual(outputs[i][2], '{}\n'.format(i).encode('utf-8'))

    def test_polling_on_large_outputs(self):
        # generate the payload
        N = 10000000
        total_output = get_count_output(N)

        # run the executor
        outputs = []
        with run_executor_context(['bash', '-c', 'sleep 1; ./Count {}'.format(N)], no_exit=True) as (proc, ctx):
            poll_output(ctx['uri'], lambda begin, data: outputs.append((time.time(), begin, data)))
        self.assertGreater(len(outputs), 0)

        # check the output
        received_count = 0
        for i in range(10):
            offset = outputs[i][1]
            chunk = outputs[i][2]
            self.assertEqual(chunk, total_output[offset: offset + len(chunk)])
            received_count += len(chunk)
        self.assertLessEqual(received_count, len(total_output))
        self.assertGreater(received_count, 0)
