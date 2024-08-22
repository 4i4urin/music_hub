from os.path import join, isfile
from random import randrange

from serv_prot import Package, TypesCsp, HeadCsp
from private import *

from crccheck.crc import Crc16Arc

from flask import Flask, jsonify
from flask import request
import os

app = Flask(__name__)


ERROR: bytearray = bytearray(b'\xff\xff\xff\xff\xff\xff\xff')
UNIQUE_DEVICE_ID: bytearray = bytearray(b'\x66\x97\xE4\x35')
MAX_TRACK_DATA_PACK: int = 25000
U16_T_MAX: int = 65535


class Track:
    hash_name: int
    path: str
    queue_num: int
    is_send: bool = False
    file_size: int
    file_by_packs: list[bytes]

    def __init__(self, path: str, queue_num: int):
        self.path = path
        self.queue_num = queue_num
        self.hash_name = Crc16Arc.calc(os.path.basename(path).encode())
        self.file_size = os.stat(path).st_size


class TrackList:
    hash_prev: int
    hash_current: int
    hash_next: int

    def __init__(self, track_list: bytearray):
        self.hash_prev = int.from_bytes(track_list[:2], "little", signed=False)
        self.hash_current = int.from_bytes(track_list[2:4], "little", signed=False)
        self.hash_next = int.from_bytes(track_list[4:6], "little", signed=False)

    def print(self):
        print(f"prev = {self.hash_prev} current = {self.hash_current} next = {self.hash_next}")


class Device:
    id: int
    volume_lvl: int
    track_list: TrackList
    dbg_status_count: int = 0

    def __init__(self):
        self.id = randrange(0, 255)

    def read_statys(self, statys: bytearray):
        self.track_list = TrackList(statys[:6])
        self.volume_lvl = statys[6]

    def print(self):
        print(f"device id = {self.id}")
        print(f"volume level = {self.volume_lvl}")
        self.track_list.print()


devices: list[Device] = []
track_list: list[Track] = []


def build_resp_ack(dev_id: int, received_pack_id: TypesCsp) -> bytes:
    head: HeadCsp = HeadCsp.build(dev_id, TypesCsp.ECSP_ACK, 1)
    pack: Package = Package.build(head, received_pack_id.value.to_bytes(1, "little"))
    print(pack.full_pack.hex(":"))
    return pack.full_pack


def build_track_pack(dev_id: int, track: Track, pack_num: int) -> bytes:
    body: bytearray = bytearray(b'\x01') + bytearray(track.hash_name.to_bytes(2, "little"))
    body += bytearray(len(track.file_by_packs).to_bytes(2, "little")) + pack_num.to_bytes(2, "little")
    body += bytearray(len(track.file_by_packs[pack_num]).to_bytes(2, "little"))
    body += bytearray(track.file_by_packs[pack_num])

    head: HeadCsp = HeadCsp.build(dev_id, TypesCsp.ECSP_TRACK_DATA, len(body))
    head.print()
    return Package.build(head, body).full_pack




def switch_playlist(dev_id: int) -> bytes:
    head: HeadCsp = HeadCsp.build(dev_id, TypesCsp.ECSP_COM_SWITCH_LIST, 4)
    body: bytearray = bytearray(track_list[0].hash_name.to_bytes(2, "little"))

    if track_list[0].file_size // MAX_TRACK_DATA_PACK + 1 > U16_T_MAX:
        print("Too big file")
        return bytes(ERROR)

    if track_list[0].file_size % MAX_TRACK_DATA_PACK != 0:
        amount_packs: int = track_list[0].file_size // MAX_TRACK_DATA_PACK + 1
    else:
        amount_packs: int = track_list[0].file_size // MAX_TRACK_DATA_PACK

    body += bytearray(amount_packs.to_bytes(2, "little"))
    pack: Package = Package.build(head, body)

    print(pack.full_pack.hex(":"))
    return pack.full_pack


def pack_validation(pack: Package, expect_msg_type: TypesCsp) -> bool:
    if not pack.validate():
        return False
    if pack.head.type != expect_msg_type:
        print("WARNING: wrong package id")
        return False
    if not devices:
        print("No such device")
        # TODO: init connection
        return False

    if devices[0].id != pack.head.dev_id:
        print("WARNING: wrong device id")
        return False
    return True


@app.route(connect, methods=['POST', 'GET'])
def connect_dev():
    pack = request.get_data()
    package = Package(bytearray(pack))
    if not package.validate():
        return bytes(ERROR)
    if package.head.type != TypesCsp.ECSP_CONNECT:
        print("WARNING: wrong package id")
        return bytes(ERROR)
    if package.body[::-1] != UNIQUE_DEVICE_ID:
        print("WARNING: wrong unique number")
        return bytes(ERROR)
    # add device
    if devices is not None:
        dev = Device()
        # TODO: remake adding dev for debug
        devices.append(dev)
        devices[0] = dev
    print("Connect dev")
    return bytes(build_resp_ack(devices[0].id, package.head.type))



@app.route(status, methods=['POST', 'GET'])
def dev_statys():
    package = Package(bytearray(request.get_data()))
    if pack_validation(package, TypesCsp.ECSP_STATUS) is False:
        return bytes(ERROR)
    print("STATYS PACK")

    devices[0].read_statys(package.body)
    if devices[0].track_list.hash_current == 0:
        print("SWITCH PLAY LIST")
        return bytes(switch_playlist(devices[0].id))

    return bytes(build_resp_ack(devices[0].id, package.head.type))


@app.route(track, methods=['POST', 'GET'])
def track_transmission():
    package = Package(bytearray(request.get_data()))
    if pack_validation(package, TypesCsp.ECSP_COM_GET_TRACK) is False:
        return bytes(ERROR)

    print("GET TRACK")
    print(package.body.hex(":"))
    track_hash: int = int.from_bytes(package.body[:2], "little")
    track_pack_num: int = int.from_bytes(package.body[2:4], "little")
    print(f"req of {track_pack_num} pack")
    print(f"req of {track_hash}")

    tracklist_index: int = find_track_by_hash(track_list, track_hash)
    if tracklist_index is None:
        print("WARNING: unexpected track hash")
        return bytes(ERROR)
    track: Track = track_list[tracklist_index]
    if track_pack_num == 0:
        track.file_by_packs = parce_music_file(track.path)

    # DBG
    if track_pack_num == 175:
        print(bytearray(track.file_by_packs[175]).hex(":"))

    return bytes(build_track_pack(devices[0].id, track, track_pack_num))


def parce_music_file(path: str) -> list[bytes]:
    with open(path, "rb") as f:
        file_data = f.read()
        track_bursts = [file_data[i: i+MAX_TRACK_DATA_PACK] for i in range(0, len(file_data), MAX_TRACK_DATA_PACK)]
    return track_bursts


def find_track_by_hash(track_list: list[Track], track_hash: int) -> int | None:
    for track in track_list:
        if track.hash_name == track_hash:
            return track_list.index(track)
    return None


@app.route(message, methods=['POST', 'GET'])
def get_message():
    return jsonify("ERROR: NO CONNECTED DEV")


if __name__ == '__main__':
    files = [f for f in os.listdir(os.getcwd()) if isfile(join(os.getcwd(), f))]
    num: int = 0
    for file in files:
        if ".mp3" in file:
            track_list.append(Track(os.getcwd() + "\\" + file, num))
            num += 1

    app.run(debug=True, port=5000, host='0.0.0.0')
