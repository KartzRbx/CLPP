use clpp::compile_file;
use pretty_assertions::assert_eq;
use std::path::PathBuf;

fn example(rel: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("examples").join(rel)
}

fn golden(rel: &str) -> String {
    std::fs::read_to_string(
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("tests")
            .join("golden")
            .join(rel),
    )
    .unwrap_or_else(|err| panic!("missing golden {rel}: {err}"))
}

fn normalize(s: &str) -> String {
    s.replace("\r\n", "\n").trim().to_string()
}

#[test]
fn compile_hello() {
    let luau = compile_file(&example("hello/hello.server.clpp")).expect("compile hello");
    assert_eq!(normalize(&luau), normalize(&golden("hello.server.luau")));
}

#[test]
fn compile_syntax_features() {
    let luau = compile_file(&example("syntax/features.clp")).expect("compile syntax");
    assert!(luau.contains("local n: number = 100"));
    assert!(luau.contains("local speed: number = 16.5"));
    assert!(luau.contains("local name: string = \"Kartz\""));
    assert!(luau.contains("local isActive: boolean = true"));
    assert!(luau.contains("local callback"));
    assert!(luau.contains("function()"));
    assert!(luau.contains("local playerRef = nil"));
    assert!(luau.contains("{ \"Kartz\", \"Player1\" }"));
    assert!(luau.contains("Coins = 100"));
    assert!(luau.contains("Gems = 50"));
    assert!(luau.contains("elseif coins == 0 then"));
    assert!(luau.contains("print(\"Enough balance!\")"));
    assert!(luau.contains("warn(\"No coins!\")"));
    assert!(luau.contains("error(\"Balance sync error.\")"));
    assert!(luau.contains("while i < 10 do"));
    assert!(luau.contains("i += 1"));
    assert!(luau.contains("n += 1"));
    assert!(luau.contains("Instance.new(\"IntValue\")"));
    assert!(luau.contains("wallet.Changed:Connect"));
    assert!(luau.contains("local function __signal()"));
    assert!(luau.contains("local function __await(value)"));
    assert!(luau.contains("__janitor:Add(OnCoinsUpdated:Connect"));
    assert!(luau.contains("__janitor:Add(OnCoinsUpdated:Once"));
    assert!(luau.contains("OnCoinsUpdated:Fire"));
    assert!(luau.contains("GetPropertyChangedSignal(\"Name\")"));
    assert!(luau.contains("if not (coins ~= nil) then"));
    assert!(luau.contains("local success, result = pcall"));
    assert!(luau.contains("task.spawn(function()"));
    assert!(luau.contains("task.wait(2)"));
    assert!(luau.contains("task.desynchronize()"));
    assert!(luau.contains("task.synchronize()"));
    assert!(luau.contains("typeof(__match1) == \"string\""));
    assert!(luau.contains("__await(DataService.Server:WaitFor(player))"));
    assert!(luau.contains("RunService\"):IsServer()"));
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
fn compile_leaderstats() {
    let luau = compile_file(&example("leaderstats/LeaderstatsServer.server.clpp"))
        .expect("compile leaderstats");
    assert_eq!(
        normalize(&luau),
        normalize(&golden("LeaderstatsServer.server.luau"))
    );
}
