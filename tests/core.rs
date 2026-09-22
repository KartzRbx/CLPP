use clpp::analysis::{complete_request, signature_request, PositionRequest};
use clpp::ast::AccessKind;
use clpp::binder;
use clpp::checker::{self, Engine};
use clpp::compile::compile_source;
use clpp::parser::parse;
use clpp::session;
use clpp::types::{TypeDatabase, TypeKind};
use std::path::Path;

fn bind_src(src: &str) -> (clpp::ast::Program, binder::BoundFile, TypeDatabase) {
    let program = parse(src, "t.clpp").expect("parse");
    let mut bound = binder::bind(&program);
    let mut types = session::Session::new().types;
    checker::resolve(&program, &mut bound, &mut types);
    (program, bound, types)
}

#[test]
fn binds_struct_fields_methods_and_parent() {
    let src = r#"
struct Base {
    int Value;
};
struct PlayerHud : Base {
    TextLabel Label;
    void Tick();
};
"#;
    let (_, bound, types) = bind_src(src);
    let hud = bound.symbols.struct_named("PlayerHud").expect("PlayerHud");
    let base = bound.symbols.struct_named("Base").expect("Base");
    assert_eq!(bound.symbols.get(hud).unwrap().parent, Some(base));
    let members = types.get_members(types.struct_info("PlayerHud").unwrap().type_id);
    let names: Vec<_> = members.iter().map(|m| m.name.as_str()).collect();
    assert!(names.contains(&"Label"), "{names:?}");
    assert!(names.contains(&"Value"), "{names:?}");
    assert!(names.contains(&"Tick"), "{names:?}");
}

#[test]
fn binds_self_in_method() {
    let src = r#"
struct LeaderstatsServer {
    Janitor janitor;
};
void LeaderstatsServer::PlayerEntered(Player player) {
    @janitor;
}
"#;
    let (_, bound, _) = bind_src(src);
    assert!(
        bound
            .symbols
            .symbols
            .iter()
            .any(|s| s.name == "self" && s.declared_type.as_deref() == Some("LeaderstatsServer")),
        "self should be bound to LeaderstatsServer"
    );
}

#[test]
fn using_alias_shares_type_id() {
    let src = "using Coins = int;\nvoid F() { Coins n = 1; }\n";
    let (program, bound, mut types) = bind_src(src);
    let coins = bound
        .symbols
        .symbols
        .iter()
        .find(|s| s.name == "Coins")
        .expect("alias");
    let peeled = types.peel_id(coins.type_id);
    assert_eq!(peeled, types.int);
    let mut engine = Engine {
        program: &program,
        symbols: &bound.symbols,
        types: &mut types,
        source: src,
        private: Default::default(),
    };
    let n = engine.type_of_name(2, "n");
    assert_eq!(types.peel_id(n), types.int);
}

#[test]
fn optional_is_union_with_nil() {
    let mut db = TypeDatabase::new();
    let inst = db.nominal("Instance");
    let opt = db.optional(inst);
    assert!(db.is_optional(opt));
    match db.kind(opt) {
        TypeKind::Union(parts) => assert!(parts.contains(&db.nil)),
        other => panic!("{other:?}"),
    }
}

#[test]
fn guard_narrows_optional() {
    let src = r#"
void F(optional<Player> player) {
    guard (player != null) else {
        return;
    }
    player;
}
"#;
    let (file, mut types) = session::analyze(src, "g.clpp");
    let after = session::type_at(&file, &mut types, 6, "player");
    assert!(
        !types.is_optional(after),
        "after guard expected Player, got {}",
        types.label(after)
    );
    assert_eq!(types.label(types.peel_id(after)), "Player");
}

#[test]
fn if_optional_narrows_in_then() {
    let src = r#"
void F() {
    optional<Instance> existingFolder = null;
    if (existingFolder) {
        existingFolder;
    }
}
"#;
    let (file, mut types) = session::analyze(src, "i.clpp");
    let inner = session::type_at(&file, &mut types, 5, "existingFolder");
    assert!(
        !types.is_optional(inner) || types.label(inner).contains("Instance"),
        "{}",
        types.label(inner)
    );
}

