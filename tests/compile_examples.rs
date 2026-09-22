use clpp::compile_file;
use std::path::PathBuf;

fn example(rel: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("examples").join(rel)
}

#[test]
fn compile_syntax_features() {
    let luau = compile_file(&example("syntax/features.clp")).expect("compile syntax");
    assert!(luau.contains("local n: number = 100"));
    assert!(luau.contains("local speed: number = 16.5"));
    assert!(luau.contains("local name: string = \"Kartz\""));
    assert!(luau.contains("function Wallet:Add"));
    assert!(luau.contains("self.coins = self.coins + n"));
    assert!(luau.contains("pocket:Add(10)"));
    assert!(luau.contains("task.spawn(function()"));
}

#[test]
fn compile_config() {
    let luau = compile_file(&example("shared/config.clp")).expect("compile config");
    assert!(luau.contains("const STARTING_COINS"));
    assert!(luau.contains("return {"));
}

#[test]
fn compile_player_data() {
    let luau = compile_file(&example("shared/PlayerData.clh")).expect("compile PlayerData");
    assert!(luau.contains("const function PlayerData()"));
    assert!(luau.contains("Coins = 0"));
}

#[test]
fn compile_named_import_consumer() {
    let luau = compile_file(&example("shared/use_player_data.clp")).expect("compile import");
    assert!(luau.contains("require") || luau.contains("Wallet") || luau.contains("Coins"));
}
