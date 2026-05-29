from __future__ import annotations

import argparse
from pathlib import Path
import sys

from PySide6.QtCore import QTimer
from PySide6.QtGui import QColor, QFont, QTextCursor
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QPlainTextEdit,
    QPushButton,
    QSplitter,
    QTabWidget,
    QTextBrowser,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

if __package__ is None or __package__ == "":
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    from Implementation.gui_support import GapFocus, GuiAnalysis, TokenFocus, analyze_source, show_expr
    from Implementation.gui_highlighter import TinyCheckerHighlighter
    from Implementation.core import CDefinition, CGlobal
    from Implementation.errors import ElabError, ParseError, TinyCheckerError, TypeCheckError
    from Implementation.pipeline import run_pipeline_detailed
    from Implementation.pretty import show_term
    from Implementation.proof_visualizer import ProofTreeBuilder, theorem_param_counts_from_decls
    from Implementation.reducer import Reducer
else:
    from .gui_support import GapFocus, GuiAnalysis, TokenFocus, analyze_source, show_expr
    from .gui_highlighter import TinyCheckerHighlighter
    from .core import CDefinition, CGlobal
    from .errors import ElabError, ParseError, TinyCheckerError, TypeCheckError
    from .pipeline import run_pipeline_detailed
    from .pretty import show_term
    from .proof_visualizer import ProofTreeBuilder, theorem_param_counts_from_decls
    from .reducer import Reducer


STYLESHEET = """
QMainWindow {
    background-color: #f5f5f5;
}

QPlainTextEdit {
    background-color: #ffffff;
    border: 1px solid #d8d8d8;
    border-radius: 5px;
    padding: 10px;
    selection-background-color: #d4e0f0;
    selection-color: #111111;
}

QTabWidget::pane {
    background-color: #fafafa;
    border: 1px solid #d8d8d8;
    border-radius: 5px;
}

QTabBar::tab {
    background-color: #e8e8e8;
    padding: 6px 18px;
    border: 1px solid #d8d8d8;
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 2px;
}

QTabBar::tab:selected {
    background-color: #fafafa;
    border-bottom-color: #fafafa;
}

QPushButton {
    background-color: #e8e8e8;
    border: 1px solid #cccccc;
    border-radius: 4px;
    padding: 5px 14px;
    color: #444444;
}

QPushButton:hover {
    background-color: #dddddd;
    border-color: #bbbbbb;
}

QPushButton:pressed {
    background-color: #cccccc;
}

QPushButton:disabled {
    color: #aaaaaa;
    background-color: #ececec;
}

QPushButton#typecheckButton,
QPushButton#proofButton,
QPushButton#nfButton {
    background-color: #3a7bd5;
    color: #ffffff;
    border: 1px solid #2c62aa;
    font-weight: bold;
}

QPushButton#typecheckButton:hover,
QPushButton#proofButton:hover,
QPushButton#nfButton:hover {
    background-color: #4a8be5;
}

QPushButton#typecheckButton:pressed,
QPushButton#proofButton:pressed,
QPushButton#nfButton:pressed {
    background-color: #2c62aa;
}

QPushButton#typecheckButton:disabled,
QPushButton#proofButton:disabled,
QPushButton#nfButton:disabled {
    background-color: #cccccc;
    color: #888888;
    border-color: #bbbbbb;
}

QPushButton#segLeft {
    border-top-right-radius: 0;
    border-bottom-right-radius: 0;
    border-right: none;
}

QPushButton#segRight {
    border-top-left-radius: 0;
    border-bottom-left-radius: 0;
}

QPushButton#segLeft[active=\"1\"],
QPushButton#segRight[active=\"1\"] {
    background-color: #3a7bd5;
    color: #ffffff;
    border-color: #2c62aa;
}

QPushButton#expandButton,
QPushButton#resetButton {
    background-color: transparent;
    border: 1px solid #e0e0e0;
    color: #666666;
}

QPushButton#expandButton:hover,
QPushButton#resetButton:hover {
    background-color: #e8e8e8;
    border-color: #cccccc;
}

QTextBrowser {
    background-color: #fafafa;
    border: none;
    padding: 10px;
}

QLabel#statusLabel {
    color: #666666;
    font-size: 12px;
    padding: 2px 8px;
}
"""

