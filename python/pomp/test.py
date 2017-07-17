#!/usr/bin/env python

#===============================================================================
# @file test.py
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

import unittest
import struct
from io import BytesIO

from pomp import EncodeException
from pomp import DecodeException
from pomp.message import Message
from pomp.encoder import Encoder
from pomp.decoder import Decoder

def getByte(val, idx):
    return (val >> (8 * idx)) & 0xff

MSGID = (42)

VAL_I8 = (-32)
VAL_U8 = (212)
VAL_I16 = (-1000)
VAL_U16 = (23000)
VAL_I32 = (-71000)
VAL_U32 = (3000000000)
VAL_I64 = (-4000000000)
VAL_U64 = (10000000000000000000)
VAL_STR = "Hello World !!!"
VAL_BUF = b"hELLO wORLD ???"
VAL_BUFLEN = 15
VAL_F32 = (3.1415927410125732421875)
VAL_F64 = (3.141592653589793115997963468544185161590576171875)

VAL_I8_ENCODED = b"\xe0"
VAL_U8_ENCODED = b"\xd4"
VAL_I16_ENCODED = b"\x18\xfc"
VAL_U16_ENCODED = b"\xd8\x59"
VAL_I32_ENCODED = b"\xaf\xd5\x08"
VAL_U32_ENCODED = b"\x80\xbc\xc1\x96\x0b"
VAL_I64_ENCODED = b"\xff\x9f\xd9\xe6\x1d"
VAL_U64_ENCODED = b"\x80\x80\xa0\xcf\xc8\xe0\xc8\xe3\x8a\x01"
VAL_STR_ENCODED = b"\x10Hello World !!!\x00"
VAL_BUF_ENCODED = b"\x0fhELLO wORLD ???"
VAL_F32_ENCODED = b"\xdb\x0f\x49\x40"
VAL_F64_ENCODED = b"\x18\x2d\x44\x54\xfb\x21\x09\x40"

PAYLOAD_BUF = BytesIO()
PAYLOAD_BUF.write(b"\x01")
PAYLOAD_BUF.write(VAL_I8_ENCODED)
PAYLOAD_BUF.write(b"\x02")
PAYLOAD_BUF.write(VAL_U8_ENCODED)
PAYLOAD_BUF.write(b"\x03")
PAYLOAD_BUF.write(VAL_I16_ENCODED)
PAYLOAD_BUF.write(b"\x04")
PAYLOAD_BUF.write(VAL_U16_ENCODED)
PAYLOAD_BUF.write(b"\x05")
PAYLOAD_BUF.write(VAL_I32_ENCODED)
PAYLOAD_BUF.write(b"\x06")
PAYLOAD_BUF.write(VAL_U32_ENCODED)
PAYLOAD_BUF.write(b"\x07")
PAYLOAD_BUF.write(VAL_I64_ENCODED)
PAYLOAD_BUF.write(b"\x08")
PAYLOAD_BUF.write(VAL_U64_ENCODED)
PAYLOAD_BUF.write(b"\x09")
PAYLOAD_BUF.write(VAL_STR_ENCODED)
PAYLOAD_BUF.write(b"\x0a")
PAYLOAD_BUF.write(VAL_BUF_ENCODED)
PAYLOAD_BUF.write(b"\x0b")
PAYLOAD_BUF.write(VAL_F32_ENCODED)
PAYLOAD_BUF.write(b"\x0c")
PAYLOAD_BUF.write(VAL_F64_ENCODED)
PAYLOAD = PAYLOAD_BUF.getvalue()

HEADER_BUF = BytesIO()
HEADER_BUF.write(b"POMP")
HEADER_BUF.write(struct.pack("<I", MSGID))
HEADER_BUF.write(struct.pack("<I", 12 + len(PAYLOAD)))
HEADER = HEADER_BUF.getvalue()

MSG_DUMP = (
    "{"
    "ID:42"
    ", I8:-32"
    ", U8:212"
    ", I16:-1000"
    ", U16:23000"
    ", I32:-71000"
    ", U32:3000000000"
    ", I64:-4000000000"
    ", U64:10000000000000000000"
    ", STR:'%s'"
    ", BUF:'%s'"
    ", F32:3.1415927410125732"
    ", F64:3.141592653589793"
    "}"
) % (VAL_STR, repr(VAL_BUF))

