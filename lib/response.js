import { readFileSync, existsSync } from 'fs';
import { join } from 'path';
import { NextResponse } from 'next/server';

const PUBLIC = join(process.cwd(), 'public');

export function serveFile(path, contentType = 'text/html') {
  const fullPath = join(PUBLIC, path);
  if (!existsSync(fullPath)) return null;
  return new NextResponse(readFileSync(fullPath, 'utf-8'), {
    headers: { 'Content-Type': contentType },
  });
}

export function html(content, status = 200) {
  return new NextResponse(content, {
    status,
    headers: { 'Content-Type': 'text/html' },
  });
}

export function json(data, status = 200) {
  return NextResponse.json(data, { status });
}