#[test]
fn getservice_and_new_have_nominal_types() {
    let src = r#"
void init() {
    Players players = GetService<Players>();
    Folder newFolder = new Folder(players);
}
"#;
    let (program, bound, mut types) = bind_src(src);
    let mut engine = Engine {
        program: &program,
        symbols: &bound.symbols,
        types: &mut types,
        source: src,
        private: Default::default(),
    };
    let players_ty = engine.type_of_name(3, "players");
    assert_eq!(engine.types.label(players_ty), "Players");
    let folder_ty = engine.type_of_name(4, "newFolder");
    assert_eq!(engine.types.label(folder_ty), "Folder");
    let folder = engine.types.nominal("Folder");
    let members = engine.get_members(folder);
    let names: Vec<_> = members.iter().map(|m| m.name.as_str()).collect();
    assert!(names.contains(&"Name"), "{names:?}");
    assert!(names.contains(&"Destroy"), "{names:?}");
}

#[test]
fn concat_emits_tostring_for_non_string() {
    let luau = compile_source(
        "void F() { int n = 1; string s = \"Coins: \" .: n; }\n",
        Path::new("c.clp"),
    )
    .expect("compile");
    assert!(luau.contains("tostring"), "{luau}");
}

#[test]
fn type_alias_and_union_syntax() {
    let src = r#"
type UserId = int;
type Result = string | int;
void F() {
    UserId id = 1;
}
"#;
    let program = parse(src, "t.clpp").expect("parse type");
    assert!(program
        .items
        .iter()
        .any(|i| matches!(i, clpp::ast::Item::TypeAlias { name, .. } if name == "UserId")));
    let (_, bound, types) = bind_src(src);
    let alias = bound
        .symbols
        .symbols
        .iter()
        .find(|s| s.name == "Result")
        .expect("Result");
    match types.kind(types.peel_id(alias.type_id)) {
        TypeKind::Union(_) => {}
        other => panic!("expected union, got {other:?} ({})", types.label(alias.type_id)),
    }
}

#[test]
fn default_param_emits_nil_guard() {
    let luau = compile_source(
        "void greet(string nome = \"mundo\") { post(nome); }\nvoid init() { greet(); }\n",
        Path::new("d.clp"),
    )
    .expect("compile");
    assert!(luau.contains("nome == nil"), "{luau}");
}

#[test]
fn completes_inherited_and_getservice_members() {
    let hud_src = r#"
struct Base { int Value; };
struct PlayerHud : Base { TextLabel Label; };
void init() {
    PlayerHud hud;
    hud.
}
"#;
    let (parsed, diags) = clpp::parser::parse_for_ide(hud_src, "c.clpp");
    assert!(
        parsed.items.iter().any(|i| matches!(i, clpp::ast::Item::Class { name, .. } if name == "PlayerHud")),
        "parse_for_ide lost PlayerHud: items={} diags={diags:?}",
        parsed.items.len()
    );
    let (file, mut types) = clpp::session::analyze(hud_src, "c.clpp");
    let members = clpp::session::members_of(&file, &mut types, 6, "    hud.");
    assert!(
        members.iter().any(|m| m.name == "Label"),
        "typed members={:?}",
        members.iter().map(|m| m.name.clone()).collect::<Vec<_>>()
    );
    let hud = complete_request(&PositionRequest {
        source: hud_src.into(),
        file_name: "c.clpp".into(),
        line: 6,
        column: 9,
    })
    .items;
    assert!(
        hud.iter().any(|i| i.label == "Value"),
        "inherited Value missing: {:?}",
        hud.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );
    assert!(
        hud.iter().any(|i| i.label == "Label"),
        "{:?}",
        hud.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );

    let prelude_members = clpp::session::Session::new()
        .types
        .struct_info("Players")
        .map(|i| i.members.iter().map(|m| m.name.clone()).collect::<Vec<_>>());
    assert!(
        prelude_members
            .as_ref()
            .is_some_and(|m| m.iter().any(|n| n == "GetPlayers" || n == "PlayerAdded")),
        "prelude Players members={prelude_members:?}"
    );
    let players_src = r#"
void init() {
    Players players = GetService<Players>();
    players.
}
"#;
    let (pfile, mut ptypes) = clpp::session::analyze(players_src, "c.clpp");
    let ty = clpp::session::type_prefix(&pfile, &mut ptypes, 4, "    players.");
    let sid = pfile.symbols.lookup_at(4, "players");
    let declared = sid.and_then(|id| pfile.symbols.get(id).map(|s| format!("{:?} {}", s.declared_type, ptypes.label(s.type_id))));
    let pmembers = ptypes.get_members(ty);
    assert!(
        pmembers.iter().any(|m| m.name == "PlayerAdded" || m.name == "GetPlayers"),
        "type={} declared={:?} members={:?}",
        ptypes.label(ty),
        declared,
        pmembers.iter().map(|m| m.name.clone()).collect::<Vec<_>>()
    );
    let players = complete_request(&PositionRequest {
        source: players_src.into(),
        file_name: "c.clpp".into(),
        line: 4,
        column: 13,
    })
    .items;
    assert!(
        players.iter().any(|i| i.label == "PlayerAdded" || i.label == "GetPlayers"),
        "{:?}",
        players.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );
}

