// A saved game read as text by the tests ahead of the rules (Vite's `?raw` import, which vitest serves).
declare module '*?raw' {
  const text: string;
  export default text;
}
