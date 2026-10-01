"""Build the CL++ VS Code extension (.vsix) without npm or vsce.

A .vsix is a zip (Open Packaging Conventions) with [Content_Types].xml, extension.vsixmanifest
and the extension files under extension/. Usage:

    python tools/vscode/package_vsix.py [--exe path/to/clpp.exe] [--out dist/clpp-language.vsix]

--exe bundles a Windows x64 server at extension/bin/win32-x64/clpp.exe, so the extension works
right after `code --install-extension clpp-language-<version>.vsix`.
"""
import argparse
import json
import os
import zipfile
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
FILES = ["package.json", "extension.js", "language-configuration.json", "README.md",
         "syntaxes/clpp.tmLanguage.json", "images/icon.png", "images/clp-file.png"]
CONTENT_TYPES = {".json": "application/json", ".js": "application/javascript", ".md": "text/markdown",
                 ".png": "image/png", ".exe": "application/octet-stream", ".txt": "text/plain",
                 ".vsixmanifest": "text/xml"}


def manifest(pkg):
    categories = ",".join(pkg.get("categories", []))
    tags = ",".join(pkg.get("keywords", []) + ["clpp"])
    return f"""<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011" xmlns:d="http://schemas.microsoft.com/developer/vsx-schema-design/2011">
  <Metadata>
    <Identity Language="en-US" Id="{pkg['name']}" Version="{pkg['version']}" Publisher="{pkg['publisher']}" />
    <DisplayName>{escape(pkg['displayName'])}</DisplayName>
    <Description xml:space="preserve">{escape(pkg['description'])}</Description>
    <Tags>{escape(tags)}</Tags>
    <Categories>{escape(categories)}</Categories>
    <GalleryFlags>Public</GalleryFlags>
    <Properties>
      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="{pkg['engines']['vscode']}" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionDependencies" Value="" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionPack" Value="" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionKind" Value="workspace" />
      <Property Id="Microsoft.VisualStudio.Code.LocalizedLanguages" Value="" />
      <Property Id="Microsoft.VisualStudio.Services.Links.Source" Value="{pkg['repository']['url']}" />
      <Property Id="Microsoft.VisualStudio.Services.GitHubFlavoredMarkdown" Value="true" />
    </Properties>
    <License>extension/LICENSE.txt</License>
    <Icon>extension/images/icon.png</Icon>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code" />
  </Installation>
  <Dependencies />
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true" />
    <Asset Type="Microsoft.VisualStudio.Services.Content.Details" Path="extension/README.md" Addressable="true" />
    <Asset Type="Microsoft.VisualStudio.Services.Content.License" Path="extension/LICENSE.txt" Addressable="true" />
    <Asset Type="Microsoft.VisualStudio.Services.Icons.Default" Path="extension/images/icon.png" Addressable="true" />
  </Assets>
</PackageManifest>
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", help="Windows x64 clpp.exe to bundle")
    parser.add_argument("--out")
    args = parser.parse_args()
    with open(os.path.join(HERE, "package.json"), encoding="utf-8") as f:
        pkg = json.load(f)
    out = args.out or os.path.join(ROOT, "dist", f"{pkg['name']}-{pkg['version']}.vsix")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    entries = [(os.path.join(HERE, name), "extension/" + name) for name in FILES]
    entries.append((os.path.join(ROOT, "LICENSE"), "extension/LICENSE.txt"))
    if args.exe:
        entries.append((args.exe, "extension/bin/win32-x64/clpp.exe"))
    types = sorted({os.path.splitext(target)[1] for _, target in entries} | {".vsixmanifest"})
    content_types = ('<?xml version="1.0" encoding="utf-8"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
                     + "".join(f'<Default Extension="{t}" ContentType="{CONTENT_TYPES.get(t, "application/octet-stream")}" />' for t in types)
                     + "</Types>\n")
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("[Content_Types].xml", content_types)
        zf.writestr("extension.vsixmanifest", manifest(pkg))
        for source, target in entries:
            zf.write(source, target)
    print(out)


if __name__ == "__main__":
    main()
