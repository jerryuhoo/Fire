#!/usr/bin/env python3
"""Build the illustrated Fire manual from its Markdown source.

Requirements: reportlab, Pillow. Run from any directory; optional --source and
--output paths override the repository defaults. Screenshots are unmodified
product captures referenced relative to the Markdown file.
"""
from __future__ import annotations

import argparse
import html
import re
from pathlib import Path

import reportlab
from PIL import Image as PILImage
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate, Flowable, Frame, HRFlowable, Image, KeepTogether,
    LongTable, NextPageTemplate, PageBreak, PageTemplate, Paragraph, Preformatted, Spacer, TableStyle,
)
from reportlab.platypus.tableofcontents import TableOfContents

MANUAL = Path(__file__).resolve().parents[1]
ROOT = MANUAL.parents[1]
INK = colors.HexColor('#17212b')
MUTED = colors.HexColor('#586675')
PEACH = colors.HexColor('#edae8c')
RUST = colors.HexColor('#a1452e')
PAPER = colors.HexColor('#f6f5f1')
HAIRLINE = colors.HexColor('#d8dedf')
PALE = colors.HexColor('#eef1f2')
PAGE_W, PAGE_H = A4
MARGIN = 47
WIDTH = PAGE_W - 2 * MARGIN


def register_fonts():
    fonts = Path(reportlab.__file__).parent / 'fonts'
    for name, filename in [('Body', 'Vera.ttf'), ('Body-Bold', 'VeraBd.ttf'),
                           ('Body-Italic', 'VeraIt.ttf'), ('Body-BoldItalic', 'VeraBI.ttf')]:
        pdfmetrics.registerFont(TTFont(name, str(fonts / filename)))
    pdfmetrics.registerFontFamily('Body', normal='Body', bold='Body-Bold',
                                  italic='Body-Italic', boldItalic='Body-BoldItalic')


def typography():
    base = dict(fontName='Body', fontSize=9.6, leading=14.1, textColor=INK,
                spaceAfter=7.3, allowWidows=0, allowOrphans=0)
    styles = {'body': ParagraphStyle('BodyText', **base)}
    styles['h2'] = ParagraphStyle('Chapter', **(base | dict(fontName='Body-Bold',
        fontSize=21, leading=27, spaceBefore=0, spaceAfter=16, keepWithNext=True)))
    styles['h3'] = ParagraphStyle('Section', **(base | dict(fontName='Body-Bold',
        fontSize=12.2, leading=16.3, spaceBefore=13, spaceAfter=7, keepWithNext=True)))
    styles['h4'] = ParagraphStyle('Subsection', **(base | dict(fontName='Body-Bold',
        fontSize=10, leading=14, spaceBefore=8, spaceAfter=5, keepWithNext=True)))
    styles['caption'] = ParagraphStyle('Caption', **(base | dict(fontSize=8.1,
        leading=11.6, textColor=MUTED, spaceBefore=5, spaceAfter=14)))
    styles['note'] = ParagraphStyle('Note', **(base | dict(fontSize=9.2, leading=13.4,
        backColor=PALE, borderColor=HAIRLINE, borderWidth=.55, borderPadding=10,
        spaceBefore=6, spaceAfter=13)))
    styles['cell'] = ParagraphStyle('Cell', **(base | dict(fontSize=8.4, leading=11.8,
        spaceAfter=0, allowWidows=1, allowOrphans=1)))
    styles['headercell'] = ParagraphStyle('HeaderCell', parent=styles['cell'],
        fontName='Body-Bold', textColor=colors.white)
    styles['bullet'] = ParagraphStyle('Bullet', **(base | dict(leftIndent=13,
        firstLineIndent=0, bulletIndent=1, bulletFontName='Body', bulletFontSize=9.2,
        spaceAfter=5)))
    styles['number'] = ParagraphStyle('Number', parent=styles['bullet'], leftIndent=20)
    styles['small'] = ParagraphStyle('Small', parent=styles['body'], fontSize=8.2,
        leading=12, textColor=MUTED)
    styles['code'] = ParagraphStyle('Code', fontName='Courier', fontSize=8.1,
        leading=11.5, textColor=INK, backColor=PALE, borderPadding=9,
        spaceBefore=6, spaceAfter=10)
    return styles


def plain(value):
    return re.sub(r'[*`_]', '', value).strip()


def slug(value):
    return re.sub(r'[^a-z0-9\s-]', '', plain(value).lower()).strip().replace(' ', '-')


