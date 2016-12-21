#===============================================================================
# @file message.py
#
# @brief Handle formating/decoding of messages
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

import pomp.protocol as protocol
from pomp.buffer import Buffer
from pomp.encoder import Encoder
from pomp.decoder import Decoder

#===============================================================================
#===============================================================================
class Message(object):
    def __init__(self):
        self.msgid = 0          # Id of message
        self.finished = False   # Header is filled
        self.buf = None         # Buffer with data

    def init(self, msgid):
        self.msgid = msgid
        self.finished = False
        self.buf = Buffer()

    def finish(self):
        # Make sure position is at least after header
        if self.buf.getPos() < protocol.HEADER_SIZE:
            self.buf.skip(protocol.HEADER_SIZE - self.buf.getPos())

        # Write header
        self.buf.rewind()
        self.buf.writeInt(protocol.POMP_MAGIC)
        self.buf.writeInt(self.msgid)
        self.buf.writeInt(len(self.buf))

        # Make message read-only
        self.finished = True
        self.buf.setReadOnly(True)

    def setFinished(self):
        self.finished = True
        self.buf.setReadOnly(True)

    def clear(self):
        self.msgid = 0
        self.finished = False
        self.buf = None

    def write(self, msgid, fmt, *args):
        self.init(msgid)
        enc = Encoder()
        enc.init(self)
        enc.write(fmt, *args)
        enc.clear()
        self.finish()

    def read(self, fmt):
        dec = Decoder()
        dec.init(self)
        res = dec.read(fmt)
        dec.clear()
        return res

    def dump(self):
        dec = Decoder()
        dec.init(self)
        res = dec.dump()
        dec.clear()
        return res
