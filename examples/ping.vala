/**
 * @file ping.vala
 *
 * @author yves-marie.morgan@parrot.com
 *
 * Copyright (c) 2014 Parrot S.A.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Parrot Company nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT COMPANY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 */
namespace Unix {
    [CCode(has_copy_function = false, has_destroy_function = false, has_type_id = false)]
    public struct SockAddrUn {
        public uint16 sun_family;
        public char sun_path[108];
    }
}

/**
 */
namespace Posix {
    [CCode(has_copy_function = false, has_destroy_function = false, has_type_id = false)]
    public struct SockAddrStorage {
        public uint16 ss_family;
        public char ss_data[128];
    }
}

/**
 */
public class App : GLib.Object {
    private const uint32 MSG_PING = 1;
    private const uint32 MSG_PONG = 2;

    private GLib.MainLoop          main_loop;
    private MyContext              my_ctx;
    private Posix.SockAddrStorage  addr_storage;
    private uint32                 addrlen;

    /**
     *
     */
    private static void usage(string progname) {
        GLib.stderr.printf("%s -s <addr>\n", progname);
        GLib.stderr.printf("    start server\n");
        GLib.stderr.printf("%s -c <addr>\n", progname);
        GLib.stderr.printf("    start client\n");
        GLib.stderr.printf("<addr> format:\n");
        GLib.stderr.printf("  inet:<addr>:<port>\n");
        GLib.stderr.printf("  inet6:<addr>:<port>\n");
        GLib.stderr.printf("  unix:<path>\n");
        GLib.stderr.printf("  unix:@<name>\n");
    }

    /**
     */
    public static int main(string[] args) {
        return new App(args).run();
    }

    /**
     */
    private App(string[] args) {
        /* Create the main loop */
        this.main_loop = new GLib.MainLoop(null, false);

        /* Check arguments */
        if (args.length != 3 || (args[1] != "-s" && args[1] != "-c")) {
            App.usage(args[0]);
            Posix.exit(Posix.EXIT_FAILURE);
        }

        /* Parse address */
        var addr = (Posix.SockAddr *)(&this.addr_storage);
        if (parse_addr(args[2], addr, out this.addrlen) < 0) {
            GLib.message("Failed to parse address : %s", args[2]);
            App.usage(args[0]);
            Posix.exit(Posix.EXIT_FAILURE);
        }

        if (args[1] == "-s")
            this.my_ctx = new Server(this.main_loop);
        else
            this.my_ctx = new Client(this.main_loop);
    }

    /**
     */
    private int run() {
        /* Create a source to catch SIGINT and SIGTERM */
        var signal_source1 = create_signal_source(Posix.SIGINT);
        var signal_source2 = create_signal_source(Posix.SIGTERM);
        signal_source1.attach(this.main_loop.get_context());
        signal_source2.attach(this.main_loop.get_context());

        try {
            /* Start client/server */
            var addr = (Posix.SockAddr *)(&this.addr_storage);
            this.my_ctx.start(addr, this.addrlen);

            /* Run the main loop */
            this.main_loop.run();

            /* Stop client/server */
            this.my_ctx.stop();
        } catch (IOError err) {
            GLib.message("%s", err.message);
        } finally {
            /* Remove signal sources */
            signal_source1.destroy();
            signal_source2.destroy();
        }
        return 0;
    }

    /**
     */
    private GLib.Source create_signal_source(int signum) {
        var signal_source = new GLib.Unix.SignalSource(signum);
        signal_source.set_callback(() => {
            GLib.message("signal %d (%s) received !",
                    signum, GLib.strsignal(signum));
            this.main_loop.quit();
            return true;
        });
        return signal_source;
    }

    /**
     *
     */
    private static int parse_inet_addr(string str, int family, void *dst, out uint16 port) {
        port = 0;
        /* Find port separator */
        int sep = str.last_index_of_char(':');
        if (sep == -1)
            return -Posix.EINVAL;

        /* Convert address and port */
        string ip = str[0:sep];
        if (Posix.inet_pton(family, ip, dst) != 1)
            return -Posix.EINVAL;
        port = (uint16)(int.parse(str[sep+1:str.length]));
        return 0;
    }

