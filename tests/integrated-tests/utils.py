import codecs
import unittest

__all__ = ['file_content', 'TestCase']


def file_content(path, binary=False):
    if binary:
        with open(path, 'rb') as f:
            return f.read()
    else:
        with codecs.open(path, 'rb', 'utf-8') as f:
            return f.read()


class TestCase(unittest.TestCase):
    """Base class for all test cases."""
