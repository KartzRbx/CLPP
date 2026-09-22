//! Name tables for globals, Instance classes, and Luau type mapping.
//! Used by emit, analysis, and the checker pass — not a second type system.

pub const INSTANCE_TYPES: &[&str] = &[
    "Instance", "Folder", "Part", "MeshPart", "BasePart", "UnionOperation", "CornerWedgePart",
    "WedgePart", "TrussPart", "Model", "WorldModel", "Actor", "Player", "Players", "Terrain",
    "SpawnLocation",
    "IntValue", "NumberValue", "StringValue", "BoolValue", "ObjectValue", "CFrameValue",
    "Vector3Value", "Color3Value", "BrickColorValue", "RayValue", "DoubleConstrainedValue",
    "IntConstrainedValue",
    "Humanoid", "HumanoidDescription", "Accessory", "Accoutrement", "Shirt", "Pants",
    "ShirtGraphic", "BodyColors", "Animation", "Animator", "AnimationTrack",
    "AnimationController", "Pose", "Keyframe", "KeyframeSequence",
    "RemoteEvent", "RemoteFunction", "BindableEvent", "BindableFunction", "UnreliableRemoteEvent",
    "ScreenGui", "SurfaceGui", "BillboardGui", "GuiMain", "Frame", "TextLabel", "TextButton",
    "TextBox", "ImageLabel", "ImageButton", "ScrollingFrame", "VideoFrame", "ViewportFrame",
    "CanvasGroup", "UIListLayout", "UIGridLayout", "UIPadding", "UICorner", "UIStroke",
    "UIGradient", "UIScale", "UIAspectRatioConstraint", "UISizeConstraint",
    "UITextSizeConstraint", "UIPageLayout", "UIFlexItem",
    "ProximityPrompt", "ClickDetector", "Seat", "VehicleSeat", "Tool", "Highlight",
    "SelectionBox", "SelectionSphere", "Attachment", "Bone", "Weld", "WeldConstraint", "Motor",
    "Motor6D", "RopeConstraint", "RodConstraint", "SpringConstraint", "AlignPosition",
    "AlignOrientation", "LinearVelocity", "AngularVelocity", "VectorForce", "Torque",
    "NoCollisionConstraint", "UniversalConstraint", "BallSocketConstraint", "HingeConstraint",
    "Sound", "SoundGroup", "SoundEffect", "EqualizerSoundEffect", "ReverbSoundEffect",
    "DistortionSoundEffect", "PitchShiftSoundEffect", "ChorusSoundEffect", "FlangeSoundEffect",
    "EchoSoundEffect", "CompressorSoundEffect", "Camera", "ParticleEmitter", "Beam", "Trail",
    "Fire", "Smoke", "Sparkles", "PointLight", "SpotLight", "SurfaceLight", "Atmosphere", "Sky",
    "Clouds", "BloomEffect", "BlurEffect", "ColorCorrectionEffect", "DepthOfFieldEffect",
    "SunRaysEffect",
    "Workspace", "Lighting", "ReplicatedStorage", "ServerStorage", "ServerScriptService",
    "StarterGui", "StarterPack", "StarterPlayer", "StarterPlayerScripts",
    "StarterCharacterScripts", "RunService", "UserInputService", "ContextActionService",
    "TweenService", "Debris", "HttpService", "DataStoreService", "MemoryStoreService",
    "BadgeService", "MarketplaceService", "TeleportService", "PolicyService", "TextService",
    "GroupService", "MessagingService", "Teams", "SoundService", "LocalizationService",
    "GuiService", "VRService", "PathfindingService", "AssetService", "AvatarEditorService",
    "VoiceChatService", "SocialService", "ProximityPromptService", "CollectionService",
    "PhysicsService",
    "LocalScript", "ModuleScript", "Script",
];

const DATATYPES: &[&str] = &[
    "Vector2", "Vector2int16", "Vector3", "Vector3int16", "CFrame", "Matrix3",
    "UDim", "UDim2", "Rect", "Region3", "Region3int16", "Faces", "Axes",
    "Color3", "ColorSequence", "ColorSequenceKeypoint", "BrickColor",
    "NumberRange", "NumberSequence", "NumberSequenceKeypoint",
    "Ray", "RaycastParams", "RaycastResult", "OverlapParams", "PhysicalProperties",
    "TweenInfo", "Font", "CatalogSearchParams", "FloatCurveKey", "RotationCurveKey",
    "Enum", "EnumItem", "Enums", "Random", "DateTime", "PathWaypoint",
    "DockWidgetPluginGuiInfo", "SharedTable", "RBXScriptSignal", "RBXScriptConnection",
];

