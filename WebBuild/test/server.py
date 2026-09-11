# server.py - serve a built web directory the way the shipped .htaccess tells
# Apache to serve it.
#
# python3 -m http.server sends no Cache-Control at all, and that is not a
# neutral choice: a response without one gets a *heuristic* lifetime in the
# browser, roughly a tenth of its age, so the harness was testing a
# configuration nobody deploys. That is how a stale touch_controls.js reached
# a real phone with every check green.
#
# The two rules are read out of WebBuild/htaccess rather than repeated here,
# because a copy is a thing that drifts: add a file to one list and forget the
# other and the test goes on passing.
#
#   python3 server.py <port> <directory>
import functools
import http.server
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

def readRules():
    """The FilesMatch patterns of htaccess, in order, with what each sets."""
    path = os.path.join(HERE, '..', 'htaccess')
    text = open(path, encoding='utf-8').read()
    rules = []
    for pattern, body in re.findall(
            r'<FilesMatch\s+"([^"]+)"\s*>(.*?)</FilesMatch>', text, re.S):
        value = re.search(r'Header\s+set\s+Cache-Control\s+"([^"]*)"', body)
        if value:
            rules.append((re.compile(pattern), value.group(1)))
    if not rules:
        raise SystemExit('server.py: no Cache-Control rules found in htaccess')
    return rules

RULES = readRules()

class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        name = self.path.lstrip('/').split('?')[0]
        for pattern, value in RULES:
            if pattern.match(name):
                self.send_header('Cache-Control', value)
                break
        super().end_headers()

    def log_message(self, *args):
        pass

if __name__ == '__main__':
    port, directory = int(sys.argv[1]), sys.argv[2]
    http.server.test(HandlerClass=functools.partial(Handler, directory=directory),
                     port=port, bind='127.0.0.1')
