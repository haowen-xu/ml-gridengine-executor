#!/usr/bin/env python

import sys
import click
import requests


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


@click.command()
@click.option('--timeout', type=click.INT, help='Timeout argument for polling output.', required=True, default=60)
@click.argument('server-uri')
def main(timeout, server_uri):
    stream_uri = server_uri.rstrip('/') + '/output/_poll'
    begin = 0
    closed = False
    while not closed:
        r = requests.get('{}?begin={}&timeout={}'.format(stream_uri, begin, timeout), stream=True)
        if r.status_code == 410:
            closed = True
        elif r.status_code == 204:
            pass
        elif r.status_code == 200:
            content_iter = yield_content(r)
            r_begin = next(content_iter)
            if r_begin > begin:
                if begin > 0:
                    sys.stdout.buffer.write(b'\n')
                sys.stdout.buffer.write('[{} bytes discarded]\n'.format(r_begin - begin).encode('utf-8'))
                sys.stdout.buffer.flush()
            r_count = 0
            for chunk in content_iter:
                r_count += len(chunk)
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
            begin = r_begin + r_count
        else:
            raise RuntimeError('Error response: {} {}'.format(r.status_code, r.content))


if __name__ == '__main__':
    main()
