import codecs
import json
import os
import subprocess
import sys
import unittest
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
                   run_after=None, buffer_size=4 * 1024 * 1024, subprocess_kwargs=None):
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
    if run_after:
        executor_args.append('--run-after={}'.format(run_after))
    executor_args.append('--')
    executor_args.extend(args)
    print('Start executor: {}'.format(executor_args))
    return subprocess.Popen(executor_args, **(subprocess_kwargs or {}))


def run_executor(args, status_file=None, port=None, callback=None, token=None, env=None,
                 buffer_size=4 * 1024 * 1024, subprocess_kwargs=None):
    with TemporaryDirectory() as tmpdir:
        output_file = os.path.join(tmpdir, 'output.log')
        subprocess_kwargs = subprocess_kwargs or {}
        subprocess_kwargs['stdout'] = subprocess.PIPE
        subprocess_kwargs['stderr'] = subprocess.STDOUT
        proc = start_executor(
            args,
            output_file=output_file,
            status_file=status_file,
            port=port,
            callback=callback,
            token=token,
            env=env,
            buffer_size=buffer_size,
            subprocess_kwargs=subprocess_kwargs
        )
        executor_output = None
        try:
            executor_output = proc.stdout.read()
            with open(output_file, 'rb') as f:
                program_output = f.read()
            return program_output, executor_output
        except Exception:
            if executor_output:
                sys.stderr.buffer.write(executor_output)
            raise
        finally:
            proc.kill()
            proc.wait()


class TestCase(unittest.TestCase):
    """Base class for all test cases."""