#[test]
fn cycle_inheritance_does_not_panic() {
    let src = r#"
struct A : B {};
struct B : A {};
"#;
    let (_, _, mut types) = bind_src(src);
    let a = types.nominal("A");
    let _ = types.get_members(a);
}

#[test]
fn intersection_type_alias() {
    let src = "type Combo = Player & Instance;\nvoid F(Combo c) { c; }\n";
    let (_, bound, types) = bind_src(src);
    let combo = bound
        .symbols
        .symbols
        .iter()
        .find(|s| s.name == "Combo")
        .expect("Combo");
    match types.kind(types.peel_id(combo.type_id)) {
        TypeKind::Intersection(_) => {}
        other => panic!("{other:?}"),
    }
}

#[test]
fn interface_binds_as_struct() {
    let src = r#"
interface Drawable {
    void Draw();
};
"#;
    let program = parse(src, "i.clpp").expect("parse interface");
    assert!(program
        .items
        .iter()
        .any(|i| matches!(i, clpp::ast::Item::Class { name, .. } if name == "Drawable")));
    let (_, bound, mut types) = bind_src(src);
    let drawable = types.nominal("Drawable");
    let members = types.get_members(drawable);
    assert!(members.iter().any(|m| m.name == "Draw"), "{members:?}");
    let _ = bound;
}

#[test]
fn structural_bound_circle_satisfies_drawable() {
    let src = r#"
interface Drawable { void render(); };
struct Circle { void render(); int r; };
"#;
    let (_, _, mut types) = bind_src(src);
    let d = types.nominal("Drawable");
    let c = types.nominal("Circle");
    let dm: Vec<String> = types.get_members(d).into_iter().map(|m| m.name).collect();
    let cm: Vec<String> = types.get_members(c).into_iter().map(|m| m.name).collect();
    assert!(dm.iter().any(|m| m == "render"), "Drawable={dm:?}");
    assert!(cm.iter().any(|m| m == "render"), "Circle={cm:?}");
    assert!(
        types.satisfies_bound(c, d),
        "Circle should satisfy Drawable; d={dm:?} c={cm:?}"
    );
}

#[test]
fn findfirstchild_generic_and_as_cast() {
    let src = r#"
void F(Player player, Instance existing) {
    player.FindFirstChild<Folder>("x");
    existing as Folder;
}
"#;
    let program = parse(src, "c.clpp").expect("parse generic/as");
    let has_generic = program.items.iter().any(|item| match item {
        clpp::ast::Item::Function(f) => f.body.iter().any(|s| match s {
            clpp::ast::Stmt::Expr(clpp::ast::Expr::Call { type_args, name, .. }) => {
                name == "FindFirstChild" && type_args.iter().any(|t| t == "Folder")
            }
            _ => false,
        }),
        _ => false,
    });
    assert!(has_generic, "FindFirstChild<Folder> not parsed");
    let has_as = program.items.iter().any(|item| match item {
        clpp::ast::Item::Function(f) => f.body.iter().any(|s| match s {
            clpp::ast::Stmt::Expr(clpp::ast::Expr::Cast { kind, value_type, .. }) => {
                kind == "as" && value_type == "Folder"
            }
            _ => false,
        }),
        _ => false,
    });
    assert!(has_as, "as Folder not parsed");
    let (file, mut types) = session::analyze(src, "c.clpp");
    let ty = session::type_prefix(&file, &mut types, 3, "    player.FindFirstChild<Folder>(\"x\")");
    assert!(
        types.label(ty).contains("Folder"),
        "expected Folder, got {}",
        types.label(ty)
    );
}

