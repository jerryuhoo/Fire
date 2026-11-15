"""Japanese line wrapping that preserves ReportLab's styled text fragments."""

from reportlab.lib.textsplit import ALL_CANNOT_END, ALL_CANNOT_START
from reportlab.platypus.paragraph import (
    Paragraph, ParaLines, _handleBulletWidth, cjkU, makeCJKParaLine,
)

NO_START = ALL_CANNOT_START + '，：；？！…”’》〉'
NO_END = ALL_CANNOT_END


def legal_break(left, right):
    if right in NO_START or left in NO_END:
        return False
    # Keep Latin words, parameter IDs and numeric units together when they fit.
    if (left and right and ord(left) < 0x2e80 and ord(right) < 0x2e80
            and not left.isspace() and not right.isspace()):
        return left in '/-' or right in '(['
    return True


class JapaneseParagraph(Paragraph):
    def breakLinesCJK(self, maxWidths):
        if getattr(self, '_splitpara', False) and hasattr(self, 'blPara'):
            return self.blPara
        widths = list(maxWidths) if isinstance(maxWidths, (list, tuple)) else [maxWidths]
        _handleBulletWidth(self.bulletText, self.style, widths)
        self.height = 0
        glyphs = [cjkU(char, frag, 'utf8') for frag in self.frags for char in frag.text]
        lines = []
        start = 0
        while start < len(glyphs):
            if glyphs[start].isspace():
                start += 1
                continue
            available = widths[min(len(lines), len(widths) - 1)]
            end, used, last_break = start, 0.0, None
            while end < len(glyphs) and used + glyphs[end].width <= available + 1e-6:
                used += glyphs[end].width
                end += 1
                if end == len(glyphs) or legal_break(glyphs[end - 1], glyphs[end]):
                    last_break = end
            if end < len(glyphs):
                # Extremely long Latin tokens may have no legal boundary.
                end = last_break or max(start + 1, end)
            draw_end = end
            while draw_end > start and glyphs[draw_end - 1].isspace():
                draw_end -= 1
            line_glyphs = glyphs[start:draw_end]
            used = sum(g.width for g in line_glyphs)
            lines.append(makeCJKParaLine(line_glyphs, available, used,
                                        available - used, False, False))
            start = end
        return ParaLines(kind=1, lines=lines)
