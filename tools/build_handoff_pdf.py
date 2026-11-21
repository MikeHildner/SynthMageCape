"""Build the designer handoff PDF from docs/designer-handoff.md plus two renders."""
import re
import sys
from pathlib import Path

from reportlab.graphics.shapes import Circle, Drawing, Group, Line, Polygon, PolyLine, Rect, String
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    Image,
    KeepTogether,
    ListFlowable,
    ListItem,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)

DOCS = Path(sys.argv[1])
MD = DOCS / "designer-handoff.md"
OUT = DOCS / "Synth_Mage_Designer_Handoff.pdf"
IMG_ON = DOCS / "renders" / "lights-on-front.png"
IMG_OFF = DOCS / "renders" / "lights-off-zone-map.png"
IMG_VIDEO = DOCS / "renders" / "video-thumbnail.png"
VIDEO_URL = "https://youtu.be/ja1__TgNOSs"

PAGE_W, PAGE_H = letter
MARGIN = 0.8 * inch
CONTENT_W = PAGE_W - 2 * MARGIN

ss = getSampleStyleSheet()
BODY = ParagraphStyle("body", parent=ss["Normal"], fontName="Helvetica", fontSize=10, leading=14, spaceAfter=6)
H1 = ParagraphStyle("h1", parent=ss["Title"], fontName="Helvetica-Bold", fontSize=20, leading=24, spaceAfter=4, alignment=0)
H2 = ParagraphStyle("h2", parent=ss["Heading2"], fontName="Helvetica-Bold", fontSize=13.5, leading=17, spaceBefore=14, spaceAfter=6, textColor=colors.HexColor("#1a1a1a"))
H3 = ParagraphStyle("h3", parent=ss["Heading3"], fontName="Helvetica-Bold", fontSize=11, leading=14, spaceBefore=10, spaceAfter=4)
SMALL = ParagraphStyle("small", parent=BODY, fontSize=9, leading=12, textColor=colors.HexColor("#555555"))
CAPTION = ParagraphStyle("cap", parent=SMALL, alignment=TA_CENTER, spaceBefore=4)
CELL = ParagraphStyle("cell", parent=BODY, fontSize=9, leading=12, spaceAfter=0)
CELL_B = ParagraphStyle("cellb", parent=CELL, fontName="Helvetica-Bold")


def inline(text: str) -> str:
    """Convert markdown bold/italic/code to reportlab markup and escape the rest."""
    text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    text = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)
    text = re.sub(r"`(.+?)`", r"<font face='Courier'>\1</font>", text)
    return text


def make_table(rows):
    header, body = rows[0], rows[2:]  # rows[1] is the |---| separator
    data = [[Paragraph(inline(c), CELL_B) for c in header]]
    for r in body:
        data.append([Paragraph(inline(c), CELL) for c in r])
    ncols = len(header)
    widths = [CONTENT_W / ncols] * ncols
    t = Table(data, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#e8e8e8")),
        ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#999999")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
    ]))
    return t


