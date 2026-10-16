use zed_extension_api::{self as zed, settings::LspSettings};

struct Clpp;

impl zed::Extension for Clpp {
    fn new() -> Self { Self }

    fn language_server_command(
        &mut self,
        id: &zed::LanguageServerId,
        worktree: &zed::Worktree,
    ) -> zed::Result<zed::Command> {
        let binary = LspSettings::for_worktree(id.as_ref(), worktree)?.binary;
        let command = binary.as_ref().and_then(|b| b.path.clone())
            .or_else(|| worktree.which("clpp-vm"))
            .ok_or("CL++: configure lsp.clpp.binary.path with the path to clpp-vm.exe, or add clpp-vm to PATH")?;
        let mut env = worktree.shell_env();
        if let Some(extra) = binary.as_ref().and_then(|b| b.env.as_ref()) {
            env.extend(extra.iter().map(|(key, value)| (key.clone(), value.clone())));
        }
        let args = binary.and_then(|b| b.arguments).unwrap_or_else(|| vec!["--lsp".into()]);
        Ok(zed::Command { command, args, env })
    }
}

zed::register_extension!(Clpp);

