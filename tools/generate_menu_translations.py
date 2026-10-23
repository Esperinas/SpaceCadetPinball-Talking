#!/usr/bin/env python3
import json
import re
import sys
import urllib.parse
import urllib.request
from pathlib import Path

root = Path(__file__).resolve().parents[1]

items = [
    ("Menu1_WindowUniformScale", "Uniform Scaling", "Uniform window scaling"),
    ("Menu1_Table_Resolution", "Table Resolution", "Pinball table resolution"),
    ("Menu1_ToggleShowMenu", "Show Menu", "Show menu"),
    ("Menu1_UseMaxResolution_640x480", "Use Maximum Resolution (640 x 480)", "Use maximum resolution (640 x 480)"),
    ("Menu1_UseMaxResolution_800x600", "Use Maximum Resolution (800 x 600)", "Use maximum resolution (800 x 600)"),
    ("Menu1_UseMaxResolution_1024x768", "Use Maximum Resolution (1024 x 768)", "Use maximum resolution (1024 x 768)"),
    ("Menu3_Tilt", "Tilt", "Pinball table tilt"),
    ("Menu3_TiltHard", "Tilt hard", "Hard pinball table tilt"),
    ("Menu3_TiltSoft", "Tilt soft", "Soft pinball table tilt"),
    ("Menu3_NoTilt", "No Tilt", "Disable pinball table tilt"),
    ("Menu3_CenterPostDuration", "Center Post Duration", "Center safety post duration"),
    ("Menu3_CenterPostOriginal", "Original (60 s)", "Original duration (60 s)"),
    ("Menu3_Language", "Language", "Language"),
    ("Menu3_Audio", "Audio", "Audio"),
    ("Menu3_StereoSoundEffects", "Stereo Sound Effects", "Stereo sound effects"),
    ("Menu3_SoundVolume", "Sound Volume", "Sound effects volume"),
    ("Menu3_SoundChannels", "Sound Channels", "Sound channels"),
    ("Menu3_MusicVolume", "Music Volume", "Music volume"),
    ("Menu3_Graphics", "Graphics", "Graphics settings"),
    ("Menu3_LinearFiltering", "Linear Filtering", "Linear filtering"),
    ("Menu3_EnhancedScaling", "Enhanced Scaling (3.0)", "Enhanced scaling (3.0)"),
    ("Menu3_IntegerScaling", "Integer Scaling", "Integer scaling"),
    ("Menu3_UIScale", "UI Scale", "User interface scale"),
    ("Menu3_GameData", "Game Data", "Game data"),
    ("Menu3_Prefer3DPBData", "Prefer 3DPB Data", "Prefer 3DPB game data"),
    ("Menu3_ResetAllOptions", "Reset All Options", "Reset all options"),
    ("Menu3_SetDefaultUpsFps", "Set Default UPS/FPS", "Set default UPS and FPS"),
    ("Menu3_UncappedUps", "Uncapped UPS (FPS ratio %02.02f)", "Uncapped UPS (FPS ratio __FLOAT__)"),
    ("Menu3_PreciseSleep", "Precise Sleep", "Precise sleep timing"),
    ("Menu3_HideCursor", "Hide Cursor", "Hide mouse cursor"),
    ("Menu3_ChangeFont", "Change Font...", "Change font..."),
    ("Menu3_ImGuiDemo", "ImGui Demo", "ImGui demo"),
    ("Menu3_SpriteViewer", "Sprite Viewer", "Sprite viewer"),
    ("Menu3_DebugOverlay", "Debug Overlay", "Debug overlay"),
    ("Menu3_CheatsCode", "Cheats code", "Cheat codes"),
]

languages = [
    ("Arabic", "ar"), ("Czech", "cs"), ("Danish", "da"), ("German", "de"),
    ("Greek", "el"), ("English", None), ("Spanish", "es"), ("Finnish", "fi"),
    ("French", "fr"), ("Hebrew", "he"), ("Hungarian", "hu"), ("Italian", "it"),
    ("Japanese", "ja"), ("Korean", "ko"), ("Norwegian", "nb"), ("Dutch", "nl"),
    ("Polish", "pl"), ("BrazilianPortuguese", "pt-BR"), ("Portuguese", "pt-PT"),
    ("Russian", "ru"), ("Swedish", "sv"), ("Turkish", "tr"),
    ("SimplifiedChinese", "zh-CN"), ("TraditionalChinese", "zh-TW"),
]

