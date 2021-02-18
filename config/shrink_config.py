import json


goodconfig = ''
with open("config.json") as config:
    for line in config.readlines():
        if not line.startswith('#'):
            goodconfig += line
    print(json.dumps(json.loads(goodconfig),separators=(',', ':')))

