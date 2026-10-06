# Checklist — notes for contributors and coding agents

A Pebble watchapp (C, in `src/c/`) with a phone-side PebbleKit JS component (`src/pkjs/`) and a config page (`config-page/`) that is baked into `src/pkjs/configDataUri.js` at build time.

## Build

```bash
node buildConfigPage.js   # ALWAYS run first if config-page/* changed — bakes the config page into src/pkjs/configDataUri.js
pebble build              # builds all target platforms into build/PebbleChecklist.pbw
```

## Run & drive in the emulator

```bash
pebble install --emulator basalt          # boots QEMU + pypkjs and launches the app
                                          # (first call after cold boot may fail with "Connection refused" — just retry)
pebble screenshot --emulator basalt out.png
pebble emu-button --emulator basalt click down|up|select|back   # -n N -i MS to repeat
```

## Inject data through the real AppMessage path

App UUID: `b938082c-c230-4b8f-847d-15b27b1f907e` (needed for `send-app-message` when not run from repo root).

```bash
# add an item (key 0 = KEY_ITEMS_TO_ADD, one item per message)
pebble send-app-message --emulator basalt --app-uuid b938082c-c230-4b8f-847d-15b27b1f907e --string 0=Milk
# replace whole list (key 3 = KEY_ITEM_UPDATES, JSON; '3=[]' empties the list)
pebble send-app-message --emulator basalt --app-uuid ... --string '3=[{"n":"Milk","c":1}]'
# settings bitfield (key 4 = KEY_SETTINGS): 1=show voice btn, 2=move checked to bottom, 4=wrap-around scroll, 8=larger font, 16=voice input on quick launch
pebble send-app-message --emulator basalt --app-uuid ... --int 4=7
```

## List limits

The list goes both ways as the same compact JSON (`[{"n":"Milk","c":1}]`): the watch sends it as key 2 (`KEY_CURRENT_STATE`, along with key 5 `KEY_TOTAL_ITEMS`) and the phone sends it back as key 3.

- At most 52 items (`MAX_CHECKLIST_ITEMS`), each name at most 89 UTF-8 bytes (`MAX_NAME_LENGTH - 1`).
- The JSON can be at most 4000 bytes in either direction (`LIST_JSON_MAX_BYTES`, sized to fit the 4096-byte AppMessage buffers). The watch sends only whole items that fit, and key 5 lets the config page warn when items were left out.
- The config page mirrors these limits as constants at the top of `config-page/config.js`, so change both sides together.

## Config page

Full pipeline test (watch state → pkjs → final data URI) without opening a browser:

```bash
BROWSER="/path/to/capture.sh %s" pebble emu-app-config --emulator basalt   # capture.sh writes "$1" to a file
# the captured file:// wrapper contains the final data URI; extract with regex /data:text\/html[^"<>\s]*/
# (do NOT stop the match at single quotes — the URI contains raw ' characters)
```

To eyeball the page standalone: decode `src/pkjs/configDataUri.js`, replace `__CURRENT_STATE__` / `__CURRENT_SETTINGS__` *before* decoding (values are percent-decoded along with the page), write to an .html file, and screenshot with headless Chrome. Use a wide window (e.g. 900px) — headless doesn't apply the meta viewport, so narrow windows crop the right edge (not a real bug; phone webviews render fine).

## Cleanup

```bash
pebble kill               # shuts down emulator QEMU/pypkjs
rm -f ~/pebble-tool-emu-app-config-*.html   # temp wrapper left by emu-app-config
```
