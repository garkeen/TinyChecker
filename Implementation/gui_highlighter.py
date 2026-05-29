from __future__ import annotations

from PySide6.QtCore import QRegularExpression
from PySide6.QtGui import QColor, QFont, QSyntaxHighlighter, QTextCharFormat


def _fmt(color: str, *, bold: bool = False, italic: bool = False) -> QTextCharFormat:
    f = QTextCharFormat()
    f.setForeground(QColor(color))
    if bold:
        f.setFontWeight(QFont.Bold)
    if italic:
        f.setFontItalic(True)
    return f


class TinyCheckerHighlighter(QSyntaxHighlighter):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self._rules: list[tuple[QRegularExpression, QTextCharFormat]] = []

        keyword_patterns = [
            r"\bvar\b", r"\bfun\b", r"\bclaim\b", r"\btheorem\b",
            r"\binductive\b", r"\bsum\b", r"\bproduct\b", r"\baxiom\b",
            r"\bexample\b", r"\blet\b", r"\bin\b", r"\bmatch\b",
            r"\bcase\b", r"\bas\b", r"\bbind\b", r"\breturn\b",
            r"\bwith\b", r"\bof\b", r"\bend\b",
        ]
        keyword_fmt = _fmt("#5b3e9e", bold=True)
        for pat in keyword_patterns:
            self._rules.append((QRegularExpression(pat), keyword_fmt))

        type_fmt = _fmt("#007a7a")
        for pat in [r"\bType\b", r"\bProp\b"]:
            self._rules.append((QRegularExpression(pat), type_fmt))

        comment_fmt = _fmt("#708090", italic=True)
        self._rules.append((QRegularExpression(r"#[^\n]*"), comment_fmt))

        ctor_fmt = _fmt("#1a6e1a")
        self._rules.append((QRegularExpression(r"^\s*\|[ \t]*[a-zA-Z_][a-zA-Z0-9_]*"), ctor_fmt))

        operator_fmt = _fmt("#8b6914")
        for pat in [r"=>", r"->", r"==", r"\\", r":", r"=", r";", r",", r"\|",
                     r"\{", r"\}", r"\(", r"\)", r"\[", r"\]", r"<", r">"]:
            self._rules.append((QRegularExpression(pat), operator_fmt))

    def highlightBlock(self, text: str) -> None:
        for pattern, fmt in self._rules:
            it = pattern.globalMatch(text)
            while it.hasNext():
                m = it.next()
                self.setFormat(m.capturedStart(), m.capturedLength(), fmt)
