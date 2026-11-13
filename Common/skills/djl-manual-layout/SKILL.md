---
name: djl-manual-layout
description: Create illustrated DJL software PDF manuals in the established Meter manual style. Use when asked to match Meter manuals, create a DJL product guide, or explain controls with beginner-friendly how and why instructions.
---

# DJL manual layout

Use the actual product implementation and current screenshots as the factual source. This skill captures Meter's page design; it does not transfer Meter's features, licensing, claims or platform support to another product.

## Workflow

1. Inspect the product's current controls, defaults, limits and save/recall behaviour. Identify version, platform and release status.
2. Gather screenshots of the real current interface. Prefer deterministic application-rendered captures when available. Do not manufacture interface screenshots. Label demonstration settings in captions.
3. Build a JSON manuscript using the schema in references/layout.md. Include a guided first result, every workflow and control, troubleshooting, limits and glossary.
4. Teach each substantial feature with **How** (exact actions), **Why** (audible consequence and when useful), and a practical starting example. Explain units at first use. Distinguish display-only controls from audio changes. Identify disabled prerequisites and destructive actions.
5. Use scripts/build_manual.py with the manuscript path and output PDF path. The supplied assets/DJLLogoWhite.png is the user's DJL brand asset. Windows uses Segoe UI; on other platforms supply licensed Segoe font files with --font-dir, or explicitly document an approved font substitution.
6. Apply the available PDF skill's artifact and verification workflow. Render every final page, inspect readable full-size pages as well as contact sheets, fix clipping/overlap/blank pages, and verify contents links and bookmarks. The renderer alone does not establish that the manual is accurate.
7. Keep the editable manuscript, screenshots and generator with the product. Put the final PDF under output/pdf/ and include it in the product package when requested.

## Design contract

A4 portrait; 46 pt side margins; 58 pt top and 48 pt bottom. Dark cover, white DJL logo, mint product name, large white manual title, actual interface image, version/platform/date. Interior is white with dark ink and mint accents. Numbered chapters start on new pages. Use the exact sizes and palette in references/layout.md and the supplied renderer. Use clickable contents, bookmarks, running header and version/page footer.

Do not use a screenshot in place of readable control explanations. Avoid unsupported equivalence claims about other software. A pitch is not proof of a major/minor key. A test result is not certification. Verify support links and external product workflows when needed.

## Run

```powershell
python scripts/build_manual.py path/to/manual.json --output path/to/output/pdf/Product-Manual.pdf
```

Paths inside the manuscript are relative to its directory. The renderer uses fresh flowables for each contents pass and checks page text/bookmark counts. It requires reportlab, Pillow and pypdf. See references/layout.md for the JSON contract and final review checklist.