    /**
     *
     */
    private static int parse_addr(string str, Posix.SockAddr *addr, out uint32 addrlen) {
        int res = 0;
        addrlen = 0;
        if (str.has_prefix("inet:")) {
            /* Inet v4 address */
            var addr_in = (Posix.SockAddrIn *)addr;
            uint16 port = 0;
            res = App.parse_inet_addr(str[5:str.length], Posix.AF_INET,
                    &addr_in.sin_addr, out port);
            if (res < 0)
                return res;
            addr_in.sin_family = Posix.AF_INET;
            addr_in.sin_port = Posix.htons(port);
            addrlen = (uint32)sizeof(Posix.SockAddrIn);
        } else if (str.has_prefix("inet6:")) {
            /* Inet v6 address */
            var addr_in6 = (Posix.SockAddrIn6 *)addr;
            uint16 port = 0;
            res = parse_inet_addr(str[6:str.length], Posix.AF_INET6,
                    &addr_in6.sin6_addr, out port);
            if (res < 0)
                return res;
            addr_in6.sin6_family = Posix.AF_INET6;
            addr_in6.sin6_port = Posix.htons(port);
            addrlen = (uint32)sizeof(Posix.SockAddrIn6);
        } else if (str.has_prefix("unix:")) {
            /* Unix address */
            var addr_un = (Unix.SockAddrUn *)addr;
            addr_un.sun_family = (uint16)Posix.AF_UNIX;
            var path = str[5:str.length];
            GLib.Memory.copy(addr_un.sun_path, path, uint.min(108, str.length - 5));
            if (str[5] == '@')
                addr_un.sun_path[0] = '\0';
            addrlen = (uint32)sizeof(Unix.SockAddrUn);
        } else {
            return -Posix.EINVAL;
        }
        return 0;
    }

    /**
     */
    private static string format_addr(Posix.SockAddr *addr, uint32 addrlen) {
        string str = null;
        switch (addr.sa_family) {
        case Posix.AF_INET:
            var ip = new uint8[Posix.INET_ADDRSTRLEN];
            var addr_in = (Posix.SockAddrIn *)addr;
            Posix.inet_ntop(Posix.AF_INET, &addr_in.sin_addr, ip);
            str = "inet:%s:%u".printf((string)ip, Posix.ntohs(addr_in.sin_port));
            break;

        case Posix.AF_INET6:
            var ip6 = new uint8[Posix.INET6_ADDRSTRLEN];
            var addr_in6 = (Posix.SockAddrIn6 *)addr;
            Posix.inet_ntop(Posix.AF_INET6, &addr_in6.sin6_addr, ip6);
            str = "inet6:%s:%u".printf((string)ip6, Posix.ntohs(addr_in6.sin6_port));
            break;

        case Posix.AF_UNIX:
            var addr_un = (Unix.SockAddrUn *)addr;
            bool abs = (addr_un.sun_path[0] == '\0');
            str = "unix:%s%.*s".printf(abs ? "@" : "",
                    108 - (abs ? 1 : 0), &addr_un.sun_path[abs ? 1 : 0]);
            break;

        default:
            str = "addr:family:%d".printf(addr.sa_family);
            break;
        }
        return str;
    }

    /**
     */
    private static void log_conn_event(Pomp.Connection conn, bool is_server) {
        /* Get local/peer addresses */
        uint32 local_addrlen = 0;
        uint32 peer_addrlen = 0;
        var local_addr = conn.get_local_addr(out local_addrlen);
        var peer_addr = conn.get_peer_addr(out peer_addrlen);

        if (local_addr == null || local_addrlen == 0) {
            GLib.message("Invalid local address");
            return;
        }
        if (peer_addr == null || peer_addrlen == 0) {
            GLib.message("Invalid peer address");
            return;
        }

        if (local_addr.sa_family == Posix.AF_UNIX) {
            /* Format using either local or peer address depending on
             * client/server side */
            string addr_str = null;
            if (is_server) {
                addr_str = App.format_addr(local_addr, local_addrlen);
            } else {
                addr_str = App.format_addr(peer_addr, peer_addrlen);
            }

            /* Get peer credentials */
            unowned Pomp.Cred? peer_cred = conn.get_peer_cred();
            if (peer_cred == null) {
                GLib.message("%s pid=%d,uid=%lu,gid=%lu -> unknown",
                        addr_str, Posix.getpid(), Posix.getuid(), Posix.getgid());
            } else {
                GLib.message("%s pid=%d,uid=%lu,gid=%lu -> pid=%u,uid=%u,gid=%u",
                        addr_str, Posix.getpid(), Posix.getuid(), Posix.getgid(),
                        peer_cred.pid,
                        peer_cred.uid,
                        peer_cred.gid);
            }
        } else {
            /* Format both addresses and log connection */
            var local_addr_str = App.format_addr(local_addr, local_addrlen);
            var peer_addr_str = App.format_addr(peer_addr, peer_addrlen);
            GLib.message("%s -> %s", local_addr_str, peer_addr_str);
        }
    }

    /**
     */
    private static void dump_msg(Pomp.Message msg) {
        uint32 msgid = 0;
        uint32 count = 0;
        string str = null;

        msgid = msg.id;
        switch (msgid) {
        case App.MSG_PING:
            msg.read("%u%ms", out count, out str);
            GLib.message("MSG_PING  : %u %s", count, str);
            break;

        case MSG_PONG:
            msg.read("%u%ms", out count, out str);
            GLib.message("MSG_PONG  : %u %s", count, str);
            break;

        default:
            GLib.message("MSG_UNKNOWN : %u", msgid);
            break;
        }
    }