INSPECTOR_CSS = """
<style>
body { font-family: "Consolas", "Microsoft YaHei", sans-serif; font-size: 12px;
       color: #333; line-height: 1.6; margin: 0; }
.expr-header { background: #eef3fa; border-left: 3px solid #3a7bd5; padding: 6px 10px;
               margin-bottom: 8px; border-radius: 0 4px 4px 0; }
.expr-header .name { color: #3a7bd5; font-weight: bold; font-size: 13px; }
.expr-header .kind { color: #888; font-size: 11px; }
.expr-header .text { color: #333; margin-top: 2px; }
.card { background: #fafafa; border: 1px solid #e8e8e8; border-radius: 5px;
        margin-bottom: 8px; overflow: hidden; }
.card-title { background: #f0f0f0; padding: 4px 10px; font-weight: bold; color: #555;
              font-size: 11px; border-bottom: 1px solid #e8e8e8; }
.card-body { padding: 6px 10px; }
.entry { padding: 1px 0; }
.entry .var-name { color: #3a7bd5; }
.entry .var-type { color: #888; }
.entry .var-tag { color: #aaa; font-size: 10px; }
.branch-marker { color: #8b5ea8; font-weight: bold; }
.ih-marker { color: #4a9a6a; }
.lowered-block { background: #fafdf5; padding: 6px 10px; color: #555;
                 font-size: 11px; border-radius: 4px; word-wrap: break-word; }
.status-ok { color: #3a8b3a; }
.status-err { color: #c04040; }
.empty-hint { color: #bbb; font-style: italic; }
</style>
"""

PROOF_CSS = """
<style>
body { font-family: "Consolas", "Microsoft YaHei", sans-serif; font-size: 13px;
       color: #333; line-height: 1.6; margin: 0; }
.theorem-block { margin-bottom: 20px; }
.theorem-header { color: #1a3a6b; font-weight: bold; font-size: 14px;
                  border-left: 3px solid #3a7bd5; padding-left: 10px;
                  margin-bottom: 4px; }
.node { margin-left: 16px; border-left: 1px solid #e0d8c0; padding-left: 12px;
        margin-top: 2px; margin-bottom: 2px; }
.node-title { white-space: nowrap; }
.node-conclusion { color: #888; font-size: 12px; }
.theorem-header { white-space: nowrap; }
.induction { border-left-color: #4a9a6a; }
.induction > .node-title { color: #2a6a3a; }
.branch { border-left-color: #8b5ea8; }
.branch > .node-title { color: #5b3e78; }
.app-step { border-left-color: #d0d0d0; }
.atomic-step > .node-title { color: #888; }
</style>
"""

NF_CSS = """
<style>
body { font-family: "Consolas", "Microsoft YaHei", sans-serif; font-size: 13px;
       color: #333; line-height: 1.6; margin: 0; }
.nf-head { color: #1a3a6b; font-weight: bold; font-size: 14px;
           border-left: 3px solid #3a7bd5; padding-left: 10px;
           margin-bottom: 8px; }
.nf-item { margin: 10px 0; padding: 8px 10px; border: 1px solid #e0e0e0;
           border-radius: 6px; background: #fafafa; }
.nf-name { color: #2c62aa; font-weight: bold; margin-bottom: 3px; }
.nf-kind { color: #888; font-size: 12px; margin-left: 6px; }
.nf-type { color: #666; font-size: 12px; margin-bottom: 4px; }
.nf-value { color: #333; white-space: pre-wrap; word-break: break-word; }
.nf-empty { color: #999; padding: 20px; }
.nf-error { color: #c04040; padding: 20px; }
</style>
"""


