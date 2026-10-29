# Meter-derived layout

Reference: the user's Meter Windows manual 1.0.4 and scripts/build-user-manual.py, inspected 29 October 2026. Product content is not part of the reusable template.

- Page: 595.276 x 841.89 pt (A4), content width 503.276 pt.
- Palette: ink #17252d; mint #6fe0c1; muted #58707c; pale #edf4f5; cover/header #10171d.
- Fonts: Segoe UI regular, bold and italic, embedded from licensed local Windows fonts.
- Body 10.2/15 pt; chapter 23/28 pt bold; subheading 12/17 pt bold; caption 8/11 italic; table 9/12; contents 9.2/12.
- Header at 29 pt from top, mint rule at 39 pt. Footer baseline 26 pt.
- Tables: dark white-text header, mint underline, alternating pale/white rows, 9 pt horizontal and 7 pt vertical padding, repeated header on continuation.
- Figures retain aspect ratio. Captions explain what the settings illustrate.
- Why panels are pale with a mint left edge and body text; a modest extension of the Meter design.
- Chapters begin on new pages; use natural continuation when needed, avoiding a solitary last paragraph or heading.

## Manuscript JSON
Required: product, version, date, platform, scope, chapters.
Optional: publisher (default DJL Software), logo, cover_image, cover_caption, footer.
chapters is a list of {title, blocks}.
Each block:
- {"type":"p","text":"Body; basic ReportLab paragraph markup allowed."}
- {"type":"sub","text":"Subheading"}
- {"type":"why","text":"Explanation of the purpose or consequence."}
- {"type":"steps","items":["First action","Next action"]}
- {"type":"table","headers":["Control","Meaning"],"rows":[["A","B"]],"widths":[0.3,0.7]}
- {"type":"image","path":"assets/screenshot.png","caption":"Figure description","max_height":300}
- {"type":"space","height":8}

Images and logo paths are relative to the JSON file. Title and table cell content is escaped by the renderer; p/sub/why/steps accept intentional paragraph markup. Encode literal ampersands as &amp; in rich text. Do not use untrusted raw markup.

An image block can include `crop: [left, top, right, bottom]` in original pixel coordinates for a readable close-up. Inspect the original first; preserve it unchanged. The cover still uses the full original image.

## Review before delivery
Check all pages at readable size, plus overview sheets for consistency. Confirm page numbers, contents destinations, links, no blank pages, body text inside margins, readable screenshot text, no duplicate headings or obsolete version numbers. Compare every control family with source, not just the previous manual. State actual limits and current release status. Explain prerequisites, save/recall and external file dependencies. Supply the complete PDF and maintainable source.
