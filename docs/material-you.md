# Material You style

The client ships with a Material 3 (Material You) colour scheme taken from the
Telegram Desktop fork **materialgram** (`kukuruzka165/materialgram`, GPLv3 with
OpenSSL exception — the same license as this repository), which is the most
complete Material Design 3 port of tdesktop. The rest of the Material You
Telegram projects are Android-only (Mercurygram, TelegramYou, MoeYou) or are
phones-first designs, so their palettes cannot be applied to a Qt desktop
client; materialgram is the only direct equivalent.

## What was taken

### Icons

materialgram's Material Symbols icon set is now the icon set of this client:
`Telegram/Resources/icons` was replaced file by file with the fork's version.
Of 2753 icon files 2253 differ and were replaced, 470 were already identical,
and the 74 icons that only this fork has (fork-specific features such as
`bubble_tail`, `chat/reactions_round_big`, `chat/reactions_bubble_shadow`) were
kept. The 30 files that exist only in materialgram belong to a different
tdesktop revision and were not imported.

### Colour scheme

The two Material 3 tonal palettes of materialgram are now the default palettes
of this client:

| Resource | Role here | Source |
| --- | --- | --- |
| `Telegram/Resources/day-custom-base.tdesktop-theme` | default **light** palette (also the base for custom themes) | materialgram `Google Day.tdesktop-theme` |
| `Telegram/Resources/night-custom-base.tdesktop-theme` | default **dark** base palette | materialgram `Google Dark.tdesktop-theme` |
| `Telegram/Resources/night.tdesktop-theme` | **night mode** palette | materialgram `Google Dark.tdesktop-theme` |

Both palettes keep materialgram's Seed colour (warm neutral surfaces with an
olive primary: `#57651a` in light, `#b9cf7a` in dark) and their flat Material
wallpapers. Colours that materialgram's themes do not define — per-peer gradient
avatars (`historyPeerNUserpicBg2`), outline buttons and the statistics chart —
were generated from the same palette so that every palette key this fork uses is
covered.

The remaining built-in themes (`Classic`, `Day`, `Night Green`) are untouched:
they are still selectable and are the escape hatch if the Material palette is
not wanted.

### Shapes

Material 3 corner radii and touch targets, where this fork owns the style file:

| Element | Before | After |
| --- | --- | --- |
| Welcome screen "next" button (`introNextButton`) | 6 px radius, 42 px high | **24 px (pill), 48 px high** |
| Welcome screen QR plate (`introQrBackgroundRadius`) | 8 px | **16 px** |
| "Search in chat" popup menu (`dialogsSearchInMenu`) | 8 px | **12 px** |
| Collectible copy button (`collectibleCopy`) | 6 px, 36 px high | **20 px (pill), 40 px high** |

### Welcome (start) screen

`Intro::Widget::paintEvent` no longer fills the window with one flat colour: it
paints a vertical tonal gradient from `windowBg` to `windowBgOver`, the Material
3 "surface → surface container" transition. Combined with the new palette this
gives the Material look on the very first screen, before login.

## Fonts

The Material You design is set in **Google Sans**. The font itself is
proprietary (Google does not license redistribution), so it is **not** stored in
this repository. The client instead picks it up automatically:

1. If the user has *Google Sans*, *Google Sans Text*, *Google Sans Display* or
   *Product Sans* installed in the system, `Core::Application::run()` selects it
   with `style::SetCustomFont()` before the style fonts are started. Nothing has
   to be configured - install the font, restart the client.
2. A family chosen manually in **Settings → Chat settings → Font** always wins
   over the automatic choice.
3. Any font file placed in `Telegram/Resources/fonts/` and added to
   `Telegram/Resources/qrc/telegram/telegram.qrc` under the `fonts` prefix is
   registered at startup through `:/fonts` (`RegisterBundledFonts()`), so a
   bundled family can be shipped later without touching the code.

See `Telegram/Resources/fonts/README.md` for the step-by-step.

## What was **not** taken

- **Widget geometry owned by `lib_ui`** (search field, popup menus, chat bubbles,
  flat buttons): those metrics live in the `Telegram/lib_ui` submodule, which is
  shared with upstream tdesktop, so they cannot be restyled from this repository
  without forking `lib_ui`.

## Enabling / disabling

Material You is the default: it is applied on first start and in night mode. To
go back to the classic Telegram look open **Settings → Chat settings → Chat
theme** and pick *Classic* (light) or *Night Green* (dark), or import any
`.tdesktop-theme` of your own.