#===============================================================================
#===============================================================================
class EncoderTestCase(unittest.TestCase):
    def testEncoderBase(self):
        # Allocate message */
        msg = Message()
        msg.init(MSGID)

        # Allocate and init encoder
        enc = Encoder()
        enc.init(msg)

        # Write
        try:
            enc.writeI8(VAL_I8)
            enc.writeU8(VAL_U8)
            enc.writeI16(VAL_I16)
            enc.writeU16(VAL_U16)
            enc.writeI32(VAL_I32)
            enc.writeU32(VAL_U32)
            enc.writeI64(VAL_I64)
            enc.writeU64(VAL_U64)
            enc.writeStr(VAL_STR)
            enc.writeBuf(VAL_BUF)
            enc.writeF32(VAL_F32)
            enc.writeF64(VAL_F64)
        except EncodeException as ex:
            self.fail(ex.message)

        msg.finish()

        # Buffer check
        data = msg.buf.getData()
        header = data[:12]
        payload = data[12:]
        self.assertEqual(len(HEADER), len(header))
        self.assertEqual(len(PAYLOAD), len(payload))
        self.assertEqual(HEADER, header)
        self.assertEqual(PAYLOAD, payload)

        # Clear
        enc.clear()
        msg.clear()

#===============================================================================
#===============================================================================
class DecoderTestCase(unittest.TestCase):
    def testDecoderBase(self):
        # Allocate message, write reference payload
        msg = Message()
        msg.init(MSGID)
        msg.buf.skip(12)
        msg.buf.write(PAYLOAD)
        msg.finish()

        # Allocate and init decoder
        dec = Decoder()
        dec.init(msg)

        # Read
        try:
            i8 = dec.readI8()
            u8 = dec.readU8()
            i16 = dec.readI16()
            u16 = dec.readU16()
            i32 = dec.readI32()
            u32 = dec.readU32()
            i64 = dec.readI64()
            u64 = dec.readU64()
            sval = dec.readStr()
            buf = dec.readBuf()
            f32 = dec.readF32()
            f64 = dec.readF64()

            # Check
            self.assertEqual(VAL_I8, i8)
            self.assertEqual(VAL_U8, u8)
            self.assertEqual(VAL_I16, i16)
            self.assertEqual(VAL_U16, u16)
            self.assertEqual(VAL_I32, i32)
            self.assertEqual(VAL_U32, u32)
            self.assertEqual(VAL_I64, i64)
            self.assertEqual(VAL_U64, u64)
            self.assertEqual(VAL_STR, sval)
            self.assertEqual(len(VAL_BUF), len(buf))
            self.assertEqual(VAL_BUF, buf)
            self.assertEqual(VAL_F32, f32)
            self.assertEqual(VAL_F64, f64)
        except DecodeException as ex:
            self.fail(ex.message)

        # Clear
        dec.clear()
        msg.clear()

#===============================================================================
#===============================================================================
class MessageTestCase(unittest.TestCase):
    def testMessageBase(self):
        msg = Message()

        # Write
        msg.write(MSGID, "%hhd%hhu%hd%hu%d%u%lld%llu%s%p%f%lf",
            VAL_I8, VAL_U8,
            VAL_I16, VAL_U16,
            VAL_I32, VAL_U32,
            VAL_I64, VAL_U64,
            VAL_STR, VAL_BUF,
            VAL_F32, VAL_F64)

        # Buffer check
        data = msg.buf.getData()
        header = data[:12]
        payload = data[12:]
        self.assertEqual(len(HEADER), len(header))
        self.assertEqual(len(PAYLOAD), len(payload))
        self.assertEqual(HEADER, header)
        self.assertEqual(PAYLOAD, payload)

        # Read
        res = msg.read("%hhd%hhu%hd%hu%d%u%lld%llu%s%p%f%lf")
        self.assertEqual(len(res), 12)
        self.assertEqual(res[0], VAL_I8)
        self.assertEqual(res[1], VAL_U8)
        self.assertEqual(res[2], VAL_I16)
        self.assertEqual(res[3], VAL_U16)
        self.assertEqual(res[4], VAL_I32)
        self.assertEqual(res[5], VAL_U32)
        self.assertEqual(res[6], VAL_I64)
        self.assertEqual(res[7], VAL_U64)
        self.assertEqual(res[8], VAL_STR)
        self.assertEqual(res[9], VAL_BUF)
        self.assertEqual(res[10], VAL_F32)
        self.assertEqual(res[11], VAL_F64)

        # Dump
        buf = msg.dump()
        self.assertEqual(MSG_DUMP, buf)

if __name__ == "__main__":
    unittest.main()
