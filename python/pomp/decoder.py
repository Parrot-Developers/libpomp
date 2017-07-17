#===============================================================================
# @file decoder.py
#
# @brief Handle message payload decoding.
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
if(sys.version_info.major == 3):
    from io import StringIO
else:
    from cStringIO import StringIO


import pomp.protocol as protocol

#===============================================================================
#===============================================================================
class DecodeException(Exception):
    pass

#===============================================================================
#===============================================================================
class Decoder(object):
    _FLAG_L = 0x01
    _FLAG_LL = 0x02
    _FLAG_H = 0x04
    _FLAG_HH = 0x08

    def __init__(self):
        self.msg = None     # Associated message

    def init(self, msg):
        self.msg = msg
        self.msg.buf.setPos(protocol.HEADER_SIZE)

    def clear(self):
        self.msg = None

    def dump(self):
        buf = StringIO()
        # Message id
        buf.write("{ID:%u" % self.msg.msgid)
        # Process message arguments
        while self.msg.buf.getPos() < len(self.msg.buf):
            # Read type, Rewind for further decoding
            datatype = self._readByte()
            self.msg.buf.setPos(self.msg.buf.getPos() - 1)
            if datatype == protocol.DATA_TYPE_I8:
                buf.write(", I8:%d" % self.readI8())
            elif datatype == protocol.DATA_TYPE_U8:
                buf.write(", U8:%u" % self.readU8())
            elif datatype == protocol.DATA_TYPE_I16:
                buf.write(", I16:%d" % self.readI16())
            elif datatype == protocol.DATA_TYPE_U16:
                buf.write(", U16:%u" % self.readU16())
            elif datatype == protocol.DATA_TYPE_I32:
                buf.write(", I32:%d" % self.readI32())
            elif datatype == protocol.DATA_TYPE_U32:
                buf.write(", U32:%u" % self.readU32())
            elif datatype == protocol.DATA_TYPE_I64:
                buf.write(", I64:%d" % self.readI64())
            elif datatype == protocol.DATA_TYPE_U64:
                buf.write(", U64:%u" % self.readU64())
            elif datatype == protocol.DATA_TYPE_STR:
                buf.write(", STR:'%s'" % self.readStr())
            elif datatype == protocol.DATA_TYPE_BUF:
                buf.write(", BUF:'%s'" % repr(self.readBuf()))
            elif datatype == protocol.DATA_TYPE_F32:
                buf.write(", F32:%s" % repr(self.readF32()))
            elif datatype == protocol.DATA_TYPE_F64:
                buf.write(", F64:%s" % repr(self.readF64()))
            else:
                raise DecodeException("decoder : unknown type: %d" % datatype)
        buf.write("}")
        return buf.getvalue()

    def read(self, fmt):
        if fmt is None:
            return ()
        res = []
        flags = 0
        waitpercent = True
        for c in fmt:
            if waitpercent:
                # Only formatting spec expected here
                if c != '%':
                    raise DecodeException("decoder : invalid format char (%c)" % c)
                waitpercent = False
                flags = 0
            elif c == 'l':
                if flags == 0:
                    flags |= Decoder._FLAG_L
                elif flags & Decoder._FLAG_L:
                    flags &= ~Decoder._FLAG_L
                    flags |= Decoder._FLAG_LL
            elif c == 'h':
                if flags == 0:
                    flags |= Decoder._FLAG_H
                elif flags & Decoder._FLAG_H:
                    flags &= ~Decoder._FLAG_H
                    flags |= Decoder._FLAG_HH
            elif c == 'i' or c == 'd':
                # Signed integer
                if flags & Decoder._FLAG_LL:
                    val = self.readI64()
                elif flags & Decoder._FLAG_L:
                    val = self.readI32()
                elif flags & Decoder._FLAG_HH:
                    val = self.readI8()
                elif flags & Decoder._FLAG_H:
                    val = self.readI16()
                else:
                    val = self.readI32()
                res.append(val)
                waitpercent = True
            elif c == 'u':
                # Unsigned integer
                if flags & Decoder._FLAG_LL:
                    val = self.readU64()
                elif flags & Decoder._FLAG_L:
                    val = self.readU32()
                elif flags & Decoder._FLAG_HH:
                    val = self.readU8()
                elif flags & Decoder._FLAG_H:
                    val = self.readU16()
                else:
                    val = self.readU32()
                res.append(val)
                waitpercent = True
            elif c == 's':
                val = self.readStr()
                res.append(val)
                waitpercent = True
            elif c == 'p':
                val = self.readBuf()
                res.append(val)
                waitpercent = True
            elif c == 'f' or c == 'F' or c == 'e' or c == 'E' or c == 'g' or c == 'G':
                # Floating point
                if flags & (Decoder._FLAG_LL | Decoder._FLAG_H | Decoder._FLAG_HH):
                    raise DecodeException("decoder : unsupported format width")
                elif flags & Decoder._FLAG_L:
                    val = self.readF64()
                else:
                    val = self.readF32()
                res.append(val)
                waitpercent = True
            else:
                raise DecodeException("decoder : invalid format specifier (%c)" % c)
        return tuple(res)

    def readI8(self):
        self._readType(protocol.DATA_TYPE_I8)
        val = self._readByte()
        return val if val <= 0x7f else val - 0xff - 1

    def readU8(self):
        self._readType(protocol.DATA_TYPE_U8)
        val = self._readByte()
        return val

    def readI16(self):
        self._readType(protocol.DATA_TYPE_I16)
        byte0 = self._readByte()
        byte1 = self._readByte()
        val = (byte1 << 8) | (byte0 & 0xff)
        return val if val <= 0x7fff else val - 0xffff - 1

    def readU16(self):
        self._readType(protocol.DATA_TYPE_U16)
        byte0 = self._readByte()
        byte1 = self._readByte()
        val = (byte1 << 8) | (byte0 & 0xff)
        return val

    def readI32(self):
        self._readType(protocol.DATA_TYPE_I32)
        zval = self._readVarint()
        # Zigzag decoding, use logical right shift, without sign propagation
        val = ((zval >> 1) ^ -(zval & 0x1)) & 0xffffffff
        return val if val <= 0x7fffffff else val - 0xffffffff - 1

    def readU32(self):
        self._readType(protocol.DATA_TYPE_U32)
        zval = self._readVarint()
        return zval & 0xffffffff

    def readI64(self):
        self._readType(protocol.DATA_TYPE_I64)
        zval = self._readVarint()
        # Zigzag decoding, use logical right shift, without sign propagation
        val = ((zval >> 1) ^ -(zval & 0x1)) & 0xffffffffffffffff
        return val if val <= 0x7fffffffffffffff else val - 0xffffffffffffffff - 1

    def readU64(self):
        self._readType(protocol.DATA_TYPE_U64)
        zval = self._readVarint()
        return zval & 0xffffffffffffffff

    def readStr(self):
        self._readType(protocol.DATA_TYPE_STR)
        slen = self._readSizeU16()
        if slen <= 0 or slen > 0xffff:
            raise DecodeException("Invalid string length: %d" % slen)
        sval = self._read(slen - 1)
        nullb = self._readByte()
        if nullb != 0:
            raise DecodeException("String not null terminated")
        return sval.decode("ascii")

    def readBuf(self):
        self._readType(protocol.DATA_TYPE_BUF)
        blen = self._readSizeU32()
        return self._read(blen)

    def readF32(self):
        self._readType(protocol.DATA_TYPE_F32)
        return struct.unpack("<f", self._read(4))[0]

    def readF64(self):
        self._readType(protocol.DATA_TYPE_F64)
        return struct.unpack("<d", self._read(8))[0]

    def _read(self, count):
        return self.msg.buf.read(count)

    def _readByte(self):
        return self.msg.buf.readByte()

    def _readType(self, datatype):
        readtype = self._readByte()
        if readtype != datatype:
            raise DecodeException("Type mismatch: %02x(%02x)" % (readtype, datatype))

    def _readVarint(self):
        val = 0
        shift = 0
        more = True
        while more:
            bval = self._readByte()
            val |= ((bval & 0x7f) << shift)
            shift += 7
            more = ((bval & 0x80) != 0)
        return val

    def _readSizeU16(self):
        return self._readVarint()

    def _readSizeU32(self):
        return self._readVarint()
