import { createServer } from "node:http";
import { readFile, stat } from "node:fs/promises";
import { extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL(".", import.meta.url));
const port = Number(process.env.PORT || 4173);
const mime = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".svg": "image/svg+xml"
};

createServer(async (request, response) => {
  try {
    const urlPath = decodeURIComponent(new URL(request.url, "http://localhost").pathname);
    const requested = urlPath === "/" ? "index.html" : urlPath.replace(/^\/+/, "");
    const filePath = normalize(join(root, requested));
    if (!filePath.startsWith(normalize(root))) throw new Error("invalid path");
    const info = await stat(filePath);
    if (!info.isFile()) throw new Error("not a file");
    response.writeHead(200, { "Content-Type": mime[extname(filePath)] || "application/octet-stream" });
    response.end(await readFile(filePath));
  } catch {
    response.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
    response.end("404 Not Found");
  }
}).listen(port, "127.0.0.1", () => {
  console.log(`Aura-Space 已启动：http://127.0.0.1:${port}`);
});
