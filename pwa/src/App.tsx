import { useEffect, useMemo, useState } from "react";

const COLS = 7;
const ROWS = 6;
type Piece = 0 | 1 | 2;
type Board = Piece[][];
type MoveState = "running" | "win" | "draw" | "invalid";
type Difficulty = "easy" | "medium" | "hard";
type Mode = "dark" | "light" | "two" | "demo";

interface GameState {
  board: Board;
  turn: 1 | 2;
  winner: 0 | 1 | 2;
  moveCount: number;
  history: { board: Board; turn: 1 | 2; winner: 0 | 1 | 2 }[];
  moves: number[];
}

interface PersistedState {
  game: GameState;
  savedGame: GameState | null;
  difficulty: Difficulty;
  mode: Mode;
}

const STORAGE_KEY = "exact-four-in-a-row-pwa-v1";
const DIRS: [number, number][] = [[1, 0], [0, 1], [1, 1], [1, -1]];

function emptyBoard(): Board {
  return Array.from({ length: ROWS }, () => Array<Piece>(COLS).fill(0));
}

function newGame(): GameState {
  const g: GameState = { board: emptyBoard(), turn: 1, winner: 0, moveCount: 0, history: [], moves: [] };
  g.history.push({ board: emptyBoard(), turn: 1, winner: 0 });
  return g;
}

function findDropRow(board: Board, col: number): number {
  for (let row = ROWS - 1; row >= 0; row--) if (board[row][col] === 0) return row;
  return -1;
}

function inBounds(col: number, row: number) { return col >= 0 && col < COLS && row >= 0 && row < ROWS; }

function hasFourFrom(board: Board, col: number, row: number, piece: Piece): boolean {
  for (const [dx, dy] of DIRS) {
    let count = 1;
    for (let s = 1; s < 4; s++) {
      const c = col + dx * s, r = row + dy * s;
      if (!inBounds(c, r) || board[r][c] !== piece) break;
      count++;
    }
    for (let s = 1; s < 4; s++) {
      const c = col - dx * s, r = row - dy * s;
      if (!inBounds(c, r) || board[r][c] !== piece) break;
      count++;
    }
    if (count >= 4) return true;
  }
  return false;
}

function isFull(board: Board): boolean {
  for (let col = 0; col < COLS; col++) if (board[0][col] === 0) return false;
  return true;
}

function collectValidCols(g: GameState): number[] {
  if (g.winner !== 0) return [];
  return Array.from({ length: COLS }, (_, c) => c).filter(c => findDropRow(g.board, c) >= 0);
}

function cloneBoard(board: Board): Board { return board.map(row => [...row] as Piece[]); }

function applyMove(g: GameState, col: number): { next: GameState; state: MoveState } {
  const row = findDropRow(g.board, col);
  if (row < 0 || g.winner !== 0) return { next: g, state: "invalid" };
  const board = cloneBoard(g.board);
  board[row][col] = g.turn;
  let winner: 0 | 1 | 2 = 0;
  let state: MoveState = "running";
  if (hasFourFrom(board, col, row, g.turn)) { winner = g.turn; state = "win"; }
  else if (isFull(board)) state = "draw";
  const nextTurn: 1 | 2 = state === "running" ? (g.turn === 1 ? 2 : 1) : g.turn;
  const next: GameState = {
    board, turn: nextTurn, winner, moveCount: g.moveCount + 1,
    moves: [...g.moves, col],
    history: [...g.history, { board: cloneBoard(board), turn: nextTurn, winner }]
  };
  return { next, state };
}

function undoMove(g: GameState): GameState {
  if (g.history.length <= 1) return g;
  const history = g.history.slice(0, -1);
  const prev = history[history.length - 1];
  return { board: cloneBoard(prev.board), turn: prev.turn, winner: prev.winner, moveCount: g.moveCount - 1, moves: g.moves.slice(0, -1), history };
}

function scoreWindow(cells: Piece[], player: Piece): number {
  const other: Piece = player === 1 ? 2 : 1;
  let mine = 0, theirs = 0, empty = 0;
  for (const c of cells) { if (c === player) mine++; else if (c === other) theirs++; else empty++; }
  if (mine === 4) return 100000;
  if (theirs === 4) return -100000;
  if (mine === 3 && empty === 1) return 80;
  if (mine === 2 && empty === 2) return 12;
  if (theirs === 3 && empty === 1) return -100;
  if (theirs === 2 && empty === 2) return -10;
  return 0;
}