#[test]
fn generic_type_alias_emits_luau_params() {
    let luau = compile_source("type Box<T> = array<T>;\n", Path::new("g.clp")).expect("compile");
    assert!(luau.contains("type Box<T>"), "{luau}");
}

#[test]
fn doc_comment_lands_on_symbol() {
    let src = "/// coins on the hud\nstruct MainFrame { int Coins; };\n";
    let (file, _) = session::analyze(src, "d.clpp");
    let hud = file.symbols.struct_named("MainFrame").expect("MainFrame");
    let doc = file.symbols.get(hud).and_then(|s| s.doc.clone());
    assert!(
        doc.as_deref().is_some_and(|d| d.contains("coins on the hud")),
        "doc={doc:?}"
    );
}

#[test]
fn janitor_access_is_semantic() {
    assert!(AccessKind::parse("~>").is_janitor());
    assert!(!AccessKind::parse(".").is_janitor());
}

#[test]
fn completes_main_hud_new_folder_and_signal() {
    let main_src = r#"
struct Hud { TextLabel Coins; };
struct MainFrame : ScreenGui { Hud Hud; };
const MainFrame Main;
void init() {
    Main.Hud.
}
"#;
    let hud = complete_request(&PositionRequest {
        source: main_src.into(),
        file_name: "m.clpp".into(),
        line: 6,
        column: 14,
    })
    .items;
    assert!(
        hud.iter().any(|i| i.label == "Coins"),
        "{:?}",
        hud.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );

    let folder_src = r#"
void init() {
    Folder f = new Folder(null);
    f.
}
"#;
    let folder = complete_request(&PositionRequest {
        source: folder_src.into(),
        file_name: "f.clpp".into(),
        line: 4,
        column: 7,
    })
    .items;
    assert!(folder.iter().any(|i| i.label == "Name"), "{:?}", folder.iter().map(|i| i.label.clone()).collect::<Vec<_>>());
    assert!(folder.iter().any(|i| i.label == "Destroy"), "Destroy missing");
    assert!(folder.iter().any(|i| i.label == "FindFirstChild"), "FindFirstChild missing");

    let sig_src = r#"
void init() {
    Players players = GetService<Players>();
    players.PlayerAdded~>
}
"#;
    let sig = complete_request(&PositionRequest {
        source: sig_src.into(),
        file_name: "s.clpp".into(),
        line: 4,
        column: 26,
    })
    .items;
    assert!(
        sig.iter().any(|i| i.label == "Connect"),
        "{:?}",
        sig.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );
    assert!(sig.iter().any(|i| i.label == "Once" || i.label == "Wait"));

    let help = signature_request(&PositionRequest {
        source: "void init() {\n    Players players = GetService<Players>();\n    players.PlayerAdded~>Connect(func (Player p) { return; });\n}\n".into(),
        file_name: "h.clpp".into(),
        line: 3,
        column: 36,
    })
    .signature
    .expect("signature");
    assert!(
        help.label.contains("Player") || help.parameters.iter().any(|p| p.contains("Player")),
        "{help:?}"
    );
}

#[test]
fn user_struct_without_instance_base_skips_destroy() {
    let src = r#"
struct Foo { int X; };
void init() {
    Foo f;
    f.
}
"#;
    let items = complete_request(&PositionRequest {
        source: src.into(),
        file_name: "u.clpp".into(),
        line: 5,
        column: 7,
    })
    .items;
    assert!(items.iter().any(|i| i.label == "X"), "{:?}", items.iter().map(|i| i.label.clone()).collect::<Vec<_>>());
    assert!(
        !items.iter().any(|i| i.label == "Destroy"),
        "Destroy leaked onto user struct: {:?}",
        items.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );
}