def inline(value):
    value = html.escape(value, quote=False)
    value = re.sub(r'`([^`]+)`', r'<font name="Courier">\1</font>', value)
    value = re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', value)
    value = re.sub(r'(?<!\*)\*([^*]+)\*(?!\*)', r'<i>\1</i>', value)
    def link(match):
        label, target = match.groups()
        if target.startswith('#') or target.startswith(('https://', 'http://')):
            return f'<link href="{html.escape(target, quote=True)}" color="#a1452e">{label}</link>'
        return label
    return re.sub(r'\[([^]]+)\]\(([^)]+)\)', link, value)


class Cover(Flowable):
    def __init__(self, image_path):
        super().__init__()
        self.image_path = image_path
        self.width, self.height = WIDTH, PAGE_H - 100

    def draw(self):
        c = self.canv
        h = self.height
        c.setFillColor(INK)
        c.setFont('Body-Bold', 66)
        c.drawString(0, h - 80, 'Fire')
        c.setFillColor(PEACH)
        c.drawString(pdfmetrics.stringWidth('Fire', 'Body-Bold', 66) + 3, h - 80, '.')
        c.setFillColor(INK)
        c.setFont('Body', 24)
        c.drawString(0, h - 127, 'User Manual')
        c.setFillColor(MUTED)
        c.setFont('Body', 10)
        c.drawString(1, h - 153, 'VERSION 2.1.0  /  DEVELOPMENT EDITION')
        c.setStrokeColor(PEACH)
        c.setLineWidth(3)
        c.line(0, h - 179, 58, h - 179)
        if self.image_path.exists():
            with PILImage.open(self.image_path) as im:
                ih = WIDTH * im.height / im.width
            c.drawImage(str(self.image_path), 0, h - 209 - ih,
                        width=WIDTH, height=ih, preserveAspectRatio=True, mask='auto')
        y = h - 209 - WIDTH / 2 - 34
        c.setFont('Body-Bold', 17)
        c.setFillColor(INK)
        c.drawString(0, y, 'Make some beautiful noise.')
        c.setFont('Body', 10)
        c.setFillColor(MUTED)
        for n, line in enumerate([
            'Multiband colour, dynamics, modulation and space.',
            'A practical guide to designing sounds and managing your work.',
        ]):
            c.drawString(0, y - 27 - n * 16, line)
        c.setStrokeColor(HAIRLINE)
        c.setLineWidth(.7)
        c.line(0, 57, WIDTH, 57)
        c.setFont('Body-Bold', 9)
        c.setFillColor(INK)
        c.drawString(0, 34, 'BLUE WINGS MUSIC')
        c.setFont('Body', 8.2)
        c.setFillColor(MUTED)
        c.drawRightString(WIDTH, 34, '24 OCTOBER 2026')
        c.drawString(0, 16, 'Illustrated with genuine interface captures from Fire 2.1.0.')


class ManualDocument(BaseDocTemplate):
    def __init__(self, path):
        super().__init__(str(path), pagesize=A4, leftMargin=MARGIN, rightMargin=MARGIN,
            topMargin=51, bottomMargin=48, title='Fire 2.1 User Manual',
            author='Blue Wings Music', subject='Illustrated operation manual for Fire 2.1.0',
            pageCompression=1, allowSplitting=1)
        frame = Frame(MARGIN, 48, WIDTH, PAGE_H - 99, leftPadding=0,
                      bottomPadding=0, rightPadding=0, topPadding=0, id='body')
        self.addPageTemplates(PageTemplate(id='manual', frames=frame, onPage=self.page_chrome))
        wide = Frame(MARGIN, 48, PAGE_H - 2*MARGIN, PAGE_W - 99,
                     leftPadding=0, bottomPadding=0, rightPadding=0, topPadding=0, id='widebody')
        self.addPageTemplates(PageTemplate(id='wide', frames=wide, pagesize=(PAGE_H,PAGE_W),
                                          onPage=self.page_chrome))
        self.chapter_title = 'USER MANUAL'

    def page_chrome(self, canvas, doc):
        canvas.saveState()
        page_w, page_h = doc.pageTemplate.pagesize or A4
        canvas.setFillColor(PAPER if doc.page == 1 else colors.white)
        canvas.rect(0, 0, page_w, page_h, fill=1, stroke=0)
        if doc.page > 1:
            canvas.setFont('Body-Bold', 7.5)
            canvas.setFillColor(MUTED)
            canvas.drawString(MARGIN, page_h - 29, 'FIRE 2.1  /  USER MANUAL')
            canvas.setFont('Body', 7.2)
            canvas.drawRightString(page_w - MARGIN, page_h - 29, 'BLUE WINGS MUSIC')
            canvas.setStrokeColor(HAIRLINE)
            canvas.setLineWidth(.5)
            canvas.line(MARGIN, page_h - 36, page_w - MARGIN, page_h - 36)
            canvas.line(MARGIN, 36, page_w - MARGIN, 36)
            canvas.setFont('Body', 7.2)
            canvas.drawString(MARGIN, 23, 'Development edition / 24 October 2026')
            canvas.drawRightString(page_w - MARGIN, 23, f'{doc.page:02d}')
        canvas.restoreState()

    def afterFlowable(self, flowable):
        if isinstance(flowable, Paragraph) and hasattr(flowable, 'manual_heading'):
            level, title, key = flowable.manual_heading
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(title, key, level=level, closed=level > 0)
            if level == 0:
                self.chapter_title = title
                self.notify('TOCEntry', (0, title, self.page, key))


