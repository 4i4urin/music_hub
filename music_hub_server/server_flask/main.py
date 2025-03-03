from os.path import join, isfile
from random import randrange

from serv_prot import Package, TypesCsp, HeadCsp
from private import *

from crccheck.crc import Crc16Arc

from flask import Flask, jsonify
from flask import request
import os

from yandex_music import Client

from yandex_api import download_user_req
import random
from test_user_req import requests


app = Flask(__name__)
client = Client(ya_token).init()


ERROR: bytearray = bytearray(b'\xff\xff\xff\xff\xff\xff\xff')
UNIQUE_DEVICE_ID: bytearray = bytearray(b'\x66\x97\xE4\x35')
MAX_TRACK_DATA_PACK: int = (8 * 1024)
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


class DeviceTrackList:
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
    track_list: DeviceTrackList
    dbg_status_count: int = 0
    speakers: int = 0

    def __init__(self):
        self.id = randrange(0, 255)

    def read_statys(self, statys: bytearray):
        self.track_list = DeviceTrackList(statys[:6])
        self.volume_lvl = statys[6]
        self.speakers = statys[7]

    def print(self):
        print(f"device id = {self.id}")
        print(f"volume level = {self.volume_lvl}")
        self.track_list.print()


class PlayList:
    track_num: int = 0
    track_list: list[Track] = []
    device_track_list: list[Track] = []
    switch_track: bool


devices: list[Device] = []
playlist = PlayList()



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
    # head.print()
    return Package.build(head, body).full_pack


def switch_playlist(dev_id: int, track_pos: int) -> bytes:
    head: HeadCsp = HeadCsp.build(dev_id, TypesCsp.ECSP_COM_SWITCH_LIST, 4)
    if playlist.device_track_list[track_pos] is None:
        print(f"ERROR: no such track num: {track_pos}")
        return bytes(ERROR)

    track_to_send: Track = playlist.device_track_list[track_pos]
    body: bytearray = bytearray(track_to_send.hash_name.to_bytes(2, "little"))

    if track_to_send.file_size // MAX_TRACK_DATA_PACK + 1 > U16_T_MAX:
        print("Too big file")
        return bytes(ERROR)

    if track_to_send.file_size % MAX_TRACK_DATA_PACK != 0:
        amount_packs: int = track_to_send.file_size // MAX_TRACK_DATA_PACK + 1
    else:
        amount_packs: int = track_to_send.file_size // MAX_TRACK_DATA_PACK

    body += bytearray(amount_packs.to_bytes(2, "little"))
    body += bytearray(track_pos.to_bytes(1, "little"))
    pack: Package = Package.build(head, body)

    # print(pack.full_pack.hex(":"))
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
    if not devices:
        print("Connect dev")
        dev = Device()
        # TODO: remake adding dev for debug
        devices.append(dev)
        devices[0] = dev
    print("Connect dev")
    return bytes(build_resp_ack(devices[0].id, package.head.type))


@app.route(status, methods=['POST', 'GET'])
def dev_statys():
    print("STATYS PACK")
    package = Package(bytearray(request.get_data()))
    if pack_validation(package, TypesCsp.ECSP_STATUS) is False:
        print("ERROR")
        return bytes(ERROR)

    devices[0].read_statys(package.body)
    if devices[0].speakers == 0:
        print("No output device")
        return bytes(build_resp_ack(devices[0].id, package.head.type))

    print("Speakers connected can transmit traks")
    # TODO: define current, next and prev positions
    # if devices[0].track_list.hash_current == 0:
    #     print("SWITCH PLAY LIST CURRENT")
    #     return bytes(switch_playlist(devices[0].id, 2))
    # elif devices[0].track_list.hash_next == 0:
    #     print("SWITCH PLAY LIST NEXT")
    #     return bytes(switch_playlist(devices[0].id, 0))
    # elif devices[0].track_list.hash_prev == 0:
    #     print("SWITCH PLAY LIST PREV")
    #     return bytes(switch_playlist(devices[0].id, 1))
    if playlist.track_num != 0 and devices[0].track_list.hash_current == 0:
        print("SWITCH PLAY LIST CURRENT")
        return bytes(switch_playlist(devices[0].id, playlist.track_num - 1))

    return bytes(build_resp_ack(devices[0].id, package.head.type))


@app.route(track, methods=['POST', 'GET'])
def track_transmission():
    print("GET TRACK")
    package = Package(bytearray(request.get_data()))
    if pack_validation(package, TypesCsp.ECSP_COM_GET_TRACK) is False:
        print("ERROR")
        return bytes(ERROR)

    if playlist.switch_track == 1:
        print(f"UPDATE playlist track pos {playlist.track_num - 1}")
        playlist.switch_track = 0
        return bytes(switch_playlist(devices[0].id, playlist.track_num - 1))

    track_hash: int = int.from_bytes(package.body[:2], "little")
    track_pack_num: int = int.from_bytes(package.body[2:4], "little")
    print(f"req of {track_pack_num} pack")
    print(f"req of {track_hash}")

    tracklist_index: int = find_track_by_hash(playlist.device_track_list, track_hash)
    if tracklist_index is None:
        print("WARNING: unexpected track hash")
        return bytes(ERROR)
    track: Track = playlist.device_track_list[tracklist_index]
    if track_pack_num == 0:
        track.file_by_packs = parce_music_file(track.path)

    print("SEND")
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


@app.route(test_user, methods=['POST', 'GET'])
def test_usr_req():
    index_list = random.randint(0, 100)
    print(requests[index_list])
    track_name = download_user_req(client, playlist, requests[index_list])
    if track_name is None:
        return jsonify("ERROR: NO CONNECTED DEV")

    update_playlist(playlist, track_name)
    print(playlist.track_num, playlist.device_track_list[playlist.track_num - 1].path)
    return jsonify("ERROR: NO CONNECTED DEV")


def update_playlist(playlist: PlayList, track_name: str):
    playlist.device_track_list.append(Track(os.getcwd() + "/" + track_name, playlist.track_num))
    playlist.track_num += 1
    playlist.switch_track = 1


@app.route(yandex_req, methods=['POST', 'GET'])
def yandex_req():
    print("Hello yandex")
    dict = request.get_json()
    print(dict["user_phrase"])
    user_req = dict["user_phrase"]
    track_name = download_user_req(client, playlist, user_req)
    if track_name is None:
        return jsonify("ERROR: NO CONNECTED DEV")

    update_playlist(playlist, track_name)
    print(playlist.track_num, playlist.device_track_list[playlist.track_num - 1].path)
    return jsonify("Hello world")


def get_device_id() -> int:
    if not devices:
        return 0
    return devices[0].id


def run_server():
    app.run(debug=True, port=5000, host='0.0.0.0')


if __name__ == '__main__':
    # files = [f for f in os.listdir(os.getcwd()) if isfile(join(os.getcwd(), f))]
    # num: int = 0
    # for file in files:
    #     if ".mp3" in file:
    #         playlist.device_track_list.append(Track(os.getcwd() + "/" + file, num))
    #         num += 1
    # playlist.switch_track = 1
    # playlist.track_num = 1
    run_server()
