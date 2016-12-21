#===============================================================================
# @brief Printf Oriented Message Protocol.
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

import socket

from pomp.context import Context
from pomp.context import EventHandler
from pomp.message import Message
from pomp.encoder import Encoder
from pomp.encoder import EncodeException
from pomp.decoder import Decoder
from pomp.decoder import DecodeException

#===============================================================================
#===============================================================================
def parseInetAddr(family, buf):
    sep = buf.rfind(":")
    if sep < 0:
        raise ValueError()
    host = buf[:sep]
    port = int(buf[sep+1:])
    return (family, (host, port))

#===============================================================================
#===============================================================================
def parseAddr(buf):
    if buf.startswith("inet:"):
        return parseInetAddr(socket.AF_INET, buf[5:])
    elif buf.startswith("inet6:"):
        return parseInetAddr(socket.AF_INET6, buf[6:])
    elif buf.startswith("unix:"):
        path = buf[5:]
        if path.startswith("@"):
            path = "\x00" + path[1:] + (108 - len(path)) * "\x00"
        return (socket.AF_UNIX, path)
    else:
        raise ValueError("Unable to parse address: '%s" % buf)
