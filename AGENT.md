# AGENT.md — guide for AI agents working on this repo

Notes for any AI model (or human) picking up work on `mate-multimedia-control`:
what the project is, how it is built and shipped, facts that were expensive to
learn, and a concrete future plan for Wayland. Read this before changing code.

## 1. Project overview

A MATE panel applet (originally by Matias De Lellis, 2014; maintained by
hmtheboy154 since 2026) that controls MPRIS2 media players: play/pause/stop/
next/previous, a "Players" radio submenu to pick the controlled player, album
art in the menu, and a Preferences/About dialog.

- **Stack:** C, GTK3, `libmate-panel-applet` (out-of-process applet, GtkPlug/
  XEmbed), `playerctl` 2.x (GObject MPRIS client), `libsoup3` (album art
  fetch), GSettings, and since 0.2-4: `libx11` + `libxi` (media keys).
- **Layout:** everything lives in `src/` — `main.c` (applet, players, menus,
  dialogs, media keys), `applet.h` (types), `art.c/h` (art download/cache),
  `settings.c/h` (GSettings prefs), `org.mate.panel.applets.multimedia-control.gschema.xml`
  (keys: `show-other-players`, `album-art-now-playing`, `album-art-other-players`).
- **Build:** autotools (`./autogen.sh && ./configure && make`). `PKG_CHECK_MODULES`
  for gtk3, playerctl, libsoup-3.0, and `x11 xi`. Never commit the regenerated
  top-level `INSTALL` (pure autotools churn — `git checkout -- INSTALL`).
- **Packaging:** `packaging/PKGBUILD` (Arch). pkgver 0.2, `pkgrel` bumped on
  every shipped change; `source=` pins an exact commit
  (`#commit=<full sha>`), `b2sums=SKIP`. Regenerate `packaging/.SRCINFO` with
  `makepkg --printsrcinfo > .SRCINFO` after every PKGBUILD change. Build with
  `makepkg -f` in `packaging/`.
- **Git conventions:** commits GPG-signed (`git commit -S`), kernel-style
  subjects (`applet: ...`, `packaging: ...`), trailer
  `Assisted-by:`. Remote `me` =
  `git@github.com:hmtheboy154/mate-multimedia-control.git`, branch `master`.
  Pinentry may time out while waiting for the user's passphrase — just retry
  the same commit command.
- **Rules learned from the user:** never change the dialog icon away from
  `multimedia-player`; never touch the user's dconf/GSettings values; submenu
  labels are `<player> - <Artist — Title>`; menu entries keep stock MATE icons.

## 2. How the applet works (architecture)

- A `PlayerctlPlayerManager` emits `name-appeared` / `player-vanished`;
  each MPRIS bus name becomes a `McPlayerEntry` (GObject) cached in
  `applet->players`, holding its `PlayerctlPlayer` proxy, name, instance,
  status, title, artist, art.
- One entry is `applet->selected` (radio item in the Players submenu). All
  button presses and media keys act on `selected->player`.
- `player_entry_refresh_metadata()` fills title/artist/art; the "metadata"
  and "playback-status" signals from playerctl trigger refreshes.
- The menu is built with the old GtkAction/panel XML scheme (deprecation
  warnings are normal; the panel API is GtkAction-era).
