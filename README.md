# SaikoroPsycho-ENX
`Saikoro Psycho - Seventh Heaven -` (サイコロサイコ -セブンスヘブン-) English translation mod for Nintendo Switch

This translation mod is based on PC mods from `Tico Translations` and `ijedi1234` that you can find here:<br>
https://tico-tl.tumblr.com/post/650393064565800960/dice-psycho-aka-saikoro-saiko <br>
https://ijedi1234.itch.io/dice-psycho-english-patch

# Installation

Mod is compatible only with version 1.0.0 of game.

Emulators are not supported by me, but it doesn't mean mod won't work with them.

1. Download SaikoroPsychoENX.zip from Releases, unpack it.
2. Copy `atmosphere` folder to the root of your sdcard, accepting any popup about overwriting folders
3. Run game

# Making mod manually

Requirements:
- Python 3.10+ (and library: `lzss`)

For plugin compilation you need:
- devkitpro (with `switch-dev` package installed)

To compile plugin, just run `make` inside plugin folder.<br>
To compile script run `sn/Prepare.cmd` on Windows or in bash opened in root of `sn` folder enter lines:
```cmd
python3 InjectTexts.py 
python3 sn_assembler_re.py Patched
```

Copy `0100AEF01A264000` folder from `sn` folder to 
```
atmosphere/contents/
```
Copy `subsdk9` and `main.npdm` from `plugin/deploy` folder to
```
atmosphere/contents/0100AEF01A264000/exefs
```

Assets are available only from Release page.

# Notes
- Why plugin is necessary:
    - Replacing XTX textures with PNG on the fly
    - Changed Clear Password from Japanese text to English text by also blocking swkbd to use only ASCII characters
    - Translated hardcoded chapter names, notifications, Playing Log texts, settings descriptions, scenario endings names
- Ported translation has some slight changes to better fit into how text is rendered in Regista engine. Fixed also multiple typos from original mod.
- On top of porting original fantranslations I have also:
	- put back uncensored CGs
	- Restored most of removed/altered texts affected by censorship
    - redone almost all scenario textures
    - translated textures unique to Switch release
    - replaced ascii glyphs in original fonts with glyphs rendered with Noto Sans Mono ExtraCondensed Regular and Bold
- Scenario files were altered to not use precalculated animations for endings names and TRUE END animated texts, instead using message text rendering. Reason is because there was not enough informations about how to decode animation table included with each file representing one text.

# Thanks to
- `Tico Translations` for making translation mod, this mod is mainly based on this translation (not directly involved with this project)
- `ijedi1234` for translating omake and for giving few ideas about how to solve some issues that existed in original translation (not directly involved with this project)
