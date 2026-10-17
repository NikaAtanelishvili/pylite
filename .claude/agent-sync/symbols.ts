/**
 * Which top-level symbols an Edit touches, so the hook can claim `file#symbol` instead of the whole file.
 * Parses with the TypeScript compiler (TS, TSX, JS, JSX). Returns null when the edit can't be pinned to
 * symbols (a statement outside any declaration, text not found, another language, no `typescript` installed),
 * and the hook then claims the whole file.
 */
import { readFileSync } from "node:fs";
import type * as TS from "typescript";

type TypeScript = typeof TS;

const EXTENSIONS = /\.(m|c)?(ts|tsx|js|jsx)$/;

let ts: TypeScript | null | undefined;
async function loadTypeScript(): Promise<TypeScript | null> {
  if (ts === undefined) {
    try {
      ts = (await import("typescript")).default as TypeScript;
    } catch {
      ts = null;
    }
  }
  return ts;
}

interface Range {
  start: number;
  end: number;
}

/** Named declarations at the top of the file, plus class members as `Class.member`. */
function declarations(ts: TypeScript, source: TS.SourceFile): { name: string; start: number; end: number; members: { name: string; start: number; end: number }[] }[] {
  const out: ReturnType<typeof declarations> = [];
  const span = (n: TS.Node) => ({ start: n.getFullStart(), end: n.getEnd() });
  for (const stmt of source.statements) {
    if (ts.isFunctionDeclaration(stmt) || ts.isInterfaceDeclaration(stmt) || ts.isTypeAliasDeclaration(stmt) || ts.isEnumDeclaration(stmt)) {
      if (stmt.name) out.push({ name: stmt.name.text, ...span(stmt), members: [] });
    } else if (ts.isClassDeclaration(stmt) && stmt.name) {
      const cls = stmt.name.text;
      const members = stmt.members.flatMap((m) => {
        const name = ts.isConstructorDeclaration(m) ? "constructor" : m.name && ts.isIdentifier(m.name) ? m.name.text : null;
        return name ? [{ name: `${cls}.${name}`, ...span(m) }] : [];
      });
      out.push({ name: cls, ...span(stmt), members });
    } else if (ts.isVariableStatement(stmt)) {
      // `export const fetchUser = ...`: one statement, usually one name. Several names claim them all.
      const names = stmt.declarationList.declarations.flatMap((d) => (ts.isIdentifier(d.name) ? [d.name.text] : []));
      if (names.length) out.push({ name: names.join(","), ...span(stmt), members: [] });
    } else if (ts.isImportDeclaration(stmt)) {
      // Imports sit next to each other, so two agents adding one would still clash in git: one shared symbol.
      out.push({ name: "imports", ...span(stmt), members: [] });
    }
  }
  return out;
}

/** Where each `old` string sits in `text`; null if any isn't found exactly once (the Edit itself would fail). */
function locate(text: string, olds: string[]): Range[] | null {
  const ranges: Range[] = [];
  for (const old of olds) {
    if (!old) return null;
    const start = text.indexOf(old);
    if (start === -1 || text.indexOf(old, start + 1) !== -1) return null;
    ranges.push({ start, end: start + old.length });
  }
  return ranges;
}

export function symbolsInText(ts: TypeScript, fileName: string, text: string, olds: string[]): string[] | null {
  const ranges = locate(text, olds);
  if (!ranges) return null;
  const source = ts.createSourceFile(fileName, text, ts.ScriptTarget.Latest, true);
  const decls = declarations(ts, source);
  const found = new Set<string>();
  for (const r of ranges) {
    const decl = decls.find((d) => d.start <= r.start && r.end <= d.end);
    if (!decl) return null;
    const member = decl.members.find((m) => m.start <= r.start && r.end <= m.end);
    for (const name of (member?.name ?? decl.name).split(",")) found.add(name);
  }
  return [...found].sort();
}

/** The symbols an Edit / MultiEdit on `absPath` touches, or null to claim the whole file. */
export async function symbolsForEdit(absPath: string, olds: string[]): Promise<string[] | null> {
  if (!EXTENSIONS.test(absPath) || olds.length === 0) return null;
  const typescript = await loadTypeScript();
  if (!typescript) return null;
  let text: string;
  try {
    text = readFileSync(absPath, "utf8");
  } catch {
    return null;
  }
  return symbolsInText(typescript, absPath, text, olds);
}
