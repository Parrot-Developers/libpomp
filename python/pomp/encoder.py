#===============================================================================
# @file encoder.py
#
# @brief Handle message payload encoding.
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

import struct
import sys

import pomp.protocol as protocol


def nextIter(it):
    if(sys.version_info.major == 3):
        return it.__next__()
    else:
        return it.next()


#===============================================================================
#===============================================================================
class EncodeException(Exception):
    pass

#===============================================================================
#===============================================================================
class Encoder(object):
    _FLAG_L = 0x01
    _FLAG_LL = 0x02
    _FLAG_H = 0x04
    _FLAG_HH = 0x08

    def __init__(self):
        self.msg = None     # Associated message

    def init(self, msg):
        self.msg = msg
        self.msg.buf.skip(protocol.HEADER_SIZE)

    def clear(self):
        self.msg = None

    def write(self, fmt, *args):
        if fmt is None:
            return
        flags = 0
        waitpercent = True
        argsiter = iter(args)
        for c in fmt:
            if waitpercent:
                # Only formatting spec expected here
                if c != '%':
                    raise EncodeException("encoder : invalid format char (%c)" % c)
                waitpercent = False
                flags = 0
            elif c == 'l':
                if flags == 0:
                    flags |= Encoder._FLAG_L
                elif flags & Encoder._FLAG_L:
                    flags &= ~Encoder._FLAG_L
                    flags |= Encoder._FLAG_LL
            elif c == 'h':
                if flags == 0:
                    flags |= Encoder._FLAG_H
                elif flags & Encoder._FLAG_H:
                    flags &= ~Encoder._FLAG_H
                    flags |= Encoder._FLAG_HH
            elif c == 'i' or c == 'd':
                # Signed integer
                val = nextIter(argsiter)
                try:
                    val = int(val)
                except ValueError:
                    raise EncodeException("encoder : failed to encode as int : '%s'" % str(val))
                if flags & Encoder._FLAG_LL:
                    self.writeI64(val)
                elif flags & Encoder._FLAG_L:
                    self.writeI32(val)
                elif flags & Encoder._FLAG_HH:
                    self.writeI8(val)
                elif flags & Encoder._FLAG_H:
                    self.writeI16(val)
                else:
                    self.writeI32(val)
                waitpercent = True
            elif c == 'u':
                # Unsigned integer
                val = nextIter(argsiter)
                try:
                    val = int(val)
                except ValueError:
                    raise EncodeException("encoder : failed to encode as uint : '%s'" % str(val))
                if flags & Encoder._FLAG_LL:
                    self.writeU64(val)
                elif flags & Encoder._FLAG_L:
                    self.writeU32(val)
                elif flags & Encoder._FLAG_HH:
                    self.writeU8(val)
                elif flags & Encoder._FLAG_H:
                    self.writeU16(val)
                else:
                    self.writeU32(val)
                waitpercent = True
            elif c == 's':
                val = nextIter(argsiter)
                self.writeStr(val)
                waitpercent = True
            elif c == 'p':
                val = nextIter(argsiter)
                self.writeBuf(val)
                waitpercent = True
            elif c == 'f' or c == 'F' or c == 'e' or c == 'E' or c == 'g' or c == 'G':
                # Floating point
                val = nextIter(argsiter)
                try:
                    val = float(val)
                except ValueError:
                    raise EncodeException("encoder : failed to encode as float : '%s'" % str(val))
                if flags & (Encoder._FLAG_LL | Encoder._FLAG_H | Encoder._FLAG_HH):
                    raise EncodeException("encoder : unsupported format width")
                elif flags & Encoder._FLAG_L:
                    self.writeF64(val)
                else:
                    self.writeF32(val)
                waitpercent = True
            else:
                raise EncodeException("encoder : invalid format specifier (%c)" % c)

    def writeI8(self, val):
        self._writeType(protocol.DATA_TYPE_I8)
        self._writeByte(val & 0xff)

    def writeU8(self, val):
        self._writeType(protocol.DATA_TYPE_U8)
        self._writeByte(val & 0xff)

    def writeI16(self, val):
        self._writeType(protocol.DATA_TYPE_I16)
        self._writeByte(val & 0xff)
        self._writeByte((val >> 8) & 0xff)

    def writeU16(self, val):
        self._writeType(protocol.DATA_TYPE_U16)
        self._writeByte(val & 0xff)
        self._writeByte((val >> 8) & 0xff)

    def writeI32(self, val):
        self._writeType(protocol.DATA_TYPE_I32)
        # Zigzag encoding, use arithmetic right shift, with sign propagation
        zval = ((val << 1) ^ (val >> 31)) & 0xffffffff
        self._writeVarint(zval)

    def writeU32(self, val):
        self._writeType(protocol.DATA_TYPE_U32)
        zval = val & 0xffffffff
        self._writeVarint(zval)

    def writeI64(self, val):
        self._writeType(protocol.DATA_TYPE_I64)
        # Zigzag encoding, use arithmetic right shift, with sign propagation
        zval = ((val << 1) ^ (val >> 63)) & 0xffffffffffffffff
        self._writeVarint(zval)

    def writeU64(self, val):
        self._writeType(protocol.DATA_TYPE_U64)
        zval = val & 0xffffffffffffffff
        self._writeVarint(zval)

    def writeStr(self, sval):
        # Check length
        if len(sval) + 1 > 0xffff:
            raise EncodeException("Invalid string length: " + len(sval))
        # Write type, size, data and final null
        self._writeType(protocol.DATA_TYPE_STR)
        self._writeSizeU16(len(sval) + 1)
        self._write(sval.encode("ascii"))
        self._writeByte(0)

    def writeBuf(self, buf):
        # Write type, size, and data
        self._writeType(protocol.DATA_TYPE_BUF)
        self._writeSizeU32(len(buf))
        self._write(buf)

    def writeF32(self, val):
        self._writeType(protocol.DATA_TYPE_F32)
        buf = struct.pack("<f", val)
        self._write(buf)

    def writeF64(self, val):
        self._writeType(protocol.DATA_TYPE_F64)
        buf = struct.pack("<d", val)
        self._write(buf)

    def _write(self, buf):
        self.msg.buf.write(buf)

    def _writeByte(self, bval):
        self.msg.buf.writeByte(bval)

    def _writeType(self, datatype):
        self._writeByte(datatype & 0xff)

    def _writeVarint(self, val):
        # Process value, use logical right shift, without sign propagation
        more = True
        while more:
            bval = val & 0x7f
            val >>= 7
            more = (val != 0)
            if more:
                bval |= 0x80
            self._writeByte(bval)

    def _writeSizeU16(self, size):
        self._writeVarint(size)

    def _writeSizeU32(self, size):
        self._writeVarint(size)
