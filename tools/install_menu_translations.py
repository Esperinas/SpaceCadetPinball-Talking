#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "SpaceCadetPinball/translations.cpp"
text = path.read_text(encoding="utf-8")
messages = [
    "Menu1_WindowUniformScale", "Menu1_Table_Resolution", "Menu1_ToggleShowMenu",
    "Menu1_UseMaxResolution_640x480", "Menu1_UseMaxResolution_800x600",
    "Menu1_UseMaxResolution_1024x768",
] + [
    "Menu3_Tilt", "Menu3_TiltHard", "Menu3_TiltSoft", "Menu3_NoTilt",
    "Menu3_CenterPostDuration", "Menu3_CenterPostOriginal", "Menu3_Language",
    "Menu3_Audio", "Menu3_StereoSoundEffects", "Menu3_SoundVolume",
    "Menu3_SoundChannels", "Menu3_MusicVolume", "Menu3_Graphics",
    "Menu3_LinearFiltering", "Menu3_EnhancedScaling", "Menu3_IntegerScaling",
    "Menu3_UIScale", "Menu3_GameData", "Menu3_Prefer3DPBData",
    "Menu3_ResetAllOptions", "Menu3_SetDefaultUpsFps", "Menu3_UncappedUps",
    "Menu3_PreciseSleep", "Menu3_HideCursor", "Menu3_ChangeFont",
    "Menu3_ImGuiDemo", "Menu3_SpriteViewer", "Menu3_DebugOverlay", "Menu3_CheatsCode",
]

def remove_block(source, message):
    marker = "Msg::" + message
    location = source.find(marker)
    if location < 0:
        raise RuntimeError("Missing block " + marker)
    start = source.rfind("\n\t{", 0, location) + 1
    depth = 0
    quoted = False
    escaped = False
    end = None
    for index in range(start, len(source)):
        char = source[index]
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
            continue
        if char == '"':
            quoted = True
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                end = index + 1
                if end < len(source) and source[end] == ",":
                    end += 1
                if end < len(source) and source[end] == "\n":
                    end += 1
                break
    if end is None:
        raise RuntimeError("Unclosed block " + marker)
    return source[:start] + source[end:]

for message in messages:
    text = remove_block(text, message)

anchor = "\t{\n\t\tMsg::CheatsUnlockHint,"
if anchor not in text:
    raise RuntimeError("CheatsUnlockHint anchor not found")
text = text.replace(anchor, '#include "MenuTranslations.inc"\n' + anchor, 1)
path.write_text(text, encoding="utf-8", newline="\n")
print(f"Installed {len(messages)} complete menu translation blocks")