function evaluatePosition(board: Board, player: Piece): number {
  let score = 0;
  for (let row = 0; row < ROWS; row++) if (board[row][Math.floor(COLS / 2)] === player) score += 6;
  for (let row = 0; row < ROWS; row++) {
    for (let col = 0; col < COLS; col++) {
      for (const [dx, dy] of DIRS) {
        const cells: Piece[] = [];
        for (let i = 0; i < 4; i++) {
          const c = col + dx * i, r = row + dy * i;
          if (!inBounds(c, r)) break;
          cells.push(board[r][c]);
        }
        if (cells.length === 4) score += scoreWindow(cells, player);
      }
    }
  }
  return score;
}

function aiPickMove(g: GameState, difficulty: Difficulty): number {
  const valid = collectValidCols(g);
  if (valid.length === 0) return -1;
  const level = difficulty === "easy" ? 1 : difficulty === "medium" ? 2 : 3;
  if (level === 1 && Math.random() < 0.4) return valid[Math.floor(Math.random() * valid.length)];
  const player = g.turn;
  const scores: number[] = valid.map(col => {
    const { next, state } = applyMove(g, col);
    if (state === "win") return 1000000;
    let score = evaluatePosition(next.board, player);
    if (level >= 2) {
      const replies = collectValidCols(next);
      for (const rc of replies) {
        const { state: rs } = applyMove(next, rc);
        if (rs === "win") { score -= 900000; break; }
      }
    }
    if (level >= 3) {
      const replies = collectValidCols(next);
      let bestReply = -Infinity;
      for (const rc of replies) {
        const { next: rg } = applyMove(next, rc);
        const s = evaluatePosition(rg.board, player === 1 ? 2 : 1);
        if (s > bestReply) bestReply = s;
      }
      if (bestReply !== -Infinity) score -= bestReply / 2;
    }
    score -= Math.abs(col - Math.floor(COLS / 2));
    return score;
  });
  let best = -Infinity, bestIdx = 0;
  for (let i = 0; i < scores.length; i++) if (scores[i] > best) { best = scores[i]; bestIdx = i; }
  return valid[bestIdx];
}

function findWinCells(board: Board, winner: Piece): [number, number][] {
  for (let row = 0; row < ROWS; row++) {
    for (let col = 0; col < COLS; col++) {
      if (board[row][col] !== winner) continue;
      for (const [dx, dy] of DIRS) {
        const run: [number, number][] = [[row, col]];
        for (let s = 1; s < 4; s++) {
          const c = col + dx * s, r = row + dy * s;
          if (!inBounds(c, r) || board[r][c] !== winner) break;
          run.push([r, c]);
        }
        if (run.length === 4) return run;
      }
    }
  }
  return [];
}

function loadState(): PersistedState {
  const fallback: PersistedState = { game: newGame(), savedGame: null, difficulty: "medium", mode: "dark" };
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return fallback;
    const parsed = JSON.parse(raw) as Partial<PersistedState>;
    // migrate old "p1" mode
    const rawMode = parsed.mode as string;
    const mode: Mode = rawMode === "p1" ? "dark" : (rawMode as Mode) || fallback.mode;
    return { ...fallback, ...parsed, mode };
  } catch { return fallback; }
}

function playerLabel(p: 1 | 2): string { return p === 1 ? "Dark" : "Light"; }

function gameStatus(g: GameState, mode: Mode, thinking: boolean): string {
  if (g.winner !== 0) return `${playerLabel(g.winner)} wins!`;
  if (g.winner === 0 && collectValidCols(g).length === 0) return "Draw!";
  if (thinking) return "Opponent is thinking…";
  if (mode === "demo") return "AI demo running.";
  return `${playerLabel(g.turn)} to move.`;
}

