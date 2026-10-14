# Website maintenance

The user requires the website to stay current with language development. For every change to syntax, semantics, editor support or supported behavior, update the corresponding public source in `docs/guide`, `docs/reference` or `docs/handbook` in the same stage. Record remaining limitations accurately; prepared CI jobs are not completed validations.

The website generates its pages from these sources before development and production builds. Its highlighting reads the same TextMate grammar as the VS Code extension. Do not maintain a separate website grammar.

Run `npm run build` in `www` after documentation or highlighting changes. This includes source policy checks, page generation and checks of links and assets in the generated HTML. A successful local build does not confirm publication. GitHub Pages deploys through `.github/workflows/pages.yml` after relevant changes reach `main`; report publication only after the deployment is verified.
