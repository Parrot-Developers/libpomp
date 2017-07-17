#!/usr/bin/env python

#===============================================================================
# @file pompcli.py
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
import optparse
import socket
import threading
import time

import pomp

#===============================================================================
#===============================================================================
_USAGE = (
    "usage: %prog [<options>] <addr> [[<addrto>] <msgid> [<fmt> [<args>...]]]\n"
    "Send a pomp message on a socket or dump messages\n"
    "received on a socket\n"
    "\n"
    "  <options>: see below\n"
    "  <addr> : address\n"
    "  <msgid>: message id\n"
    "  <fmt>  : message format\n"
    "  <args> : message arguments\n"
    "\n"
    "<addr> format:\n"
    "  inet:<addr>:<port>\n"
    "  inet6:<addr>:<port>\n"
    "  unix:<path>\n"
    "  unix:@<name>\n"
)

#===============================================================================
#===============================================================================
class App(object):
    def __init__(self, options):
        self.sockFamily = None
        self.sockAddr = None
        self.server = options.server
        self.udp = options.udp
        self.dump = options.dump
        self.timeout = options.timeout
        self.hasmsg = False
        self.msgid = 0
        self.msgfmt = None
        self.msgargs = None
        self.ctx = None
        self.timer = None
        self.timerHandler = None

    def run(self):
        # Setup loop for main thread
        pomp.looper.prepareLoop()
        self.timerHandler = pomp.looper.Handler(self.onTimer)

        # Setup timeout if needed
        if self.timeout >= 0:
            self.setupTimeout()

        # Create pomp context
        self.ctx = pomp.Context(self)
        if self.udp:
            self.ctx.bind(self.sockFamily, self.sockAddr)
        elif self.server:
            self.ctx.listen(self.sockFamily, self.sockAddr)
        else:
            self.ctx.connect(self.sockFamily, self.sockAddr)

        # Run main loop
        try:
            pomp.looper.runLoop()
        except KeyboardInterrupt:
            pomp.looper.exitLoop()

        # Stop everything (wait a little to make sure message is sent)
        time.sleep(0.100)
        self.ctx.stop()

    def onConnected(self, ctx, conn):
        logging.info("CONNECTED")

        # Send message once connected
        if self.hasmsg:
            try:
                self.ctx.send(self.msgid, self.msgfmt, *self.msgargs)
            except pomp.EncodeException as ex:
                logging.error(ex.message)
            except Exception as ex:
                logging.exception(ex)
            self.hasmsg = False

        # Exit loop if not dumping message
        if not self.dump:
            pomp.looper.exitLoop()
        self.cancelTimeout()

    def onDisconnected(self, ctx, conn):
        logging.info("DISCONNECTED")

        # Setup timeout if needed
        if self.timeout >= 0:
            self.setupTimeout()

    def recvMessage(self, ctx, conn, msg):
        if self.dump:
            try:
                buf = msg.dump()
                logging.info("MSG: %s", buf)
            except pomp.DecodeException as ex:
                logging.error(ex.message)
            except Exception as ex:
                logging.exception(ex)

    def setupTimeout(self):
        assert self.timer is None
        self.timer = threading.Timer(self.timeout, self.timerHandler.post, [None])
        self.timer.start()

    def cancelTimeout(self):
        if self.timer is not None:
            self.timer.cancel()
            self.timer = None

    def onTimer(self, req):
        logging.info("Timeout !")
        pomp.looper.exitLoop()

#===============================================================================
#===============================================================================
def main():
    (options, args) = parseArgs()
    setupLog(options)

    app = App(options)

    # Get address
    if len(args) > 0:
        try:
            (app.sockFamily, app.sockAddr) = pomp.parseAddr(args[0])
        except (ValueError, socket.error):
            sys.stderr.write("Failed to parse address: %s\n" % args[0])
            sys.exit(1)
        args = args[1:]
    else:
        sys.stderr.write("Missing address\n")
        sys.exit(1)

    # Get message id (optional if dumping)
    if len(args) > 0:
        try:
            app.msgid = int(args[0])
            app.hasmsg = True
        except ValueError:
            sys.stderr.write("Failed to parse message id: %s\n" % args[0])
            sys.exit(1)
        args = args[1:]
    elif not options.dump:
        sys.stderr.write("Missing message id\n")
        sys.exit(1)

    # Get message format (optional)
    if len(args) > 0:
        app.msgfmt = args[0]
        args = args[1:]

    # Get message arguments (optional)
    if len(args) > 0:
        app.msgargs = args
        args = []

    # Go !
    app.run()
    sys.exit(0)

#===============================================================================
#===============================================================================
def parseArgs():
    # Setup parser
    parser = optparse.OptionParser(usage=_USAGE)

    parser.add_option("-s", "--server",
        dest="server",
        action="store_true",
        default=False,
        help="use a server socket")

    parser.add_option("-c", "--client",
        dest="server",
        action="store_false",
        default=False,
        help="use a client socket (default)")

    parser.add_option("-u", "--udp",
        dest="udp",
        action="store_true",
        default=False,
        help="use a udp socket")

    parser.add_option("-d", "--dump",
        dest="dump",
        action="store_true",
        default=False,
        help="stay connected and dump messages")

    parser.add_option("-t", "--timeout",
        dest="timeout",
        action="store",
        type="int",
        default=-1,
        help="timeout to wait connection in seconds (default no timeout)")

    parser.add_option("-q", "--quiet",
        dest="quiet",
        action="store_true",
        default=False,
        help="be quiet")

    parser.add_option("-v", "--verbose",
        dest="verbose",
        action="count",
        default=0,
        help="verbose output (more verbose if specified twice)")

    # Parse arguments
    (options, args) = parser.parse_args()
    return (options, args)

#===============================================================================
#===============================================================================
def setupLog(options):
    logging.basicConfig(
        level=logging.DEBUG,
        format="[%(levelname)s][%(asctime)s][%(name)s] %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
        stream=sys.stderr)
    logging.addLevelName(logging.CRITICAL, "C")
    logging.addLevelName(logging.ERROR, "E")
    logging.addLevelName(logging.WARNING, "W")
    logging.addLevelName(logging.INFO, "I")
    logging.addLevelName(logging.DEBUG, "D")

    # Setup log level
    if options.quiet == True:
        logging.getLogger().setLevel(logging.CRITICAL)
    elif options.verbose >= 2:
        logging.getLogger().setLevel(logging.DEBUG)
    elif options.verbose >= 1:
        logging.getLogger().setLevel(logging.INFO)

#===============================================================================
#===============================================================================
if __name__ == "__main__":
    main()
