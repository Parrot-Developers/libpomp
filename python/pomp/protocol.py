#===============================================================================
# @file protocol.py
#
# @brief Handle message protocol parsing.
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
import struct

from pomp.buffer import Buffer

_log = logging.getLogger("pomp")

# Magic bytes
POMP_MAGIC_0 = ord('P')  # Magic byte 0
POMP_MAGIC_1 = ord('O')  # Magic byte 1
POMP_MAGIC_2 = ord('M')  # Magic byte 2
POMP_MAGIC_3 = ord('P')  # Magic byte 3

# Magic value
POMP_MAGIC = (
    (POMP_MAGIC_3 << 24) |
    (POMP_MAGIC_2 << 16) |
    (POMP_MAGIC_1 << 8) |
    POMP_MAGIC_0)

# Data types
DATA_TYPE_I8 = 0x01    # 8-bit signed integer
DATA_TYPE_U8 = 0x02    # 8-bit unsigned integer
DATA_TYPE_I16 = 0x03   # 16-bit signed integer
DATA_TYPE_U16 = 0x04   # 16-bit unsigned integer
DATA_TYPE_I32 = 0x05   # 32-bit signed integer
DATA_TYPE_U32 = 0x06   # 32-bit unsigned integer
DATA_TYPE_I64 = 0x07   # 64-bit signed integer
DATA_TYPE_U64 = 0x08   # 64-bit unsigned integer
DATA_TYPE_STR = 0x09   # String
DATA_TYPE_BUF = 0x0a   # Buffer
DATA_TYPE_F32 = 0x0b   # 32-bit floating point
DATA_TYPE_F64 = 0x0c   # 64-bit floating point

# Size of protocol header */
HEADER_SIZE = 12

#===============================================================================
#===============================================================================
class Header(object):
    def __init__(self, magic, msgid, size):
        self.magic = magic
        self.msgid = msgid
        self.size = size

#===============================================================================
#===============================================================================
class Protocol(object):
    _STATE_IDLE = 0
    _STATE_HEADER_MAGIC_0 = 1
    _STATE_HEADER_MAGIC_1 = 2
    _STATE_HEADER_MAGIC_2 = 3
    _STATE_HEADER_MAGIC_3 = 4
    _STATE_HEADER = 5
    _STATE_PAYLOAD = 6

    def __init__(self):
        self.state = Protocol._STATE_IDLE
        self.headerBuf = None
        self.header = None
        self.msg = None
        self.bufSrc = None
        self.offSrc = 0
        self.lenSrc = 0
        self._reset()

    def decode(self, buf, off):
        rxMsg = None
        # If idle start a new parsing
        if self.state == Protocol._STATE_IDLE:
            self.state = Protocol._STATE_HEADER_MAGIC_0

        # Setup source buffer
        self.bufSrc = buf
        self.offSrc = off
        self.lenSrc = len(buf) - off
        while self.lenSrc > 0 and self.state != Protocol._STATE_IDLE:
            if self.state == Protocol._STATE_HEADER_MAGIC_0:
                self._reset()
                self.state = Protocol._STATE_HEADER_MAGIC_0
                self._copyOne(self.headerBuf)
                self._checkMagic(0, POMP_MAGIC_0, Protocol._STATE_HEADER_MAGIC_1)
            elif self.state == Protocol._STATE_HEADER_MAGIC_1:
                self._copyOne(self.headerBuf)
                self._checkMagic(1, POMP_MAGIC_1, Protocol._STATE_HEADER_MAGIC_2)
            elif self.state == Protocol._STATE_HEADER_MAGIC_2:
                self._copyOne(self.headerBuf)
                self._checkMagic(2, POMP_MAGIC_2, Protocol._STATE_HEADER_MAGIC_3)
            elif self.state == Protocol._STATE_HEADER_MAGIC_3:
                self._copyOne(self.headerBuf)
                self._checkMagic(3, POMP_MAGIC_3, Protocol._STATE_HEADER)
            elif self.state == Protocol._STATE_HEADER:
                self._copy(self.headerBuf, HEADER_SIZE)
                if len(self.headerBuf) == HEADER_SIZE:
                    self._decodeHeader()
            elif self.state == Protocol._STATE_PAYLOAD:
                self._copy(self.msg.buf, self.header.size)
            else:
                assert False

            # Check end of payload
            if (self.state == Protocol._STATE_PAYLOAD
                    and len(self.msg.buf) == self.header.size):
                # Give ownership of message to caller
                self.msg.setFinished()
                rxMsg = self.msg
                self.msg = None
                self.state = Protocol._STATE_IDLE

        return (self.offSrc, rxMsg)

    def _reset(self):
        self.state = Protocol._STATE_IDLE
        self.headerBuf = Buffer()
        self.header = None
        self.msg = None

    def _checkMagic(self, idx, val, state):
        if isinstance(self.headerBuf.getData(), str):
            magic = ord(self.headerBuf.getData()[idx])
        else:
            magic = self.headerBuf.getData()[idx]
        if magic != val:
            _log.warning("Bad header magic %d: 0x%02x(0x%02x)", idx, magic, val)
            self.state = Protocol._STATE_HEADER_MAGIC_0
        else:
            self.state = state

    def _copyOne(self, bufDst):
        bufDst.write(self.bufSrc[self.offSrc:self.offSrc+1])
        self.offSrc += 1
        self.lenSrc -= 1

    def _copy(self, bufDst, sizeDst):
        cpyLen = min(self.lenSrc, sizeDst - len(bufDst))
        bufDst.write(self.bufSrc[self.offSrc:self.offSrc+cpyLen])
        self.offSrc += cpyLen
        self.lenSrc -= cpyLen

    def _decodeHeader(self):
        try:
            # Decode header fields
            self.headerBuf.rewind()
            magic = self.headerBuf.readInt()
            msgid = self.headerBuf.readInt()
            size = self.headerBuf.readInt()
            self.header = Header(magic, msgid, size)

            # Check header and setup payload decoding */
            if self.header.size < HEADER_SIZE:
                _log.warning("Bad header size: %d", self.header.size)
                self.state = Protocol._STATE_HEADER_MAGIC_0
            else:
                self._allocMsg(self.header.msgid, self.header.size)
                self.msg.buf.write(self.headerBuf.getData())
                self.state = Protocol._STATE_PAYLOAD
        except struct.error as ex:
            _log.error(ex)
            self.state = Protocol._STATE_HEADER_MAGIC_0

    def _allocMsg(self, msgid, size):
        from pomp.message import Message
        self.msg = Message()
        self.msg.init(msgid)
