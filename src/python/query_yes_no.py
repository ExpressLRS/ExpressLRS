import sys
from inputimeout import inputimeout, TimeoutOccurred

def query_yes_no(question='') -> bool: #https://code.activestate.com/recipes/577058/
    """Ask a yes/no question via raw_input() and return their answer.
    "question" is a string that is presented to the user.
    The "answer" return value is True for "yes" or False for "no".
    """
    # Always return false if not in an interactive shell
    if not sys.stdin.isatty():
        return False

    valid = {"yes": True, "y": True, "ye": True, "no": False, "n": False}

    while True:
        choice = ''
        try:
            choice = inputimeout(prompt=question, timeout=5)
        except TimeoutOccurred:
            sys.stdout.write("Please respond with 'yes' or 'no' (or 'y' or 'n')")
            sys.stdout.flush()

        if choice in valid:
            return valid[choice]