    /**
     */
    private abstract class MyContext {
        protected GLib.MainLoop  main_loop;
        protected Pomp.Context   ctx;
        protected GLib.IOSource  ctx_source;

        /**
         */
        public MyContext(GLib.MainLoop main_loop) {
            this.main_loop = main_loop;
            this.ctx = new Pomp.Context(this.event_cb);
        }

        /**
         */
        public abstract void start(Posix.SockAddr *addr, uint32 addrlen) throws IOError;
        public abstract void stop();
        protected abstract void event_cb(Pomp.Context ctx, Pomp.Event event,
                Pomp.Connection conn, Pomp.Message? msg);

        /**
         */
        protected void attach_ctx() {
            /* Create a source for context and attach it to main loop */
            var io_channel = new GLib.IOChannel.unix_new(this.ctx.get_fd());
            this.ctx_source = new GLib.IOSource(io_channel, GLib.IOCondition.IN);
            this.ctx_source.set_callback((source, condition) => {
                this.ctx.process_fd();
                return true;
            });
            this.ctx_source.attach(this.main_loop.get_context());
        }

        /**
         */
        protected void detach_ctx() {
            /* Remove context source from main loop */
            this.ctx_source.destroy();
            this.ctx_source = null;
        }
    }

    /**
     */
    private class Server : MyContext {
        /**
         */
        public Server(GLib.MainLoop main_loop) {
            base(main_loop);
        }

        /**
         */
        public override void start(Posix.SockAddr *addr, uint32 addrlen) throws IOError {
            /* Start listening for incoming connections */
            int res = this.ctx.listen(addr, addrlen);
            if (res < 0) {
                throw new IOError.FAILED("pomp_ctx_listen: err=%d(%s)",
                        res, Posix.strerror(-res));
            }
            this.attach_ctx();
        }

        /**
         */
        public override void stop() {
            this.ctx.stop();
            this.detach_ctx();
        }

        /**
         */
        protected override void event_cb(Pomp.Context ctx, Pomp.Event event,
                Pomp.Connection conn, Pomp.Message? msg) {
            GLib.message("Server: event=%d(%s) conn=%p msg=%p",
                    event, event.to_string(), conn, msg);
            switch (event) {
            case Pomp.Event.CONNECTED: /* NO BREAK */
            case Pomp.Event.DISCONNECTED:
                App.log_conn_event(conn, false);
                break;
            case Pomp.Event.MSG:
                App.dump_msg(msg);
                if (msg.id == App.MSG_PING) {
                    uint32 count = 0;
                    string str = null;
                    msg.read("%u%ms", out count, out str);
                    conn.send(App.MSG_PONG, "%u%s", count, "PONG");
                }
                break;
            default:
                GLib.message("Unknown event: %d", event);
                break;
            }
        }
    }

    /**
     */
    private class Client : MyContext {
        private GLib.Source  timer_source;
        private uint32       count;

        /**
         */
        public Client(GLib.MainLoop main_loop) {
            base(main_loop);
            this.count = 0;
        }

        /**
         */
        protected override void start(Posix.SockAddr *addr, uint32 addrlen) throws IOError {
            /* Start connecting to server */
            int res = this.ctx.connect(addr, addrlen);
            if (res < 0) {
                throw new IOError.FAILED("pomp_ctx_connect : err=%d(%s)",
                        res, Posix.strerror(-res));
            }
            this.attach_ctx();

            /* Create new timer source and attach it to main loop */
            this.timer_source = new GLib.TimeoutSource(1000);
            this.timer_source.set_callback(() => {
                this.ctx.send(App.MSG_PING, "%u%s", ++this.count, "PING");
                return true;
            });
            this.timer_source.attach(this.main_loop.get_context());
        }

        /**
         */
        public override void stop() {
            this.ctx.stop();
            this.detach_ctx();
            this.timer_source.destroy();
            this.timer_source = null;
        }

        /**
         */
        protected override void event_cb(Pomp.Context ctx, Pomp.Event event,
                Pomp.Connection conn, Pomp.Message? msg) {
            GLib.message("Client: event=%d(%s) conn=%p msg=%p",
                    event, event.to_string(), conn, msg);
            switch (event) {
            case Pomp.Event.CONNECTED: /* NO BREAK */
            case Pomp.Event.DISCONNECTED:
                App.log_conn_event(conn, false);
                break;
            case Pomp.Event.MSG:
                App.dump_msg(msg);
                break;
            default:
                GLib.message("Unknown event: %d", event);
                break;
            }
        }
    }

}