const METHODS: &[&str] = &[
    "FindFirstChild", "FindFirstChildOfClass", "FindFirstChildWhichIsA", "WaitForChild",
    "GetChildren", "GetDescendants", "GetPlayers", "GetPlayerFromCharacter",
    "GetCharacterFromPlayer", "GetService", "IsA", "IsDescendantOf", "Clone", "Destroy",
    "ClearAllChildren", "GetAttribute", "SetAttribute", "GetAttributes",
    "GetAttributeChangedSignal", "GetPropertyChangedSignal", "GetFullName", "PivotTo",
    "GetPivot", "SetPrimaryPartCFrame",
    "Connect", "Once", "Disconnect", "Wait", "Fire", "FireServer", "FireClient",
    "FireAllClients", "Invoke", "InvokeServer", "BindToClose",
    "Kick", "LoadCharacter", "MoveTo", "ApplyDescription", "GetAppliedDescription",
    "Add", "Cleanup", "LinkToInstance", "WaitFor", "GetChangedSignal", "Init", "Get", "Set",
    "Update", "Remove", "OnChange", "AndThen", "Catch", "Finally", "Expect",
];

const LIBRARY_TYPES: &[&str] = &[
    "Janitor", "Maid", "Promise", "Signal", "Net", "DataService", "DataServiceServer",
    "DataServiceClient", "Data", "Fusion", "Vide", "Roact", "React", "Cmdr", "Spring", "FormatNumber",
    "MathUtils", "Module3D", "Zap", "Flamework",
];

const GLOBALS: &[&str] = &[
    "post", "warn", "report", "print", "error", "game", "workspace", "script",
    "cout", "cerr", "endl", "tick", "time", "task", "typeof", "type", "tonumber", "tostring",
    "to_string", "to_number", "to_bool",
    "pcall", "xpcall", "select", "pairs", "ipairs", "next", "unpack", "rawget", "rawset",
    "setmetatable", "getmetatable", "assert", "require",
];

const LUAU_LIBS: &[&str] = &[
    "task", "table", "string", "math", "coroutine", "utf8", "bit32", "os", "debug",
    "buffer", "vector", "net",
];

fn is_pascal_case(s: &str) -> bool {
    let mut chars = s.chars();
    let Some(first) = chars.next() else {
        return false;
    };
    if !first.is_uppercase() {
        return false;
    }
    let rest: Vec<char> = chars.collect();
    if rest.is_empty() {
        return true;
    }
    if rest
        .iter()
        .all(|c| c.is_uppercase() || c.is_ascii_digit() || *c == '_')
    {
        return false;
    }
    rest.iter().any(|c| c.is_lowercase())
}

pub fn is_datatype(name: &str) -> bool {
    DATATYPES.contains(&name)
}

pub fn is_instance_type(name: &str) -> bool {
    INSTANCE_TYPES.contains(&name)
        || (is_pascal_case(name) && !is_datatype(name) && !is_library_type(name))
}

pub fn is_method(name: &str) -> bool {
    METHODS.contains(&name)
}

pub fn is_library_type(name: &str) -> bool {
    LIBRARY_TYPES.contains(&name)
}

pub fn is_luau_lib(name: &str) -> bool {
    LUAU_LIBS.contains(&name)
}

pub fn is_global(name: &str) -> bool {
    GLOBALS.contains(&name)
}

pub fn is_bare_global(name: &str) -> bool {
    is_global(name)
        || is_library_type(name)
        || is_instance_type(name)
        || is_datatype(name)
        || is_luau_lib(name)
}

pub fn is_signal_type(cpp_type: &str) -> bool {
    let cleaned = cpp_type.trim().trim_end_matches('*').trim();
    cleaned == "signal"
        || cleaned.starts_with("signal<")
        || cleaned.starts_with("Signal<")
}

pub fn observable_class(cpp_type: &str) -> &'static str {
    let cleaned = cpp_type
        .trim()
        .trim_end_matches('*')
        .trim()
        .trim_start_matches("const ");
    match cleaned {
        "int" => "IntValue",
        "float" | "double" => "NumberValue",
        "string" => "StringValue",
        "bool" => "BoolValue",
        "Vector3" => "Vector3Value",
        "CFrame" => "CFrameValue",
        "Color3" => "Color3Value",
        _ => "ObjectValue",
    }
}

