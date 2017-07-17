#===============================================================================
# @file lopper.py
#
# @brief Handle event loop in a thread.
#
# @author yves-marie.morgan@parrot.com
#
# Copyright (c) 2014 Parrot S.A.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#   * Neither the name of the Parrot Company nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT COMPANY BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#===============================================================================

import sys
import threading
if(sys.version_info.major == 3):
    import queue
else:
    import Queue as queue

#===============================================================================
#===============================================================================

_tls = threading.local()

#===============================================================================
#===============================================================================
class _Loop(object):
    def __init__(self):
        self._queue = queue.Queue()
        self._running = False

    def post(self, handler, req):
        self._queue.put((handler, req))

    def run(self):
        self._running = True
        while self._running:
            # Use timeout so we can interrupt wait
            self.step(timeout=0.1)

    def step(self, timeout=0):
        try:
            (handler, req) = self._queue.get(timeout=timeout)
            handler.cb(req)
            return True
        except queue.Empty:
            return False


    def exit(self):
        self._running = False

#===============================================================================
#===============================================================================
class Handler(object):
    def __init__(self, cb):
        self._loop = _tls.loop
        self.cb = cb

    def post(self, req):
        self._loop.post(self, req)

#===============================================================================
#===============================================================================
def prepareLoop(loop=None):
    # Make sure that current thread does not already have a loop object
    if hasattr(_tls, "loop") and _tls.loop is not None:
        raise Exception("Current thread already have a loop object")
    # Create a new loop object
    if loop is None:
        _tls.loop = _Loop()
    else:
        _tls.loop = loop

#===============================================================================
#===============================================================================
def runLoop():
    _tls.loop.run()
    _tls.loop = None

#===============================================================================
#===============================================================================
def stepLoop(maxMsg=32):
    count = 0
    while _tls.loop.step() and count < maxMsg:
        count += 1

#===============================================================================
#===============================================================================
def exitLoop():
    _tls.loop.exit()
