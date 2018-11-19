#!/usr/bin/env python

from pprint import pprint
from flask import Flask, request, jsonify


app = Flask(__name__)


@app.route('/<task_id>/executor/_callback', methods=['POST'])
def task_executor_callback(task_id):
    body = request.json
    print('Executor callback for task {}'.format(task_id))
    if 'Authentication' in request.headers:
        print('Authentication: {}'.format(request.headers['Authentication']))
    pprint(body)
    return jsonify({})


if __name__ == '__main__':
    app.run()
