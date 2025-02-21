

KEY_PHRASE: str = "на колонке"

def extract_music_name(text):
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

