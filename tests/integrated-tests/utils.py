import codecs
import json
import os
import subprocess
import sys
import time
import unittest
from contextlib import contextmanager
from tempfile import TemporaryDirectory


def file_content(path, binary=True):
    if binary:
        with open(path, 'rb') as f:
            return f.read()
    else:
        with codecs.open(path, 'rb', 'utf-8') as f:
            return f.read()


def get_after_script(after_log):
    return 'python "{}" "{}"'.format(
        os.path.join(os.path.split(os.path.abspath(__file__))[0], 'after_script.py'), after_log)


def get_after_log(after_log):
    with codecs.open(after_log, 'rb', 'utf-8') as f:
        return json.loads(f.read())


def start_executor(args, output_file=None, status_file=None, port=None, callback=None, token=None, env=None,
                   work_dir=None, run_after=None, no_exit=False, buffer_size=4 * 1024 * 1024, subprocess_kwargs=None):
    S = lambda s: s.decode('utf-8') if isinstance(s, bytes) else s
    executor_args = [
        './ml-gridengine-executor',
        '--server-host=127.0.0.1',
        '--buffer-size={}'.format(buffer_size),
    ]
    if output_file:
        executor_args.append('--output-file={}'.format(output_file))
    if status_file:
        executor_args.append('--status-file={}'.format(status_file))
    if port:
        executor_args.append('--port={}'.format(port))
    if callback:
        executor_args.append('--callback-api={}'.format(callback))
    if token:
        executor_args.append('--callback-token={}'.format(token))
    if env:
        for k, v in env.items():
            executor_args.append('--env={}={}'.format(S(k), S(v)))
    if work_dir:
        executor_args.append('--work-dir={}'.format(work_dir))
    if run_after:
        executor_args.append('--run-after={}'.format(run_after))
    if no_exit:
        executor_args.append('--no-exit')
    executor_args.append('--')
    executor_args.extend(args)
    print('Start executor: {}'.format(executor_args))
    return subprocess.Popen(executor_args, **(subprocess_kwargs or {}))


@contextmanager
def run_executor_context(args, **kwargs):
    with TemporaryDirectory() as tmpdir:
        kwargs.setdefault('status_file', os.path.join(tmpdir, 'status.json'))
        kwargs.setdefault('output_file', os.path.join(tmpdir, 'output.log'))
        status_file = kwargs['status_file']
        proc = start_executor(args, **kwargs)
        try:
            while proc.poll() is None and not os.path.exists(status_file):
                time.sleep(.1)
            status = json.loads(file_content(status_file, binary=False))
            yield proc, {'uri': 'http://127.0.0.1:{}'.format(status['executor.port']),
                         'status_file': status_file,
                         'output_file': kwargs['output_file']}
        finally:
            proc.kill()
            proc.wait()


def run_executor(args, **kwargs):
    kwargs.setdefault('subprocess_kwargs', {})
    kwargs['subprocess_kwargs'].setdefault('stdout', subprocess.PIPE)
    kwargs['subprocess_kwargs'].setdefault('stderr', subprocess.STDOUT)
    with run_executor_context(args, **kwargs) as (proc, ctx):
        executor_output = None
        try:
            executor_output = proc.stdout.read()
            with open(ctx['output_file'], 'rb') as f:
                program_output = f.read()
            return program_output, executor_output
        except Exception:
            if executor_output:
                sys.stderr.buffer.write(executor_output)
            raise
        finally:
            proc.kill()
            proc.wait()


def get_count_output(N):
    if N not in _cached_output_output:
        _cached_output_output[N] = subprocess.check_output(['./Count', str(N)])
    return _cached_output_output[N]

_cached_output_output = {}


class TestCase(unittest.TestCase):
    """Base class for all test cases."""