- **Media keys (0.2-4):** `applet_media_keys_take()` sets XI2 passive grabs on
  XF86Audio{Play,Pause,Stop,Next,Previous} (root window, masks
  {0, LockMask, Mod2Mask, LockMask|Mod2Mask}, XIAllMasterDevices), a GDK filter
  (`gdk_window_add_filter(NULL, ...)`) catches the `XI_KeyPress` GenericEvents
  and forwards to `applet->selected` via playerctl. Grabs are process-global
  and refcounted: first applet instance takes, last instance releases
  (`applet_media_key_refs`, `applet_media_key_instances`). `MappingNotify`
  (Keyboard) → release + re-take so keymap changes are survivable. X errors
  around grab/ungrab are swallowed by a temporary tolerant handler + XFlush
  (GDK's default handler would kill the process).

## 3. Hard-won facts (do not relearn these the hard way)

### playerctl 2.4.1
- `g_object_get (player, "metadata", ...)` **aborts the process**: the
  `PROP_METADATA` getter calls `g_error()` on any error
  (playerctl-player.c:421 in the 2.4.1 tarball). Any player whose D-Bus name
  vanished mid-read kills the applet. Use the public GError API instead:
  `playerctl_player_print_metadata_prop (player, "mpris:artUrl", &err)`
  (returns NULL, no error, when the property is simply absent).
- `g_object_get (player, "playback-status", ...)` is safe (cache-only getter,
  no D-Bus call).
- `playerctl_player_get_title/get_artist` are GError-safe.
- The manager's `"player-names"` property returns the internal GList —
  iterate, never free.

### Players
- **Audacious (4.6.2) flickers its MPRIS name** on startup
  (appear → vanish → re-appear). This caused the crash bug and is why
  `applet_add_player()` dedupes by (name, instance). Do not remove that loop.
- Audacious with its Global Hotkeys plugin disabled has **no media-key
  reception at all** (verified in audacious + audacious-plugins 4.6.2 sources;
  `mpris2.so` never touches keys).
- Chromium holds no X media-key grabs while paused (verified empirically);
  on this system the keys go to mate-settings-daemon.

### mate-settings-daemon 1.28 (the media-key routing story)
- `msd-media-keys` does **not** speak MPRIS. It grabs the XF86 keys (core
  XGrabKey) and broadcasts the legacy `MediaPlayerKeyPressed` D-Bus signal
  (org.mate.SettingsDaemon, GrabMediaPlayerKeys protocol, copied from Totem).
- The MPRIS bridge is a **separate msd plugin, `libmpris.so`**
  (source: msd master `plugins/mpris/msd-mpris-manager.c`; NOT in the 1.28.0
  tag — Arch's 1.28.0-3 package ships it anyway). It keeps a `GQueue` of MPRIS
  names, `g_queue_push_head` on name-appeared, and forwards every key press to
  `g_queue_peek_head` → **the newest player always wins the keyboard**. There
  is no public way to reorder its queue. It ignores the signal's application
  field.
- This is why the applet cannot "tell msd" which player to control — it had to
  take the keys itself.

### X11 / XI2 mechanics (all empirically verified on this machine)
- Core `XGrabKey` is exclusive per (window, keycode, modifiers). Grabbing on
  the root window after msd → **BadAccess**, and because X errors arrive
  asynchronously, the error lands after the temporary handler is restored and
  **GDK's fatal handler exits the process**.
- XI2 passive grabs (`XIGrabKeycode`) live in a separate namespace: they
  succeed (ret=0, status=0) even where msd holds core grabs, and an active
  XI2 grab **suppresses core delivery** — verified with a standalone tester
  that received `XI_KeyPress` while msd's libmpris did NOT forward the key.
- API names: `XIGrabKeycode` / `XIUngrabKeycode` (the `XIGrabKey` in
  XInput2.h is XI1-era; do not use). Conflict results come back in
  `XIGrabModifiers.status`, not as async errors. Event mask:
  `XISetMask(bits, XI_KeyPress)` with `XIMaskLen`, in an `XIEventMask`
  struct. `/usr/include/X11/extensions/XInput2.h` has the exact prototypes.
- Prototype blueprint for any XI2 work: it was written as
  `/tmp/opencode/xi2test.c` (grabs, prints per-mask status, waits for
  XI_KeyPress). If /tmp was wiped, recreate from the description in §2 +
  this header knowledge; it is ~100 lines.

### Testing panel applets in a live session (the recipe that works)
- The panel D-Bus-activates `org.mate.panel.applet.MultimediaControlAppletFactory`;
  whoever owns that name gets the panel's instances. To test a dev build:
  `pkill -x multimedia-cont` then immediately launch the dev binary
  (`GSETTINGS_SCHEMA_DIR` pointing at a compiled schema dir), then
  `gdbus call ... GetApplet` on the factory path → returns the instance path
  (usually `/0` for the panel's instance, later calls give `/1`, `/2`, ...).
  Embed a returned instance's XID with a tiny XEmbed socket host
  (`/tmp/opencode/socket-host <XID>`), then interact **with real XTEST
  clicks**, not the D-Bus `PopupMenu` method (that path is flaky: returns
  `()` with no menu; a real button-3 click on the plug always works).
- Killing a socket-host destroys that applet instance (plug dies with the
  socket), which runs the applet's destroy path — remember the grabs are
  process-global (that is why the refcount exists).
- Menus/dialogs move: capture windows by ID from `xwininfo -root -tree`
  (grep for the process name), but `xwd -id <win>` captures a screen REGION —
  if the window died before the capture you photograph whatever is behind it
  (user's windows included). Raise your window first; the user is often
  actively using the machine.
- Tiny helpers used (recreate as needed, all ~30 lines of Xlib/Xtst):
  `xkey <KEYSYM>` (XTEST keypress), `xtick`/`xtick3` (XTEST left/right click
  at x y), `xraise <winid>`, `xi2test` (XI2 grab tester),
  `fake-mpris.py` (minimal MPRIS player `org.mpris.MediaPlayer2.faketest`,
  `--flicker` = unown 400 ms / re-own 1000 ms).
- **`pkill -f` footgun (bit twice in one day):** if the plain pattern string
  appears ANYWHERE else in the same command line (including the launch part
  of the same command), pkill kills your own shell. Either use
  `pkill -f 'name[-]trick'` and keep the plain string out of that command
  entirely, or kill by exact process name (`pkill -x`) / collected PIDs.
- `coredumpctl list` + `coredumpctl gdb <pid>` is the fastest way to a crash
  backtrace; SIGABRT with `g_log`/`g_error` frames usually means a playerctl
  property getter (see above).

## 4. Shipped state

- 0.2-4 (commits `3012711` applet, `f22935c` packaging, pushed to `me/master`):
  crash fix (GError-safe art read + flicker dedupe) and the XI2 media-key
  takeover verified live: chromium selected → only chromium toggles;
  audacious selected → only audacious; both kdeconnect players untouched;
  keys fall back to msd when the applet exits.

## 5. Future plan: Wayland (mate-wayland-session)

### Why this matters
MATE ships an experimental Wayland session (`mate-desktop/mate-wayland-session`,
1.29.1 as of Aug 2026): a **Wayfire**-based session where a script
(`mate-wayland-components.sh`) loops `mate-panel`, `mate-settings-daemon`,
caja while Wayfire runs, and behavior is configured in `~/.config/mate/wayfire.ini`.
msd 1.29.1 runs native Wayland (wlrandr, background, clipboard plugins) —
but its **media-keys/mpris plugins have no Wayland support at all** (verified
against commit history), and on Wayland the compositor owns global key
bindings by design. Wayland forbids client-side global grabs; XI2 grabs
inside XWayland never see compositor-delivered keys. The current XI2 takeover
is X11-only, full stop.

Also relevant: mate-panel has a native Wayland backend (gtk-layer-shell;
tasklist ported to libxfce4windowing), but **out-of-process applets are
XEmbed = X11-only**; MATE 1.28 notes say applets support Wayland "with
improvements for in-process building". This applet is out-of-process, so on a
Wayland panel it only exists under XWayland today.

### Implementation plan (ordered)
1. **Graceful X11 guard (do early, trivial):** in `src/main.c`, make the whole
   media-key block no-op when the GDK display is not X11
   (`!GDK_IS_X11_DISPLAY (gdk_display_get_default ())` → skip take/release and
   never call `gdk_x11_display_get_xdisplay()` with a NULL display). Same for
   any future X11 touch.
2. **Persist the selection:** add a `selected-player` (string, e.g.
   `chromium` or `chromium@instance`) key to the gschema; write it in
   `applet_select_player()` and the vanished-fallback; restore it at startup
   when the player is present. This is the piece a Wayland forwarder needs —
   the selection must outlive any single client and be readable without X.
3. **Forwarder CLI:** a tiny `mate-multimedia-control-key
   <play|pause|stop|next|previous>` binary (new `src/` file, links playerctl
   + glib only, no GTK) that reads the persisted selection from dconf and
   forwards via playerctl. A D-Bus method on the applet is an alternative,
   but the CLI also works when the panel/applet is on another seat of the
   session and is trivially bindable in any compositor.
4. **Wayfire wiring (the "how the user gets the keys" half):** Wayfire binds
   keys to commands via its commands plugin (`binding_<name>` /
   `command_<name>` pairs in `~/.config/mate/wayfire.ini`; the session's
   `mate-wayland.sh` already edits exactly such pairs, e.g. it rebinds
   `command_launcher = mate-panel --run-dialog`). Document — and optionally
   contribute upstream to mate-wayland-session's default wayfire.ini —
   bindings like `binding_media_play = XF86AudioPlay` /
   `command_media_play = mate-multimedia-control-key play` (verify exact
   keysym spelling against Wayfire's xkb binding syntax at implementation
   time). All five keys, one command each.
5. **Watch msd:** if msd ever gains native Wayland media keys (the mutter
   model: compositor → `org.gnome.Mutter.MediaKeys`-style D-Bus), prefer
   plugging the applet into that signal path and only use the CLI as
   fallback. The design goal is that X11 (XI2 grabs) and Wayland (compositor
   binding → CLI → persisted selection) converge on the same "forward to
   selected player" code.
6. **In-process applet question (prerequisite research):** for the applet to
   appear on a native Wayland panel it must be loadable there — either build
   the factory as in-process as well (`MATE_PANEL_APPLET_IN_PROCESS_FACTORY`)
   or confirm how mate-panel 1.29 handles out-of-process applets under
   Wayland. Decide before promising Wayland UI support; the media-key plan
   above works regardless of which way this goes.
