#!/usr/bin/env python

import codecs
import json
import os
import sys

if __name__ == '__main__':
    env_dict = {
        'workDir': os.environ.get('ML_GRIDENGINE_PROGRAM_WORK_DIR', None),
        'exitStatus': os.environ.get('ML_GRIDENGINE_PROGRAM_EXIT_STATUS', None),
        'exitCode': os.environ.get('ML_GRIDENGINE_PROGRAM_EXIT_CODE', None),
        'exitSignal': os.environ.get('ML_GRIDENGINE_PROGRAM_EXIT_SIGNAL', None)
    }
    env_dict = {k: v for k, v in env_dict.items() if v is not None}
    with codecs.open(sys.argv[1], 'wb', 'utf-8') as f:
        f.write(json.dumps(env_dict))
