"""Tiny patch helper for bot sources that keeps each file's line endings.

    from patch import Patch
    p = Patch('bots/v221/src/spread.cpp'); p.rep(old, new); p.save()
"""


class Patch:
    def __init__(self, path):
        self.path = path
        raw = open(path, encoding='utf-8', newline='').read()
        self.crlf = '\r\n' in raw
        self.s = raw.replace('\r\n', '\n')

    def rep(self, old, new, count=1):
        assert self.s.count(old) >= 1, 'not found in %s: %r' % (self.path, old[:80])
        if count == 1:
            assert self.s.count(old) == 1, 'not unique in %s: %r' % (self.path, old[:80])
        self.s = self.s.replace(old, new)
        return self

    def before(self, anchor, text):
        return self.rep(anchor, text + anchor)

    def after(self, anchor, text):
        return self.rep(anchor, anchor + text)

    def save(self):
        out = self.s.replace('\n', '\r\n') if self.crlf else self.s
        open(self.path, 'w', encoding='utf-8', newline='').write(out)
