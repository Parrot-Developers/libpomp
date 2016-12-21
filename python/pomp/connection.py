#===============================================================================
# @file connection.py
#
# @brief Handle read/write IO of a socket connection.
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

import sys, logging
import threading
import socket

import pomp.looper as looper
from pomp.protocol import Protocol
from pomp.message import Message

_log = logging.getLogger("pomp")

#===============================================================================
#===============================================================================
def activateSocketKeepalive(sock, keepidle=5, keepintvl=1, keepcnt=2):
    if sys.platform != "win32":
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, keepidle)
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, keepintvl)
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, keepcnt)
        except (OSError, IOError) as ex:
            _log.warning("setsockopt: err=%d(%s)", ex.errno, ex.strerror)

#===============================================================================
#===============================================================================
class Connection(object):
    _WRITE_STOP = 1
    _WRITE_MSG = 2
    _WRITE_MSG_TO = 3

    def __init__(self, ctx, sock, isDgram=False):
        self.ctx = ctx
        self.sock = sock
        self.isDgram = isDgram
        self.running = False
        self.readThread = None
        self.writeThread = None
        self.writeHandler = None
        self.cond = threading.Condition()
        self.localAddr = (self.sock.family, self.sock.getsockname())
        if not isDgram:
            self.peerAddr = (self.sock.family, self.sock.getpeername())
        else:
            self.peerAddr = None

    def getLocalAddr(self):
        return self.localAddr

    def getPeerAddr(self):
        return self.peerAddr

#    def getPeerCred(self):
#        return self.sock.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED)

    def sendMsg(self, msg):
        if self.isDgram:
            assert self.peerAddr is not None
            self.writeHandler.post((Connection._WRITE_MSG_TO, msg, self.peerAddr))
        else:
            self.writeHandler.post((Connection._WRITE_MSG, msg))

    def sendMsgTo(self, msg, addr):
        assert self.isDgram
        self.writeHandler.post((Connection._WRITE_MSG_TO, msg, addr))

    def send(self, msgid, fmt, *args):
        msg = Message()
        msg.write(msgid, fmt, *args)
        self.sendMsg(msg)

    def disconnect(self):
        self.ctx.removeConn(self)

    def start(self):
        # Thread for Rx and Tx
        self.readThread = threading.Thread(target=self._reader)
        self.writeThread = threading.Thread(target=self._writer)

        # Put socket in blocking mode and activate keep alive
        self.sock.setblocking(True)
        if self.sock.family == socket.AF_INET or self.sock.family == socket.AF_INET6:
            if not self.isDgram:
                activateSocketKeepalive(self.sock)

        # Start everything !
        self.running = True
        self.readThread.start()
        self.writeThread.start()

        # Make sure the write handler has been created by writer thread
        try:
            self.cond.acquire()
            while self.writeHandler is None:
                self.cond.wait()
        finally:
            self.cond.release()

    def close(self):
        # Ask everyone to stop
        try:
            self.cond.acquire()
            self.running = False
            self.writeHandler.post((Connection._WRITE_STOP, None))
            self.sock.shutdown(socket.SHUT_RDWR)
        except (OSError, IOError):
            pass
        finally:
            self.cond.release()

        # Wait everyone
        self.readThread.join()
        self.writeThread.join()

        # Cleanup
        self.sock.close()
        self.sock = None
        self.readThread = None
        self.writeThread = None
        self.writeHandler = None

    def _reader(self):
        # Read loop
        try:
            prot = Protocol()
            while self.running:
                # Read and decode data into messages and notify them
                if self.isDgram:
                    (buf, self.peerAddr) = self.sock.recvfrom(1024)
                else:
                    buf = self.sock.recv(1024)
                if not buf and not self.isDgram:
                    break
                off = 0
                while off < len(buf):
                    (off, rxMsg) = prot.decode(buf, off)
                    if rxMsg is not None:
                        self.ctx.notifyMessage(self, rxMsg)
                if self.isDgram:
                    self.peerAddr
        except (OSError, IOError) as ex:
            _log.warning("recv: err=%d(%s)", ex.errno, ex.strerror)

        # Socket disconnected, remove connection
        self.ctx.removeConn(self)

    def _writer(self):
        # Prepare a message loop
        looper.prepareLoop()

        # Setup the write handler and wakeup start
        try:
            self.cond.acquire()
            self.writeHandler = looper.Handler(self._onWriteRequest)
            self.cond.notifyAll()
        finally:
            self.cond.release()

        # Go !
        looper.runLoop()

    def _onWriteRequest(self, req):
        (what, txMsg) = req
        if what == Connection._WRITE_MSG:
            try:
                self.sock.sendall(txMsg.buf.getData())
            except (OSError, IOError) as ex:
                _log.warning("send: err=%d(%s)", ex.errno, ex.strerror)
        elif what == Connection._WRITE_MSG_TO:
            (what, txMsg, addr) = req
            try:
                self.sock.sendto(txMsg.buf.getData(), addr)
            except (OSError, IOError) as ex:
                _log.warning("sendto: err=%d(%s)", ex.errno, ex.strerror)
        elif what == Connection._WRITE_STOP:
            looper.exitLoop()
        else:
            _log.warning("Unknown message: %d", what)
