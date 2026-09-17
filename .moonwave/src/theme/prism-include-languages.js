/**
 * Registers Prism languages for the CL++ docs site.
 *
 * Moonwave's generated Docusaurus config only lists stock Prism ids
 * (lua, bash, …). Markdown fences use ```clpp, which is not a Prism
 * language, so this swizzle loads C/C++ and aliases CL++ onto them.
 */
import siteConfig from "@generated/docusaurus.config";

export default function prismIncludeLanguages(PrismObject) {
  const {
    themeConfig: { prism },
  } = siteConfig;
  const { additionalLanguages } = prism;

  const PrismBefore = globalThis.Prism;
  globalThis.Prism = PrismObject;

  additionalLanguages.forEach((lang) => {
    if (lang === "php") {
      require("prismjs/components/prism-markup-templating.js");
    }
    require(`prismjs/components/prism-${lang}`);
  });

  if (!PrismObject.languages.cpp) {
    require("prismjs/components/prism-c");
    require("prismjs/components/prism-cpp");
  }

  const cpp = PrismObject.languages.cpp;
  if (cpp) {
    PrismObject.languages.clpp = PrismObject.languages.extend("cpp", {
      keyword:
        /\b(?:alignas|alignof|asm|auto|bool|break|case|catch|char|class|const|consteval|constexpr|continue|default|delete|do|double|else|enum|explicit|export|extern|false|float|for|friend|goto|if|inline|in|int|long|mutable|namespace|new|noexcept|nullptr|operator|private|protected|public|register|return|short|signed|sizeof|static|struct|switch|template|this|throw|true|try|typedef|typeid|typename|union|unsigned|using|virtual|void|volatile|wchar_t|while|observable|signal|guard|match|async|await|spawn|parallel|func|null|array|dictionary|optional|vector|span|map)\b/,
      function:
        /\b(?:post|warn|report|GetService|static_cast|pcall|xpcall|string_concat|to_string|to_number|to_bool|Fire|Connect|Once|Wait|GetPropertyChangedSignal)\b/,
      "class-name": [
        {
          pattern: /(\b(?:struct|class|enum|new)\s+)[A-Z][A-Za-z0-9_]*/,
          lookbehind: true,
        },
        {
          pattern: /\b[A-Z][A-Za-z0-9_]*(?=\s*\*?\s+[A-Za-z_])/,
        },
        {
          pattern: /\b[A-Z][A-Za-z0-9_]*(?=\s*::)/,
        },
      ],
      property: [
        {
          pattern: /(\.)[A-Za-z_]\w*/,
          lookbehind: true,
        },
        {
          pattern: /((?<![.:]):)[A-Za-z_]\w*/,
          lookbehind: true,
        },
      ],
    });
  } else {
    PrismObject.languages.clpp =
      PrismObject.languages.clike || PrismObject.languages.javascript;
  }

  PrismObject.languages.clp = PrismObject.languages.clpp;
  PrismObject.languages.clh = PrismObject.languages.clpp;

  delete globalThis.Prism;
  if (typeof PrismBefore !== "undefined") {
    globalThis.Prism = PrismObject;
  }
}
