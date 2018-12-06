import base64
import re
import uuid
from collections import defaultdict

from flask import Flask, jsonify
from werkzeug.exceptions import InternalServerError

from utils import *


class CallbackApp(Flask):

    def __init__(self, *args, **kwargs):
        super(CallbackApp, self).__init__(*args, **kwargs)
        self.calls = []
        self.errors = 0
        self.route('/_callback', methods=['POST'])(self.callback_view)
        self.route('/_error', methods=['POST'])(self.error_view)

    def callback_view(self):
        self.calls.append({
            'token': request.headers.get('Authentication', None),
            'data': request.json,
        })
        return jsonify({})

    def error_view(self):
        self.errors += 1
        raise InternalServerError()


class CallbackTestCase(TestCase):

    def test_callback(self):
        token = str(uuid.uuid4())
        app = CallbackApp(__name__)
        args = ['python', os.path.join(os.path.split(os.path.abspath(__file__))[0], 'callback_demo.py')]
        with AppServer(app).run_context() as uri, \
                run_executor_context(args, callback=uri + '/_callback', token=token,
                                     watch_generated=True) as (proc, ctx):
            work_dir = ctx['work_dir']
            os.makedirs(os.path.join(work_dir, 'nested'))
            with open(os.path.join(work_dir, 'nested/payload.txt'), 'wb') as f:
                f.write(b'hello, world!')

            proc.wait()
            work_dir_size = compute_fs_size(work_dir)

        # check the number of callbacks
        self.assertEqual(len(app.calls), 10)

        # check the tokens
        token_str = 'TOKEN {}'.format(base64.b64encode(token.encode('utf-8')).decode('utf-8'))
        for call in app.calls:
            self.assertEqual(call['token'], token_str)

        # check the RUNNING notification
        self.assertEqual(app.calls[0]['data']['eventType'], 'statusUpdated')
        self.assertEqual(app.calls[0]['data']['data']['status'], 'RUNNING')

        # check the EXITED notification
        self.assertEqual(app.calls[-1]['data']['eventType'], 'statusUpdated')
        self.assertEqual(app.calls[-1]['data']['data']['status'], 'EXITED')
        self.assertEqual(app.calls[-1]['data']['data']['exitCode'], 123)
        self.assertEqual(app.calls[-1]['data']['data']['workDirSize'], work_dir_size)

        # check the generated content callbacks
        met = defaultdict(lambda: 0)
        for c in app.calls[1:5]:
            m = re.match('^fileGenerated:(result|config|defConfig|webUI)$', c['data']['eventType'])
            self.assertIsNotNone(m)
            self.assertEqual(c['data']['data']['{}Value'.format(m.group(1))],
                             '{}Value1'.format(m.group(1)))
            met[m.group(1)] += 1
        for k, v in met.items():
            self.assertEqual(v, 1)

        met = defaultdict(lambda: 0)
        for c in app.calls[5:9]:
            m = re.match('^fileGenerated:(result|config|defConfig|webUI)$', c['data']['eventType'])
            self.assertIsNotNone(m)
            self.assertEqual(c['data']['data']['{}Value'.format(m.group(1))],
                             '{}Value2'.format(m.group(1)))
            met[m.group(1)] += 1
        for k, v in met.items():
            self.assertEqual(v, 1)

    def test_error_callback(self):
        app = CallbackApp(__name__)
        args = ['sh', '-c', 'echo "hello, world!"; exit 123']
        env = os.environb.copy()
        env[b'ML_GRIDENGINE_CALLBACK_MAX_RETRY'] = b'1'
        with AppServer(app).run_context() as uri, \
                run_executor_context(args, callback=uri + '/_error', watch_generated=True,
                                     subprocess_kwargs={'env': env}) as (proc, ctx):
            proc.wait()
            self.assertEqual(app.errors, 4)
            # the status file should be written even callback failed
            status = json.loads(file_content(ctx['status_file'], binary=False))
            self.assertEqual(status['status'], 'EXITED')
            self.assertEqual(status['exitCode'], 123)
            # the output file should be written even callback failed
            self.assertEqual(file_content(ctx['output_file']), b'hello, world!\n')
