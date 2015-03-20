# -*- coding: UTF-8 -*-
# Copyright © 2010-2012 Piotr Ożarowski <piotr@debian.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import logging
import os
import re
from datetime import datetime
from pickle import dumps
from os.path import isdir, islink, join, split
from subprocess import Popen, PIPE
from debpython.version import getver

log = logging.getLogger(__name__)
EGGnPTH_RE = re.compile(r'(.*?)(-py\d\.\d(?:-[^.]*)?)?(\.egg-info|\.pth)$')
SHEBANG_RE = re.compile(r'^#!\s*(.*?/bin/.*?)(python(?:(3.\d+)|3)(?:-dbg)?)(?:\s(.*))?')


def relpath(target, link):
    """Return relative path.

    >>> relpath('/usr/share/python-foo/foo.py', '/usr/bin/foo', )
    '../share/python-foo/foo.py'
    """
    t = target.split('/')
    l = link.split('/')
    while l[0] == t[0]:
        del l[0], t[0]
    return '/'.join(['..'] * (len(l) - 1) + t)


def relative_symlink(target, link):
    """Create relative symlink."""
    return os.symlink(relpath(target, link), link)


def move_file(fpath, dstdir):
    """Move file to dstdir. Works with symlinks (including relative ones)."""
    if isdir(fpath):
        dname = split(fpath)[-1]
        for fn in os.listdir(fpath):
            move_file(join(fpath, fn), join(dstdir, dname))

    if islink(fpath):
        dstpath = join(dstdir, split(fpath)[-1])
        relative_symlink(os.readlink(fpath), dstpath)
        os.remove(fpath)
    else:
        os.rename(fpath, dstdir)


def move_matching_files(src, dst, pattern):
    """Move files (preserving path) that match given pattern.

    move_files('foo/bar/', 'foo/baz/', 'spam/.*\.so$')
    will move foo/bar/a/b/c/spam/file.so to foo/baz/a/b/c/spam/file.so
    """
    match = re.compile(pattern).search
    for root, dirs, filenames in os.walk(src):
        for fn in filenames:
            spath = join(root, fn)
            if match(spath):
                dpath = join(dst, spath.lstrip(src).lstrip('/'))
                os.renames(spath, dpath)



def fix_shebang(fpath, replacement=None):
    """Normalize file's shebang.

    :param replacement: new shebang command (path to interpreter and options)
    """
    try:
        with open(fpath, 'rb') as fp:
            fcontent = fp.readlines()
            if not fcontent:
                log.info('fix_shebang: ignoring empty file: %s', fpath)
                return None
            try:
                first_line = str(fcontent[0], 'utf8')
            except UnicodeDecodeError:
                return None
    except IOError:
        log.error('cannot open %s', fpath)
        return False

    match = SHEBANG_RE.match(first_line)
    if not match:
        return None
    if not replacement:
        path, interpreter, version, argv = match.groups()
        if path != '/usr/bin':  # f.e. /usr/local/* or */bin/env
            replacement = "/usr/bin/%s" % interpreter
        if replacement and argv:
            replacement += " %s" % argv
    if replacement:
        log.info('replacing shebang in %s (%s)', fpath, first_line)
        # do not catch IOError here, the file is zeroed at this stage so it's
        # better to fail dh_python2
        with open(fpath, 'wb') as fp:
            fp.write(("#! %s\n" % replacement).encode('utf-8'))
            fp.writelines(fcontent[1:])
    return True


def shebang2pyver(fpath):
    """Check file's shebang.

    :rtype: tuple
    :returns: pair of Python interpreter string and Python version
    """
    try:
        with open(fpath, 'rb') as fp:
            data = fp.read(64)
            if b"\0" in data:
                # binary file
                return None
            match = SHEBANG_RE.match(str(data, 'utf-8'))
            if not match:
                return None
            res = match.groups()
            if res[1:3] != (None, None):
                if res[2]:
                    return res[1], getver(res[2])
                return res[1], None
    except IOError:
        log.error('cannot open %s', fpath)


def clean_egg_name(name):
    """Remove Python version and platform name from Egg files/dirs.

    >>> clean_egg_name('python_pipeline-0.1.3_py3k-py3.1.egg-info')
    'python_pipeline-0.1.3_py3k.egg-info'
    >>> clean_egg_name('Foo-1.2-py2.7-linux-x86_64.egg-info')
    'Foo-1.2.egg-info'
    """
    match = EGGnPTH_RE.match(name)
    if match and match.group(2) is not None:
        return ''.join(match.group(1, 3))
    return name


def execute(command, cwd=None, env=None, log_output=None):
    """Execute external shell commad.

    :param cdw: currennt working directory
    :param env: environment
    :param log_output:
        * opened log file or path to this file, or
        * None if output should be included in the returned dict, or
        * False if output should be redirectored to stdout/stderr
    """
    args = {'shell': True, 'cwd': cwd, 'env': env}
    close = False
    if log_output is False:
        pass
    elif log_output is None:
        args.update(stdout=PIPE, stderr=PIPE)
    elif log_output:
        if isinstance(log_output, str):
            close = True
            log_output = open(log_output, 'a')
        log_output.write('\n# command executed on {}'.format(datetime.now().isoformat()))
        log_output.write('\n$ {}\n'.format(command))
        log_output.flush()
        args.update(stdout=log_output, stderr=log_output)

    log.debug('invoking: %s', command)
    with Popen(command, **args) as process:
        stdout, stderr = process.communicate()
        close and log_output.close()
        return dict(returncode=process.returncode,
                    stdout=stdout and str(stdout, 'utf-8'),
                    stderr=stderr and str(stderr, 'utf-8'))


class memoize:
    def __init__(self, func):
        self.func = func
        self.cache = {}

    def __call__(self, *args, **kwargs):
        key = dumps((args, kwargs))
        if key not in self.cache:
            self.cache[key] = self.func(*args, **kwargs)
        return self.cache[key]
