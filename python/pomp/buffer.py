#===============================================================================
# @file buffer.py
#
# @brief Wrap a StringIO buffer.
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

import os
import struct
import sys
if(sys.version_info.major == 3):
    from io import BytesIO
else:
    from cStringIO import StringIO as BytesIO

#===============================================================================
#===============================================================================
class Buffer(object):
    def __init__(self):
        self.data = BytesIO()
        self.readonly = False

    def __len__(self):
        pos = self.data.tell()
        self.data.seek(0, os.SEEK_END)
        size = self.data.tell()
        self.data.seek(pos, os.SEEK_SET)
        return size

    def getData(self):
        return self.data.getvalue()

    def setReadOnly(self, readonly):
        self.readonly = readonly

    def getPos(self):
        return self.data.tell()

    def setPos(self, pos):
        self.data.seek(pos, os.SEEK_SET)

    def rewind(self):
        self.data.seek(0, os.SEEK_SET)

    def skip(self, count):
        assert not self.readonly
        self.data.seek(count, os.SEEK_CUR)

    def write(self, buf):
        assert not self.readonly
        self.data.write(buf)

    def writeByte(self, val):
        assert not self.readonly
        self.data.write(struct.pack("<B", val))

    def writeInt(self, val):
        assert not self.readonly
        self.data.write(struct.pack("<I", val))

    def read(self, count):
        return self.data.read(count)

    def readByte(self):
        return struct.unpack("<B", self.data.read(1))[0]

    def readInt(self):
        return struct.unpack("<I", self.data.read(4))[0]
