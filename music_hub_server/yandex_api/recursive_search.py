# def search_target(search_data: SearchRequest) -> str:
#     if search_data.playlist is not None:
#         return 'playlist'
#     elif search_data.track is not None:
#         return 'track'
#     elif search_data.album is not None:
#         return 'album'
#     elif search_data.artist is not None:
#         return 'artist'
#     else:
#         return 'all'
#
#
# def yandex_music_search_playlist(query_text: str) -> str:
#     search_result = client.search(query_text, type_='playlist')
#     print(f"Playlist total: {search_result.playlists.total}")
#     print(f"Playlist name: {search_result.playlists.results[0].title}")
#     return str(search_result.playlists.results[0].playlistId)
#
#
# def yandex_music_search_artist(query_text: str) -> int:
#     search_result = client.search(query_text, type_='artist')
#     print(f"Artist total: {search_result.artists.total}")
#     print(f"Artist name: {search_result.artists.results[0].name}")
#     return search_result.artists.results[0].id
#
#
# def yandex_music_search_album(query_text: str) -> int:
#     search_result = client.search(query_text, type_='album')
#     print(f"Albums total: {search_result.albums.total}")
#     print(f"Album name: {search_result.albums.results[0].title}")
#     return search_result.albums.results[0].id
#
#
# def yandex_music_search_track(query_text: str) -> int:
#     search_result = client.search(query_text, type_='track')
#     print(f"Tracks total: {search_result.tracks.total}")
#     print(f"Track name: {search_result.tracks.results[0].title}")
#     return search_result.tracks.results[0].id
#
#
# def yandex_music_artist_album(album_name, artist_id: int) -> int:
#     albums = client.artists(artist_id)[0].getAlbums()
#     print(f"Albums total: {len(albums)}")
#     for album in albums:
#         if album.title == album_name:
#             print(f"Album name: {album.title}")
#             return album.id
#     print(f"No such album {album_name}, in album discography")
#
#
# def yandex_music_album_track(track_name, album_id: int) -> int:
#     tracks = []
#     for i, volume in enumerate(client.albums(album_id)[0].withTracks().volumes):
#         tracks += volume
#     print(f"Tracks total: {len(tracks)}")
#     for track in tracks:
#         if track.title == track_name:
#             print(f"Track name: {track.title}")
#             return track.id
#     print(f"No such track {track_name}, in album")
#     return 0
#
#
# def yandex_music_artist_track(track_name, artist_id: int) -> int:
#     for track in client.artists(artist_id)[0].getTracks():
#         if track.title == track_name:
#             print(f"Track name: {track.title}")
#             return track.id
#     print(f"No such track {track_name}, in artist track list")
#
#
# def yandex_music_search(query: SearchRequest) -> SearchResult | None:
#     artist_id: int = 0
#     album_id: int = 0
#     result_id: int = 0
#     if query.playlist is not None:
#
#         playlist_id = yandex_music_search_playlist(query.playlist)
#         return SearchResult("playlist", playlist_id)
#     if query.artist is not None:
#         artist_id = yandex_music_search_artist(query.artist)
#         result_id = artist_id
#     if query.album is not None:
#         if artist_id == 0:
#             album_id = yandex_music_search_album(query.album)
#         else:
#             album_id = yandex_music_artist_album(query.album, artist_id)
#         result_id = album_id
#     if query.track is not None:
#         if album_id == 0 and artist_id == 0:
#             result_id = yandex_music_search_track(query.track)
#         elif album_id != 0:
#             result_id = yandex_music_album_track(query.track, album_id)
#         elif artist_id != 0:
#             result_id = yandex_music_artist_track(query.track, artist_id)
#
#     return SearchResult(search_target(query), result_id)