pub fn is_array_type(cpp_type: &str) -> bool {
    cpp_type.contains("LuaArray")
        || cpp_type.contains("vector<")
        || cpp_type.contains("array<")
        || cpp_type.contains("span<")
        || cpp_type.contains("dictionary<")
        || cpp_type.contains("map<")
}

pub fn lib_from_include(path: &str) -> Option<String> {
    let normalized = path.replace('\\', "/").to_lowercase();
    if normalized.contains("clpp/libs.clh") || normalized.ends_with("libs.clh") {
        return Some("*".into());
    }
    if let Some(rest) = normalized.split("clpp/libs/").nth(1) {
        let stem = rest.split(['.', '/']).next().unwrap_or("");
        return match stem {
            "janitor" => Some("Janitor".into()),
            "sweep" => Some("Sweep".into()),
            "maid" => Some("Maid".into()),
            "dataservice" | "dataservicev2" => Some("DataService".into()),
            "keep" => Some("Keep".into()),
            "promise" => Some("Promise".into()),
            "net" => Some("Net".into()),
            "fusion" => Some("Fusion".into()),
            "gleam" => Some("Gleam".into()),
            "zap" => Some("Zap".into()),
            "flare" => Some("Flare".into()),
            "signal" => Some("Signal".into()),
            "spark" => Some("Spark".into()),
            "mint" => Some("Mint".into()),
            "axiom" => Some("Axiom".into()),
            "roster" => Some("Roster".into()),
            "bloom" => Some("Bloom".into()),
            "lens" => Some("Lens".into()),
            "crest" => Some("Crest".into()),
            "pin" => Some("Pin".into()),
            "stage" => Some("Stage".into()),
            "coil" => Some("Coil".into()),
            "helm" => Some("Helm".into()),
            "shift" => Some("Shift".into()),
            "hive" => Some("Hive".into()),
            "ember" => Some("Ember".into()),
            "echo" => Some("Echo".into()),
            "guide" => Some("Guide".into()),
            "trace" => Some("Trace".into()),
            _ => None,
        };
    }
    None
}

pub fn luau_type(name: Option<&str>) -> Option<String> {
    let name = name?;
    let cleaned = name
        .trim()
        .trim_end_matches('*')
        .trim()
        .trim_start_matches("const ")
        .replace("Enum::", "Enum.");
    match cleaned.as_str() {
        "void" => Some("()".into()),
        "int" | "float" | "double" => Some("number".into()),
        "bool" => Some("boolean".into()),
        "string" => Some("string".into()),
        "func" => Some("(...any) -> any".into()),
        "auto" => None,
        other if other == "signal"
            || other.starts_with("signal<")
            || other.starts_with("Signal<") =>
        {
            Some("RBXScriptSignal".into())
        }
        other if other.starts_with("LuaArray<")
            || other.starts_with("vector<")
            || other.starts_with("array<")
            || other.starts_with("span<") =>
        {
            let inner = other
                .split_once('<')
                .and_then(|(_, rest)| rest.rsplit_once('>'))
                .map(|(inner, _)| inner.trim())
                .unwrap_or("any");
            let inner_type = luau_type(Some(inner)).unwrap_or_else(|| inner.to_string());
            Some(format!("{{{inner_type}}}"))
        }
        other if other.starts_with("dictionary<") || other.starts_with("map<") => {
            let inner = other
                .split_once('<')
                .and_then(|(_, rest)| rest.rsplit_once('>'))
                .map(|(inner, _)| inner.trim())
                .unwrap_or("any, any");
            let mut parts = inner.splitn(2, ',');
            let key = parts.next().unwrap_or("any").trim();
            let value = parts.next().unwrap_or("any").trim();
            let key_type = luau_type(Some(key)).unwrap_or_else(|| key.to_string());
            let value_type = luau_type(Some(value)).unwrap_or_else(|| value.to_string());
            Some(format!("{{ [{key_type}]: {value_type} }}"))
        }
        other if other.starts_with("optional<") => {
            let inner = other
                .split_once('<')
                .and_then(|(_, rest)| rest.rsplit_once('>'))
                .map(|(inner, _)| inner.trim())
                .unwrap_or("any");
            let inner_type = luau_type(Some(inner)).unwrap_or_else(|| inner.to_string());
            Some(format!("{inner_type}?"))
        }
        other => Some(other.to_string()),
    }
}
