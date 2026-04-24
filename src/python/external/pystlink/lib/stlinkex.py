class StlinkException(Exception):
    def __init__(self, msg):
        self._msg = msg

    def __str__(self):
        return self._msg


class StlinkExceptionBadParam(StlinkException):
    def __init__(self, info=None, cmd=None):
        self._info = info
        self._cmd = cmd

    def set_cmd(self, cmd):
        self._cmd = cmd
        return self

    def __str__(self):
        msg = 'Bad param: "%s"' % self._cmd
        if self._info:
            msg += ': %s' % self._info
        return msg


class StlinkExceptionCpuNotSelected(StlinkException):
    def __init__(self):
        self._msg = 'CPU is not selected'
