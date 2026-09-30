use clpp_front::{
    dump_ast, file_stem, lex, link_binding_name, parse, render, render_codespan, LinkTarget,
    SyntaxKind,
};

fn kinds(source: &str) -> Vec<SyntaxKind> {
    lex(source)
        .tokens
        .into_iter()
        .filter(|t| !t.kind.is_trivia())
        .map(|t| t.kind)
        .collect()
}

fn assert_lossless(source: &str) {
    let parsed = parse("t.clp", source);
    assert_eq!(
        parsed.syntax.text().to_string(),
        source,
        "tree dropped or invented bytes:\n{}",
        dump_ast(&parsed.ast)
    );
}

#[test]
fn lexer_covers_tokens_comments_and_literals() {
    assert_eq!(
        kinds("void init() {}"),
        vec![
            SyntaxKind::KwVoid,
            SyntaxKind::Ident,
            SyntaxKind::LParen,
            SyntaxKind::RParen,
            SyntaxKind::LBrace,
            SyntaxKind::RBrace,
        ]
    );
    assert_eq!(
        kinds("n //= 2; /// doc\n//! bang\n// note"),
        vec![
            SyntaxKind::Ident,
            SyntaxKind::FloorDivEq,
            SyntaxKind::IntLit,
            SyntaxKind::Semi,
        ]
    );
    assert!(kinds("/// keep\n//! keep").is_empty());
    assert_eq!(
        kinds("0xFF 1.5e-2"),
        vec![SyntaxKind::HexLit, SyntaxKind::FloatLit]
    );
    let bad_num = lex("1e 0x");
    assert_eq!(
        bad_num
            .tokens
            .iter()
            .filter(|t| !t.kind.is_trivia())
            .map(|t| t.kind)
            .collect::<Vec<_>>(),
        vec![SyntaxKind::IntLit, SyntaxKind::Ident, SyntaxKind::HexLit]
    );
    assert!(bad_num.diagnostics.len() >= 2);
    assert_eq!(kinds(r#""hi\n\x41\u{1F}"#), vec![SyntaxKind::StringLit]);
    assert_eq!(kinds(r#"R"(a"b)""#), vec![SyntaxKind::RawString]);
    assert_eq!(
        kinds("`a {x} b`"),
        vec![
            SyntaxKind::Backtick,
            SyntaxKind::TemplateText,
            SyntaxKind::LBrace,
            SyntaxKind::Ident,
            SyntaxKind::RBrace,
            SyntaxKind::TemplateText,
            SyntaxKind::Backtick,
        ]
    );
    let open = lex("\"unterminated");
    assert!(open.diagnostics.iter().any(|d| d.code.as_deref() == Some("CLPP1201")));
    let block = lex("/* never ends");
    assert!(block.diagnostics.iter().any(|d| d.code.as_deref() == Some("CLPP1201")));
    assert_eq!(kinds("signal observable spawn"), vec![SyntaxKind::Ident; 3]);
    assert_eq!(kinds("~"), vec![SyntaxKind::Error]);
    assert_eq!(kinds("[[pure]]"), vec![
        SyntaxKind::LBracket,
        SyntaxKind::LBracket,
        SyntaxKind::Ident,
        SyntaxKind::RBracket,
        SyntaxKind::RBracket,
    ]);
}

#[test]
fn lexer_is_lossless_on_garbage() {
    for source in ["", "@@@", "void", "void f(", "/*", "\"", "import x", "#include <a>", "~~~~", "1e", "0x", "a //= b"] {
        let lexed = lex(source);
        let mut at = 0u32;
        for t in &lexed.tokens {
            assert_eq!(t.span.start, at, "{source:?}");
            at = t.span.end;
        }
        assert_eq!(at as usize, source.len(), "{source:?}");
        assert_lossless(source);
    }
}

#[test]
fn link_binding_uses_alias_or_stem() {
    assert_eq!(
        link_binding_name(
            &LinkTarget::Path("./shared/Wallet.clp".into()),
            Some("Purse")
        ),
        "Purse"
    );
    assert_eq!(
        link_binding_name(&LinkTarget::Path("./shared/Wallet.clp".into()), None),
        "Wallet"
    );
    assert_eq!(
        link_binding_name(&LinkTarget::Path("../PlayerData.clh".into()), None),
        "PlayerData"
    );
    assert_eq!(file_stem("A.B.clpp"), "A.B");
    assert_eq!(file_stem("Main.server.clpp"), "Main.server");
    let pkg = LinkTarget::Package(vec!["clpp".into(), "std".into()]);
    assert_eq!(link_binding_name(&pkg, None), "std");
    assert!(!pkg.display().contains('/'));
    assert_eq!(pkg.display(), "@clpp.std");
}

#[test]
fn parser_lowers_links_and_rejects_old_modules() {
    let src = r#"
        link "./PlayerData.clh" as Purse;
        link "./Wallet.clp";
        link @clpp.std;
        link @a.b.c as Bag;
        import { Wallet } from "./x.clh";
        #include <header>
        #include "src/PlayerData.clp"
    "#;
    let parsed = parse("m.clp", src);
    assert_lossless(src);
    let dump = dump_ast(&parsed.ast);
    assert!(dump.contains("(link \"./PlayerData.clh\" as Purse)"), "{dump}");
    assert!(dump.contains("(link \"./Wallet.clp\" as Wallet)"), "{dump}");
    assert!(dump.contains("(link @clpp.std as std)"), "{dump}");
    assert!(dump.contains("(link @a.b.c as Bag)"), "{dump}");
    assert!(!dump.contains("clp)"), "{dump}");
    assert_eq!(dump.matches("(rejected import)").count(), 1, "{dump}");
    assert_eq!(dump.matches("(rejected include)").count(), 2, "{dump}");
    let rejected: Vec<_> = parsed
        .diagnostics
        .iter()
        .filter(|d| d.code.as_deref() == Some("CLPP0801"))
        .collect();
    assert!(rejected.len() >= 3, "{:?}", parsed.diagnostics);
    let text = render("m.clp", src, &parsed.diagnostics);
    assert!(text.contains("m.clp:"), "{text}");
    assert!(text.contains("link"), "{text}");
    let fancy = render_codespan("m.clp", src, &parsed.diagnostics);
    assert!(fancy.contains("CLPP0801") || fancy.contains("removed"), "{fancy}");
}

#[test]
fn types_are_structured_and_attributes_are_not_keywords() {
    let src = r#"
        type Handler = function<void(int, string)>;
        const int* p;
        type Bits = int | bool & string;
        signal<int> changed;
        [[pure]] void marked() {}
        [[server]] void tagged() {}
        void init() { int n = @coins; }
    "#;
    let parsed = parse("t.clp", src);
    assert!(parsed.ok(), "{}", render("t.clp", src, &parsed.diagnostics));
    let dump = dump_ast(&parsed.ast);
    assert!(dump.contains("(alias Handler = function<void(int, string)>)"), "{dump}");
    assert!(dump.contains("int*"), "{dump}");
    assert!(dump.contains("(var changed : signal<int>)"), "{dump}");
    assert!(dump.contains("(fn marked : void)"), "{dump}");
    assert!(dump.contains("(fn tagged : void)"), "{dump}");
    let bad = parse("t.clp", "observable int x;");
    assert!(!bad.ok());
    let arrow = parse("t.clp", "void f() { a ~> b; }");
    assert!(!arrow.ok());
}

#[test]
fn constructors_and_qualified_methods() {
    let src = r#"
        struct W { int x; W(int x) {} };
        W::W(int x) {}
        void Owner::Method() {}
    "#;
    let parsed = parse("t.clp", src);
    assert!(parsed.ok(), "{}", render("t.clp", src, &parsed.diagnostics));
    let dump = dump_ast(&parsed.ast);
    assert!(dump.contains("(method W)"), "{dump}");
    assert!(dump.contains("(fn W::W ctor)"), "{dump}");
    assert!(dump.contains("(fn Owner::Method : void)"), "{dump}");
}

#[test]
fn error_recovery_keeps_later_declarations() {
    let src = "void f( {\nvoid g() {}\n";
    let parsed = parse("t.clp", src);
    assert_lossless(src);
    let dump = dump_ast(&parsed.ast);
    assert!(dump.contains("(fn g : void)"), "{dump}\n{}", render("t.clp", src, &parsed.diagnostics));
    assert!(parsed.diagnostics.len() >= 2 || !parsed.ok());
    let view = parsed.diagnostics[0].api_view(src);
    assert!(view.line >= 1);
    assert!(view.column >= 1);
    assert!(view.end_column >= view.column || view.end_line > view.line);
}

#[test]
fn expressions_follow_the_grammar_layers() {
    let src = r#"
        void init() {
            int z = a ?? b;
            int q = cond ? a : b;
            auto m = obj?.field;
            auto tried = maybe?;
            int s = a + b?;
            n //= 2;
            post(`a`, b, `c`);
            auto casted = static_cast<int>(f);
        }
    "#;
    let parsed = parse("t.clp", src);
    assert!(parsed.ok(), "{}", render("t.clp", src, &parsed.diagnostics));
    assert_lossless(src);
}

#[test]
fn core_example_parses() {
    let path = concat!(env!("CARGO_MANIFEST_DIR"), "/../../examples/syntax/core.clp");
    let src = std::fs::read_to_string(path).unwrap();
    let parsed = parse("examples/syntax/core.clp", &src);
    assert_lossless(&src);
    assert!(
        parsed.ok(),
        "{}\n{}",
        render("examples/syntax/core.clp", &src, &parsed.diagnostics),
        dump_ast(&parsed.ast)
    );
    let dump = dump_ast(&parsed.ast);
    assert!(dump.contains("(namespace Demo)"), "{dump}");
    assert!(dump.contains("(struct Box)"), "{dump}");
    assert!(dump.contains("(interface Named)"), "{dump}");
    assert!(dump.contains("(enum Color"), "{dump}");
    assert!(dump.contains("(method W)"), "{dump}");
    assert!(dump.contains("(fn W::W ctor)"), "{dump}");
    assert!(dump.contains("(link \"./PlayerData.clh\" as Purse)"), "{dump}");
    assert!(dump.contains("(link \"./Wallet.clp\" as Wallet)"), "{dump}");
    assert!(dump.contains("(link @clpp.std as std)"), "{dump}");
}

#[test]
fn shared_headers_parse() {
    let root = concat!(env!("CARGO_MANIFEST_DIR"), "/../../examples/shared/");
    for name in ["config.clp", "PlayerData.clh"] {
        let src = std::fs::read_to_string(format!("{root}{name}")).unwrap();
        let parsed = parse(name, &src);
        assert_lossless(&src);
        assert!(parsed.ok(), "{} {}", name, render(name, &src, &parsed.diagnostics));
    }
}