existing = {
    "Russian": ["Однородное масштабирование", "Разрешение стола", "Показать меню", "Использовать максимальное разрешение (640 x 480)", "Использовать максимальное разрешение (800 x 600)", "Использовать максимальное разрешение (1024 x 768)", "Наклон", "Сильный наклон", "Мягкий наклон", "Без наклона", "Длительность центрального штифта", "Оригинал (60 с)", "Язык", "Аудио", "Стереоэффекты", "Громкость звука", "Звуковые каналы", "Громкость музыки", "Графика", "Линейная фильтрация", "Улучшенное масштабирование (3.0)", "Целочисленное масштабирование", "Масштаб интерфейса", "Данные игры", "Предпочитать данные 3DPB", "Сбросить все настройки", "Стандартные UPS/FPS", "UPS без ограничения (отношение FPS %02.02f)", "Точный режим ожидания", "Скрыть курсор", "Изменить шрифт...", "Демонстрация ImGui", "Просмотр спрайтов", "Отладочный слой", "Чит-коды"],
    "Japanese": ["均一スケーリング", "テーブル解像度", "メニューを表示", "最大解像度を使用 (640 x 480)", "最大解像度を使用 (800 x 600)", "最大解像度を使用 (1024 x 768)", "台揺らし", "強い台揺らし", "弱い台揺らし", "台揺らしなし", "中央ポストの時間", "オリジナル (60秒)", "言語", "オーディオ", "ステレオ効果音", "効果音の音量", "サウンドチャンネル", "音楽の音量", "グラフィック", "線形フィルタリング", "拡張スケーリング (3.0)", "整数スケーリング", "UIスケール", "ゲームデータ", "3DPBデータを優先", "全設定をリセット", "標準UPS/FPSに戻す", "UPS無制限 (FPS比 %02.02f)", "高精度スリープ", "カーソルを隠す", "フォントを変更...", "ImGuiデモ", "スプライト表示", "デバッグ表示", "チートコード"],
    "SimplifiedChinese": ["统一缩放", "球台分辨率", "显示菜单", "使用最大分辨率 (640 x 480)", "使用最大分辨率 (800 x 600)", "使用最大分辨率 (1024 x 768)", "倾斜", "强倾斜", "轻倾斜", "无倾斜", "中央挡柱持续时间", "原始设置（60秒）", "语言", "音频", "立体声音效", "音效音量", "声音通道", "音乐音量", "图形", "线性过滤", "增强缩放 (3.0)", "整数缩放", "界面缩放", "游戏数据", "优先使用3DPB数据", "重置所有选项", "恢复默认UPS/FPS", "无限制UPS（FPS比例 %02.02f）", "精确休眠", "隐藏光标", "更改字体...", "ImGui演示", "精灵查看器", "调试叠加层", "作弊码"],
}

def translate(code):
    query = "\n".join(prompt for _, _, prompt in items)
    url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=en&tl=" + code + "&dt=t&q=" + urllib.parse.quote(query)
    data = json.loads(urllib.request.urlopen(url, timeout=60).read().decode("utf-8"))
    text = "".join(part[0] for part in data[0])
    values = text.splitlines()
    if len(values) != len(items):
        raise RuntimeError(f"{code}: expected {len(items)} translations, got {len(values)}")
    return [value.replace("__FLOAT__", "%02.02f") for value in values]

translations = {"English": [english for _, english, _ in items]}
for language, code in languages:
    if language == "English":
        continue
    translations[language] = existing.get(language) or translate(code)

# "Tilt" is the established pinball term, not a request to incline the table.
# Keep the four gameplay mode names identical in every language.
universal_tilt_labels = {
    "Menu3_Tilt": "Tilt",
    "Menu3_TiltHard": "Tilt hard",
    "Menu3_TiltSoft": "Tilt soft",
    "Menu3_NoTilt": "No Tilt",
}
for language, _ in languages:
    for index, (msg, _, _) in enumerate(items):
        if msg in universal_tilt_labels:
            translations[language][index] = universal_tilt_labels[msg]

# Shape the two right-to-left scripts for Dear ImGui, which has no BiDi/shaping engine.
sys.path.insert(0, "/tmp/alpha10-pydeps")
import arabic_reshaper
from bidi.algorithm import get_display
translations["Arabic"] = [get_display(arabic_reshaper.reshape(value)) for value in translations["Arabic"]]
translations["Hebrew"] = [get_display(value) for value in translations["Hebrew"]]

def cpp(value):
    return value.replace("\\", "\\\\").replace('"', '\\"')

out = root / "SpaceCadetPinball/MenuTranslations.inc"
with out.open("w", encoding="utf-8", newline="\n") as target:
    target.write("// Generated menu translations for all supported languages.\n")
    for index, (msg, _, _) in enumerate(items):
        target.write(f"\t{{\n\t\tMsg::{msg},\n\t\t{{\n")
        for language, _ in languages:
            target.write(f'\t\t\t{{ Lang::{language}, "{cpp(translations[language][index])}" }},\n')
        target.write("\t\t},\n\t},\n")
print(f"Generated {len(items)} messages x {len(languages)} languages in {out}")