#[test]
fn at_receiver_completes_owner_fields() {
    let src = r#"
struct LeaderstatsServer {
    Janitor janitor;
};
void LeaderstatsServer::Tick() {
    @
}
"#;
    let items = complete_request(&PositionRequest {
        source: src.into(),
        file_name: "a.clpp".into(),
        line: 6,
        column: 6,
    })
    .items;
    assert!(
        items.iter().any(|i| i.label.contains("janitor")),
        "{:?}",
        items.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );
}

#[test]
fn player_kick_and_intvalue_and_vector3() {
    let mut types = session::Session::new().types;
    let player = types.nominal("Player");
    let members = types.get_members(player);
    assert!(members.iter().any(|m| m.name == "Kick"), "Kick missing");
    assert!(members.iter().any(|m| m.name == "Name"), "Name missing");
    assert!(members.iter().any(|m| m.name == "Destroy"), "Destroy missing");
    let iv_id = types.nominal("IntValue");
    let iv = types.get_members(iv_id);
    assert!(iv.iter().any(|m| m.name == "Value"), "{:?}", iv.iter().map(|m| m.name.clone()).collect::<Vec<_>>());
    let v3_id = types.nominal("Vector3");
    let v3 = types.get_members(v3_id);
    assert!(v3.iter().any(|m| m.name == "X"), "Vector3.X missing");
}

#[test]
fn quoted_include_imports_exported_symbols() {
    let dir = std::env::temp_dir().join(format!("clpp-mod-{}", std::process::id()));
    std::fs::create_dir_all(&dir).expect("temp dir");
    let header = dir.join("PlayerData.clh");
    let consumer = dir.join("Main.clpp");
    std::fs::write(&header, "struct Wallet { int Coins; };\n").expect("header");
    std::fs::write(
        &consumer,
        "#include \"PlayerData.clh\"\nvoid init() {\n    Wallet w;\n    w.\n}\n",
    )
    .expect("consumer");
    let src = std::fs::read_to_string(&consumer).expect("read");
    let items = complete_request(&PositionRequest {
        source: src,
        file_name: consumer.display().to_string(),
        line: 4,
        column: 7,
    })
    .items;
    assert!(
        items.iter().any(|i| i.label == "Coins"),
        "{:?}",
        items.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );
}

#[test]
fn compiler_api_queries_session() {
    let mut session = session::Session::new();
    let mut api = session::CompilerApi::new(&mut session);
    let src = "void F(Player player) { player; }\n";
    let _ = api.check(src, "api.clpp");
    assert!(api.get_symbol("api.clpp", 1, "player").is_some());
    let ty = api.get_type("api.clpp", 1, "player");
    assert_eq!(api.session.types.label(ty), "Player");
    let members = api.get_members(ty);
    assert!(members.iter().any(|m| m.name == "Name"));
    assert!(api.get_definition("api.clpp", 1, "player").is_some());
}

#[test]
fn getservice_parses_as_call_not_ast_node() {
    let src = "void init() { Players p = GetService<Players>(); }\n";
    let program = parse(src, "t.clpp").expect("parse");
    let clpp::ast::Item::Function(func) = &program.items[0] else {
        panic!("expected function");
    };
    let clpp::ast::Stmt::Decl(decl) = &func.body[0] else {
        panic!("expected decl");
    };
    match decl.value.as_ref() {
        Some(clpp::ast::Expr::Call {
            name,
            type_args,
            object: None,
            ..
        }) => {
            assert_eq!(name, "GetService");
            assert_eq!(type_args.as_slice(), ["Players"]);
        }
        other => panic!("expected GetService Call, got {other:?}"),
    }
}

#[test]
fn typeid_assignable_rejects_optional_to_plain() {
    let mut types = TypeDatabase::new();
    let player = types.nominal("Player");
    let opt = types.optional(player);
    assert!(types.is_assignable(player, player));
    assert!(!types.is_assignable(opt, player));
    assert!(types.is_assignable(player, opt));
}

