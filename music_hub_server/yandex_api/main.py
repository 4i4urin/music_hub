from yandex_music import Client
from dataclasses import dataclass

from private import *

client = Client(token).init()


@dataclass
class SearchRequest:
    track: str = None
    album: str = None
    artist: str = None
    playlist: str = None


@dataclass
class SearchResult:
    type: str
    object_id: int | str


def build_search_req(search_data: SearchRequest) -> str:
    if search_data.playlist is not None:
        return search_data.playlist
    request_string: str = ""
    if search_data.artist is not None:
        request_string += search_data.artist
    if search_data.album is not None:
        request_string += search_data.album if not request_string else " " + search_data.album
    if search_data.track is not None:
        request_string += search_data.track if not request_string else " " + search_data.track

    return request_string


def yandex_music_search_playlist(query_text: str) -> str:
    search_result = client.search(query_text, type_='playlist')
    print(f"Playlist total: {search_result.playlists.total}")
    print(f"Playlist name: {search_result.playlists.results[0].title}")
    return str(search_result.playlists.results[0].playlistId)


def yandex_music_search_artist(query_text: str) -> int:
    search_result = client.search(query_text, type_='artist')
    print(f"Artist total: {search_result.artists.total}")
    print(f"Artist name: {search_result.artists.results[0].name}")
    return search_result.artists.results[0].id


def yandex_music_search_album(query_text: str) -> int:
    search_result = client.search(query_text, type_='album')
    print(f"Albums total: {search_result.albums.total}")
    print(f"Album name: {search_result.albums.results[0].title}")
    return search_result.albums.results[0].id


def yandex_music_search_track(query_text: str) -> int:
    search_result = client.search(query_text, type_='track')
    print(f"Tracks total: {search_result.tracks.total}")
    print(f"Track name: {search_result.tracks.results[0].title}")
    return search_result.tracks.results[0].id


def yandex_music_search(query: SearchRequest) -> SearchResult | None:
    if query.playlist is not None:
        playlist_id = yandex_music_search_playlist(query.playlist)
        return SearchResult("playlist", playlist_id)

    if query.track is None and query.album is None and query.artist is not None:
        artist_id = yandex_music_search_artist(query.artist)
        return SearchResult("artist", artist_id)

    if query.track is None and query.album is not None and query.artist is None:
        album_id = yandex_music_search_album(query.album)
        return SearchResult("album", album_id)

    if query.track is not None and query.album is None and query.artist is None:
        track_id = yandex_music_search_track(query.track)
        return SearchResult("track", track_id)

    search_result = client.search(build_search_req(query))
    print(f"search req = {build_search_req(query)}")
    if search_result.best:
        print(f"Result type: {search_result.best.type}")
        print(f"Result: {search_result.best.result}")
        return SearchResult(search_result.best.type, search_result.best.result.id)
    else:
        print("No best result")
    return None


if __name__ == '__main__':
    client.init()
    search = [SearchRequest(track="Марабу"), SearchRequest(album="Queen"), SearchRequest(artist="Jungle"),
              SearchRequest(playlist="100 инди-хитов"), SearchRequest(artist="Phoebe Bridgers", album="Punisher"),
              SearchRequest(track="Я устал", album="1917"), SearchRequest(track="Машины летят", artist="НЕДРЫ"),
              SearchRequest(track="Europe is Lost", album="Let Them Eat Chaos", artist="Kae Tempest")]
    i = 0
    for request in search:
        result: SearchResult = yandex_music_search(request)
        if result is None:
            continue

        print(f"{result.type}, id = {result.object_id}")
        if result.type == "track":
            i += 1
            client.tracks(result.object_id)[0].download(f"track_{i}.mp3")
            similar_tracks = client.tracks_similar(result.object_id)
            for track in similar_tracks:
                print(f"{track['title']} - {track['artists'][0]['name']}", )
        print("\n\n")