def wiring_drawing():
    """Front view of the cape: lights, the pouch, the cables and the plug-in points.

    Drawn in the coordinates of the on-screen sketch it was copied from
    (x 60 to 660, y 14 to 624, y downwards), then scaled to the page width.
    """
    SRC_W, SRC_H = 600, 610
    scale = 0.76 * CONTENT_W / SRC_W  # a little under full width, so the table below it fits on the page
    drawing = Drawing(SRC_W * scale, SRC_H * scale)
    drawing.hAlign = "CENTER"
    g = Group(transform=(scale, 0, 0, scale, 0, 0))
    drawing.add(g)

    LIGHT = colors.HexColor("#534AB7")
    CABLE = colors.HexColor("#D85A30")
    INK = colors.HexColor("#2C2C2A")
    GREY = colors.HexColor("#888780")
    PANEL = colors.HexColor("#F1EFE8")

    def X(x):
        return x - 60

    def Y(y):
        return SRC_H - (y - 14)

    def pts(*xy):
        out = []
        for i in range(0, len(xy), 2):
            out += [X(xy[i]), Y(xy[i + 1])]
        return out

    def line(x1, y1, x2, y2, colour, width, dash=None):
        g.add(Line(X(x1), Y(y1), X(x2), Y(y2), strokeColor=colour, strokeWidth=width,
                   strokeDashArray=dash, strokeLineCap=1))

    def poly(colour, width, *xy, dash=None):
        g.add(PolyLine(pts(*xy), strokeColor=colour, strokeWidth=width, strokeDashArray=dash,
                       strokeLineCap=1, strokeLineJoin=1))

    def dot(x, y, r, colour):
        g.add(Circle(X(x), Y(y), r, fillColor=colour, strokeColor=None))

    def text(x, y, words, anchor="start", bold=False):
        g.add(String(X(x), Y(y), words, fontName="Helvetica-Bold" if bold else "Helvetica",
                     fontSize=12, fillColor=INK, textAnchor=anchor))

    def label(x1, y1, x2, y2, words):
        line(x1, y1, x2, y2, GREY, 0.5, dash=[3, 3])
        text(452, y2 + 4, words)

    text(150, 30, "Mike's right", anchor="middle")
    text(350, 30, "Mike's left", anchor="middle")

    # The two front panels of the open cape, and the belt behind them.
    g.add(Polygon(pts(225, 100, 150, 120, 80, 520, 225, 520), fillColor=PANEL, strokeColor=GREY, strokeWidth=0.5))
    g.add(Polygon(pts(275, 100, 350, 120, 420, 520, 275, 520), fillColor=PANEL, strokeColor=GREY, strokeWidth=0.5))
    line(118, 336, 382, 336, GREY, 0.5)

    # Strip lights: the collar (one piece), shoulders (two rows each), lapels.
    poly(LIGHT, 3, 225, 100, 225, 60, 231, 52, 240, 47, 250, 46)
    poly(LIGHT, 3, 275, 100, 275, 60, 269, 52, 260, 47, 250, 46)
    line(160, 121, 200, 110, LIGHT, 3)
    line(161.6, 126.8, 201.6, 115.8, LIGHT, 3)
    line(300, 110, 340, 121, LIGHT, 3)
    line(298.4, 115.8, 338.4, 126.8, LIGHT, 3)
    for x in (212, 219, 281, 288):
        line(x, 118, x, 298, LIGHT, 3)

    # String lights: down the front edge, unlit across, up the outer line.
    poly(LIGHT, 0.8, 215, 311, 215, 470, 108, 470, 118, 455, 178, 366)
    poly(LIGHT, 0.8, 285, 311, 285, 470, 392, 470, 382, 455, 322, 366)
    for y in (322, 341, 360, 379, 398, 417, 436, 455):
        dot(215, y, 3, LIGHT)
        dot(285, y, 3, LIGHT)
    for x, y in ((178, 366), (166, 384), (154, 402), (142, 419), (130, 437), (118, 455)):
        dot(x, y, 3, LIGHT)
        dot(500 - x, y, 3, LIGHT)

    # Cables. From the pouch: to the near shoulder, to the collar's front
    # edge, to the near lapel, and along the belt to the far lapel. The far
    # shoulder is fed by the link behind the neck.
    poly(CABLE, 2, 182, 316, 186, 140, 197, 114)
    poly(CABLE, 2, 191, 316, 203, 130, 222, 103)
    line(200, 316, 214, 309, CABLE, 2)
    line(200, 350, 312, 350, CABLE, 2, dash=[6, 4])
    line(312, 350, 288, 309, CABLE, 2)
    line(205, 109, 298, 109, CABLE, 2, dash=[6, 4])

    # The pouch, and the four plug-in points.
    g.add(Rect(X(140), Y(356), 60, 40, rx=4, ry=4, fillColor=colors.HexColor("#D3D1C7"), strokeColor=GREY,
               strokeWidth=0.5))
    text(170, 340, "Pouch", anchor="middle", bold=True)
    for x, y in ((199, 111), (224, 101), (216, 306), (285, 306)):
        dot(x, y, 5, CABLE)

    label(280, 66, 446, 66, "Collar light, one piece")
    label(270, 109, 446, 92, "Link behind the neck")
    label(346, 122, 446, 122, "Shoulder strips, two rows")
    label(294, 200, 446, 200, "Lapel lights")
    label(200, 250, 446, 250, "Two cables inside the lining")
    label(292, 306, 446, 300, "Lapel plug, string plugs in too")
    label(318, 352, 446, 350, "Around the back, on the belt")
    label(382, 336, 446, 396, "Belt, worn under the cape")
    label(366, 428, 446, 446, "String lights, outer line")
    label(340, 472, 446, 492, "Unlit stretch above the hem")

    # Legend.
    line(80, 558, 110, 558, LIGHT, 3)
    text(118, 562, "Strip lights")
    line(220, 558, 250, 558, CABLE, 2)
    text(258, 562, "Cable inside the cape's lining")
    for x in (86, 96, 106):
        dot(x, 584, 3, LIGHT)
    text(118, 588, "String lights")
    line(220, 584, 250, 584, CABLE, 2, dash=[6, 4])
    text(258, 588, "Cable behind Mike: belt, or neck")
    dot(95, 610, 5, CABLE)
    text(118, 614, "Plug-in point")
    return drawing


