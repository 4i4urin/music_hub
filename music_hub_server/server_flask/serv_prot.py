from enum import Enum
from crccheck.crc import Crc16Arc, Crc8


class TypesCsp(Enum):
    ECSP_CONNECT = 11
    ECSP_DISCONNECT = 12
    ECSP_STATUS = 13
    ECSP_TRACK_DATA = 14
    ECSP_COM_PAUSE = 15
    ECSP_COM_RESUME = 16
    ECSP_COM_NEXT = 17
    ECSP_COM_PREV = 18
    ECSP_COM_REPEAT = 19
    ECSP_COM_VOL_INC = 20
    ECSP_COM_VOL_DEC = 21
    ECSP_COM_SWITCH_LIST = 22
    ECSP_COM_GET_TRACK = 23
    ECSP_ACK = 24


class HeadCsp:
    dev_id: int
    type: TypesCsp
    body_len: int
    crc8: int = 0

    bytes: bytearray

    valid: bool = False

    def __init__(self, pack: bytearray):
        self.bytes = pack
        self.dev_id = pack[0]  # uint8_t
        self.type = TypesCsp(pack[1])  # uint8_t
        self.body_len = pack[2] + pack[3] * 256  # uint16_t
        self.valid = True if pack[4] == Crc8.calc(self.bytes[0:4]) else False

    @classmethod
    def build(cls, device_id: int, msg_type: TypesCsp, body_len: int):
        head_burst: bytearray = bytearray(device_id.to_bytes(1, "little")) + \
            msg_type.value.to_bytes(1, "little") + \
            body_len.to_bytes(2, "little")
        cls.crc8 = Crc8.calc(head_burst)
        cls.bytes = head_burst + bytearray(cls.crc8.to_bytes(1, "little"))
        return cls(cls.bytes)


    def print(self):
        print(f"ID = {self.dev_id}")
        print(f"type = {self.type}")
        print(f"body_len = {self.body_len}")
        print(f"crc = {hex(self.crc8)}")



class Package:
    head: HeadCsp
    body: bytearray
    crc16: bytearray = bytearray(b'\x00\x00')

    full_pack: bytearray

    def __init__(self, pack: bytearray):
        self.full_pack = pack
        self.head = HeadCsp(pack)
        self.body = pack[5:-2]
        self.crc16 = (pack[-2:])[::-1]
        self.bytes = pack


    @classmethod
    def build(cls, head: HeadCsp, body: bytearray | None):
        if body is None:
            pack_burst: bytearray = bytearray(head.bytes)
        else:
            pack_burst: bytearray = bytearray(head.bytes) + body
        crc16: bytearray = bytearray(Crc16Arc.calc(pack_burst).to_bytes(2, "little"))
        return cls(pack_burst + crc16)


    def validate(self) -> bool:
        if self.head.valid and self.crc16 == bytearray(Crc16Arc.calc(self.bytes[:-2]).to_bytes(2, "big")):
            return True
        else:
            if self.head.valid:
                print("ERROR: full pack crc")
            else:
                print("ERROR: head crc")
            return False


if __name__ == '__main__':
    head: HeadCsp = HeadCsp.build(30, TypesCsp.ECSP_ACK, 1)
    print(bytearray(TypesCsp.ECSP_CONNECT.value.to_bytes(1, "big")).hex(":"))
    pack: Package = Package.build(head, bytearray(TypesCsp.ECSP_CONNECT.value.to_bytes(1, "big")))
    print(pack.full_pack.hex(":"))
