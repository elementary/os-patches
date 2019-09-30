// (C) Michael 'Mickey' Lauer <mickey@vanille-media.de>
// LGPL2
// scheduled for inclusion in linux.vapi

namespace Linux
{
    /*
     * RfKill
     */
    [CCode (cname = "struct rfkill_event", cheader_filename = "linux/rfkill.h")]
    public struct RfKillEvent {
        public uint32 idx;
        public RfKillType type;
        public RfKillOp op;
        public uint8 soft;
        public uint8 hard;
    }

    [CCode (cname = "guint8", cprefix = "RFKILL_OP_", cheader_filename = "linux/rfkill.h")]
    public enum RfKillOp {
        ADD,
        DEL,
        CHANGE,
        CHANGE_ALL
    }

    [CCode (cname = "guint8", cprefix = "RFKILL_STATE_", cheader_filename = "linux/rfkill.h")]
    public enum RfKillState {
        SOFT_BLOCKED,
        UNBLOCKED,
        HARD_BLOCKED
    }

    [CCode (cname = "guint8", cprefix = "RFKILL_TYPE_", cheader_filename = "linux/rfkill.h")]
    public enum RfKillType {
        ALL,
        WLAN,
        BLUETOOTH,
        UWB,
        WIMAX,
        WWAN
    }

} /* namespace Linux */