def parse_markdown(md: str):
    """Turn the handoff markdown into a list of flowables."""
    story = []
    lines = md.splitlines()
    i = 0
    para_buf = []
    list_buf = []
    list_kind = None

    def flush_para():
        nonlocal para_buf
        if para_buf:
            story.append(Paragraph(inline(" ".join(para_buf)), BODY))
            para_buf = []

    def flush_list():
        nonlocal list_buf, list_kind
        if list_buf:
            items = [ListItem(Paragraph(inline(t), BODY), leftIndent=14) for t in list_buf]
            if list_kind == "ol":
                lf = ListFlowable(items, bulletType="1", leftIndent=16, bulletFontSize=10)
            else:
                lf = ListFlowable(items, bulletType="bullet", start="•", leftIndent=16, bulletFontSize=10)
            story.append(lf)
            story.append(Spacer(1, 4))
            list_buf = []
            list_kind = None

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        if not stripped:
            flush_para(); flush_list(); i += 1; continue

        if stripped == "[[page-break]]":
            flush_para(); flush_list()
            story.append(PageBreak()); i += 1; continue

        if stripped == "[[wiring-drawing]]":
            flush_para(); flush_list()
            story.append(KeepTogether([
                wiring_drawing(),
                Paragraph("Front view, not to scale. Lights, the pouch, and one way to run the four cables. "
                          "The lines the lights follow are set on the real cape.", CAPTION),
                Spacer(1, 8),
            ]))
            i += 1; continue

        if stripped.startswith("# "):
            flush_para(); flush_list()
            story.append(Paragraph(inline(stripped[2:]), H1)); i += 1; continue
        if stripped.startswith("## "):
            flush_para(); flush_list()
            story.append(Paragraph(inline(stripped[3:]), H2)); i += 1; continue
        if stripped.startswith("### "):
            flush_para(); flush_list()
            story.append(Paragraph(inline(stripped[4:]), H3)); i += 1; continue

        if stripped.startswith("|"):
            flush_para(); flush_list()
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                cells = [c.strip() for c in lines[i].strip().strip("|").split("|")]
                rows.append(cells); i += 1
            story.append(make_table(rows)); story.append(Spacer(1, 8)); continue

        m_ul = re.match(r"^[-*] (.*)", stripped)
        m_ol = re.match(r"^\d+\. (.*)", stripped)
        if m_ul or m_ol:
            flush_para()
            kind = "ul" if m_ul else "ol"
            if list_kind and list_kind != kind:
                flush_list()
            list_kind = kind
            list_buf.append((m_ul or m_ol).group(1)); i += 1; continue

        # continuation of a list item (indented) or plain paragraph text
        if list_buf and line.startswith("  "):
            list_buf[-1] += " " + stripped; i += 1; continue
        flush_list()
        para_buf.append(stripped); i += 1

    flush_para(); flush_list()
    return story


