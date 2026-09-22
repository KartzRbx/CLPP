//! Roblox API as a bound prelude (generated headers + extra instance stubs).

use crate::binder::{self, BoundFile};
use crate::checker;
use crate::parser::{parse, parse_for_ide};
use crate::types::TypeDatabase;

const GENERATED: &str = include_str!("../../stdlib/clpp/generated/instances.clh");

const EXTRA: &str = r#"
struct Workspace : Instance {};
struct ReplicatedStorage : Instance {};
struct ServerStorage : Instance {};
struct ServerScriptService : Instance {};
struct Lighting : Instance {};
struct SoundService : Instance {};
struct TweenService : Instance {};
struct UserInputService : Instance {};
struct RunService : Instance {};
struct HttpService : Instance {};
struct DataStoreService : Instance {};
struct TeleportService : Instance {};
struct CollectionService : Instance {};
struct StarterGui : Instance {};
struct StarterPlayer : Instance {};
struct Teams : Instance {};
struct MarketplaceService : Instance {};
struct ScreenGui : Instance {};
struct Frame : Instance {};
struct TextLabel : Instance {
    string Text;
};
struct TextButton : Instance {
    string Text;
};
struct TextBox : Instance {
    string Text;
};
struct ImageLabel : Instance {};
struct ScrollingFrame : Instance {};
struct PlayerGui : Instance {};
struct LocalScript : Instance {};
struct ModuleScript : Instance {};
struct Script : Instance {};
struct Camera : Instance {};
struct Tool : Instance {};
struct Part : BasePart {};
struct MeshPart : BasePart {};
struct RemoteEvent : Instance {
    signal<any> OnServerEvent;
    signal<any> OnClientEvent;
};
struct RemoteFunction : Instance {};
struct BindableEvent : Instance {};
struct Janitor {
    void Add(any object, string methodName, string key);
    void Remove(string key);
    void Cleanup();
    void Destroy();
};
"#;

pub fn load_prelude(types: &mut TypeDatabase) -> BoundFile {
    let source = format!("{GENERATED}\n{EXTRA}\n");
    let program = match parse(&source, "roblox-prelude.clh") {
        Ok(p) if !p.items.is_empty() => p,
        Ok(_) | Err(_) => parse_for_ide(&source, "roblox-prelude.clh").0,
    };
    let mut bound = binder::bind(&program);
    checker::resolve(&program, &mut bound, types);
    enrich(types);
    seed_instance_surface(types);
    bound
}

fn enrich(types: &mut TypeDatabase) {
    let instance = types.nominal("Instance");
    types.define_struct("Players", vec![instance]);
    types.set_bases("Players", vec![instance]);
    types.define_struct("Player", vec![instance]);
    types.set_bases("Player", vec![instance]);
    let player = types.nominal("Player");
    let sig = types.signal(vec![player]);
    let array_player = types.array(player);
    types.add_member(
        "Players",
        crate::types::StructMember {
            name: "GetPlayers".into(),
            type_id: array_player,
            kind: crate::types::MemberKind::Method,
            params: Vec::new(),
            is_static: false,
            doc: None,
        },
    );
    types.add_member(
        "Players",
        crate::types::StructMember {
            name: "PlayerAdded".into(),
            type_id: sig,
            kind: crate::types::MemberKind::Field,
            params: Vec::new(),
            is_static: false,
            doc: None,
        },
    );
    let player_gui = types.nominal("PlayerGui");
    let instance = types.nominal("Instance");
    types.define_struct("PlayerGui", vec![instance]);
    types.add_member(
        "Player",
        crate::types::StructMember {
            name: "PlayerGui".into(),
            type_id: player_gui,
            kind: crate::types::MemberKind::Field,
            params: Vec::new(),
            is_static: false,
            doc: None,
        },
    );
    types.add_member(
        "Players",
        crate::types::StructMember {
            name: "LocalPlayer".into(),
            type_id: player,
            kind: crate::types::MemberKind::Field,
            params: Vec::new(),
            is_static: false,
            doc: None,
        },
    );
    let sig = types.signal(vec![player]);
    types.add_member(
        "Players",
        crate::types::StructMember {
            name: "PlayerRemoving".into(),
            type_id: sig,
            kind: crate::types::MemberKind::Field,
            params: Vec::new(),
            is_static: false,
            doc: None,
        },
    );
    let void = types.void;
    let string = types.string;
    let int = types.int;
    let opt_string = types.optional(string);
    types.add_member(
        "Player",
        crate::types::StructMember {
            name: "Kick".into(),
            type_id: void,
            kind: crate::types::MemberKind::Method,
            params: vec![("message".into(), opt_string)],
            is_static: false,
            doc: None,
        },
    );
    let instance = types.nominal("Instance");
    types.define_struct("IntValue", vec![instance]);
    types.set_bases("IntValue", vec![instance]);
    types.add_member(
        "IntValue",
        crate::types::StructMember {
            name: "Value".into(),
            type_id: int,
            kind: crate::types::MemberKind::Field,
            params: Vec::new(),
            is_static: false,
            doc: None,
        },
    );
    seed_datatypes(types);
    for name in crate::names::INSTANCE_TYPES {
        if !types.has_struct(name) {
            let instance = types.nominal("Instance");
            types.define_struct(name, vec![instance]);
        }
    }
}

