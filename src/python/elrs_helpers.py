import os
import re

def get_git(env):
    """
    Returns a git.Repo class for a git repo in the current directory
    installing GitPython if neeeded
    """
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
                return None

    try:
        # Check one directory up for a git repo
        orig_dir = os.path.abspath(os.path.join(os.getcwd(), os.pardir))
        git_repo = git.Repo(orig_dir, search_parent_directories=False)
        # If that succeeded then find out where the top level is and open that
        git_root = git_repo.git.rev_parse("--show-toplevel")
        if orig_dir != git_root:
            git_repo = git.Repo(git_root, search_parent_directories=False)
        return git_repo
    except git.InvalidGitRepositoryError:
        pass

    return None

def get_git_version(env):
    """
    Return a dict with keys
    version: The version tag if HEAD is a version, or branch otherwise
    sha: the 6 character short sha for the current HEAD revison, falling back to
        VERSION file if not in a git repo
    """
    ver = "ver.unknown"
    sha = "000000"

    git_repo = get_git(env)
    if git_repo:
        import git
        sha = git_repo.head.object.hexsha
        try:
            ver = re.sub(r".*/", "", git_repo.git.describe("--all", "--exact-match"))
        except git.exc.GitCommandError:
            try:
                ver = git_repo.git.symbolic_ref("-q", "--short", "HEAD")
            except git.exc.GitCommandError:
                pass
    elif os.path.exists("VERSION"):
        with open("VERSION") as _f:
            data = _f.readline()
            _f.close()
        sha = data.split()[1].strip()

    return dict(version=ver, sha=sha[:6])