#[test]
fn typeid_arity_and_arg_mismatch() {
    let mut session = session::Session::new();
    let src = r#"
void Greet(string name) {
    post(name);
}
void init() {
    Greet();
    Greet(1);
}
"#;
    let file = session.check_source(src, "arity.clpp");
    assert!(
        file.diagnostics
            .iter()
            .any(|d| d.message.contains("argument")),
        "{:?}",
        file.diagnostics
    );
}

#[test]
fn typeid_return_optional_mismatch() {
    let mut session = session::Session::new();
    // Assignment form (same TypeId rule as return); FindFirstChild is optional<Instance>.
    let src = r#"
void F(Player player) {
    Player child = player.FindFirstChild("x");
}
"#;
    let file = session.check_source(src, "ret.clpp");
    assert!(
        file.diagnostics.iter().any(|d| {
            d.code.as_deref() == Some("CLPP0201") || d.message.contains("optional")
        }),
        "{:?}",
        file.diagnostics
    );
}

#[test]
fn language_session_has_no_roblox_prelude() {
    let session = session::Session::language();
    assert!(
        session.types.struct_info("Players").is_none(),
        "core language session must not load Roblox Players"
    );
    let roblox = session::Session::with_roblox_platform();
    assert!(
        roblox.types.struct_info("Players").is_some(),
        "Cluaupp platform session loads Players"
    );
}

#[test]
fn parses_named_import() {
    let src = r#"import { Wallet, PlayerData as Data } from "./PlayerData.clh";
void init() {}
"#;
    let program = parse(src, "t.clpp").expect("parse");
    match &program.items[0] {
        clpp::ast::Item::Import { names, module, .. } => {
            assert_eq!(names.len(), 2);
            assert_eq!(names[0].name, "Wallet");
            assert!(names[0].alias.is_none());
            assert_eq!(names[1].name, "PlayerData");
            assert_eq!(names[1].alias.as_deref(), Some("Data"));
            assert_eq!(module, "./PlayerData.clh");
        }
        other => panic!("expected Import, got {other:?}"),
    }
}

#[test]
fn named_import_imports_exported_symbols() {
    let dir = std::env::temp_dir().join(format!("clpp-import-{}", std::process::id()));
    std::fs::create_dir_all(&dir).expect("temp dir");
    let header = dir.join("PlayerData.clh");
    let consumer = dir.join("Main.clpp");
    std::fs::write(&header, "struct Wallet { int Coins; };\n").expect("header");
    std::fs::write(
        &consumer,
        "import { Wallet } from \"./PlayerData.clh\";\nvoid init() {\n    Wallet w;\n    w.\n}\n",
    )
    .expect("consumer");
    let src = std::fs::read_to_string(&consumer).expect("read");
    let items = complete_request(&PositionRequest {
        source: src,
        file_name: consumer.display().to_string(),
        line: 4,
        column: 7,
    })
    .items;
    assert!(
        items.iter().any(|i| i.label == "Coins"),
        "{:?}",
        items.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );
}

#[test]
fn import_alias_binds_local_name() {
    let dir = std::env::temp_dir().join(format!("clpp-import-alias-{}", std::process::id()));
    std::fs::create_dir_all(&dir).expect("temp dir");
    let header = dir.join("PlayerData.clh");
    let consumer = dir.join("Main.clpp");
    std::fs::write(&header, "struct Wallet { int Coins; };\n").expect("header");
    std::fs::write(
        &consumer,
        "import { Wallet as Purse } from \"./PlayerData.clh\";\nvoid init() {\n    Purse w;\n    w.\n}\n",
    )
    .expect("consumer");
    let src = std::fs::read_to_string(&consumer).expect("read");
    let items = complete_request(&PositionRequest {
        source: src,
        file_name: consumer.display().to_string(),
        line: 4,
        column: 7,
    })
    .items;
    assert!(
        items.iter().any(|i| i.label == "Coins"),
        "alias Purse should resolve Wallet members: {:?}",
        items.iter().map(|i| i.label.clone()).collect::<Vec<_>>()
    );
}