fn seed_instance_surface(types: &mut TypeDatabase) {
    let instance = types.define_struct("Instance", Vec::new());
    let string = types.string;
    let boolean = types.boolean;
    let opt_inst = types.optional(instance);
    let array_inst = types.array(instance);
    let fields = [
        ("Name", string, crate::types::MemberKind::Field),
        ("ClassName", string, crate::types::MemberKind::Field),
        ("Parent", instance, crate::types::MemberKind::Field),
    ];
    for (name, ty, kind) in fields {
        types.add_member(
            "Instance",
            crate::types::StructMember {
                name: name.into(),
                type_id: ty,
                kind,
                params: Vec::new(),
                is_static: false,
                doc: None,
            },
        );
    }
    let methods = [
        ("FindFirstChild", opt_inst, vec![("name".into(), string)]),
        ("FindFirstChildOfClass", opt_inst, vec![("className".into(), string)]),
        ("FindFirstChildWhichIsA", opt_inst, vec![("className".into(), string)]),
        ("WaitForChild", instance, vec![("name".into(), string)]),
        ("GetChildren", array_inst, vec![]),
        ("GetDescendants", array_inst, vec![]),
        ("IsA", boolean, vec![("className".into(), string)]),
        ("Clone", instance, vec![]),
        ("Destroy", types.void, vec![]),
        ("ClearAllChildren", types.void, vec![]),
    ];
    for (name, ret, params) in methods {
        types.add_member(
            "Instance",
            crate::types::StructMember {
                name: name.into(),
                type_id: ret,
                kind: crate::types::MemberKind::Method,
                params,
                is_static: false,
                doc: None,
            },
        );
    }
}

fn seed_datatypes(types: &mut TypeDatabase) {
    let float = types.float;
    let string = types.string;
    for name in ["Vector3", "Vector2", "CFrame", "UDim2", "UDim", "Color3", "BrickColor", "Rect"] {
        types.define_struct(name, Vec::new());
    }
    let vector3 = types.nominal("Vector3");
    let udim = types.nominal("UDim");
    let fields = [
        ("Vector3", "X", float),
        ("Vector3", "Y", float),
        ("Vector3", "Z", float),
        ("Vector2", "X", float),
        ("Vector2", "Y", float),
        ("CFrame", "Position", vector3),
        ("Color3", "R", float),
        ("Color3", "G", float),
        ("Color3", "B", float),
        ("UDim2", "X", udim),
        ("UDim2", "Y", udim),
        ("BrickColor", "Name", string),
    ];
    for (owner, field, ty) in fields {
        types.add_member(
            owner,
            crate::types::StructMember {
                name: field.into(),
                type_id: ty,
                kind: crate::types::MemberKind::Field,
                params: Vec::new(),
                is_static: false,
                doc: None,
            },
        );
    }
}

pub fn is_service(name: &str) -> bool {
    matches!(
        name,
        "Players"
            | "Workspace"
            | "Lighting"
            | "ReplicatedStorage"
            | "ServerStorage"
            | "ServerScriptService"
            | "RunService"
            | "UserInputService"
            | "TweenService"
            | "HttpService"
            | "DataStoreService"
            | "TeleportService"
            | "CollectionService"
            | "SoundService"
            | "StarterGui"
            | "StarterPlayer"
            | "Teams"
            | "MarketplaceService"
            | "Chat"
            | "Debris"
            | "TextService"
            | "PathfindingService"
            | "PhysicsService"
    )
}