export default function App() {
  const initial = useMemo(loadState, []);
  const [game, setGame] = useState<GameState>(initial.game);
  const [savedGame, setSavedGame] = useState<GameState | null>(initial.savedGame);
  const [difficulty, setDifficulty] = useState<Difficulty>(initial.difficulty);
  const [mode, setMode] = useState<Mode>(initial.mode);
  const [thinking, setThinking] = useState(false);
  const [message, setMessage] = useState("");
  const [page, setPage] = useState<"game" | "about">("game");
  const [showHistory, setShowHistory] = useState(true);
  const [reviewIdx, setReviewIdx] = useState<number | null>(null);

  const totalMoves = game.history.length - 1; // history[0] = initial
  const isReviewing = reviewIdx !== null;
  const displayIdx = reviewIdx ?? totalMoves;

  const displaySnap = isReviewing ? game.history[Math.min(reviewIdx!, game.history.length - 1)] : { board: game.board, turn: game.turn, winner: game.winner };
  const displayBoard = displaySnap.board;
  const displayWinner = displaySnap.winner;

  const validCols = useMemo(() => new Set(collectValidCols(game)), [game]);

  const winCells = useMemo(() => {
    if (displayWinner !== 0) return new Set(findWinCells(displayBoard, displayWinner).map(([r, c]) => `${r},${c}`));
    return new Set<string>();
  }, [displayBoard, displayWinner]);

  // Last move highlight during review
  const reviewLastCol = isReviewing && displayIdx > 0 ? game.moves[displayIdx - 1] : (!isReviewing && game.moves.length > 0 ? game.moves[game.moves.length - 1] : -1);
  const reviewPrevBoard = isReviewing && displayIdx > 0 ? game.history[displayIdx - 1].board : (!isReviewing && game.history.length >= 2 ? game.history[game.history.length - 2].board : null);
  const reviewLastRow = reviewLastCol >= 0 && reviewPrevBoard ? findDropRow(reviewPrevBoard, reviewLastCol) : -1;

  const isHumanTurn = game.winner === 0 && !isReviewing && (
    mode === "two" ||
    (mode === "dark" && game.turn === 1) ||
    (mode === "light" && game.turn === 2)
  );

  function goReview(idx: number | null) { setReviewIdx(idx); }

  useEffect(() => {
    const s: PersistedState = { game, savedGame, difficulty, mode };
    localStorage.setItem(STORAGE_KEY, JSON.stringify(s));
  }, [game, savedGame, difficulty, mode]);

  useEffect(() => {
    if (game.winner !== 0 || collectValidCols(game).length === 0) { setThinking(false); return; }
    const shouldAI = mode === "demo" ||
      (mode === "dark" && game.turn === 2) ||
      (mode === "light" && game.turn === 1);
    if (!shouldAI) return;
    setThinking(true);
    const id = window.setTimeout(() => {
      const col = aiPickMove(game, difficulty);
      if (col < 0) { setThinking(false); return; }
      const { next } = applyMove(game, col);
      setGame(next);
      setMessage(`${playerLabel(game.turn)}: column ${col + 1}`);
      setThinking(false);
    }, mode === "demo" ? 400 : 500);
    return () => clearTimeout(id);
  }, [game, mode, difficulty]);

  function dropPiece(col: number) {
    if (!isHumanTurn || thinking || !validCols.has(col)) return;
    const { next, state } = applyMove(game, col);
    if (state === "invalid") return;
    setMessage(state === "win" ? `${playerLabel(game.turn)} wins!` : state === "draw" ? "Draw!" : `${playerLabel(game.turn)}: column ${col + 1}`);
    setGame(next);
    setReviewIdx(null);
  }

  function handleNew() { setGame(newGame()); setMessage("New game."); setThinking(false); setReviewIdx(null); }
  function handleUndo() {
    let g = undoMove(game);
    if (mode !== "two" && g.history.length > 1) {
      const humanPiece: Piece = mode === "dark" ? 1 : 2;
      if (g.history.length > 0 && g.turn !== humanPiece) g = undoMove(g);
    }
    setGame(g); setMessage("Undone."); setThinking(false); setReviewIdx(null);
  }
  function handleSave() { setSavedGame(game); setMessage("Game saved."); }
  function handleLoad() {
    if (!savedGame) { setMessage("No saved game."); return; }
    setGame(savedGame); setThinking(false); setMessage("Loaded saved game."); setReviewIdx(null);
  }

  return (
    <main className="app">
      <header className="hero">
        <h1>Exact Four in a Row</h1>
        <p>{message || gameStatus(game, mode, thinking)}</p>
      </header>

      <section className="toolbar" aria-label="Game controls">
        <button onClick={handleNew}>New</button>
        <button onClick={handleUndo} disabled={game.history.length <= 1 || thinking}>Undo</button>
        <button onClick={handleSave}>Save</button>
        <button onClick={handleLoad}>Load</button>
        <button onClick={() => setShowHistory(v => !v)} aria-pressed={showHistory}>
          {showHistory ? "Hide Moves" : "Show Moves"}
        </button>
        <button onClick={() => setPage(p => p === "game" ? "about" : "game")}>{page === "game" ? "About" : "Game"}</button>
      </section>

      <section className="settings" aria-label="Settings">
        <label>Mode
          <select value={mode} onChange={e => { setMode(e.target.value as Mode); setThinking(false); }}>
            <option value="dark">Play Dark</option>
            <option value="light">Play Light</option>
            <option value="two">2 Player</option>
            <option value="demo">AI Demo</option>
          </select>
        </label>
        <label>Level
          <select value={difficulty} onChange={e => setDifficulty(e.target.value as Difficulty)}>
            <option value="easy">Easy</option>
            <option value="medium">Medium</option>
            <option value="hard">Hard</option>
          </select>
        </label>
      </section>

      {page === "about" ? (
        <section className="about-page">
          <h2>About Exact Four in a Row</h2>
          <p>
            An installable browser port of Exact Four in a Row, part of the Exact Games family for Kindle e-ink devices.
            Rules engine ported from the native GNECT Connect Four implementation in GNOME Games.
          </p>
          <p>
            Dark pieces vs Light pieces. Click the drop buttons above each column to place a piece. First to get four in a
            row horizontally, vertically, or diagonally wins.
          </p>
          <p>
            Attribution: GNOME Games / GNOME Project authors for the original GNECT engine lineage.
            License: GPL-3.0-or-later.
          </p>
          <button onClick={() => { localStorage.removeItem(STORAGE_KEY); handleNew(); }}>Clear Browser Save</button>
        </section>
      ) : (
        <section className={["play-area", showHistory ? "" : "history-hidden"].join(" ")}>
          <div className="board-wrap">
            <div className="col-buttons">
              {Array.from({ length: COLS }, (_, col) => (
                <button
                  key={col}
                  className="col-btn"
                  disabled={!isHumanTurn || thinking || !validCols.has(col) || game.winner !== 0}
                  onClick={() => dropPiece(col)}
                  aria-label={`Drop in column ${col + 1}`}
                >▼</button>
              ))}
            </div>
            <div className="board" aria-label="Connect Four board">
              {Array.from({ length: ROWS }, (_, row) =>
                Array.from({ length: COLS }, (_, col) => {
                  const piece = displayBoard[row][col];
                  const isLast = col === reviewLastCol && row === reviewLastRow;
                  const isWin = winCells.has(`${row},${col}`);
                  return (
                    <div key={`${row}-${col}`} className="cell">
                      <div className={[
                        "hole",
                        piece === 0 ? "empty" : piece === 1 ? "p1" : "p2",
                        isLast ? "last" : "",
                        isWin ? "win" : ""
                      ].join(" ")} />
                    </div>
                  );
                })
              )}
            </div>
          </div>

          {showHistory && (
          <aside className="history">
            <h2>Moves</h2>
            <div className="review-nav">
              <button onClick={() => goReview(0)} disabled={displayIdx === 0} title="Start">◀◀</button>
              <button onClick={() => goReview(Math.max(0, displayIdx - 1))} disabled={displayIdx === 0} title="Previous">◀</button>
              <span className="review-label">{isReviewing ? `${displayIdx}/${totalMoves}` : "Live"}</span>
              <button onClick={() => displayIdx < totalMoves ? goReview(displayIdx + 1) : goReview(null)} disabled={displayIdx >= totalMoves} title="Next">▶</button>
              <button onClick={() => goReview(null)} disabled={!isReviewing} title="Live">▶▶</button>
            </div>
            <ol>
              {game.moves.map((col, i) => (
                <li
                  key={i}
                  className={displayIdx === i + 1 ? "active" : ""}
                  onClick={() => goReview(i + 1)}
                >{i % 2 === 0 ? "●" : "○"} Col {col + 1}</li>
              ))}
            </ol>
          </aside>
          )}
        </section>
      )}

      <footer className="notes">
        <p>Dark plays first. Auto-save always on. Use Save/Load for a manual restore point.</p>
      </footer>
    </main>
  );
}
