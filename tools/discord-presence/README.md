# CL++ Discord Rich Presence

This is optional editor integration, separate from the compiler. No user token,
password or Client Secret is needed. The presence uses the editor application's
existing public ID, so the activity title is Zed or Visual Studio Code; CL++ is
shown in its details. It shares the workspace name and file basename with Discord.

## Zed (local Windows package)

The small Rust adapter registers an additional server only for CL++ buffers. It
uses [xhyrom's Discord Presence LSP](https://github.com/xhyrom/zed-discord-presence)
with its documented default application ID. The CL++ compiler remains enabled.
The presence executable is upstream GPL-3.0 software; its source and license are
available in the linked project. The adapter does not contain that server's code.

Build the adapter with `cargo build --manifest-path tools/discord-presence/zed/Cargo.toml --release --target wasm32-wasip2`.
Download the Windows x64 server from its official release and verify its GitHub
asset SHA-256 digest before installation. Then run:

```text
node tools/discord-presence/install-local.cjs --server <discord-presence-lsp.exe> --wasm <clpp_discord_presence_zed.wasm>
```

The installer backs up and preserves existing JSONC settings. It adds
the disabled `!clpp_discord_presence` alongside `clpp` by default. Use `--enable`
to activate after authorizing workspace/file sharing with Discord. It keeps the compiler unchanged and clears
the activity after five minutes idle. It uses the Zed image; a separately branded
CL++ application/image would require your own public Application ID and assets.

## VS Code and Cursor

Use the existing [Discord Presence extension](https://marketplace.visualstudio.com/items?itemName=icrawl.discord-vscode).
The repository's `.vscode/settings.json` sets CL++ project details and the file
basename; `discord.enabled` controls activation. The author approved sharing
workspace/file names and activity time, so this development workspace enables
it. Remote repository links are disabled. These workspace settings also work in Cursor with that extension
installed. Other projects keep their own presence details. Avoid enabling two
presence extensions at once in the same editor.

Open a `.clp` buffer with the desktop Discord client running. Discord's activity
sharing setting must be enabled. Configuration and local protocol tests do not
prove that a public profile displayed the activity; verify that separately.
The Windows `probe.cjs` sends a one-shot activity with CL++ / CLPP_Rework / main.clp
and verifies Discord's RPC acknowledgement, then clears the test activity.
Run this only after authorizing those names and activity time for Discord.

### CL++ image in VS Code

iCrawl 5.9.2 has no CL++ entry in its image table and no configurable image-key
setting. The local `vscode-icon.cjs` adapter adds only that mapping, using the
public CL++ logo URL. It retains the editor's application ID/title, backs up the
original bundle, checks the installed version and is idempotent. It does not add
a second presence connection or change the other language images.

```text
node tools/discord-presence/vscode-icon.cjs <installed icrawl.discord-vscode-5.9.2 folder>
```

Reload the editor window after installation. An upstream update can replace this
local compatibility patch; rerun only against its supported version, or update
the adapter after reviewing the new source. The original bundle is retained as
`dist/extension.cjs.clpp-original`; hashes are recorded alongside it. The
`--vscode-icon` option of `probe.cjs` verifies RPC acceptance with that public
image URL and then clears its own test activity. Acceptance is not visual proof
that an already-running editor host has reloaded the mapping.
