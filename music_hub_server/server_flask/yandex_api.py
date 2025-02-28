from private import KEY_PHRASE, ya_token
from crccheck.crc import Crc16Arc
from os import stat, path, getcwd


class Track:
    hash_name: int
    path: str
    queue_num: int
    is_send: bool = False
    file_size: int
    file_by_packs: list[bytes]

    def __init__(self, user_path: str, queue_num: int):
        self.path = user_path
        self.queue_num = queue_num
        self.hash_name = Crc16Arc.calc(path.basename(user_path).encode())
        self.file_size = stat(user_path).st_size


def download_track(client, track_id: int, track_index: int) -> str:
    file_name: str = f"track_{track_index}.mp3"
    client.tracks(track_id)[0].download(file_name)
    return file_name


def extract_music_name(text: str) -> str | None:
    if KEY_PHRASE not in text:
        return None

    text_before = text.split(KEY_PHRASE)[0].split()
    is_phrase: bool = True
    for i, word in reversed(list(enumerate(text_before))):
        if word[0].islower() and is_phrase:
            continue
        elif word[0].islower():
            return " ".join(text_before[i+1:])

        if word[0].isupper():
            is_phrase = False

    return " ".join(text_before[1:])


def download_user_req(client, playlist, text_req: str) -> str | None:
    search_str: str = extract_music_name(text_req)
    if search_str is None:
        print(f"ERROR: can't parse user req: {text_req}")
        return

    search_result = client.search(search_str)

    if search_result.best.type == "track":
        track_id = search_result.best.result.id
    elif search_result.best.type == "album":
        album = client.albums(search_result.best.result.id)[0]
        track_id = album.with_tracks().volumes[0][0].id
    elif search_result.best.type == "artist":
        artist = client.artists(search_result.best.result.id)[0]
        track_id = artist.get_tracks().tracks[0].id
    elif search_result.best.type == "playlist":
        playlist_id = f"{search_result.best.result['uid']}:{search_result.best.result['kind']}"
        playlist = client.playlistsList(playlist_id)[0]
        track_id = playlist.fetch_tracks()[0].track.id
    else:
        print(f"ERROR: unknown type {search_result.best.type}")
        return

    print(track_id)
    return download_track(client, track_id, playlist.track_num)


