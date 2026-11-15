"""Build the designer handoff PDF from docs/designer-handoff.md plus two renders."""
import re
import sys
from pathlib import Path

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


def footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 8)
    canvas.setFillColor(colors.HexColor("#777777"))
    canvas.drawString(MARGIN, 0.5 * inch, "Synth Mage cape / designer handoff / 20 October 2026")
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
        "written dimensions in the handoff take precedence wherever they disagree with a picture.",
        BODY))
    story.append(Spacer(1, 10))

    col_w = (CONTENT_W - 0.3 * inch) / 2
    img_h = 6.6 * inch
    left = [fitted_image(IMG_ON, col_w, img_h), Paragraph("Lights on, from the audience. Collar outline, two-row lapel bars, shoulder segments, dotted lower edges.", CAPTION)]
    right = [fitted_image(IMG_OFF, col_w, img_h), Paragraph("Lights off, zone map. Charcoal strips show every channel: collar edges and rim, full-length front channels, shoulders, outer side seams. Nothing in the bottom 15 cm.", CAPTION)]
    t = Table([[left, right]], colWidths=[col_w, col_w], hAlign="CENTER")
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0), ("RIGHTPADDING", (0, 0), (-1, -1), 0)]))
    story.append(t)
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
