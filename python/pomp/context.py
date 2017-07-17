#===============================================================================
# @file contex.py
#
# @brief Handle client/server context, socket listen/connect and events.
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

import logging
import threading
import socket
import errno

import pomp.looper as looper
from pomp.connection import Connection
from pomp.message import Message

_log = logging.getLogger("pomp")

_CONN_ADD = 0
_CONN_REMOVE = 1
_CONN_MSG = 2

_SERVER_RECONNECT_DELAY = 2.0
_CLIENT_RECONNECT_DELAY = 2.0
_DGRAM_RECONNECT_DELAY = 2.0

#===============================================================================
#===============================================================================
class EventHandler(object):
    def onConnected(self, ctx, conn):
        raise NotImplementedError()
    def onDisconnected(self, ctx, conn):
        raise NotImplementedError()
    def recvMessage(self, ctx, conn, msg):
        raise NotImplementedError()

#===============================================================================
#===============================================================================
class Context(object):
    def __init__(self, eventHandler):
        self.eventHandler = eventHandler
        self.connections = []
        self.semaphore = None
        self.sock = None
        self.sockFamily = None
        self.sockAddr = None
        self.sockThread = None
        self.mainHandler = None
        self.running = False
        self.isDgram = False
        self.cond = threading.Condition()

    def listen(self, family, addr):
        assert self.sockAddr is None
        self.sockFamily = family
        self.sockAddr = addr
        self.isDgram = False
        self._start(16, self._runServer)

    def connect(self, family, addr):
        assert self.sockAddr is None
        self.sockFamily = family
        self.sockAddr = addr
        self.isDgram = False
        self._start(1, self._runClient)

    def bind(self, family, addr):
        assert self.sockAddr is None
        self.sockFamily = family
        self.sockAddr = addr
        self.isDgram = True
        self._start(1, self._runDgram)

    def getConnections(self):
        return self.connections

    def getConnection(self):
        if len(self.connections) > 0:
            return self.connections[0]
        else:
            return None

    def stop(self):
        # Make sure nobody will react to out actions
        self.running = False

        # Close all connections
        while len(self.connections) > 0:
            self._doConnRemove(self.connections[0])

        # Ask socket thread to stop
        try:
            self.cond.acquire()
            self.cond.notifyAll()
            if self.sock is not None:
                try:
                    self.sock.shutdown(socket.SHUT_RDWR)
                except (OSError, IOError):
                    pass
        finally:
            self.cond.release()

        # Wait socket thread
        self.sockThread.join()

        # Cleanup
        self.semaphore = None
        self.sock = None
        self.sockAddr = None
        self.sockThread = None
        self.mainHandler = None

    def sendMsg(self, msg):
        for conn in self.connections:
            conn.sendMsg(msg)

    def send(self, msgid, fmt, *args):
        msg = Message()
        msg.write(msgid, fmt, *args)
        self.sendMsg(msg)

    def _start(self, maxConnectionCount, run):
        self.semaphore = threading.Semaphore(maxConnectionCount)

        # Handler to process request in main thread context
        self.mainHandler = looper.Handler(self._onRequest)

        # Thread for Server/Client socket
        self.sockThread = threading.Thread(target=run)

        # Start everything
        self.running = True
        self.sockThread.start()

    def _onRequest(self, req):
        (what, conn, rxMsg) = req
        if not self.running:
            _log.warning("Handler request lost")
            if what == _CONN_ADD:
                conn.close()
        elif what == _CONN_ADD:
            self._doConnAdd(conn)
        elif what == _CONN_REMOVE:
            self._doConnRemove(conn)
        elif what == _CONN_MSG:
            self.eventHandler.recvMessage(self, conn, rxMsg)
        else:
            assert False

    def removeConn(self, conn):
        self.mainHandler.post((_CONN_REMOVE, conn, None))

    def notifyMessage(self, conn, msg):
        self.mainHandler.post((_CONN_MSG, conn, msg))

    def _doConnAdd(self, conn):
        self.connections.append(conn)
        conn.start()
        if not self.isDgram:
            self.eventHandler.onConnected(self, conn)

    def _doConnRemove(self, conn):
        # Could be called again during close or disconnection
        if conn in self.connections:
            if not self.isDgram:
                self.eventHandler.onDisconnected(self, conn)
            self.connections.remove(conn)
            conn.close()
            self.semaphore.release()

    def _runServer(self):
        while self.running:
            # Open a socket
            if not self._setupSock():
                break

            # Bind and listen to address
            listening = False
            try:
                self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                self.sock.bind(self.sockAddr)
                self.sock.listen(1)
                listening = True
            except (OSError, IOError) as ex:
                # Some errors are expected
                if self.running and ex.errno != errno.EADDRNOTAVAIL:
                    _log.warning("bind: err=%d(%s)", ex.errno, ex.strerror)

            if listening:
                # Acquire initial token before entering loop
                self.semaphore.acquire()
                while self.running:
                    # Accept a new connection
                    sockPeer = None
                    try:
                        sockPeer = self.sock.accept()
                    except socket.timeout:
                        pass
                    except (OSError, IOError) as ex:
                        if self.running:
                            _log.warning("accept: err=%d(%s)", ex.errno, ex.strerror)

                    # Create connection object
                    if sockPeer is not None:
                        conn = Connection(self, sockPeer[0])
                        self.mainHandler.post((_CONN_ADD, conn, None))
                    else:
                        # Release our token
                        self.semaphore.release()

                    # Acquire a token before continuing loop
                    self.semaphore.acquire()

            # Cleanup socket
            self._cleanupSock()

            # Retry again later
            self._waitForRetry(_SERVER_RECONNECT_DELAY)

    def _runClient(self):
        # Acquire initial token before entering loop
        self.semaphore.acquire()
        while self.running:
            # Open a socket
            if not self._setupSock():
                break

            # Connect to address
            connected = False
            try:
                self.sock.connect(self.sockAddr)
                connected = True
            except socket.timeout:
                pass
            except (OSError, IOError) as ex:
                # Some errors are expected
                if (self.running
                        and ex.errno != errno.ECONNREFUSED
                        and ex.errno != errno.EHOSTUNREACH
                        and ex.errno != errno.EHOSTDOWN
                        and ex.errno != errno.ENETUNREACH
                        and ex.errno != errno.ENETDOWN
                        and ex.errno != errno.ENOENT
                        and ex.errno != errno.ETIMEDOUT):
                    if ex.errno is None:
                        _log.warning("connect: %s", str(ex))
                    else:
                        _log.warning("connect: err=%d(%s)", ex.errno, ex.strerror)

            # Create connection object, give ownership of socket
            if connected:
                try:
                    self.cond.acquire()
                    conn = Connection(self, self.sock)
                    self.sock = None
                    self.mainHandler.post((_CONN_ADD, conn, None))
                finally:
                    self.cond.release()
            else:
                # Release our token
                self.semaphore.release()

            # Cleanup socket
            self._cleanupSock()

            # Wait for current connection if any to complete
            self.semaphore.acquire()

            # Retry again later
            self._waitForRetry(_CLIENT_RECONNECT_DELAY)

    def _runDgram(self):
        while self.running:
            # Open a socket
            if not self._setupSock(socktype=socket.SOCK_DGRAM):
                break

            # Bind to address
            bound = False
            try:
                self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                self.sock.bind(self.sockAddr)
                bound = True
            except (OSError, IOError) as ex:
                # Some errors are expected
                if self.running and ex.errno != errno.EADDRNOTAVAIL:
                    _log.warning("bind: err=%d(%s)", ex.errno, ex.strerror)

            # Create connection object, give ownership of socket
            if bound:
                try:
                    self.cond.acquire()
                    conn = Connection(self, self.sock, isDgram=True)
                    self.sock = None
                    self.mainHandler.post((_CONN_ADD, conn, None))
                finally:
                    self.cond.release()
            else:
                # Release our token
                self.semaphore.release()

            # Cleanup socket
            self._cleanupSock()

            # Wait for current connection if any to complete
            self.semaphore.acquire()

            # Retry again later
            self._waitForRetry(_DGRAM_RECONNECT_DELAY)

    def _setupSock(self, socktype=socket.SOCK_STREAM):
        try:
            self.cond.acquire()
            if not self.running:
                return False
            self.sock = socket.socket(self.sockFamily, socktype)
        finally:
            self.cond.release()
        return True

    def _cleanupSock(self):
        try:
            self.cond.acquire()
            if self.sock is not None:
                self.sock.close()
        finally:
            self.sock = None
            self.cond.release()

    def _waitForRetry(self, delay):
        try:
            self.cond.acquire()
            if self.running:
                self.cond.wait(delay)
        finally:
            self.cond.release()
