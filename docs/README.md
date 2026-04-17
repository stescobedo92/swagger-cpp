# swaggercpp — Documentation site

This folder contains the source of the `swaggercpp` documentation site. It is a **VitePress** project; every page is plain Markdown so the content can also be browsed directly on GitHub.

## Local preview

From this directory:

```bash
npm install
npm run docs:dev
```

VitePress starts on `http://localhost:5173` with hot reload.

## Build a static site

```bash
npm run docs:build
```

Output is written to `.vitepress/dist/`. Serve with any static host (GitHub Pages, Netlify, Cloudflare Pages, nginx...).

```bash
npm run docs:preview
```

## Layout

```
docs/
├── .vitepress/
│   ├── config.ts        # Navigation, sidebar, theme metadata
│   └── theme/
│       ├── index.ts     # Theme entry
│       └── custom.css   # Swagger-inspired palette overrides
├── index.md             # Hero landing page
├── install.md           # Full installation guide
├── quick-start.md       # 5-minute tutorial
├── guide.md             # Narrative walkthrough
├── architecture.md      # Architecture notes
├── api/                 # API reference, one file per subsystem
├── examples/            # Recipes from simple to elaborate
└── packaging/           # vcpkg / Conan / CMake / FetchContent
```

## Theme

The color palette in `.vitepress/theme/custom.css` is inspired by Swagger UI's brand: `#85EA2D` (signature green), `#173647` (dark teal), `#61AFFE` (GET blue), `#FCA130` (POST orange), `#F93E3E` (DELETE red).

## Contributing

Every Markdown file links back to GitHub via an "Edit this page on GitHub" link (see `editLink` in `config.ts`). PRs welcome.
