# Bundled fonts

The client follows the Material You design, which is set in **Google Sans**.
Google Sans is proprietary and is not redistributed in this repository, so this
folder is empty by default.

## Use Google Sans that is already installed

Nothing to do. On start, `Core::Application::run()` checks the system font
database for these families and uses the first one it finds:

1. `Google Sans Text`
2. `Google Sans`
3. `Google Sans Display`
4. `Product Sans`

Install the font files into Windows (right click → *Install for all users*) or
into `~/.local/share/fonts` on other systems, then restart the client.

A font picked manually in **Settings → Chat settings → Font** always overrides
the automatic choice.

## Bundle a font with the client

1. Put the font files here, for example `GoogleSans-Regular.ttf` and
   `GoogleSans-Bold.ttf`.
2. Register them in `Telegram/Resources/qrc/telegram/telegram.qrc` inside the
   `/fonts` prefix:

```xml
  <qresource prefix="/fonts">
    <file alias="GoogleSans-Regular.ttf">../../fonts/GoogleSans-Regular.ttf</file>
    <file alias="GoogleSans-Bold.ttf">../../fonts/GoogleSans-Bold.ttf</file>
  </qresource>
```

3. Rebuild. `RegisterBundledFonts()` registers everything under `:/fonts` before
   the style fonts are created, so the family then shows up in the font list and
   is selected automatically.

Make sure you are allowed to redistribute whatever you put here: Google Sans is
not licensed for redistribution, while Roboto (Apache License 2.0) and
Noto Sans (SIL Open Font License) are. Roboto is the closest freely licensed
substitute if Google Sans cannot be shipped.
