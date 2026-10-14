use zed_extension_api::{self as zed, settings::LspSettings};

struct Presence;

impl zed::Extension for Presence {
    fn new() -> Self { Self }

    fn language_server_command(
        &mut self,
        id: &zed::LanguageServerId,
        worktree: &zed::Worktree,
    ) -> zed::Result<zed::Command> {
        let binary = LspSettings::for_worktree(id.as_ref(), worktree)?.binary;
        let command = binary.as_ref().and_then(|b| b.path.clone())
            .ok_or("Run tools/discord-presence/install-local.cjs to configure the presence server")?;
        Ok(zed::Command {
            command,
            args: binary.and_then(|b| b.arguments).unwrap_or_default(),
            env: worktree.shell_env(),
        })
    }
}

zed::register_extension!(Presence);
