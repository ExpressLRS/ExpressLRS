import os
import re
import sys
import subprocess

import gzip
from minify import (html_minifier, rcssmin, rjsmin)

def get_git_version(env):
    # Don't try to pull the git revision when doing tests, as
    # `pio remote test` doesn't copy the entire repository, just the files
    if env['PIOPLATFORM'] == "native":
        return "001122334455"

    try:
        import git
    except ImportError:
        sys.stdout.write("Installing GitPython")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "GitPython"])
        try:
            import git
        except ImportError:
            env.Execute("$PYTHONEXE -m pip install GitPython")
            try:
                import git
            except ImportError:
                git = None

    sha = None
    if git:
        try:
            git_repo = git.Repo(
                os.path.abspath(os.path.join(os.getcwd(), os.pardir)),
                search_parent_directories=False)
            try:
                ver = git_repo.git.describe("--tags", "--exact-match")
            except git.exc.GitCommandError:
                try:
                    ver = git_repo.git.symbolic_ref("-q", "--short", "HEAD")
                except git.exc.GitCommandError:
                    ver = "unknown"
            hash = git_repo.git.rev_parse("--short", "HEAD")
        except git.InvalidGitRepositoryError:
            pass
    return '%s (%s)' % (ver, hash)

def build_version(out, env):
    out.write('static const char VERSION[] = "%s";\n\n' % get_git_version(env))

def build_html(mainfile, var, out, env):
    with open('html/%s' % mainfile, 'r') as file:
        data = file.read()
    if mainfile.endswith('.html'):
        data = html_minifier.html_minify(data).replace('@VERSION@', get_git_version(env)).replace('@PLATFORM@', re.sub("_via_.*", "", env['PIOENV']))
    if mainfile.endswith('.css'):
        data = rcssmin.cssmin(data)
    if mainfile.endswith('.js'):
        data = rjsmin.jsmin(data)
    out.write('static const char PROGMEM %s[] = {\n' % var)
    out.write(','.join("0x{:02x}".format(c) for c in gzip.compress(data.encode('utf-8'))))
    out.write('\n};\n\n')

def build_common(out, env):
    build_html("scan.js", "SCAN_JS", out, env)
    build_html("main.css", "CSS", out, env)
    build_html("flag.svg", "FLAG", out, env)

def build_tx_html(source, target, env):
    out = open("src/ESP32_WebContent.h", 'w')
    build_version(out, env)
    build_html("tx_index.html", "INDEX_HTML", out, env)
    build_common(out, env)
    out.close

def build_rx_html(source, target, env):
    out = open("src/ESP8266_WebContent.h", 'w')
    build_version(out, env)
    build_html("rx_index.html", "INDEX_HTML", out, env)
    build_common(out, env)
    out.close
