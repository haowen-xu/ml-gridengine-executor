import codecs
import sys
import time

time.sleep(.5)
with codecs.open('result.json', 'wb', 'utf-8') as f:
    f.write('{"resultValue": "resultValue1"}')
with codecs.open('config.json', 'wb', 'utf-8') as f:
    f.write('{"configValue": "configValue1"}')
with codecs.open('config.defaults.json', 'wb', 'utf-8') as f:
    f.write('{"defConfigValue": "defConfigValue1"}')

time.sleep(6)
with codecs.open('result.json', 'wb', 'utf-8') as f:
    f.write('{"resultValue": "resultValue2"}')
with codecs.open('config.json', 'wb', 'utf-8') as f:
    f.write('{"configValue": "configValue2"}')
with codecs.open('config.defaults.json', 'wb', 'utf-8') as f:
    f.write('{"defConfigValue": "defConfigValue2"}')

sys.exit(123)
