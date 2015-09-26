/**
 * @file libpomp.vapi
 *
 * @brief Printf Oriented Message Protocol.
 *
 * @author yves-marie.morgan@parrot.com
 *
 * @copyright Copyright (C) 2014 Parrot S.A.
 */

[CCode (cheader_filename = "libpomp.h")]
namespace Pomp {

    [Compact]
    [CCode (cname = "struct pomp_cred", destroy_function = "")]
    public struct Cred {
        uint32 pid;
        uint32 uid;
        uint32 gid;
    }

    [CCode (cname = "enum pomp_event", cprefix = "POMP_EVENT_", has_type_id = false)]
    public enum Event {
        CONNECTED,
        DISCONNECTED,
        MSG;
        [CCode (cname = "pomp_event_str")]
        public unowned string to_string();
    }

    [CCode (cname = "pomp_event_cb_t")]
    public delegate void EventCb(Context ctx, Event event, Connection conn, Message? msg);

    [Compact]
    [CCode (cname = "struct pomp_ctx", cprefix = "pomp_ctx_", free_function = "pomp_ctx_destroy", has_type_id = false)]
    public class Context {
        public Context(EventCb cb);
        public int listen(Posix.SockAddr *addr, uint32 addrlen);
        public int connect(Posix.SockAddr *addr, uint32 addrlen);
        public int stop();
        public int get_fd();
        public int process_fd();
        public int wait_and_process(int timeout);
        public unowned Connection? get_next_conn(Connection? prev);
        public unowned Connection? get_conn();
        public int send_msg(Message msg);
        [PrintfFormat]
        public int send(uint32 msgid, string fmt, ...);
        public int sendv(uint32 msgid, string fmt, va_list args);
    }

    [Compact]
    [CCode (cname = "struct pomp_conn", cprefix = "pomp_conn_", has_type_id = false)]
    public class Connection {
        public int disconnect();
        public Posix.SockAddr *get_local_addr(out uint32 addrlen);
        public Posix.SockAddr *get_peer_addr(out uint32 addrlen);
        public unowned Cred? get_peer_cred();
        public int send_msg(Message msg);
        [PrintfFormat]
        public int send(uint32 msgid, string fmt, ...);
        public int sendv(uint32 msgid, string fmt, va_list args);
    }

    [Compact]
    [CCode (cname = "struct pomp_msg", cprefix = "pomp_msg_", free_function = "pomp_msg_destroy", has_type_id = false)]
    public class Message {
        public Message();
        public Message.copy(Message msg);
        public uint32 id {get;}
        [PrintfFormat]
        public int write(uint32 msgid, string fmt, ...);
        public int writev(uint32 msgid, string fmt, va_list args);
        [ScanfFormat]
        public int read(string fmt, ...);
        public int readv(string fmt, va_list args);
    }

}
