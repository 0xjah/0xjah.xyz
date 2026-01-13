import { NextResponse } from 'next/server';

const rewrites = {
  '/': '/index.html',
  '/misc': '/misc.html',
  '/gallery': '/gallery.html',
  '/partials/gallery/grid.html': '/api/gallery-grid',
};

export function middleware(request) {
  const dest = rewrites[request.nextUrl.pathname];
  return dest ? NextResponse.rewrite(new URL(dest, request.url)) : NextResponse.next();
}

export const config = {
  matcher: ['/', '/misc', '/gallery', '/partials/gallery/grid.html'],
};
