//! Fails if a host-runtime name lands in a new compiler crate.
//!
//! The list is a checked-in slice of the Roblox API, not a live dump. Every
//! crate under `crates/` is scanned except the Pest-era stubs that still
//! mention a host on purpose. A new crate is included automatically.

use std::fs;
use std::path::Path;

const OLD_CRATES: &[&str] = &[
    "clpp_syntax",
    "clpp_parser",
    "clpp_hir",
    "clpp_ty",
    "clpp_codegen",
    "clpp_lsp",
    "clpp_cli",
];

/// Distinctive API names. Matched on identifier boundaries so `Player` does
/// not fire inside `PlayerData`. Bare `server` / `client` are omitted: the
/// front end talks about a language server, and role tags are diagnosed as
/// stems rather than spelled as API types.
const NEEDLES: &[&str] = &[
    "roblox",
    "rojo",
    "janitor",
    "getservice",
    "rbxscriptsignal",
    "rbxscriptconnection",
    "vector2",
    "vector3",
    "cframe",
    "color3",
    "udim",
    "udim2",
    "brickcolor",
    "replicatedstorage",
    "replicatedfirst",
    "serverstorage",
    "serverscriptservice",
    "startergui",
    "starterpack",
    "starterplayer",
    "localscript",
    "modulescript",
    "script",
    "datamodel",
    "instance",
    "workspace",
    "players",
    "player",
    "humanoid",
    "basepart",
    "runservice",
    "tweenservice",
    "userinputservice",
    "httpservice",
    "datastoreservice",
    "messagingservice",
    "teleportservice",
    "soundservice",
    "remoteevent",
    "remotefunction",
    "bindableevent",
    "bindablefunction",
    "textlabel",
    "textbutton",
    "textbox",
    "screengui",
    "billboardgui",
    "surfacegui",
    "frame",
    "scrollingframe",
    "meshpart",
    "part",
    "model",
    "folder",
    "camera",
    "humanoidrootpart",
    "animator",
    "animation",
    "proximityprompt",
    "clickdetector",
    "particleemitter",
    "beam",
    "attachment",
    "weldconstraint",
    "motor6d",
    "alignposition",
    "numbervalue",
    "stringvalue",
    "boolvalue",
    "intvalue",
    "objectvalue",
    "raycastparams",
    "overlapparams",
    "physicalproperties",
    "tweeninfo",
    "random",
    "datetime",
    "content",
    "font",
    "ray",
    "region3",
    "catalogsearchparams",
    "plugin",
    "changingservice",
    "collectionservice",
    "contextactionservice",
    "gamepadService",
    "guiservice",
    "hapticservice",
    "insertservice",
    "localizationservice",
    "logservice",
    "marketplaceservice",
    "materialservice",
    "memorystoreservice",
    "notificationservice",
    "pathfindingservice",
    "physicsservice",
    "playerservice",
    "policyservice",
    "proximitypromptservice",
    "selections",
    "socialservice",
    "stats",
    "teams",
    "teleportservice",
    "textservice",
    "textchatservice",
    "timingservice",
    "touchinputservice",
    "ugcservice",
    "usergamesettings",
    "vrsservice",
    "voicechatservice",
    "debris",
    "lighting",
    "materialservice",
    "jointsservice",
    "keyframesequence",
    "keyframe",
    "pose",
    "tool",
    "hopperbin",
    "backpack",
    "startergears",
    "spawnlocation",
    "team",
    "seat",
    "vehicleseat",
    "skateboardplatform",
    "trussPart",
    "wedgepart",
    "cornerwedgepart",
    "terrain",
    "atmosphere",
    "sky",
    "clouds",
    "bloomEffect",
    "blurEffect",
    "colorcorrectioneffect",
    "depthoffieldeffect",
    "sunraysEffect",
    "highlight",
    "selectionbox",
    "decals",
    "decal",
    "texture",
    "specialmesh",
    "blockmesh",
    "cylindermesh",
    "fire",
    "smoke",
    "sparkles",
    "trail",
    "light",
    "pointlight",
    "spotlight",
    "surfacelight",
    "sound",
    "soundgroup",
    "dialog",
    "dialogchoice",
    "handles",
    "arcHandles",
    "selectionbox",
    "surfaceSelection",
    "videoframe",
    "viewportframe",
    "canvasgroup",
    "uipadding",
    "uilistlayout",
    "uigridlayout",
    "uitablelayout",
    "uipagelayout",
    "uiaspectratioconstraint",
    "uisizeconstraint",
    "uitextsizeconstraint",
    "uiscale",
    "uicorner",
    "uigradient",
    "uistroke",
    "uiflexitem",
    "imagelabel",
    "imagebutton",
    "proximityprompt",
];

#[test]
fn new_core_crates_have_no_host_runtime_names() {
    let crates = Path::new(env!("CARGO_MANIFEST_DIR")).join("../..").join("crates");
    let crates = crates.canonicalize().unwrap();
    let mut scanned = Vec::new();
    let mut hits = Vec::new();
    for entry in fs::read_dir(&crates).unwrap() {
        let entry = entry.unwrap();
        if !entry.path().is_dir() {
            continue;
        }
        let name = entry.file_name().to_string_lossy().to_string();
        if OLD_CRATES.contains(&name.as_str()) {
            continue;
        }
        let src = entry.path().join("src");
        if src.is_dir() {
            scanned.push(name);
            walk(&src, &mut hits);
        }
    }
    assert!(
        scanned.iter().any(|name| name == "clpp_front"),
        "the phase-1 crate was not scanned: {scanned:?}"
    );
    assert!(
        hits.is_empty(),
        "host-runtime names in a new core crate:\n{}",
        hits.join("\n")
    );
}

fn walk(dir: &Path, hits: &mut Vec<String>) {
    for entry in fs::read_dir(dir).unwrap() {
        let entry = entry.unwrap();
        let path = entry.path();
        if path.is_dir() {
            walk(&path, hits);
            continue;
        }
        if path.extension().and_then(|e| e.to_str()) != Some("rs") {
            continue;
        }
        let text = fs::read_to_string(&path).unwrap_or_default();
        let lower = text.to_ascii_lowercase();
        for needle in NEEDLES {
            let needle = needle.to_ascii_lowercase();
            if word_boundary(&lower, &needle) {
                hits.push(format!("{}: {needle}", path.display()));
            }
        }
        if lower.contains("task.") {
            hits.push(format!("{}: task.", path.display()));
        }
    }
}

fn word_boundary(text: &str, word: &str) -> bool {
    text.match_indices(word).any(|(i, _)| {
        let before = text[..i].chars().next_back().unwrap_or(' ');
        let after = text[i + word.len()..].chars().next().unwrap_or(' ');
        !before.is_ascii_alphanumeric() && before != '_' && !after.is_ascii_alphanumeric() && after != '_'
    })
}