def fitted_image(path: Path, max_w: float, max_h: float) -> Image:
    img = Image(str(path))
    scale = min(max_w / img.imageWidth, max_h / img.imageHeight)
    img.drawWidth = img.imageWidth * scale
    img.drawHeight = img.imageHeight * scale
    return img


class LinkedImage(Image):
    """An image that opens a web address when clicked."""

    def __init__(self, path, url, **kwargs):
        super().__init__(path, **kwargs)
        self.url = url

    def draw(self):
        super().draw()
        self.canv.linkURL(self.url, (0, 0, self.drawWidth, self.drawHeight), relative=1)


def video_block():
    """The thumbnail of the demonstration video beside its link."""
    thumb = LinkedImage(str(IMG_VIDEO), VIDEO_URL)
    thumb_w = 2.2 * inch
    thumb.drawHeight = thumb.imageHeight * thumb_w / thumb.imageWidth
    thumb.drawWidth = thumb_w
    text = [
        Paragraph("Video: the working lapels", H3),
        Paragraph(
            "A short clip of the two lapel light pieces on a stand-in cape, following what Mike plays "
            "on the keyboard. This is the real hardware, not a render. A webcam flattens the colours; "
            "they are richer to the eye.",
            BODY),
        # Not underlined: an underline would hide the two underscores in the address.
        Paragraph(f"<a href='{VIDEO_URL}' color='#1a55aa'>{VIDEO_URL}</a>", BODY),
    ]
    t = Table([[thumb, text]], colWidths=[thumb_w + 0.25 * inch, CONTENT_W - thumb_w - 0.25 * inch])
    t.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 0),
        ("RIGHTPADDING", (0, 0), (-1, -1), 0),
        ("TOPPADDING", (0, 0), (-1, -1), 0),
    ]))
    return t


def footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 8)
    canvas.setFillColor(colors.HexColor("#777777"))
    canvas.drawString(MARGIN, 0.5 * inch, "Synth Mage cape / handoff for the cape maker / revised 21 November 2026")
    canvas.drawRightString(PAGE_W - MARGIN, 0.5 * inch, f"Page {doc.page}")
    canvas.restoreState()


def build():
    md = MD.read_text(encoding="utf-8")
    story = []

    # Page 1: reference images side by side
    story.append(Paragraph("Synth Mage Cape: Reference Images", H1))
    story.append(Paragraph(
        "Two concept renders to read alongside the handoff that follows. The lit image shows the look from "
        "the audience at the keyboard. The unlit image is the construction reference: the lighter charcoal "
        "strips mark where the mesh-faced channels go. Both are AI concept renders, not patterns; the "
        "written handoff takes precedence wherever it disagrees with a picture. In particular, the dots of "
        "light below the lapels will be strings of small lights on thin wire (section 4 of the handoff), "
        "and the lines they follow are set on the real cape.",
        BODY))
    story.append(Spacer(1, 10))

    col_w = (CONTENT_W - 0.3 * inch) / 2
    img_h = 6.6 * inch
    left = [fitted_image(IMG_ON, col_w, img_h), Paragraph("Lights on, from the audience. Collar outline, two-row lapel bars, shoulder segments, and the dotted lower lines that section 4 describes.", CAPTION)]
    right = [fitted_image(IMG_OFF, col_w, img_h), Paragraph("Lights off, zone map. Charcoal strips show every channel: collar edges and rim, full-length front channels, shoulders, outer side seams. Nothing in the bottom 15 cm.", CAPTION)]
    t = Table([[left, right]], colWidths=[col_w, col_w], hAlign="CENTER")
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0), ("RIGHTPADDING", (0, 0), (-1, -1), 0)]))
    story.append(t)
    story.append(Spacer(1, 18))
    story.append(video_block())
    story.append(PageBreak())

    story.extend(parse_markdown(md))

    doc = SimpleDocTemplate(
        str(OUT), pagesize=letter,
        leftMargin=MARGIN, rightMargin=MARGIN, topMargin=0.75 * inch, bottomMargin=0.8 * inch,
        title="Synth Mage Cape: Designer Handoff", author="Mike Hildner",
    )
    doc.build(story, onFirstPage=footer, onLaterPages=footer)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    build()