class TinyCheckerGuiWindow(QMainWindow):
    def __init__(self, initial_source: str) -> None:
        super().__init__()
        self.setWindowTitle("TinyChecker GUI")
        self.resize(1440, 920)
        self.setStyleSheet(STYLESHEET)

        self.override_target_id: int | None = None
        self._cached_proof_nodes: list | None = None
        self._cached_nf_rows: list[tuple[str, str, str, str]] | None = None
        self._cached_compile_status: str | None = None
        self._compile_mode: bool = False

        root = QWidget()
        self.setCentralWidget(root)
        root_layout = QVBoxLayout(root)
        root_layout.setContentsMargins(10, 8, 10, 6)
        root_layout.setSpacing(6)

        top_bar = QHBoxLayout()
        top_bar.setSpacing(8)

        # --- segmented mode control ---
        seg_container = QWidget()
        seg_layout = QHBoxLayout(seg_container)
        seg_layout.setContentsMargins(0, 0, 0, 0)
        seg_layout.setSpacing(0)
        self.analysis_button = QPushButton("分析")
        self.analysis_button.setObjectName("segLeft")
        self.analysis_button.setFixedWidth(56)
        self.compile_button = QPushButton("编译")
        self.compile_button.setObjectName("segRight")
        self.compile_button.setFixedWidth(56)
        seg_layout.addWidget(self.analysis_button)
        seg_layout.addWidget(self.compile_button)

        self.typecheck_button = QPushButton("类型检查")
        self.typecheck_button.setObjectName("typecheckButton")
        self.typecheck_button.setEnabled(True)
        self.proof_button = QPushButton("生成证明")
        self.proof_button.setObjectName("proofButton")
        self.nf_button = QPushButton("求范式")
        self.nf_button.setObjectName("nfButton")

        self.expand_button = QPushButton("向外扩展")
        self.expand_button.setObjectName("expandButton")
        self.reset_button = QPushButton("回到最小")
        self.reset_button.setObjectName("resetButton")

        top_bar.addWidget(seg_container)
        top_bar.addSpacing(12)
        top_bar.addWidget(self.typecheck_button)
        top_bar.addWidget(self.proof_button)
        top_bar.addWidget(self.nf_button)
        top_bar.addStretch(1)
        top_bar.addWidget(self.expand_button)
        top_bar.addWidget(self.reset_button)
        root_layout.addLayout(top_bar)
        self._apply_seg_style("analysis")

        splitter = QSplitter()
        root_layout.addWidget(splitter, 1)

        self.editor = QPlainTextEdit()
        self.editor.setPlainText(initial_source)
        font = QFont("Consolas")
        font.setStyleHint(QFont.Monospace)
        font.setPointSize(12)
        self.editor.setFont(font)
        self.highlighter = TinyCheckerHighlighter(self.editor.document())
        splitter.addWidget(self.editor)

        self.right_tabs = QTabWidget()
        self.right_tabs.setFont(QFont("Microsoft YaHei", 9))

        self.inspector_browser = QTextBrowser()
        self.inspector_browser.setFont(font)
        self.right_tabs.addTab(self.inspector_browser, "上下文环境")

        self.proof_browser = QTextBrowser()
        self.proof_browser.setFont(QFont("Consolas", 11))

        self.nf_browser = QTextBrowser()
        self.nf_browser.setFont(QFont("Consolas", 11))

        splitter.addWidget(self.right_tabs)
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)

        self.status = QLabel("")
        self.status.setObjectName("statusLabel")
        root_layout.addWidget(self.status)

        self.timer = QTimer(self)
        self.timer.setSingleShot(True)
        self.timer.setInterval(120)
        self.timer.timeout.connect(self.refresh_analysis)

        self.editor.textChanged.connect(self.on_editor_text_changed)
        self.editor.cursorPositionChanged.connect(self.on_cursor_position_changed)
        self.analysis_button.clicked.connect(self._on_mode_changed)
        self.compile_button.clicked.connect(self._on_mode_changed)
        self.typecheck_button.clicked.connect(self._on_typecheck)
        self.proof_button.clicked.connect(self._on_generate_proof)
        self.nf_button.clicked.connect(self._on_compute_nf)
        self.expand_button.clicked.connect(self.expand_target)
        self.reset_button.clicked.connect(self.reset_target)

        self._apply_mode_ui()
        self.refresh_analysis()

    @property
    def is_compile_mode(self) -> bool:
        return self._compile_mode

    def _apply_seg_style(self, active: str) -> None:
        active_color = "background-color:#3a7bd5; color:#ffffff; border-color:#2c62aa;"
        inactive_color = "background-color:#e8e8e8; color:#444444; border-color:#cccccc;"
        if active == "analysis":
            self.analysis_button.setStyleSheet(active_color)
            self.compile_button.setStyleSheet(inactive_color)
        else:
            self.analysis_button.setStyleSheet(inactive_color)
            self.compile_button.setStyleSheet(active_color)

    def _on_mode_changed(self) -> None:
        sender = self.sender()
        if sender is self.analysis_button:
            self._compile_mode = False
            self._apply_seg_style("analysis")
        else:
            self._compile_mode = True
            self._apply_seg_style("compile")
        self._apply_mode_ui()
        self.schedule_refresh()

    def _apply_mode_ui(self) -> None:
        self.typecheck_button.setVisible(not self.is_compile_mode)
        self.proof_button.setVisible(self.is_compile_mode)
        self.nf_button.setVisible(self.is_compile_mode)
        self.expand_button.setEnabled(not self.is_compile_mode)
        self.reset_button.setEnabled(not self.is_compile_mode and self.override_target_id is not None)
        self.right_tabs.clear()
        if self.is_compile_mode:
            self.right_tabs.addTab(self.proof_browser, "证明视图")
            self.right_tabs.addTab(self.nf_browser, "范式视图")
            if self._cached_proof_nodes is None:
                self.proof_browser.setHtml("<p style='color:#999; padding:20px;'>点击「生成证明」按钮查看证明</p>")
            self.render_nf()
        else:
            self.right_tabs.addTab(self.inspector_browser, "上下文环境")

    def _on_compile_failed(self, status: str) -> None:
        self._cached_compile_status = status
        self._cached_proof_nodes = None
        self._cached_nf_rows = None
        self.proof_browser.setHtml(f"{PROOF_CSS}<p style='color:#c04040; padding:20px;'>{self._escape(status)}</p>")
        self.render_nf()
        self.status.setText("编译失败")

    def _compile_snapshot(self):
        source = self.editor.toPlainText()
        tokens, decls, _, core_decls, global_ctx = run_pipeline_detailed(source)
        return tokens, decls, core_decls, global_ctx

    def _build_nf_rows(self, core_decls, global_ctx) -> list[tuple[str, str, str, str]]:
        reducer = Reducer(global_ctx)
        rows: list[tuple[str, str, str, str]] = []
        for decl in core_decls:
            if not isinstance(decl, CDefinition) or decl.value is None:
                continue
            nf = reducer.nf(CGlobal(decl.name))
            rows.append((decl.name, decl.kind, show_term(decl.typ), show_term(nf)))
        return rows

    def _on_generate_proof(self) -> None:
        if self.is_compile_mode:
            self.right_tabs.setCurrentWidget(self.proof_browser)
        self.proof_browser.setHtml("<p style='color:#999; padding:20px;'>生成证明中...</p>")
        try:
            _, decls, core_decls, global_ctx = self._compile_snapshot()
            self._cached_compile_status = "typecheck: OK"
            param_counts = theorem_param_counts_from_decls(decls)
            self._cached_proof_nodes = ProofTreeBuilder(core_decls, global_ctx, param_counts).build()
            self.status.setText("类型检查通过，证明已生成")
            analysis = self._current_analysis_with_cache()
            self.render_proof(analysis)
        except (TinyCheckerError, ParseError, ElabError, TypeCheckError) as exc:
            self._on_compile_failed(f"typecheck error: {exc}")

    def _on_compute_nf(self) -> None:
        if self.is_compile_mode:
            self.right_tabs.setCurrentWidget(self.nf_browser)
        self.nf_browser.setHtml(f"{NF_CSS}<p class='nf-empty'>求范式中...</p>")
        try:
            _, _, core_decls, global_ctx = self._compile_snapshot()
            self._cached_compile_status = "typecheck: OK"
            self._cached_nf_rows = self._build_nf_rows(core_decls, global_ctx)
            self.status.setText("类型检查通过，范式已生成")
            self.render_nf()
        except (TinyCheckerError, ParseError, ElabError, TypeCheckError) as exc:
            self._on_compile_failed(f"typecheck error: {exc}")

    def _on_typecheck(self) -> None:
        try:
            self._compile_snapshot()
            self._cached_compile_status = "typecheck: OK"
            self.status.setText("类型检查通过")
        except (TinyCheckerError, ParseError, ElabError, TypeCheckError) as exc:
            self._cached_compile_status = f"typecheck error: {exc}"
            self.status.setText("类型检查失败")
        self.refresh_analysis()

    def on_editor_text_changed(self) -> None:
        self._cached_compile_status = None
        self._cached_proof_nodes = None
        self._cached_nf_rows = None
        self.schedule_refresh()

    def schedule_refresh(self) -> None:
        self.timer.start()

    def on_cursor_position_changed(self) -> None:
        self.override_target_id = None
        self.schedule_refresh()

    def expand_target(self) -> None:
        source = self.editor.toPlainText()
        cursor_pos = self.editor.textCursor().position()
        analysis = analyze_source(
            source,
            cursor_pos,
            compile_mode=False,
            target_override_id=self.override_target_id,
        )
        if analysis.target_parent_id is not None:
            self.override_target_id = analysis.target_parent_id
            self.refresh_analysis()

    def reset_target(self) -> None:
        self.override_target_id = None
        self.refresh_analysis()

    def refresh_analysis(self) -> None:
        source = self.editor.toPlainText()
        cursor_pos = self.editor.textCursor().position()
        analysis = analyze_source(
            source,
            cursor_pos,
            compile_mode=False,
            target_override_id=self.override_target_id,
        )
        if analysis.target_expr is None and analysis.target_name_site is None:
            self.override_target_id = None

        analysis = GuiAnalysis(
            source=analysis.source,
            tokens=analysis.tokens,
            artifacts=analysis.artifacts,
            focus=analysis.focus,
            decl=analysis.decl,
            target_expr=analysis.target_expr,
            target_name_site=analysis.target_name_site,
            target_span=analysis.target_span,
            target_offsets=analysis.target_offsets,
            probe=analysis.probe,
            minimal_expr=analysis.minimal_expr,
            target_parent_id=analysis.target_parent_id,
            compile_status=self._cached_compile_status,
            proof_nodes=self._cached_proof_nodes,
            error=analysis.error,
        )

        self.render_highlight(analysis)
        if self.is_compile_mode:
            self.status.setText("编译模式")
            self.render_proof(analysis)
            self.render_nf()
        else:
            self.render_inspector(analysis)

    def _current_analysis_with_cache(self) -> GuiAnalysis:
        source = self.editor.toPlainText()
        cursor_pos = self.editor.textCursor().position()
        analysis = analyze_source(
            source,
            cursor_pos,
            compile_mode=False,
            target_override_id=self.override_target_id,
        )
        return GuiAnalysis(
            source=analysis.source,
            tokens=analysis.tokens,
            artifacts=analysis.artifacts,
            focus=analysis.focus,
            decl=analysis.decl,
            target_expr=analysis.target_expr,
            target_name_site=analysis.target_name_site,
            target_span=analysis.target_span,
            target_offsets=analysis.target_offsets,
            probe=analysis.probe,
            minimal_expr=analysis.minimal_expr,
            target_parent_id=analysis.target_parent_id,
            compile_status=self._cached_compile_status,
            proof_nodes=self._cached_proof_nodes,
            error=analysis.error,
        )

    def render_highlight(self, analysis: GuiAnalysis) -> None:
        selections = []
        if analysis.target_offsets is not None:
            start, end = analysis.target_offsets
            cursor = self.editor.textCursor()
            cursor.setPosition(start)
            cursor.setPosition(end, QTextCursor.KeepAnchor)
            selection = QTextEdit.ExtraSelection()
            selection.cursor = cursor
            selection.format.setBackground(QColor("#f2e6b8"))
            selection.format.setForeground(QColor("#111111"))
            selections.append(selection)
        self.editor.setExtraSelections(selections)

    def render_inspector(self, analysis: GuiAnalysis) -> None:
        if analysis.error is not None:
            self.status.setText("分析失败")
            self.inspector_browser.setPlainText(analysis.error)
            return

        parts: list[str] = [INSPECTOR_CSS]

        # --- expression header ---
        parts.append('<div class="expr-header">')
        if analysis.target_name_site is not None:
            parts.append(f'<div class="name">{self._escape(analysis.target_name_site.name)}</div>')
            parts.append(f'<div class="kind">{self._escape(analysis.target_name_site.role)}</div>')
            if analysis.minimal_expr is not None:
                parts.append(f'<div class="text">{self._escape(show_expr(analysis.minimal_expr))}</div>')
        elif analysis.target_expr is not None:
            parts.append(f'<div class="text">{self._escape(show_expr(analysis.target_expr))}</div>')
        else:
            parts.append('<div class="kind">无</div>')
        if analysis.compile_status is not None:
            cls = "status-ok" if analysis.compile_status.startswith("typecheck: OK") else "status-err"
            parts.append(f'<div class="{cls}" style="margin-top:4px;">{self._escape(analysis.compile_status)}</div>')
        parts.append("</div>")

        probe = analysis.probe
        if probe is None:
            parts.append('<div class="card"><div class="card-body"><span class="empty-hint">光标未命中表达式</span></div></div>')
            self.inspector_browser.setHtml("".join(parts))
            self.status.setText("分析完成")
            self.expand_button.setEnabled(False)
            self.reset_button.setEnabled(self.override_target_id is not None)
            return

        # --- local context card ---
        parts.append('<div class="card">')
        parts.append('<div class="card-title">局部上下文</div>')
        parts.append('<div class="card-body">')
        if probe.local_context:
            for entry in probe.local_context:
                parts.append(f'<div class="entry"><span class="var-name">{self._escape(entry.name)}</span> <span class="var-type">: {self._escape(entry.typ)}</span></div>')
        else:
            parts.append('<span class="empty-hint">空</span>')
        if probe.resolved_name:
            parts.append(f'<div style="margin-top:4px; color:#888; font-size:11px;">解析: {self._escape(probe.resolved_name)}</div>')
        parts.append("</div></div>")

        # --- branch info card ---
        if probe.branch_stack:
            cur = probe.branch_stack[-1]
            parts.append('<div class="card">')
            parts.append('<div class="card-title">分支信息 — <span class="branch-marker">' + self._escape(cur.constructor) + "</span></div>")
            parts.append('<div class="card-body">')
            if cur.fields:
                parts.append('<div style="margin-bottom:4px;">字段: ' + ", ".join(f'<span class="var-name">{self._escape(f)}</span>' for f in cur.fields) + "</div>")
            if cur.ihs:
                parts.append('<div>归纳假设: ' + ", ".join(f'<span class="ih-marker">{self._escape(h)}</span>' for h in cur.ihs) + "</div>")
            if not cur.fields and not cur.ihs:
                parts.append('<span class="empty-hint">无字段和归纳假设</span>')
            if len(probe.branch_stack) > 1:
                outer = " -> ".join(f.constructor for f in probe.branch_stack[:-1])
                parts.append(f'<div style="margin-top:4px; color:#999; font-size:10px;">外层: {self._escape(outer)}</div>')
            parts.append("</div></div>")

        # --- global context card ---
        parts.append('<div class="card">')
        parts.append('<div class="card-title">全局上下文</div>')
        parts.append('<div class="card-body">')
        if probe.global_context:
            for entry in probe.global_context:
                parts.append(
                    f'<div class="entry"><span class="var-name">{self._escape(entry.name)}</span>'
                    f' <span class="var-type">: {self._escape(entry.typ)}</span>'
                    f' <span class="var-tag">[{self._escape(entry.tag)}]</span></div>'
                )
        else:
            parts.append('<span class="empty-hint">空</span>')
        parts.append("</div></div>")

        self.status.setText(
            "分析完成"
            if not isinstance(analysis.focus, GapFocus) or analysis.target_expr is not None or analysis.target_name_site is not None
            else "已回退到最近表达式"
        )
        self.inspector_browser.setHtml("".join(parts))
        self.expand_button.setEnabled(analysis.target_parent_id is not None)
        self.reset_button.setEnabled(self.override_target_id is not None)

    def render_proof(self, analysis: GuiAnalysis) -> None:
        if not self.is_compile_mode:
            return
        nodes = analysis.proof_nodes
        if nodes is None:
            nodes = self._cached_proof_nodes
        if nodes is None:
            if analysis.compile_status is not None and not analysis.compile_status.startswith("typecheck: OK"):
                self.proof_browser.setHtml(
                    f"{PROOF_CSS}<p style='color:#c04040; padding:20px;'>{self._escape(analysis.compile_status)}</p>"
                )
            else:
                self.proof_browser.setHtml(
                    "<p style='color:#999; padding:20px;'>点击「生成证明」按钮查看证明</p>"
                )
            return

        if not nodes:
            self.proof_browser.setHtml(
                "<p style='color:#999; padding:20px;'>当前文件中没有 theorem 或 claim</p>"
            )
            return

        html = PROOF_CSS
        for node in nodes:
            html += self._proof_node_html(node)
        self.proof_browser.setHtml(html)

    def render_nf(self) -> None:
        if not self.is_compile_mode:
            return
        if self._cached_compile_status is not None and not self._cached_compile_status.startswith("typecheck: OK"):
            self.nf_browser.setHtml(
                f"{NF_CSS}<p class='nf-error'>{self._escape(self._cached_compile_status)}</p>"
            )
            return
        if self._cached_nf_rows is None:
            self.nf_browser.setHtml(f"{NF_CSS}<p class='nf-empty'>点击「求范式」按钮查看结果</p>")
            return
        if not self._cached_nf_rows:
            self.nf_browser.setHtml(
                f"{NF_CSS}<div class='nf-head'>范式视图</div><p class='nf-empty'>没有可求值的声明（仅展示有值定义）</p>"
            )
            return
        parts: list[str] = [NF_CSS, "<div class='nf-head'>范式视图</div>"]
        for name, kind, typ, nf in self._cached_nf_rows:
            parts.append("<div class='nf-item'>")
            parts.append(
                f"<div><span class='nf-name'>{self._escape(name)}</span>"
                f"<span class='nf-kind'>[{self._escape(kind)}]</span></div>"
            )
            parts.append(f"<div class='nf-type'>type: {self._escape(typ)}</div>")
            parts.append(f"<div class='nf-value'>{self._escape(nf)}</div>")
            parts.append("</div>")
        self.nf_browser.setHtml("".join(parts))

    def _proof_node_html(self, node, depth: int = 0) -> str:
        title = node.title or ""
        conclusion = node.conclusion or ""

        css_class = ""
        if "归纳法" in title:
            css_class = "induction"
        elif title.startswith("分支"):
            css_class = "branch"
        elif "推导即得" in title or "构造 " in title or "上一步" in title:
            css_class = "app-step"
        elif "引用" in title or "使用 " in title or "直接给出" in title:
            css_class = "atomic-step"

        if not title and not conclusion:
            parts = ""
            for child in node.children:
                parts += self._proof_node_html(child, depth)
            return parts

        html = ""
        if depth == 0:
            anchor_parts = title.split(None, 1)
            anchor_name = anchor_parts[1] if len(anchor_parts) > 1 else anchor_parts[0]
            html += f'<div class="theorem-block"><a id="thm-{self._escape(anchor_name)}"/>'
            if conclusion:
                html += f'<div class="theorem-header">{self._escape(title)}: {self._escape(conclusion)}</div>'
            else:
                html += f'<div class="theorem-header">{self._escape(title)}</div>'
        else:
            html += f'<div class="node {css_class}">'
            if conclusion:
                html += f'<div class="node-title">{self._escape(title)}</div>'
                html += f'<div class="node-conclusion">{self._escape(conclusion)}</div>'
            else:
                html += f'<div class="node-title">{self._escape(title)}</div>'

        for child in node.children:
            html += self._proof_node_html(child, depth + 1)

        html += "</div>"
        return html

    @staticmethod
    def _escape(text: str) -> str:
        return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def load_initial_source(path: str | None) -> str:
    if path is None:
        return (
            "inductive Nat {\n"
            "| zero: Nat\n"
            "| succ: Nat -> Nat\n"
            "};\n\n"
            "fun add1 (n:Nat): Nat {\n"
            "  match n as q in Nat return Nat with\n"
            "  | zero => succ zero\n"
            "  | succ k => succ (succ k)\n"
            "  end\n"
            "};\n"
        )
    return Path(path).read_text(encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="TinyChecker GUI")
    parser.add_argument("path", nargs="?", help="optional source file path")
    args = parser.parse_args()

    app = QApplication([])
    window = TinyCheckerGuiWindow(load_initial_source(args.path))
    window.show()
    app.exec()


if __name__ == "__main__":
    main()