def paragraph(text, style):
    return Paragraph(inline(text), style)


def table_widths(rows):
    n = len(rows[0])
    if n == 2:
        first = max(len(plain(r[0])) for r in rows)
        left = min(174, max(112, 4.5 * min(first, 32)))
        return [left, WIDTH - left]
    if n == 3:
        return [WIDTH * .24, WIDTH * .32, WIDTH * .44]
    if n == 4:
        return [WIDTH * .18, WIDTH * .27, WIDTH * .27, WIDTH * .28]
    return [WIDTH / n] * n


def markdown_flowables(text, source_dir, styles):
    lines = text.splitlines()
    flow = []
    i = 0
    figure = 0
    landscape_next = False
    def special(line):
        return (not line.strip() or line.startswith(('#', '>', '|', '```', '<!--'))
                or re.match(r'^\s*(?:[-*+] |\d+\. |!\[)', line))
    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue
        if line.startswith('<!--'):
            if 'figure:landscape' in line:
                landscape_next = True
            if 'pagebreak' in line:
                flow.append(PageBreak())
            i += 1
            continue
        if line.startswith('## '):
            title = line[3:]
            if flow and not isinstance(flow[-1], PageBreak):
                flow.append(PageBreak())
            p = paragraph(title, styles['h2'])
            p.manual_heading = (0, plain(title), slug(title))
            flow.extend([p, HRFlowable(width=44, thickness=2.4, color=PEACH,
                                      spaceBefore=0, spaceAfter=14, hAlign='LEFT')])
            i += 1
            continue
        if line.startswith(('### ', '#### ')):
            marker, title = line.split(' ', 1)
            p = paragraph(title, styles['h3' if len(marker) == 3 else 'h4'])
            # Prefix with source line to keep repeated subsection titles unique.
            p.manual_heading = (1, plain(title), slug(title) + f'-{i}')
            flow.append(p)
            i += 1
            continue
        image = re.fullmatch(r'!\[([^]]*)\]\(([^)]+)\)', line)
        if image:
            caption, relative = image.groups()
            path = source_dir / relative
            if not path.is_file():
                raise FileNotFoundError(path)
            with PILImage.open(path) as picture:
                pw, ph = picture.size
                if pw <= 0 or ph <= 0:
                    raise ValueError(f'Empty figure: {path}')
            available = PAGE_H - 2*MARGIN if landscape_next else WIDTH
            width = min(available, 420 if ph / pw > 1.1 else available)
            height = width * ph / pw
            maximum_height = 400 if landscape_next else (510 if ph / pw > 1.1 else 325)
            if height > maximum_height:
                width *= maximum_height / height
                height = maximum_height
            figure += 1
            img = Image(str(path), width=width, height=height)
            img.hAlign = 'CENTER'
            cap = paragraph(f'**Figure {figure:02d}.** {caption}', styles['caption'])
            figure_group = [Spacer(1, 5), img, cap]
            if (not landscape_next and flow and isinstance(flow[-1], Paragraph)
                    and getattr(flow[-1], 'manual_heading', (None,))[0] == 1):
                figure_group.insert(0, flow.pop())
            if landscape_next:
                flow.extend([NextPageTemplate('wide'), PageBreak()])
            flow.append(KeepTogether(figure_group))
            if landscape_next:
                flow.extend([NextPageTemplate('manual'), PageBreak()])
            landscape_next = False
            i += 1
            continue
        if line.startswith('|'):
            rows = []
            while i < len(lines) and lines[i].strip().startswith('|'):
                row = [cell.strip() for cell in lines[i].strip().strip('|').split('|')]
                if not all(re.fullmatch(r':?-{2,}:?', cell.replace(' ', '')) for cell in row):
                    rows.append(row)
                i += 1
            n = len(rows[0])
            if any(len(row) != n for row in rows):
                raise ValueError(f'Unequal table columns near {rows[0]}')
            cells = [[paragraph(cell, styles['headercell' if ri == 0 else 'cell'])
                      for cell in row] for ri, row in enumerate(rows)]
            table = LongTable(cells, colWidths=table_widths(rows), repeatRows=1,
                              hAlign='LEFT', spaceBefore=5, spaceAfter=12,
                              splitByRow=1, splitInRow=0)
            table.setStyle(TableStyle([
                ('BACKGROUND', (0, 0), (-1, 0), INK),
                ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, PAPER]),
                ('VALIGN', (0, 0), (-1, -1), 'TOP'),
                ('LEFTPADDING', (0, 0), (-1, -1), 9),
                ('RIGHTPADDING', (0, 0), (-1, -1), 9),
                ('TOPPADDING', (0, 0), (-1, -1), 6),
                ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
                ('LINEBELOW', (0, 0), (-1, 0), .6, INK),
                ('LINEBELOW', (0, 1), (-1, -1), .35, HAIRLINE),
            ]))
            flow.append(table)
            continue
        if line.startswith('```'):
            i += 1
            code = []
            while i < len(lines) and not lines[i].strip().startswith('```'):
                code.append(lines[i])
                i += 1
            flow.append(Preformatted('\n'.join(code), styles['code'], maxLineLength=88))
            i += 1
            continue
        if line.startswith('>'):
            note = []
            while i < len(lines) and lines[i].lstrip().startswith('>'):
                note.append(lines[i].lstrip()[1:].strip())
                i += 1
            flow.append(paragraph(' '.join(note), styles['note']))
            continue
        bullet = re.match(r'^(\s*)([-*+] |\d+\. )(.*)', lines[i])
        if bullet:
            indent, mark, content = bullet.groups()
            i += 1
            while i < len(lines) and lines[i].strip() and not special(lines[i]):
                content += ' ' + lines[i].strip()
                i += 1
            style = styles['number' if mark[0].isdigit() else 'bullet']
            if indent:
                style = ParagraphStyle('Nested', parent=style, leftIndent=style.leftIndent + 12)
            p = Paragraph(inline(content), style, bulletText=mark.strip() if mark[0].isdigit() else '-')
            p.manual_list_kind = 'number' if mark[0].isdigit() else 'bullet'
            flow.append(p)
            continue
        if line == '---':
            flow.append(HRFlowable(width=WIDTH, thickness=.6, color=HAIRLINE,
                                  spaceBefore=9, spaceAfter=11))
            i += 1
            continue
        content = [line]
        i += 1
        while i < len(lines) and not special(lines[i]):
            content.append(lines[i].strip())
            i += 1
        flow.append(KeepTogether([paragraph(' '.join(content), styles['body'])]))
    # Keep short procedures together so a new page does not start at step 4
    # without the task heading that gives the step its context.
    grouped = []
    index = 0
    while index < len(flow):
        item = flow[index]
        if getattr(item, 'manual_list_kind', None) == 'number':
            group = []
            while index < len(flow) and getattr(flow[index], 'manual_list_kind', None) == 'number':
                group.append(flow[index]); index += 1
            if grouped and isinstance(grouped[-1], Paragraph) and hasattr(grouped[-1], 'manual_heading'):
                group.insert(0, grouped.pop())
            grouped.append(KeepTogether(group))
        else:
            grouped.append(item); index += 1
    return grouped, figure


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=MANUAL / 'Fire-2.1-User-Manual.md')
    parser.add_argument('--output', type=Path, default=ROOT / 'output/pdf/Fire-2.1-User-Manual.pdf')
    args = parser.parse_args()
    register_fonts()
    styles = typography()
    text = args.source.read_text()
    if '<!-- manual:body -->' in text:
        text = text.split('<!-- manual:body -->', 1)[1]
    if '[FIGURE:' in text or re.search(r'\bTODO\b|\bTBD\b', text):
        raise ValueError('Manual still has editorial placeholders')
    toc = TableOfContents()
    toc.levelStyles = [ParagraphStyle('TOCLevel0', fontName='Body', fontSize=9.1,
        leading=14.5, leftIndent=0, firstLineIndent=0, rightIndent=23,
        spaceBefore=0, textColor=INK)]
    toc.dotsMinLevel = 0
    story = [Cover(args.source.parent / 'images/04-vintage-tube.png'), PageBreak(),
             paragraph('Contents', styles['h2']),
             paragraph('Start with Chapters 01-02, then follow the chapters relevant to your session. '
                       'Contents entries and PDF bookmarks are clickable.', styles['body']),
             Spacer(1, 12), toc, Spacer(1, 17),
             paragraph('This manual describes Fire 2.1.0 in development. Reference build: '
                       '9ea20d2. Illustrations show example settings, not mandatory presets. '
                       'A public download may have a different version; check Menu > Settings.', styles['note']),
             PageBreak()]
    content, figures = markdown_flowables(text, args.source.parent, styles)
    story.extend(content)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    document = ManualDocument(args.output)
    document.multiBuild(story)
    print(f'Built {args.output} ({document.page} pages, {figures} figures)')


if __name__ == '__main__':
    main()
